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

VkSwapchainKHR swapchain = VK_NULL_HANDLE;
uint32_t numimages;
VkImage* images;
VkImageView* views;

uint32_t currentswapimage;

void VK_FreeSwapchainViews()
{
	if (views)
	{
		for (int i = 0; i < numimages; i++)
		{
			vkDestroyImageView(vk_state.device.handle, views[i], NULL);
		}
		free(views);
		views = NULL;
	}

	if (images)
		free(images);
}

//Create a swapchain. Returns qfalse if no error, qtrue on error.
//Some errors are not fatal, and the game should continue running in their presence.
qboolean VK_CreateSwapchain()
{
	VK_FreeSwapchainViews();

	//just to be safe, ensure the current surface format is up to date.
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_state.phys_device.handle, vk_state.surf, &vk_state.phys_device.surfacecapabilities);

	//Is the window minimized? (TODO: is this possible in Quake 2?)
	if (vk_state.phys_device.surfacecapabilities.currentExtent.width == 0 ||
		vk_state.phys_device.surfacecapabilities.currentExtent.height == 0)
	{
		return qtrue;
	}

	//pick present mode. This will always be immediate, unless vsync is set, to which mailbox or fifo will be set, based on availability
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR desiredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (vk_vsync->value)
		desiredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	else
		desiredPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

	for (int i = 0; i < vk_state.phys_device.numpresentmodes; i++)
	{
		if (vk_state.phys_device.presentmodes[i] == desiredPresentMode)
		{
			presentMode = desiredPresentMode;
			break;
		}
	}
	
	const char* debugstr = "VK_PRESENT_MODE_FIFO_KHR";
	if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		debugstr = "VK_PRESENT_MODE_IMMEDIATE_KHR";
	else if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		debugstr = "VK_PRESENT_MODE_MAILBOX_KHR";

	ri.Con_Printf(PRINT_ALL, "Using present mode %s\n", debugstr);

	VkSwapchainCreateInfoKHR swapchaininfo;
	swapchaininfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchaininfo.pNext = NULL;
	swapchaininfo.surface = vk_state.surf;
	swapchaininfo.minImageCount = min(2, vk_state.phys_device.surfacecapabilities.maxImageCount);
	swapchaininfo.imageExtent = vk_state.phys_device.surfacecapabilities.currentExtent;
	swapchaininfo.imageFormat = vk_state.phys_device.surfaceformat.format;
	swapchaininfo.imageColorSpace = vk_state.phys_device.surfaceformat.colorSpace;
	swapchaininfo.imageArrayLayers = 1;
	swapchaininfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	//do I really have to support this? Is there any implementation (presumably a software one?) that enforces this?
	uint32_t families[] = { vk_state.device.drawqueuefamily, vk_state.device.presentqueuefamily };
	if (vk_state.device.drawqueuefamily != vk_state.device.presentqueuefamily)
	{
		swapchaininfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchaininfo.pQueueFamilyIndices = families;
		swapchaininfo.queueFamilyIndexCount = 2;
	}
	else
	{
		swapchaininfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	swapchaininfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchaininfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchaininfo.presentMode = presentMode;
	swapchaininfo.flags = 0;
	swapchaininfo.clipped = qtrue;
	swapchaininfo.oldSwapchain = swapchain;

	VkSwapchainKHR newSwapchain;
	VkResult res = vkCreateSwapchainKHR(vk_state.device.handle, &swapchaininfo, NULL, &newSwapchain);
	if (res != VK_SUCCESS)
	{
		return qtrue;
	}

	if (swapchain)
	{
		vkDestroySwapchainKHR(vk_state.device.handle, swapchain, NULL);
	}

	swapchain = newSwapchain;

	res = vkGetSwapchainImagesKHR(vk_state.device.handle, swapchain, &numimages, NULL);
	if (res != VK_SUCCESS)
	{
		ri.Sys_Error(ERR_FATAL, "VK_CreateSwapchain(): Failed to obtain swapchain image count!");
		return qtrue;
	}
	images = (VkImage*)malloc(sizeof(*images) * numimages);
	if (!images)
	{
		ri.Sys_Error(ERR_FATAL, "VK_CreateSwapchain(): Failed to allocate swapchain images!");
		return qtrue;
	}
	res = vkGetSwapchainImagesKHR(vk_state.device.handle, swapchain, &numimages, images);
	if (res != VK_SUCCESS)
	{
		ri.Sys_Error(ERR_FATAL, "VK_CreateSwapchain(): Failed to obtain swapchain images!");
		return qtrue;
	}

	views = (VkImageView*)malloc(sizeof(*views) * numimages);
	if (!views)
	{
		ri.Sys_Error(ERR_FATAL, "VK_CreateSwapchain(): Failed to allocate swapchain image views!");
		return qtrue;
	}
	for (int i = 0; i < numimages; i++)
	{
		VkImageViewCreateInfo viewinfo;
		viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewinfo.pNext = NULL;
		viewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewinfo.format = vk_state.phys_device.surfaceformat.format;
		viewinfo.image = images[i];
		viewinfo.flags = 0;
		viewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewinfo.subresourceRange.baseMipLevel = 0;
		viewinfo.subresourceRange.levelCount = 1;
		viewinfo.subresourceRange.baseArrayLayer = 0;
		viewinfo.subresourceRange.layerCount = 1;
		viewinfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewinfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewinfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewinfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		res = vkCreateImageView(vk_state.device.handle, &viewinfo, NULL, &views[i]);
		if (res != VK_SUCCESS)
		{
			ri.Sys_Error(ERR_FATAL, "VK_CreateSwapchain(): Failed to allocate image view %d!", i);
			return qtrue;
		}
	}

	return qfalse;
}

void VK_DestroySwapchain()
{
	if (views)
	{
		for (int i = 0; i < numimages; i++)
			vkDestroyImageView(vk_state.device.handle, views[i], NULL);
	}

	if (swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(vk_state.device.handle, swapchain, NULL);
	}
}

VkImage* VK_GetSwapchainImage(VkSemaphore signal, VkImageView* view)
{
	if (swapchain == VK_NULL_HANDLE)
		return NULL;
	VkResult res = vkAcquireNextImageKHR(vk_state.device.handle, swapchain, 500000000ULL, signal, VK_NULL_HANDLE, &currentswapimage);

	//TODO: Can any suboptimal situations occur within the confines of Quake 2? any mode set seems to completely destroy the context. 
	if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR)
	{
		if (view)
			*view = views[currentswapimage];
		return &images[currentswapimage];
	}
	else
		return NULL;
}

void VK_Present(VkSemaphore wait)
{
	VkResult swapchainRes;
	VkPresentInfoKHR presentinfo = {};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.pNext = NULL;
	presentinfo.swapchainCount = 1;
	presentinfo.pSwapchains = &swapchain;
	presentinfo.pResults = &swapchainRes;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &wait;
	presentinfo.pImageIndices = &currentswapimage;

	VkResult res = vkQueuePresentKHR(vk_state.device.presentqueue, &presentinfo);
	//TODO: Determine if these will ever happen with Quake 2 and need handling
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_ERROR_SURFACE_LOST_KHR)
	{
		ri.Sys_Error(ERR_FATAL, "VK_Present: Surface is lost or out of date!");
	}
	else if (res != VK_SUCCESS)
	{
		ri.Con_Printf(PRINT_DEVELOPER, "Result %d when presenting image", res);
	}
}
