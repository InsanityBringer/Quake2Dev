/*
Copyright (C) 1997-2001 Id Software, Inc.
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

#include <Windows.h>
#include <vector>
#include <io.h>
#include "vk_local.h"
#include "vkw_win.h"
#include "winquake.h"

vkwstate_t vkw_state;
qboolean VKimp_CreateInstance();
/*
** VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME	"Quake 2"

qboolean VID_CreateWindow(int width, int height, qboolean fullscreen)
{
	WNDCLASS		wc;
	RECT			r;
	cvar_t* vid_xpos, * vid_ypos;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;

	/* Register the frame class */
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)vkw_state.wndproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = vkw_state.hInstance;
	wc.hIcon = 0;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_GRAYTEXT;
	wc.lpszMenuName = 0;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass(&wc))
		ri.Sys_Error(ERR_FATAL, "Couldn't register window class");

	if (fullscreen)
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP | WS_VISIBLE;
	}
	else
	{
		exstyle = 0;
		stylebits = WINDOW_STYLE;
	}

	r.left = 0;
	r.top = 0;
	r.right = width;
	r.bottom = height;

	AdjustWindowRect(&r, stylebits, FALSE);

	w = r.right - r.left;
	h = r.bottom - r.top;

	if (fullscreen)
	{
		x = 0;
		y = 0;
	}
	else
	{
		vid_xpos = ri.Cvar_Get("vid_xpos", "0", 0);
		vid_ypos = ri.Cvar_Get("vid_ypos", "0", 0);
		x = vid_xpos->value;
		y = vid_ypos->value;
	}

	vkw_state.hWnd = CreateWindowEx(
		exstyle,
		WINDOW_CLASS_NAME,
		"Quake 2",
		stylebits,
		x, y, w, h,
		NULL,
		NULL,
		vkw_state.hInstance,
		NULL);

	if (!vkw_state.hWnd)
	{
		ri.Sys_Error(ERR_FATAL, "Couldn't create window");
		return qfalse; //appease the linter
	}

	ShowWindow(vkw_state.hWnd, SW_SHOW);
	UpdateWindow(vkw_state.hWnd);

	// init all the gl stuff for the window
	if (!VKimp_CreateInstance())
	{
		ri.Con_Printf(PRINT_ALL, "VID_CreateWindow() - GLimp_InitGL failed\n");
		return qfalse;
	}

	SetForegroundWindow(vkw_state.hWnd);
	SetFocus(vkw_state.hWnd);

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow(width, height);

	return qtrue;
}

/*
** GLimp_SetMode
*/
rserr_t VKimp_SetMode(unsigned int* pwidth, unsigned int* pheight, int mode, qboolean fullscreen)
{
	int width, height;
	const char* win_fs[] = { "W", "FS" };

	ri.Con_Printf(PRINT_ALL, "Initializing Vulkan display\n");

	ri.Con_Printf(PRINT_ALL, "...setting mode %d:", mode);

	if (!ri.Vid_GetModeInfo(&width, &height, mode))
	{
		ri.Con_Printf(PRINT_ALL, " invalid mode\n");
		return rserr_invalid_mode;
	}

	ri.Con_Printf(PRINT_ALL, " %d %d %s\n", width, height, win_fs[fullscreen]);

	//If a window has already been created, resize it instead
	if (vkw_state.hWnd)
	{
		//VKimp_Shutdown();
		RECT windowRect;
		GetWindowRect(vkw_state.hWnd, &windowRect);

		//The window rect may include the non-client area, so adjust the window by comparing the difference. 
		int deltaWidth = width - *pwidth;
		int deltaHeight = height - *pheight;

		windowRect.right += deltaWidth;
		windowRect.bottom += deltaHeight;

		MoveWindow(vkw_state.hWnd, windowRect.left, windowRect.top, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, TRUE);

		//Inform the game that things have changed. 
		ri.Vid_NewWindow(width, height);

		ri.Con_Printf(PRINT_ALL, "...setting windowed mode\n");

		*pwidth = width;
		*pheight = height;
		vk_state.fullscreen = qfalse;
		return rserr_ok;
	}

	ri.Con_Printf(PRINT_ALL, "...setting windowed mode\n");

	*pwidth = width;
	*pheight = height;
	vk_state.fullscreen = qfalse;
	if (!VID_CreateWindow(width, height, qfalse))
		return rserr_invalid_mode;

	return rserr_ok;
}

