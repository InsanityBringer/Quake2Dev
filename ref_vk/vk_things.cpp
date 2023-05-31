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
#include "vk_data.h"

//r_things.c
//Because the things code was cluttering up vk_rmain.c

#define MAX_MODELVIEWS 2048

VkBuffer modelview_uniform_buffer;
vkallocinfo_t modelview_memory;

uint32_t modelview_packsize;

vkdescriptorset_t modelview_descriptor;

int num_modelviews;

byte* modelview_uniform_data;

void VK_InitThings()
{
	uint32_t minpacksize = vk_state.phys_device.properties.limits.minUniformBufferOffsetAlignment;

	modelview_packsize = sizeof(entitymodelview_t);

	while (modelview_packsize % minpacksize != 0)
	{
		modelview_packsize += 4;
	}

	ri.Con_Printf(PRINT_ALL, "modelview buffer pack size: %u\n", modelview_packsize);

	VkBufferCreateInfo modelview_buffer_info = {};
	modelview_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	modelview_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	modelview_buffer_info.size = modelview_packsize * MAX_MODELVIEWS;
	modelview_buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &modelview_buffer_info, NULL, &modelview_uniform_buffer));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, modelview_uniform_buffer, &reqs);

	modelview_memory = VK_AllocateMemory(&vk_state.device.host_visible_device_local_pool, reqs);
	vkBindBufferMemory(vk_state.device.handle, modelview_uniform_buffer, modelview_memory.memory, modelview_memory.offset);

	modelview_uniform_data = (byte*)vk_state.device.host_visible_device_local_pool.host_ptr;
	modelview_uniform_data += modelview_memory.offset;

	if (VK_AllocateDescriptor(&vk_state.device.misc_descriptor_pool, 1, &vk_state.device.modelview_layout, &modelview_descriptor))
	{
		ri.Sys_Error(ERR_FATAL, "VK_InitThings(): Failed to allocate descriptor set for modelview buffer");
	}

	VkDescriptorBufferInfo bufferinfo = {};
	bufferinfo.buffer = modelview_uniform_buffer;
	bufferinfo.offset = 0;
	bufferinfo.range = modelview_packsize;

	VkWriteDescriptorSet writeinfo = {};
	writeinfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	writeinfo.dstBinding = 0;
	writeinfo.dstSet = modelview_descriptor.set;
	writeinfo.pBufferInfo = &bufferinfo;

	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);
}

void VK_ShutdownThings()
{
	vkDestroyBuffer(vk_state.device.handle, modelview_uniform_buffer, NULL);
}

void VK_StartThings()
{
	num_modelviews = 1;

	//The offset 0 is always origin, for postrenders used by the world.
	entitymodelview_t* ptr = (entitymodelview_t*)modelview_uniform_data;

	memset(ptr, 0, sizeof(*ptr));
}

uint32_t VK_PushThing(entitymodelview_t** view)
{
	uint32_t offset = modelview_packsize * num_modelviews;
	/*entitymodelview_t* ptr = (entitymodelview_t*)(modelview_uniform_data + offset);

	memcpy(ptr, view, sizeof(*view));*/
	entitymodelview_t* ptr = (entitymodelview_t*)(modelview_uniform_data + offset);
	*view = ptr;

	num_modelviews++;

	return offset;
}

void VK_FlushThingRanges()
{
	if (vk_state.device.host_visible_device_local_pool.host_ptr_incoherent)
	{
		VkMappedMemoryRange range = {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.offset = modelview_memory.offset;
		range.size = num_modelviews * modelview_packsize;

		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}
}
