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

//vk_builder.c
//The buffer builder, for building vertex buffers with some convenience.
//It should probably be made dynamic at some point, if I don't before this thing is completed

void R_CreateBufferBuilder(bufferbuilder_t* builder, size_t size)
{
	builder->data = (byte*)malloc(size);
	if (!builder->data)
		ri.Sys_Error(ERR_FATAL, "R_CreateBufferBuilder: Failed on allocation of %d bytes.");

	builder->currentsize = size;
	builder->byteswritten = 0;
	builder->host_visible_buffer = VK_NULL_HANDLE;
	builder->pool = NULL;
}

void R_CreateBufferBuilderFromPool(bufferbuilder_t* builder, vkmempool_t* pool, size_t size)
{
	VkBufferCreateInfo bufferinfo = {};
	bufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferinfo.size = size;
	bufferinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	if (!pool->host_ptr)
	{
		ri.Sys_Error(ERR_FATAL, "R_CreateBufferBuilderFromPool: Memory pool is not host visible.");
	}

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &bufferinfo, NULL, &builder->host_visible_buffer));
	
	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, builder->host_visible_buffer, &reqs);
	builder->pool_allocation = VK_AllocateMemory(pool, reqs);
	vkBindBufferMemory(vk_state.device.handle, builder->host_visible_buffer, builder->pool_allocation.memory, builder->pool_allocation.offset);
	builder->pool = pool;

	builder->data = (byte*)pool->host_ptr; builder->data += builder->pool_allocation.offset;
	builder->currentsize = size;
	builder->byteswritten = 0;
}

void R_FreeBufferBuilder(bufferbuilder_t* builder)
{
	if (!builder->host_visible_buffer)
	{
		free(builder->data);
	}
	else
	{
		VK_FreeMemory(builder->pool, &builder->pool_allocation);
		vkDestroyBuffer(vk_state.device.handle, builder->host_visible_buffer, NULL);
	}

	builder->data = NULL;
}

size_t R_BufferBuilderPut(bufferbuilder_t* builder, void* data, size_t size)
{
	if (builder->byteswritten + size >= builder->currentsize)
	{
		if (builder->host_visible_buffer)
			ri.Sys_Error(ERR_FATAL, "R_BufferBuilderPut: buffer overrun");
		else
		{
			void* newblock = realloc(builder->data, builder->currentsize * 2);
			if (!newblock)
			{
				ri.Sys_Error(ERR_FATAL, "R_BufferBuilderPut: Failed on resizing the current buffer. ");
				return 0;
			}

			builder->data = (byte*)newblock;
			builder->currentsize *= 2;
		}
	}

	memcpy(builder->data + builder->byteswritten, data, size);
	builder->byteswritten += size;

	return builder->currentsize;
}

void R_BufferBuilderFlush(bufferbuilder_t* builder)
{
	if (builder->pool && builder->pool->host_ptr_incoherent)
	{
		VkMappedMemoryRange range = {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = builder->pool->memory;
		range.offset = builder->pool_allocation.offset;
		range.size = builder->currentsize;

		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}
}