void VKimp_Shutdown()
{
	if (vk_state.inst)
	{
		if (vk_state.messenger != VK_NULL_HANDLE)
		{
			vkDestroyDebugUtilsMessengerEXT(vk_state.inst, vk_state.messenger, NULL);
		}
		//R_DestroyDevice();

		if (vk_state.surf)
		{
			vkDestroySurfaceKHR(vk_state.inst, vk_state.surf, NULL);
			vk_state.surf = VK_NULL_HANDLE;
		}

		vkDestroyInstance(vk_state.inst, NULL);
		vk_state.inst = VK_NULL_HANDLE;
	}

	if (vkw_state.hWnd)
	{
		DestroyWindow(vkw_state.hWnd);
		vkw_state.hWnd = NULL;
	}

	if (vkw_state.log_fp)
	{
		fclose(vkw_state.log_fp);
		vkw_state.log_fp = NULL;
	}

	UnregisterClass(WINDOW_CLASS_NAME, vkw_state.hInstance);
}

/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of Vulkan. In practice nothing needs to be done right now. 
** Initialization of the instance could be moved here, but closing the window will need to be restructured.  
*/
bool VKimp_Init(void* hinstance, void* wndproc)
{
	vkw_state.hInstance = (HINSTANCE)hinstance;
	vkw_state.wndproc = wndproc;

	return true;
}

//TODO: this isn't actually Windows specific, so most of the instance creation could be moved out. 
FILE* debugfile;

VkBool32 VKAPI_PTR VulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT		messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT				messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	if (!debugfile)
		return VK_FALSE;

	fprintf(debugfile, "MESSAGE %s\nBits: ", pCallbackData->pMessageIdName);
	if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
		fprintf(debugfile, "GENERAL ");
	if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
		fprintf(debugfile, "VALIDATION ");
	if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		fprintf(debugfile, "PERF ");
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		fprintf(debugfile, "INFO ");
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		fprintf(debugfile, "WARN ");
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		fprintf(debugfile, "ERROR ");
	fprintf(debugfile, "\n---------------------------------------\n");
	fprintf(debugfile, "%s\n", pCallbackData->pMessage);
	fprintf(debugfile, "---------------------------------------\n\n");

	//also echo it to the game console if it won't destroy the console buffer. 
	if (strlen(pCallbackData->pMessage) < 4096)
	{
		ri.Con_Printf(PRINT_ALL, "MESSAGE %s\nBits: ", pCallbackData->pMessageIdName);
		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
			ri.Con_Printf(PRINT_ALL, "GENERAL ");
		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
			ri.Con_Printf(PRINT_ALL, "VALIDATION ");
		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
			ri.Con_Printf(PRINT_ALL, "PERF ");
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			ri.Con_Printf(PRINT_ALL, "INFO ");
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			ri.Con_Printf(PRINT_ALL, "WARN ");
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			ri.Con_Printf(PRINT_ALL, "ERROR ");
		ri.Con_Printf(PRINT_ALL, "\n---------------------------------------\n");
		ri.Con_Printf(PRINT_ALL, "%s\n", pCallbackData->pMessage);
		ri.Con_Printf(PRINT_ALL, "---------------------------------------\n\n");
	}
	//By specs, always return false
	return VK_FALSE;
}
/*
const char* instExtensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
#define NUM_INSTEXTENSIONS sizeof(instExtensions) / sizeof(*instExtensions)*/

