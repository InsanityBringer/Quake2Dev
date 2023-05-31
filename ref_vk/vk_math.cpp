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
#include "vk_math.h"

//vk_math.c
//Math specifically for the Vulkan renderer, replacing calls like glOrtho. 

//Putting this here so it isn't completely cluttering vk_rmain.c
matrix_t ortho_matrix;
VkBuffer ortho_projection_buffer;
vkallocinfo_t ortho_memory;
vkdescriptorset_t ortho_descriptor_set;

projectionblock_t projection_block;
VkBuffer persp_projection_buffer;
vkallocinfo_t persp_memory;
vkdescriptorset_t persp_descriptor_set;

extern VkBuffer globals_buffer;

void VK_CreatePerspBuffer()
{
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.size = sizeof(projectionblock_t);
	info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &info, NULL, &persp_projection_buffer));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, persp_projection_buffer, &reqs);

	persp_memory = VK_AllocateMemory(&vk_state.device.host_visible_pool, reqs);

	vkBindBufferMemory(vk_state.device.handle, persp_projection_buffer, persp_memory.memory, persp_memory.offset);

	/*matrix_t* buffer = (matrix_t*)&((byte*)vk_state.device.host_visible_pool.host_ptr)[persp_memory.offset];

	memcpy(buffer, persp_matrix, sizeof(matrix_t));

	if (vk_state.device.host_visible_pool.host_ptr_incoherent)
	{
		VkMappedMemoryRange range =
		{
			.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			.memory = vk_state.device.host_visible_pool.memory,
			.offset = ortho_memory.offset,
			.size = sizeof(matrix_t),
		};
		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}*/

	//Create and update a descriptor set that can be used for this buffer. 
	if (VK_AllocateDescriptor(&vk_state.device.misc_descriptor_pool, 1, &vk_state.device.projection_modelview_layout, &persp_descriptor_set))
	{
		ri.Sys_Error(ERR_FATAL, "Failed to create perspective projection descriptor set");
	}

	VkDescriptorBufferInfo bufferinfo = {};
	bufferinfo.buffer = persp_projection_buffer;
	bufferinfo.offset = 0;
	bufferinfo.range = sizeof(projectionblock_t);
	
	VkWriteDescriptorSet writeinfo = {};
	writeinfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeinfo.dstBinding = 0;
	writeinfo.dstSet = persp_descriptor_set.set;
	writeinfo.pBufferInfo = &bufferinfo;

	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);

	//can't update both of them at once, since they have different stage flags.
	bufferinfo.buffer = globals_buffer;
	bufferinfo.offset = 0;
	bufferinfo.range = sizeof(vkshaderglobal_t);
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeinfo.dstBinding = 1;
	writeinfo.dstSet = persp_descriptor_set.set;
	writeinfo.pBufferInfo = &bufferinfo;
	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);
}

void VK_CreateOrthoBuffer()
{
	VK_Ortho(ortho_matrix, 0, vid.width, 0, vid.height, -99999, 99999);

	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.size = sizeof(matrix_t);
	info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &info, NULL, &ortho_projection_buffer));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, ortho_projection_buffer, &reqs);

	ortho_memory = VK_AllocateMemory(&vk_state.device.host_visible_pool, reqs);

	vkBindBufferMemory(vk_state.device.handle, ortho_projection_buffer, ortho_memory.memory, ortho_memory.offset);

	matrix_t* buffer = (matrix_t*)&((byte*)vk_state.device.host_visible_pool.host_ptr)[ortho_memory.offset];

	memcpy(buffer, ortho_matrix, sizeof(matrix_t));

	if (vk_state.device.host_visible_pool.host_ptr_incoherent)
	{
		VkMappedMemoryRange range = {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = vk_state.device.host_visible_pool.memory;
		range.offset = ortho_memory.offset;
		range.size = sizeof(matrix_t);
		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}

	//Create and update a descriptor set that can be used for this buffer. 
	if (VK_AllocateDescriptor(&vk_state.device.misc_descriptor_pool, 1, &vk_state.device.projection_modelview_layout, &ortho_descriptor_set))
	{
		ri.Sys_Error(ERR_FATAL, "Failed to create ortho projection descriptor set");
	}

	VkDescriptorBufferInfo bufferinfo = {};
	bufferinfo.buffer = ortho_projection_buffer;
	bufferinfo.offset = 0;
	bufferinfo.range = sizeof(matrix_t);

	VkWriteDescriptorSet writeinfo = {};
	writeinfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeinfo.dstBinding = 0;
	writeinfo.dstSet = ortho_descriptor_set.set;
	writeinfo.pBufferInfo = &bufferinfo;

	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);

	//can't update both of them at once, since they have different stage flags.
	//TODO: This shouldn't be duplicated. 
	bufferinfo.buffer = globals_buffer;
	bufferinfo.offset = 0;
	bufferinfo.range = sizeof(vkshaderglobal_t);
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeinfo.dstBinding = 1;
	writeinfo.dstSet = ortho_descriptor_set.set;
	writeinfo.pBufferInfo = &bufferinfo;
	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);

	VK_CreatePerspBuffer();
}

void VK_DestroyOrthoBuffer()
{
	vkDestroyBuffer(vk_state.device.handle, ortho_projection_buffer, NULL);
	vkDestroyBuffer(vk_state.device.handle, persp_projection_buffer, NULL);

	VK_FreeMemory(&vk_state.device.host_visible_pool, &ortho_memory);
	VK_FreeMemory(&vk_state.device.host_visible_pool, &persp_memory);
}

void VK_Ortho(matrix_t matrix, float left, float right, float top, float bottom, float znear, float zfar)
{
	memset(matrix, 0, sizeof(float) * 16);
	matrix[0] = 2 / (right - left);
	matrix[5] = 2 / (bottom - top);
	matrix[10] = 1 / (zfar - znear);
	matrix[12] = (left + right) / (left - right);
	matrix[13] = (bottom + top) / (top - bottom);
	matrix[14] = -znear / (zfar - znear);
	matrix[15] = 1;
}

void VK_Perspective(float left, float right, float top, float bottom, float znear, float zfar)
{
	projectionblock_t* block = (projectionblock_t*)&((byte*)vk_state.device.host_visible_pool.host_ptr)[persp_memory.offset];
	memset(block->projection, 0, sizeof(float) * 16);
	block->projection[0] = 2 * znear / (right - left);

	block->projection[5] = 2 * znear / (bottom - top);

	block->projection[8] = (right + left) / (right - left);
	block->projection[9] = (top + bottom) / (top - bottom);
	block->projection[10] = zfar / (zfar - znear);
	block->projection[11] = 1;

	block->projection[14] = -znear * zfar / (zfar - znear);
}

void VK_UpdateCamera(float* pos, float* rot)
{
	projectionblock_t* block = (projectionblock_t*)&((byte*)vk_state.device.host_visible_pool.host_ptr)[persp_memory.offset];
	block->x = pos[0];
	block->y = pos[1];
	block->z = pos[2];

	block->rotx = rot[0];
	block->roty = rot[1];
	block->rotz = rot[2];

	if (vk_state.device.host_visible_pool.host_ptr_incoherent)
	{
		VkMappedMemoryRange range = {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = vk_state.device.host_visible_pool.memory;
		range.offset = persp_memory.offset;
		range.size = sizeof(projectionblock_t);
		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}
}
