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
#include <stdlib.h>

#include "vk_local.h"

void R_CreatePhysDevice(VkPhysicalDevice devicehandle, vkphysdevice_t* device)
{
	device->handle = devicehandle;
	vkGetPhysicalDeviceProperties(devicehandle, &device->properties);

	//Find supported extensions for this device.
	vkEnumerateDeviceExtensionProperties(devicehandle, NULL, &device->numextensions, NULL);
	device->extensions = (VkExtensionProperties*)malloc(sizeof(*device->extensions) * device->numextensions);
	if (!device->extensions)
	{
		ri.Sys_Error(ERR_FATAL, "R_CreatePhysDevice: can't allocate device->extensions");
		return;
	}
	vkEnumerateDeviceExtensionProperties(devicehandle, NULL, &device->numextensions, device->extensions);

	//Find supported queue families. 
	vkGetPhysicalDeviceQueueFamilyProperties(devicehandle, &device->numqueuefamilies, NULL);
	device->queuefamilies = (VkQueueFamilyProperties*)malloc(sizeof(*device->queuefamilies) * device->numqueuefamilies);
	if (!device->queuefamilies)
	{
		ri.Sys_Error(ERR_FATAL, "R_CreatePhysDevice: can't allocate device->queuefamilies");
		return;
	}
	vkGetPhysicalDeviceQueueFamilyProperties(devicehandle, &device->numqueuefamilies, device->queuefamilies);

	//Find the surface capabilities for this device.
	//TODO: also done at swapchain creation time. 
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devicehandle, vk_state.surf, &device->surfacecapabilities);

	//Find supported surface formats.
	vkGetPhysicalDeviceSurfaceFormatsKHR(devicehandle, vk_state.surf, &device->numsurfaceformats, NULL);
	device->surfaceformats = (VkSurfaceFormatKHR*)malloc(sizeof(*device->surfaceformats) * device->numsurfaceformats);
	if (!device->surfaceformats)
	{
		ri.Sys_Error(ERR_FATAL, "R_CreatePhysDevice: can't allocate device->surfaceformats");
		return;
	}
	vkGetPhysicalDeviceSurfaceFormatsKHR(devicehandle, vk_state.surf, &device->numsurfaceformats, device->surfaceformats);

	//Find supported present modes
	vkGetPhysicalDeviceSurfacePresentModesKHR(devicehandle, vk_state.surf, &device->numpresentmodes, NULL);
	device->presentmodes = (VkPresentModeKHR*)malloc(sizeof(*device->presentmodes) * device->numpresentmodes);
	if (!device->presentmodes)
	{
		ri.Sys_Error(ERR_FATAL, "R_CreatePhysDevice: can't allocate device->presentmodes");
		return;
	}
	vkGetPhysicalDeviceSurfacePresentModesKHR(devicehandle, vk_state.surf, &device->numpresentmodes, device->presentmodes);

	//Find supported features for this device
	memset(&device->features, 0, sizeof(device->features));
	memset(&device->features13, 0, sizeof(device->features13));
	device->features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	device->features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	device->features.pNext = &device->features13;
	vkGetPhysicalDeviceFeatures2(devicehandle, &device->features);

	vkGetPhysicalDeviceMemoryProperties(devicehandle, &device->memory);
}

void R_FreePhysDevice(vkphysdevice_t* device)
{
	free(device->extensions); device->extensions = 0;
	free(device->queuefamilies); device->numqueuefamilies = 0;
	free(device->surfaceformats); device->numsurfaceformats = 0;
	free(device->presentmodes); device->numpresentmodes = 0;
}

const char* desired_device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#define NUM_DEVICEEXTENSIONS sizeof(desired_device_extensions) / sizeof(*desired_device_extensions)

qboolean R_CheckDeviceSupportsExtensions(vkphysdevice_t* device)
{
	int count = 0;
	for (int i = 0; i < NUM_DEVICEEXTENSIONS; i++)
	{
		for (int j = 0; j < device->numextensions; j++)
		{
			if (!stricmp(desired_device_extensions[i], device->extensions[j].extensionName))
				count++;
		}
	}

	return count == NUM_DEVICEEXTENSIONS ? qtrue : qfalse;
}

