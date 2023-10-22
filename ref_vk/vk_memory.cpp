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

//vk_memory.c
//Heap management because I'm a masochist I guess.
//Should probably just use AMD's vulkan memory allocator library. 

//Simple validation of the heap, for debugging. 
void VK_CheckHeap(vkmempool_t* pool)
{
#ifndef NDEBUG
	if (pool->num_allocations < 1)
		ri.Sys_Error(ERR_FATAL, "VK_CheckHeap: Pool has an invalid allocation count (%d)", pool->num_allocations);

	size_t offset = 0;

	for (int i = 0; i < pool->num_allocations; i++)
	{
		if (offset != pool->allocations[i].offset)
			ri.Sys_Error(ERR_FATAL, "VK_CheckHeap: Pool has blocks which don't touch");

		offset += pool->allocations[i].size;
	}

	if (offset != pool->total_pool_size)
		ri.Sys_Error(ERR_FATAL, "VK_CheckHeap: Pool does not reach the end of the allocation");
#endif
}

void VK_AllocatePool(vklogicaldevice_t* device, vkmempool_t* pool, size_t bytes, qboolean device_local, qboolean host_visible)
{
	const char* yesno[] = { "yes", "no" };
	//Find a heap which matches the requested parameters. 
	//device_local_only is absolute, but host_visible is not. 
	int heapindex = -1;
	uint64_t bestsize = 0;
	for (int i = 0; i < device->phys_device->memory.memoryTypeCount; i++)
	{
		VkMemoryType* type = &device->phys_device->memory.memoryTypes[i];
		uint64_t heapsize = device->phys_device->memory.memoryHeaps[type->heapIndex].size;

		if (heapsize < bytes)
			continue;
		else if (host_visible && (type->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
			continue;
		else if (device_local && (type->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0)
			continue;

		//This memory type is suitable, but always prefer the largest heap. 
		if (heapsize > bestsize)
		{
			heapindex = i;
			bestsize = heapsize;
		}
	}

	//Wanted both device local and host visible, but GPU doesn't have it
	if (heapindex < 0 && device_local && host_visible)
	{
		//Try again without device local requirement. 
		VK_AllocatePool(device, pool, bytes, qfalse, host_visible);
		return;
	}

	if (heapindex < 0 || heapindex >= device->phys_device->memory.memoryTypeCount)
	{
		ri.Sys_Error(ERR_FATAL, "VK_AllocatePool: Failed to find a suitable heap (size: %d, device_local: %s, host_visible: %s)", bytes, yesno[device_local ? 1 : 0], yesno[host_visible ? 1 : 0]);
		return;
	}

	pool->total_pool_size = bytes;
	pool->memory_type = heapindex;

	VkMemoryAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	info.allocationSize = bytes;
	info.memoryTypeIndex = heapindex;

	VK_CHECK(vkAllocateMemory(device->handle, &info, NULL, &pool->memory));

	pool->num_allocations = 1;
	pool->allocations[0].free = qtrue;
	pool->allocations[0].offset = 0;
	pool->allocations[0].size = bytes;

	//If a pool is made with host visible memory, map the entire dang range for further use. 
	pool->host_ptr = NULL;
	if (device->phys_device->memory.memoryTypes[heapindex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VK_CHECK(vkMapMemory(device->handle, pool->memory, 0, pool->total_pool_size, 0, &pool->host_ptr));
		//check if the range needs to be flushed
		if ((device->phys_device->memory.memoryTypes[heapindex].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			pool->host_ptr_incoherent = qtrue;
	}
}

void VK_FreePool(vklogicaldevice_t* device, vkmempool_t* pool)
{
	if (!pool->memory)
		return;

	if (pool->host_ptr)
	{
		vkUnmapMemory(device->handle, pool->memory);
	}

	vkFreeMemory(device->handle, pool->memory, NULL);
	pool->memory = VK_NULL_HANDLE;
	pool->num_allocations = 0;
}

vkallocinfo_t VK_AllocateMemory(vkmempool_t* pool, VkMemoryRequirements requirements)
{
	int bestblock = -1;
	size_t alignedoffset = 0;
	vkallocinfo_t allocinfo = {};
	//Scan for a block that will support this allocation. This is made more complicated by strict alignment requirements
	for (int i = 0; i < pool->num_allocations; i++)
	{
		vkmemallocation_t* block = &pool->allocations[i];
		if (!block->free)
			continue;

		//The offset must be aligned, so see if it'll fit within this block
		//by spec alignment is always a power of 2 so this should be safe to do. 
		alignedoffset = (block->offset + requirements.alignment - 1) & ~(requirements.alignment - 1);
		if (alignedoffset >= block->offset + block->size)
			continue;

		if (alignedoffset + requirements.size >= block->offset + block->size)
			continue;

		bestblock = i;
		break;
	}

	if (bestblock == -1)
		ri.Sys_Error(ERR_FATAL, "VK_AllocateMemory: Failed on allocation of %d bytes (aligned to %d)", requirements.size, requirements.alignment);

	vkmemallocation_t* freeblock = &pool->allocations[bestblock];

	//Found a free block, so decide how to slice it up
	//If offset is greater than the block's start, add a new block before it.
	if (alignedoffset > freeblock->offset)
	{
		if (pool->num_allocations == MAX_POOL_ALLOCATIONS)
		{
			ri.Sys_Error(ERR_FATAL, "VK_AllocateMemory: allocation made while pool is at MAX_POOL_ALLOCATIONS");
			return allocinfo;
		}

		for (int i = pool->num_allocations; i > bestblock; i--)
		{
			pool->allocations[i] = pool->allocations[i - 1];
		}

		pool->allocations[bestblock + 1].offset = alignedoffset;
		pool->allocations[bestblock + 1].size = pool->allocations[bestblock].size + pool->allocations[bestblock].offset - alignedoffset;
		pool->allocations[bestblock + 1].free = qtrue;

		pool->allocations[bestblock].size = alignedoffset - freeblock->offset;
		bestblock++;
		freeblock++;
		pool->num_allocations++;
	}

	//If alignedoffset + requirments.size is less than freeblock->size + freeblock->offset, it needs to be chopped up at the end. 
	if (alignedoffset + requirements.size < freeblock->offset + freeblock->size)
	{
		if (pool->num_allocations >= MAX_POOL_ALLOCATIONS)
		{
			ri.Sys_Error(ERR_FATAL, "VK_AllocateMemory: allocation made while pool is at MAX_POOL_ALLOCATIONS");
			return allocinfo;
		}

		for (int i = pool->num_allocations; i > bestblock; i--)
		{
			pool->allocations[i] = pool->allocations[i - 1];
		}

		//at this point, bestblock is aligned with alignedoffset so we can work with it. 

		size_t oldend = pool->allocations[bestblock].offset + pool->allocations[bestblock].size;
		pool->allocations[bestblock].size = requirements.size;
		pool->allocations[bestblock + 1].offset = alignedoffset + requirements.size;
		pool->allocations[bestblock + 1].size = oldend - (alignedoffset + requirements.size);

		pool->num_allocations++;
	}

	freeblock->free = qfalse;
	VK_CheckHeap(pool);

	allocinfo.memory = pool->memory;
	allocinfo.offset = alignedoffset;

	return allocinfo;
}

void VK_FreeMemory(vkmempool_t* pool, vkallocinfo_t* memory)
{
	if (memory->memory != pool->memory)
		ri.Sys_Error(ERR_FATAL, "VK_FreeMemory: Tried to free a block of memory allocated from a different pool");

	int allocationNumber = -1;

	for (int i = 0; i < pool->num_allocations; i++)
	{
		if (memory->offset == pool->allocations[i].offset)
		{
			allocationNumber = i;
			break;
		}
	}

	if (allocationNumber == -1)
		ri.Sys_Error(ERR_FATAL, "VK_FreeMemory: Can't locate block in pool, heap is probably corrupted.");

	vkmemallocation_t* block = &pool->allocations[allocationNumber];
	block->free = qtrue;
	
	//Merge to the previous block, if it is free
	if (allocationNumber > 0 && pool->allocations[allocationNumber - 1].free)
	{
		pool->allocations[allocationNumber - 1].size += block->size;
		for (int i = allocationNumber; i < pool->num_allocations; i++)
		{
			pool->allocations[i] = pool->allocations[i + 1];
		}
		block--;
		allocationNumber--;
		pool->num_allocations--;
	}

	//Merge to the next block, if it is free
	if (allocationNumber < pool->num_allocations - 1 && pool->allocations[allocationNumber + 1].free)
	{
		block->size += pool->allocations[allocationNumber + 1].size;
		for (int i = allocationNumber + 1; i < pool->num_allocations; i++)
		{
			pool->allocations[i] = pool->allocations[i + 1];
		}
		pool->num_allocations--;
	}

	VK_CheckHeap(pool);
}

void VK_CreateMemory(vklogicaldevice_t* device)
{
	VK_AllocatePool(device, &device->device_local_pool, 256 * 1024 * 1024, qtrue, qfalse); //bumped to 256 since escape orbital1 would fill the memory. I should spend some time optimizing memory usage and actually handle extra allocations
	VK_AllocatePool(device, &device->host_visible_pool, 64 * 1024 * 1024, qfalse, qtrue);
	VK_AllocatePool(device, &device->host_visible_device_local_pool, 32 * 1024 * 1024, qtrue, qtrue);
}

void VK_DestroyMemory(vklogicaldevice_t* device)
{
	VK_FreePool(device, &device->device_local_pool);
	VK_FreePool(device, &device->host_visible_pool);
	VK_FreePool(device, &device->host_visible_device_local_pool);
}

void VK_PrintPoolStatus(vkmempool_t* pool)
{
	ri.Con_Printf(PRINT_ALL, "  Memory type: %d\n", pool->memory_type);
	ri.Con_Printf(PRINT_ALL, "  Total size: %u\n", pool->total_pool_size);
	size_t free = pool->total_pool_size;
	for (int i = 0; i < pool->num_allocations; i++)
	{
		if (!pool->allocations[i].free)
			free -= pool->allocations[i].size;
	}
	ri.Con_Printf(PRINT_ALL, "  Bytes free: %u\n", free);
	ri.Con_Printf(PRINT_ALL, "  Number of blocks: %u\n", pool->num_allocations);
	ri.Con_Printf(PRINT_ALL, "  Host pointer: %p\n", pool->host_ptr);
	ri.Con_Printf(PRINT_ALL, "  Host incoherent: %s\n", pool->host_ptr_incoherent ? "yes" : "no");
}

void VK_Memory_f()
{
	for (int i = 0; i < vk_state.phys_device.memory.memoryHeapCount; i++)
	{
		ri.Con_Printf(PRINT_ALL, "Heap %d:\n", i);
		ri.Con_Printf(PRINT_ALL, "  Size: %llu\n", vk_state.phys_device.memory.memoryHeaps[i].size);
		ri.Con_Printf(PRINT_ALL, "  Flags:\n");
		if (vk_state.phys_device.memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			ri.Con_Printf(PRINT_ALL, "    VK_MEMORY_HEAP_DEVICE_LOCAL_BIT\n");
		if (vk_state.phys_device.memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT)
			ri.Con_Printf(PRINT_ALL, "    VK_MEMORY_HEAP_MULTI_INSTANCE_BIT\n");
		ri.Con_Printf(PRINT_ALL, "  Types:\n");
		for (int j = 0; j < vk_state.phys_device.memory.memoryTypeCount; j++)
		{
			if (vk_state.phys_device.memory.memoryTypes[j].heapIndex == i)
			{
				ri.Con_Printf(PRINT_ALL, "    Type %d\n", j);
				ri.Con_Printf(PRINT_ALL, "    Flags:\n");
				if (vk_state.phys_device.memory.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
					ri.Con_Printf(PRINT_ALL, "    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT\n");
				if (vk_state.phys_device.memory.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
					ri.Con_Printf(PRINT_ALL, "    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT\n");
				if (vk_state.phys_device.memory.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
					ri.Con_Printf(PRINT_ALL, "    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT\n");
				if (vk_state.phys_device.memory.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
					ri.Con_Printf(PRINT_ALL, "    VK_MEMORY_PROPERTY_HOST_CACHED_BIT\n");
				if (vk_state.phys_device.memory.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)
					ri.Con_Printf(PRINT_ALL, "    VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD\n");
				if (vk_state.phys_device.memory.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)
					ri.Con_Printf(PRINT_ALL, "    VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD\n");
			}
		}
	}

	ri.Con_Printf(PRINT_ALL, "\nDevice local pool status:\n");
	VK_PrintPoolStatus(&vk_state.device.device_local_pool);

	ri.Con_Printf(PRINT_ALL, "Host visible pool status:\n");
	VK_PrintPoolStatus(&vk_state.device.host_visible_pool);

	ri.Con_Printf(PRINT_ALL, "Host visible & device local pool status:\n");
	VK_PrintPoolStatus(&vk_state.device.host_visible_device_local_pool);
}