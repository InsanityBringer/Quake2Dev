/*
Copyright (C) 2023 SaladBadger

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "vk_local.h"

//vk_stage.c
//Responsible for getting data from the CPU onto your dang GPU

const VkBufferCreateInfo bufferinfo =
{
	VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
	nullptr,
	0,
	STAGING_BUFFER_SIZE, //size,
	VK_BUFFER_USAGE_TRANSFER_SRC_BIT, //usage,
	VK_SHARING_MODE_EXCLUSIVE //sharingMode
};

const VkFenceCreateInfo fenceinfo =
{
	VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	nullptr,
	VK_FENCE_CREATE_SIGNALED_BIT //flags
};

void VK_CreateStage(vklogicaldevice_t* device)
{
	for (int i = 0; i < NUM_STAGING_BUFFERS; i++)
	{
		VK_CHECK(vkCreateFence(device->handle, &fenceinfo, NULL, &device->stage.fences[i]));
		VK_CHECK(vkCreateBuffer(device->handle, &bufferinfo, NULL, &device->stage.stagebuffers[i]));

		VkMemoryRequirements bufferreqs;
		vkGetBufferMemoryRequirements(device->handle, device->stage.stagebuffers[i], &bufferreqs);
		device->stage.stagebufferoffsets[i] = VK_AllocateMemory(&device->host_visible_pool, bufferreqs);
		VK_CHECK(vkBindBufferMemory(device->handle, device->stage.stagebuffers[i], device->stage.stagebufferoffsets[i].memory, device->stage.stagebufferoffsets[i].offset));

		device->stage.commandbuffers[i] = VK_NULL_HANDLE;
	}

	device->stage.stagenum = 0;
	device->stage.staging = qfalse;
}

void VK_DestroyStage(vklogicaldevice_t* device)
{
	VkResult res = vkWaitForFences(device->handle, 2, device->stage.fences, VK_FALSE, 5000000000ull);

	if (res == VK_TIMEOUT)
		ri.Sys_Error(ERR_FATAL, "VK_DestroyStage: Timed out waiting on render fence");
	else if (res == VK_ERROR_DEVICE_LOST)
		ri.Sys_Error(ERR_FATAL, "VK_DestroyStage: Device lost waiting on render fence");

	for (int i = 0; i < NUM_STAGING_BUFFERS; i++)
	{
		vkDestroyFence(device->handle, device->stage.fences[i], NULL);
		vkDestroyBuffer(device->handle, device->stage.stagebuffers[i], NULL);
		if (device->stage.commandbuffers[i] != VK_NULL_HANDLE)
			vkFreeCommandBuffers(device->handle, device->resourcepool, 1, &device->stage.commandbuffers[i]);
	}
}

VkCommandBuffer VK_GetStageCommandBuffer(vkstage_t* stage)
{
	if (!stage->staging)
	{
		//Wait on the fence, to ensure data isn't overwritten when staging
		VkResult res = vkWaitForFences(vk_state.device.handle, 1, &stage->fences[stage->stagenum], VK_FALSE, 2000000000ull);
		if (res == VK_TIMEOUT)
			ri.Sys_Error(ERR_FATAL, "Timed out waiting on stage fence");
		else if (res == VK_ERROR_DEVICE_LOST)
			ri.Sys_Error(ERR_FATAL, "Device lost waiting on stage fence");

		//Free any previous command buffers that have been used for this stage state.
		if (stage->commandbuffers[stage->stagenum] != VK_NULL_HANDLE)
			vkFreeCommandBuffers(vk_state.device.handle, vk_state.device.resourcepool, 1, &stage->commandbuffers[stage->stagenum]);

		VkCommandBufferAllocateInfo allocinfo = {};
		allocinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocinfo.commandPool = vk_state.device.resourcepool;
		allocinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocinfo.commandBufferCount = 1;

		VK_CHECK(vkAllocateCommandBuffers(vk_state.device.handle, &allocinfo, &stage->commandbuffers[stage->stagenum]));

		stage->staging = qtrue;

		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(stage->commandbuffers[stage->stagenum], &info);
	}

	return stage->commandbuffers[stage->stagenum];
}

void VK_StartStagingImageData(vkstage_t* stage, image_t* dest)
{
	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(stage);

	VkImageMemoryBarrier2 imgbarrier = {};
	imgbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imgbarrier.image = dest->handle;
	imgbarrier.oldLayout = dest->renderdochack;
	imgbarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imgbarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	imgbarrier.srcAccessMask = VK_ACCESS_2_NONE;
	imgbarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	imgbarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgbarrier.subresourceRange.baseArrayLayer = 0;
	imgbarrier.subresourceRange.baseMipLevel = 0;
	imgbarrier.subresourceRange.layerCount = dest->layers;
	imgbarrier.subresourceRange.levelCount = dest->miplevels;

	VkDependencyInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	info.imageMemoryBarrierCount = 1;
	info.pImageMemoryBarriers = &imgbarrier;

	vkCmdPipelineBarrier2(cmdbuffer, &info);
}

void VK_StartStagingImageLayerData(vkstage_t* stage, image_t* dest, int layernum)
{
	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(stage);

	VkImageMemoryBarrier2 imgbarrier = {};
	imgbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imgbarrier.image = dest->handle;
	imgbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imgbarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imgbarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	imgbarrier.srcAccessMask = VK_ACCESS_2_NONE;
	imgbarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	imgbarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgbarrier.subresourceRange.baseArrayLayer = layernum;
	imgbarrier.subresourceRange.baseMipLevel = 0;
	imgbarrier.subresourceRange.layerCount = 1;
	imgbarrier.subresourceRange.levelCount = dest->miplevels;

	VkDependencyInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	info.imageMemoryBarrierCount = 1;
	info.pImageMemoryBarriers = &imgbarrier;

	vkCmdPipelineBarrier2(cmdbuffer, &info);
}

void VK_StageImageData(vkstage_t* stage, image_t* dest, int miplevel, int width, int height, void* data)
{
	size_t datasize = width * height * dest->data_size;
	//If the data is larger than the staging buffer (unlikely), yell at me to increase the amount that can be staged. 
	if (datasize >= STAGING_BUFFER_SIZE)
		ri.Sys_Error(ERR_FATAL, "VK_StageImageData: data is too large, increase STAGING_BUFFER_SIZE");

	//VkCmdCopyBufferToImage requires bufferOffset to be aligned. If totalbyteswritten isn't aligned, extra bytes are added until it is
	while ((stage->totalbyteswritten % dest->data_size) != 0)
		stage->totalbyteswritten++;

	//However, if the data fits but would push the bytes written over the limit, flush the stage buffer and swap pages
	if (stage->totalbyteswritten + datasize >= STAGING_BUFFER_SIZE)
		VK_FlushStage(stage);

	size_t offset = stage->stagebufferoffsets[stage->stagenum].offset + stage->totalbyteswritten;
	byte* ptr = (byte*)vk_state.device.host_visible_pool.host_ptr;
	ptr += offset;

	memcpy(ptr, data, datasize);

	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(stage);

	VkBufferImageCopy copy = {};
	copy.bufferOffset = stage->totalbyteswritten;
	copy.bufferRowLength = width;
	copy.bufferImageHeight = height;
	copy.imageExtent.width = width;
	copy.imageExtent.height = height;
	copy.imageExtent.depth = 1;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.baseArrayLayer = 0;
	copy.imageSubresource.layerCount = 1;
	copy.imageSubresource.mipLevel = miplevel;

	vkCmdCopyBufferToImage(cmdbuffer, stage->stagebuffers[stage->stagenum], dest->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

	stage->totalbyteswritten += datasize;
}

void VK_StageLayeredImageData(vkstage_t* stage, image_t* dest, int layer, int width, int height, void* data)
{
	assert(dest->miplevels == 1);

	size_t datasize = width * height * dest->data_size;
	//If the data is larger than the staging buffer (unlikely), yell at me to increase the amount that can be staged. 
	if (datasize >= STAGING_BUFFER_SIZE)
		ri.Sys_Error(ERR_FATAL, "VK_StageImageData: data is too large, increase STAGING_BUFFER_SIZE");

	//VkCmdCopyBufferToImage requires bufferOffset to be aligned. If totalbyteswritten isn't aligned, extra bytes are added until it is
	while ((stage->totalbyteswritten % dest->data_size) != 0)
		stage->totalbyteswritten++;

	//However, if the data fits but would push the bytes written over the limit, flush the stage buffer and swap pages
	if (stage->totalbyteswritten + datasize >= STAGING_BUFFER_SIZE)
		VK_FlushStage(stage);

	size_t offset = stage->stagebufferoffsets[stage->stagenum].offset + stage->totalbyteswritten;
	byte* ptr = (byte*)vk_state.device.host_visible_pool.host_ptr;
	ptr += offset;

	memcpy(ptr, data, datasize);

	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(stage);


	VkBufferImageCopy copy = {};
	copy.bufferOffset = stage->totalbyteswritten;
	copy.bufferRowLength = width;
	copy.bufferImageHeight = height;
	copy.imageExtent.width = width;
	copy.imageExtent.height = height;
	copy.imageExtent.depth = 1;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.baseArrayLayer = layer;
	copy.imageSubresource.layerCount = 1;
	copy.imageSubresource.mipLevel = 0;

	vkCmdCopyBufferToImage(cmdbuffer, stage->stagebuffers[stage->stagenum], dest->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

	stage->totalbyteswritten += datasize;
}

void VK_StageLayeredImageDataSub(vkstage_t* stage, image_t* dest, int layer, int xoffs, int yoffs, int width, int height, void* data)
{
	assert(dest->miplevels == 1);

	size_t datasize = width * height * dest->data_size; 
	//If the data is larger than the staging buffer (unlikely), yell at me to increase the amount that can be staged. 
	if (datasize >= STAGING_BUFFER_SIZE)
		ri.Sys_Error(ERR_FATAL, "VK_StageImageData: data is too large, increase STAGING_BUFFER_SIZE");

	//VkCmdCopyBufferToImage requires bufferOffset to be aligned. If totalbyteswritten isn't aligned, extra bytes are added until it is
	while ((stage->totalbyteswritten % dest->data_size) != 0)
		stage->totalbyteswritten++;

	//However, if the data fits but would push the bytes written over the limit, flush the stage buffer and swap pages
	if (stage->totalbyteswritten + datasize >= STAGING_BUFFER_SIZE)
		VK_FlushStage(stage);

	size_t offset = stage->stagebufferoffsets[stage->stagenum].offset + stage->totalbyteswritten;
	byte* ptr = (byte*)vk_state.device.host_visible_pool.host_ptr;
	ptr += offset;

	memcpy(ptr, data, datasize);

	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(stage);

	VkBufferImageCopy copy = {};
	copy.bufferOffset = stage->totalbyteswritten;
	copy.bufferRowLength = width;
	copy.bufferImageHeight = height;
	copy.imageOffset.x = xoffs;
	copy.imageOffset.y = yoffs;
	copy.imageExtent.width = width;
	copy.imageExtent.height = height;
	copy.imageExtent.depth = 1;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.baseArrayLayer = layer;
	copy.imageSubresource.layerCount = 1;
	copy.imageSubresource.mipLevel = 0;

	vkCmdCopyBufferToImage(cmdbuffer, stage->stagebuffers[stage->stagenum], dest->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

	stage->totalbyteswritten += datasize;
}

void VK_FinishStagingImage(vkstage_t* stage, image_t* dest)
{
	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(stage);

	VkImageMemoryBarrier2 imgbarrier = {};
	imgbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imgbarrier.image = dest->handle;
	imgbarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		//also assuming the intention
	imgbarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgbarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	imgbarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		//TODO: This assumes the only purpose of a image is to be sampled from a fragment shader, 
		//but given the game's age this assumption is basically 100% true. 
	imgbarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	imgbarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgbarrier.subresourceRange.baseArrayLayer = 0;
	imgbarrier.subresourceRange.baseMipLevel = 0;
	imgbarrier.subresourceRange.layerCount = dest->layers;
	imgbarrier.subresourceRange.levelCount = dest->miplevels;

	dest->renderdochack = imgbarrier.newLayout;

	VkDependencyInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	info.imageMemoryBarrierCount = 1;
	info.pImageMemoryBarriers = &imgbarrier;

	vkCmdPipelineBarrier2(cmdbuffer, &info);
}

void VK_FinishStagingImageLayer(vkstage_t* stage, image_t* dest, int layernum)
{
	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(stage);

	VkImageMemoryBarrier2 imgbarrier = {};
	imgbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imgbarrier.image = dest->handle;
	imgbarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	//also assuming the intention
	imgbarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgbarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	imgbarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	//TODO: This assumes the only purpose of a image is to be sampled from a fragment shader, 
	//but given the game's age this assumption is basically 100% true. 
	imgbarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	imgbarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgbarrier.subresourceRange.baseArrayLayer = layernum;
	imgbarrier.subresourceRange.baseMipLevel = 0;
	imgbarrier.subresourceRange.layerCount = 1;
	imgbarrier.subresourceRange.levelCount = dest->miplevels;

	VkDependencyInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	info.imageMemoryBarrierCount = 1;
	info.pImageMemoryBarriers = &imgbarrier;

	vkCmdPipelineBarrier2(cmdbuffer, &info);
}

void VK_StageBufferData(vkstage_t* stage, VkBuffer dest, void* src, size_t srcoffset, size_t destoffset, size_t size)
{
	//If the data is larger than the staging buffer (unlikely), yell at me to increase the amount that can be staged. 
	if (size >= STAGING_BUFFER_SIZE)
		ri.Sys_Error(ERR_FATAL, "VK_StageImageData: data is too large, increase STAGING_BUFFER_SIZE");

	//However, if the data fits but would push the bytes written over the limit, flush the stage buffer and swap pages
	if (stage->totalbyteswritten + size >= STAGING_BUFFER_SIZE)
		VK_FlushStage(stage);

	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(stage);

	size_t offset = stage->stagebufferoffsets[stage->stagenum].offset + stage->totalbyteswritten + srcoffset;
	byte* ptr = (byte*)vk_state.device.host_visible_pool.host_ptr;
	ptr += offset;

	memcpy(ptr, src, size);

	VkBufferCopy copyinfo = {};
	copyinfo.srcOffset = stage->totalbyteswritten;
	copyinfo.dstOffset = destoffset;
	copyinfo.size = size;

	vkCmdCopyBuffer(cmdbuffer, stage->stagebuffers[stage->stagenum], dest, 1, &copyinfo);

	stage->totalbyteswritten += size;
}

qboolean VK_DataStaged(vkstage_t* stage)
{
	return stage->staging;
}

void VK_FlushStage(vkstage_t* stage)
{
	if (!stage->staging)
		return;

	unsigned int previous = (stage->stagenum - 1) % NUM_STAGING_BUFFERS;

	vkEndCommandBuffer(stage->commandbuffers[stage->stagenum]);

	vkResetFences(vk_state.device.handle, 1, &stage->fences[stage->stagenum]);

	VkCommandBufferSubmitInfo cmdinfo = {};
	cmdinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdinfo.commandBuffer = stage->commandbuffers[stage->stagenum];

	VkSubmitInfo2 submitinfo = {};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitinfo.commandBufferInfoCount = 1;
	submitinfo.pCommandBufferInfos = &cmdinfo;

	vkQueueSubmit2(vk_state.device.drawqueue, 1, &submitinfo, stage->fences[stage->stagenum]);

	stage->totalbyteswritten = 0;
	stage->stagenum = (stage->stagenum + 1) % NUM_STAGING_BUFFERS;
	stage->staging = qfalse;
}

void VK_FinishStage(vkstage_t* stage)
{
	stage->staging = qfalse;
}