qboolean R_CheckPhysicalDevice(vkphysdevice_t* device)
{
	//Most devices will be compatible here, since all they need to do is support a basic RGB format and an reasonable present mode. 
	//But may as well double check to be absolutely sure.

	//Must support swapchain extension, of course. 
	if (!R_CheckDeviceSupportsExtensions(device))
		return qfalse;

	qboolean presentSupported = qfalse, drawSupported = qfalse;
	//Check all the queues to ensure that both draw and present are needed.
	for (int i = 0; i < device->numqueuefamilies; i++)
	{
		if (device->queuefamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			drawSupported = qtrue;

		VkBool32 presentSupportedW;
		VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(device->handle, i, vk_state.surf, &presentSupportedW);
		if (res == VK_SUCCESS && presentSupportedW)
			presentSupported = qtrue;
	}

	if (!drawSupported || !presentSupported)
		return qfalse;

	//Check that the physical device supports the dynamic rendering and synchronization2 features
	if (!device->features13.synchronization2)
		return qfalse;
	if (!device->features13.dynamicRendering)
		return qfalse;
	if (!device->features.features.imageCubeArray)
		return qfalse;
	if (!device->features.features.largePoints)
		return qfalse;

	//Pick the present format ahead of time
	if (device->numsurfaceformats == 1 && device->surfaceformats[0].format == VK_FORMAT_UNDEFINED)
	{
		//all guides I've read, and even some practical source implies that this will occasionally happen for no good reason.
		//they always respond to it byforcing a desirable format
		device->surfaceformat.format = VK_FORMAT_B8G8R8A8_UNORM;
		device->surfaceformat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	}
	else
	{
		int desiredFormat = 0; //Default to the first mode if a desirable one can't be found.
		for (int i = 0; i < device->numsurfaceformats; i++)
		{
			if (device->surfaceformats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
				device->surfaceformats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
			{
				//.. but the desired format is just BGRA8888 with the only default supported color space..
				desiredFormat = i;
				break;
			}
		}

		device->surfaceformat = device->surfaceformats[desiredFormat];
	}

	return qtrue;
}

void R_CreateLogicalDevice(vkphysdevice_t* physdevice, vklogicaldevice_t* device)
{
	float hack = 1.0f;
	VkPhysicalDeviceVulkan12Features features12 = {};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.imagelessFramebuffer = VK_TRUE;

	VkPhysicalDeviceVulkan13Features features13 = {};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.pNext = (void*)&features12;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	VkPhysicalDeviceFeatures2 features = {};
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features.pNext = &features13;
	features.features.imageCubeArray = VK_TRUE;
	features.features.largePoints = VK_TRUE;

	//At most two queue families will be needed, but usually only one will be, since it supports both present and draw.
	//A separate queue may be used later for compute if really needed. 
	int numqueues = 2;
	VkDeviceQueueCreateInfo queueinfo[2] = { {},{} };
	queueinfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueinfo[0].pNext = NULL;
	queueinfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueinfo[1].pNext = NULL;

	int drawqueuefamily = -1, presentqueuefamily = -1;
	for (int i = 0; i < physdevice->numqueuefamilies; i++)
	{
		//todo: just finds the first ATM, like 
		if (drawqueuefamily == -1 && physdevice->queuefamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			drawqueuefamily = i;

		VkBool32 presentSupportedW;
		VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(physdevice->handle, i, vk_state.surf, &presentSupportedW);
		if (presentqueuefamily == -1 && res == VK_SUCCESS && presentSupportedW)
			presentqueuefamily = i;
	}

	if (drawqueuefamily == -1 || presentqueuefamily == -1)
	{
		ri.Sys_Error(ERR_FATAL, "R_CreateLogicalDevice: failed to find a queue");
		return;
	}

	queueinfo[0].queueCount = 1;
	queueinfo[0].queueFamilyIndex = drawqueuefamily;
	queueinfo[0].pQueuePriorities = &hack;
	if (drawqueuefamily == presentqueuefamily)
	{
		device->sharedqueues = qtrue;
		numqueues = 1;
	}
	else
	{
		device->sharedqueues = qfalse;
		queueinfo[1].queueCount = 1;
		queueinfo[1].queueFamilyIndex = presentqueuefamily;
		queueinfo[1].pQueuePriorities = &hack;
	}

	device->drawqueuefamily = drawqueuefamily;
	device->presentqueuefamily = presentqueuefamily;

	VkDeviceCreateInfo createinfo = {};
	createinfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createinfo.pNext = (void*)&features;
	createinfo.pQueueCreateInfos = queueinfo;
	createinfo.queueCreateInfoCount = numqueues;
	createinfo.ppEnabledExtensionNames = desired_device_extensions;
	createinfo.enabledExtensionCount = NUM_DEVICEEXTENSIONS;

	VK_CHECK(vkCreateDevice(physdevice->handle, &createinfo, NULL, &device->handle));
	device->phys_device = physdevice;

	vkGetDeviceQueue(device->handle, drawqueuefamily, 0, &device->drawqueue);
	if (drawqueuefamily == presentqueuefamily)
		device->presentqueue = device->drawqueue;
	else
		vkGetDeviceQueue(device->handle, presentqueuefamily, 0, &device->presentqueue);
}

void VK_CreateCommandPools(vklogicaldevice_t* device)
{
	VkCommandPoolCreateInfo poolinfo = {};
	poolinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolinfo.pNext = NULL;
	poolinfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolinfo.queueFamilyIndex = device->drawqueuefamily;

	VK_CHECK(vkCreateCommandPool(device->handle, &poolinfo, NULL, &device->drawpool));

	//poolinfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	VK_CHECK(vkCreateCommandPool(device->handle, &poolinfo, NULL, &device->resourcepool));

	poolinfo.queueFamilyIndex = device->presentqueuefamily;
	VK_CHECK(vkCreateCommandPool(device->handle, &poolinfo, NULL, &device->presentpool));
}

void VK_DestroyCommandPools(vklogicaldevice_t* device)
{
	vkDestroyCommandPool(device->handle, device->resourcepool, NULL);
	vkDestroyCommandPool(device->handle, device->drawpool, NULL);
	vkDestroyCommandPool(device->handle, device->presentpool, NULL);
}

//Creates a device and all needed resources derived from the device.
qboolean R_CreateDevice()
{
	uint32_t numphysdevices = 0;

	vkEnumeratePhysicalDevices(vk_state.inst, &numphysdevices, NULL);
	VkPhysicalDevice* physdevicelist = (VkPhysicalDevice*)malloc(sizeof(*physdevicelist) * numphysdevices);
	if (!physdevicelist)
	{
		ri.Sys_Error(ERR_FATAL, "R_CreateDevice: Failed to allocate physical device list");
		return qtrue;
	}
	else if (numphysdevices == 0) //this is just to see if doing this check will fix C6385
	{
		ri.Sys_Error(ERR_FATAL, "R_CreateDevice: No physical devices found.");
		return qtrue;
	}

	VkResult res = vkEnumeratePhysicalDevices(vk_state.inst, &numphysdevices, physdevicelist);
	if (res != VK_SUCCESS)
	{
		free(physdevicelist);
		Sys_Error("R_CreateDevice: Failed to enumerate physical devices");
		return qtrue;
	}

	vkphysdevice_t* physdeviceobjs = (vkphysdevice_t*)malloc(sizeof(*physdeviceobjs) * numphysdevices);
	if (!physdeviceobjs)
	{
		ri.Sys_Error(ERR_FATAL, "R_CreateDevice: Failed to allocate physical device objects");
		return qtrue;
	}

	int candidate_device = -1;
	for (int i = 0; i < numphysdevices; i++)
	{
		R_CreatePhysDevice(physdevicelist[i], &physdeviceobjs[i]);
		if (R_CheckPhysicalDevice(&physdeviceobjs[i]))
			candidate_device = i;
	}

	if (candidate_device < 0)
	{
		ri.Sys_Error(ERR_FATAL, "R_CreateDevice: Failed to find a suitable physical device");
		return qtrue;
	}

	for (int i = 0; i < numphysdevices; i++)
	{
		if (i == candidate_device)
			vk_state.phys_device = physdeviceobjs[i];
		else
			R_FreePhysDevice(&physdeviceobjs[i]);
	}

	free(physdevicelist);
	free(physdeviceobjs);

	R_CreateLogicalDevice(&vk_state.phys_device, &vk_state.device);
	VK_CreateResources(&vk_state.device);

	return qfalse;
}

void R_DestroyLogicalDevice(vklogicaldevice_t* device)
{
	vkDestroyDevice(device->handle, NULL);
	device->handle = NULL;
}

//Destroys a device and all its resources derived from it. 
//This function ensures the device is free, then closes all the resources created from the device.
//Once all the resources are destroyed, it will destroy the device. 
void R_DestroyDevice()
{
	vkDeviceWaitIdle(vk_state.device.handle);
	VK_DestroyResources(&vk_state.device);
	R_DestroyLogicalDevice(&vk_state.device);

	R_FreePhysDevice(&vk_state.phys_device);
}

const VkDescriptorPoolSize descriptor_buffer_size[] =
{
	{
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, //type
		64 //descriptorCount
	},
	{
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		32
	},
	{
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		256
	}
};

const VkDescriptorPoolSize descriptor_texture_size =
{
	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	MAX_GLTEXTURES
};

void VK_CreateDescriptorPools(vklogicaldevice_t* device)
{
	VkDescriptorPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.maxSets = 256;
	info.poolSizeCount = 2;
	info.pPoolSizes = descriptor_buffer_size;

	device->misc_descriptor_pool.info = info;
	VK_CreateDescriptorPool(device, &device->misc_descriptor_pool);

	info.maxSets = MAX_GLTEXTURES;
	info.poolSizeCount = 1;
	info.pPoolSizes = &descriptor_texture_size;
	device->image_descriptor_pool.info = info;
	VK_CreateDescriptorPool(device, &device->image_descriptor_pool);
}

void VK_DestroyDescriptorPools(vklogicaldevice_t* device)
{
	VK_DestroyDescriptorPool(device, &device->image_descriptor_pool);
	VK_DestroyDescriptorPool(device, &device->misc_descriptor_pool);
}

void VK_CreateResources(vklogicaldevice_t* device)
{
	VK_CreateDescriptorPools(device);
	VK_CreateCommandPools(device);
	VK_CreatePipelines(device);
	VK_CreateSamplers(device);
	VK_CreateMemory(device);
	VK_CreateStage(device);

	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VK_CHECK(vkCreateSemaphore(device->handle, &info, NULL, &device->swap_acquire));
	VK_CHECK(vkCreateSemaphore(device->handle, &info, NULL, &device->swap_present));
	VK_CHECK(vkCreateSemaphore(device->handle, &info, NULL, &device->swap_present_pre));

	VkFenceCreateInfo fenceinfo = {};
	fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //The fence will be used on the first frame, so it should start signaled

	VK_CHECK(vkCreateFence(device->handle, &fenceinfo, NULL, &device->complete_fence));
}

void VK_DestroyResources(vklogicaldevice_t* device)
{
	vkDestroySemaphore(device->handle, device->swap_acquire, NULL);
	vkDestroySemaphore(device->handle, device->swap_present, NULL);
	vkDestroySemaphore(device->handle, device->swap_present_pre, NULL);
	vkDestroyFence(device->handle, device->complete_fence, NULL);

	VK_DestroyStage(device);
	VK_DestroyMemory(device);
	VK_DestroySamplers(device);
	VK_DestroyPipelines(device);
	VK_DestroyCommandPools(device);
	VK_DestroyDescriptorPools(device);
}