qboolean VKimp_CreateInstance()
{
	//Recycle the old instance if it is around
	if (vk_state.inst == VK_NULL_HANDLE)
	{
		VkInstanceCreateInfo instInfo;
		const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

		vk_debug = ri.Cvar_Get("vk_debug", "0", CVAR_ARCHIVE);
		vk_debug_amd = ri.Cvar_Get("vk_debug_amd", "0", 0);

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = NULL;
		appInfo.apiVersion = VK_HEADER_VERSION_COMPLETE;
		appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
		appInfo.engineVersion = VK_MAKE_VERSION(1, 20, 0);
		appInfo.pApplicationName = "Quake 2";
		appInfo.pEngineName = "id tech 2";

		std::vector<const char*> extension_names;
		extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		extension_names.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

		uint32_t num_instance_extensions;
		VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_extensions, nullptr));
		std::vector<VkExtensionProperties> instance_extension; instance_extension.resize(num_instance_extensions);
		VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_extensions, instance_extension.data()));

		for (VkExtensionProperties& properties : instance_extension)
		{
			if (!strcmp(properties.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
			{
				extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				debug_utils_available = true;
			}
		}

		instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instInfo.pNext = NULL;
		instInfo.ppEnabledExtensionNames = extension_names.data();
		instInfo.enabledExtensionCount = extension_names.size();

		instInfo.enabledLayerCount = 0;

		//Check if validation should be used. 
		std::vector<const char*> instlayers;
		if (vk_debug->value)
		{
			std::vector<VkLayerProperties> layerprops;
			uint32_t numLayers;
			int validationAvailable = 0;
			int hasOldValidation = 0, hasNewValidation = 0;

			VkResult res = vkEnumerateInstanceLayerProperties(&numLayers, NULL);
			layerprops.resize(numLayers);
			res = vkEnumerateInstanceLayerProperties(&numLayers, layerprops.data());

			for (int i = 0; i < numLayers; i++)
			{
				if (!stricmp(layerprops[i].layerName, "VK_LAYER_LUNARG_standard_validation"))
					hasOldValidation = 1;
				else if (!stricmp(layerprops[i].layerName, "VK_LAYER_KHRONOS_validation"))
					hasNewValidation = 1;
			}

			if (hasNewValidation) //Always prefer khronos validation, though both should never be present
				validationAvailable = 1;
			else if (hasOldValidation) //If using older implementation (though you probably don't have 1.3 if that's the case...), try old validation
			{
				validationAvailable = 1;
				validationLayerName = "VK_LAYER_LUNARG_standard_validation";
			}

			if (validationAvailable)
				instlayers.push_back(validationLayerName);

			if (vk_debug_amd->value)
			{
				//Ew. Though the only other way to configure a layer is to generate a vk_layer_options.txt?
				int flarg = _putenv("VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT;VALIDATION_CHECK_ENABLE_VENDOR_SPECIFIC_AMD");
			}
		}

		instInfo.enabledLayerCount = instlayers.size();
		instInfo.ppEnabledLayerNames = instlayers.data();
		instInfo.flags = 0;
		instInfo.pApplicationInfo = &appInfo;

		VK_CHECK(vkCreateInstance(&instInfo, NULL, &vk_state.inst));

		volkLoadInstance(vk_state.inst);
	}

	if (vk_state.surf == VK_NULL_HANDLE)
	{
		VkWin32SurfaceCreateInfoKHR surfInfo;
		//A instance will have been created by now, so it's surfin time
		surfInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfInfo.pNext = NULL;
		surfInfo.hinstance = vkw_state.hInstance;
		surfInfo.hwnd = vkw_state.hWnd;
		surfInfo.flags = 0;

		VK_CHECK(vkCreateWin32SurfaceKHR(vk_state.inst, &surfInfo, NULL, &vk_state.surf));
	}

	if (debug_utils_available && vk_state.messenger == VK_NULL_HANDLE)
	{
		VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {};
		debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugMessengerInfo.pfnUserCallback = &VulkanDebugCallback;

		VkResult res = vkCreateDebugUtilsMessengerEXT(vk_state.inst, &debugMessengerInfo, NULL, &vk_state.messenger);
		if (res != VK_SUCCESS)
		{
			fprintf(stderr, "Cannot create debug messenger handle.\n");
		}
		else
		{
			if (!debugfile)
			{
				debugfile = fopen("vk_out.txt", "w");
				setbuf(debugfile, NULL);
			}
			VkDebugUtilsMessengerCallbackDataEXT msg = {};
			msg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
			msg.pMessageIdName = "Info";
			msg.pMessage = "Debug callback is working.";

			vkSubmitDebugUtilsMessageEXT(vk_state.inst, VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &msg);
		}
	}

	if (vk_state.device.handle == VK_NULL_HANDLE)
	{
		//Finally, create the device. The device code is owned by the system-independent code, but is triggered here for convenience
		R_CreateDevice();
	}

	return qtrue;
}

/*
** GLimp_AppActivate
*/
void VKimp_AppActivate(qboolean active)
{
	if (active)
	{
		SetForegroundWindow(vkw_state.hWnd);
		ShowWindow(vkw_state.hWnd, SW_RESTORE);
	}
	else
	{
		//if (vid_fullscreen->value) //TODO
		//	ShowWindow(glw_state.hWnd, SW_MINIMIZE);
	}
}
