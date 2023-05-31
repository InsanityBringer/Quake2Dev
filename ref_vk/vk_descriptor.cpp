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
#include <malloc.h>
#include "vk_local.h"

//vk_descriptor.c
//Implements descriptor set pools. These pools will automatically chain additional pools when an allocation isn't possible. 

vkdpoollink_t* VK_CreateLink(vkdpool_t* pool)
{
	vkdpoollink_t* link = (vkdpoollink_t*)malloc(sizeof(*link));
	if (!link)
	{
		ri.Sys_Error(ERR_FATAL, "Failed to allocate a new descriptor pool link");
		return NULL;
	}

	VK_CHECK(vkCreateDescriptorPool(pool->device, &pool->info, NULL, &link->pool));
	link->parent = pool;
	link->next = NULL;

	return link;
}

void VK_DestroyLinks(VkDevice device, vkdpoollink_t* link)
{
	//Recursively destroy the further links
	if (link->next)
	{
		VK_DestroyLinks(device, link->next);
	}

	vkDestroyDescriptorPool(device, link->pool, NULL);

	free(link);
}

void VK_CreateDescriptorPool(vklogicaldevice_t* device, vkdpool_t* pool)
{
	pool->device = device->handle;
	pool->links = VK_CreateLink(pool);
}

void VK_DestroyDescriptorPool(vklogicaldevice_t* device, vkdpool_t* pool)
{
	VK_DestroyLinks(device->handle, pool->links);
}

qboolean VK_AllocateDescriptor(vkdpool_t* pool, uint32_t count, VkDescriptorSetLayout* layouts, vkdescriptorset_t* results)
{
	//This isn't the most elegant code, but run through the pools until a descriptor allocation works
	vkdpoollink_t* link = pool->links;
	qboolean newpool = qfalse; //true when a new pool was created this frame

	//Allocate a temporary buffer for the descriptor sets, since they need to be converted into vkdescriptorset_ts

	VkDescriptorSet* sets;
	__try
	{
		sets = (VkDescriptorSet*)alloca(sizeof(*sets) * count);
	}
	__except (GetExceptionCode() == STATUS_STACK_OVERFLOW)
	{
		_resetstkoflw();
		ri.Sys_Error(ERR_FATAL, "VK_AllocateDescriptor: stack overflow on descriptor allocation");
		return qtrue;
	}

	VkDescriptorSetAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.descriptorSetCount = count;
	info.pSetLayouts = layouts;

	while (link)
	{
		info.descriptorPool = link->pool;
		VkResult res = vkAllocateDescriptorSets(pool->device, &info, sets);
		//out of memory, this is pretty serious. may as well just Sys_Error here
		if (res == VK_ERROR_OUT_OF_HOST_MEMORY || res == VK_ERROR_OUT_OF_DEVICE_MEMORY)
		{
			return qtrue;
		}
		//out of pool memory, so spin up another pool
		else if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL)
		{
			if (link->next)
				link = link->next;
			else
			{
				if (newpool)
				{
					ri.Sys_Error(ERR_FATAL, "VK_AllocateDescriptor: Desired descriptor sets cannot be allocated from specified pool");
				}
				newpool = qtrue;
				ri.Con_Printf(PRINT_DEVELOPER, "VK_AllocateDescriptor creating new pool\n");
				link->next = VK_CreateLink(pool);
				link = link->next;
			}
			continue;
		}

		break;
	}

	//Write the descriptors into the results
	for (int i = 0; i < count; i++)
	{
		results[i].source = link;
		results[i].set = sets[i];
	}

	return qfalse;
}

void VK_ResetPool(vkdpool_t* pool)
{
	vkdpoollink_t* link = pool->links;

	while (link)
	{
		vkResetDescriptorPool(vk_state.device.handle, link->pool, 0);
	}
}

void VK_FreeDescriptors(uint32_t count, vkdescriptorset_t* descriptors)
{
	if (count == 0) return;

	//this is the less efficient way, but it'll make it easy to free things sourced from many pools. 
	for (int i = 0; i < count; i++)
	{
		vkdescriptorset_t set = descriptors[i];
		vkFreeDescriptorSets(set.source->parent->device, set.source->pool, 1, &set.set);
	}
}
