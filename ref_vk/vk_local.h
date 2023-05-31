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
#pragma once

#ifdef _WIN32
#  include <windows.h>
#endif

#include <stdio.h>
#include "volk.h"

#include "ref.h"

#define	REF_VERSION	"Vulkan 0.01"

#ifndef __VIDDEF_T
#define __VIDDEF_T
typedef struct
{
	unsigned		width, height;			// coordinates from main game
} viddef_t;
#endif

extern	viddef_t	vid;

#define VK_CHECK(func) if (func != VK_SUCCESS) { ri.Sys_Error(ERR_FATAL, __FILE__ " %d: Statement "#func" failed!", __LINE__); }

//This should be cleaned up some, if I ever make the leap to C++ I probbaly will use vectors here
#define MAX_POOL_ALLOCATIONS 4000
//A pool, wrapping a vulkan memory allocation. For now, there's only one allocation for pool, since the amount of data Quake 2 uses is relatively low.
typedef struct
{
	size_t offset;
	size_t size;
	qboolean free;
} vkmemallocation_t;

typedef struct
{
	size_t			offset;
	VkDeviceMemory	memory;
} vkallocinfo_t;

typedef struct
{
	VkDeviceMemory		memory;
	int					memory_type;		//informational, for debug. 
	size_t				total_pool_size;
	int					num_allocations;
	void* host_ptr;							//Pointer to access the data from the host. This is NULL if the memory pool is device local and not host visible. 
	qboolean			host_ptr_incoherent;				//If this is set, a vkFlushMappedMemoryRanges must be performed when using host_ptr
	vkmemallocation_t	allocations[MAX_POOL_ALLOCATIONS];
} vkmempool_t;

struct vkdpool_s;

//Linked structure of descriptor pools
typedef struct vkdpoollink_s
{
	VkDescriptorPool pool;
	struct vkdpool_s* parent;
	struct vkdpoollink_s* next;
} vkdpoollink_t;

//Needed structure to determine which pool the allocation came out of.
typedef struct
{
	vkdpoollink_t* source;
	VkDescriptorSet set;
} vkdescriptorset_t;

typedef struct vkdpool_s
{
	VkDevice device;
	VkDescriptorPoolCreateInfo info;
	vkdpoollink_t* links;
} vkdpool_t;

/*

  skins will be outline flood filled and mip mapped
  pics and sprites with alpha will be outline flood filled
  pic won't be mip mapped

  model skin
  sprite frame
  wall texture
  pic

*/

typedef enum
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef struct image_s
{
	char				name[MAX_QPATH];				// game path, including extension
	imagetype_t			type;
	int					width, height;					// source image
	int					miplevels;						// total miplevels
	int					layers;							// layers, almost always 1. 
	int					upload_width, upload_height;	// after power of two and picmip
	int					registration_sequence;			// 0 = free
	struct msurface_s*	texturechain;					// for sort-by-texture world drawing
	VkImage				handle;							//image handle
	vkallocinfo_t		memory;							//image memory
	VkImageView			viewhandle;						//image view handle for things that need it. 
	vkdescriptorset_t	descriptor;						//descriptor for sampling this texture in a shader
	float				sl, tl, sh, th;					// 0,0 - 1,1 unless part of the scrap
	bool				scrap;
	bool				has_alpha;
	bool				locked;							// if true, image won't be purged on a VK_FreeUnusedImages
	bool				paletted;
	size_t				data_size;						//Set on creation, used to determine how much data to copy when uploading. 
	VkImageLayout		renderdochack;					//crutch for RenderDoc, since it treats transitioning from a VK_IMAGE_LAYOUT_UNDEFINED as a clear.
} image_t;

extern image_t	vktextures[];

//===================================================================

typedef enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

#include "vk_model.h"

//Push constant range for models
typedef struct
{
	vec3_t origin;
	int frame_offset;
	vec3_t rot;
	int old_frame_offset;
	vec3_t color;
	float backlerp;
	vec3_t move;
	float swell;
	float xscale; //for weapons, mostly
	float lightscale; //for shells, mostly
	float pad; //for nothing, mostly
	float alpha;
} entitymodelview_t;

#define		MAX_GLTEXTURES	1100 //bumped up this a bit due to extra images being allocated. 

extern	image_t* r_notexture;
extern	image_t* r_particletexture;
extern	image_t* r_whitetexture; //Used to avoid another pipeline for shells. 
extern	entity_t* currententity;
extern	model_t* currentmodel;
extern	model_t flash_model;

extern bool effect_visible, effect2_visible;
extern entity_t effect_ent, effect2_ent;
extern float muzzle_flash_detect_time;

extern	int			r_visframecount;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys, c_num_light_pushes;

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_newrefdef;
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;
extern	mleaf_t* current_viewleaf;

extern cvar_t* vk_round_down;
extern cvar_t* vk_no_po2;
extern cvar_t* vk_picmip;
extern cvar_t* vk_vsync;
extern cvar_t* vk_dynamiclightmode;
extern cvar_t* vk_lockpvs;
extern cvar_t* vk_modulate;
extern cvar_t* vk_lightmode; //0 = original GL lighting, 1 = overbright lighting
extern cvar_t* vk_lightmap;
extern cvar_t* vk_modelswell;
extern cvar_t* vk_debug;
extern cvar_t* vk_debug_amd;
extern cvar_t* vk_particle_size;
extern cvar_t* vk_particle_size_min;
extern cvar_t* vk_particle_size_max;
extern cvar_t* vk_muzzleflash;
extern cvar_t* vk_texturemode;
extern cvar_t* vk_particle_mode;

extern	cvar_t* vid_fullscreen;
extern	cvar_t* vid_gamma;
extern	cvar_t* intensity;

extern cvar_t* r_novis;
extern cvar_t* r_lightlevel;
extern cvar_t* r_lerpmodels;

extern cvar_t* vk_intensity;

extern bool debug_utils_available;

void R_LightPoint(vec3_t p, vec3_t color);
void R_PushDlights(void);

extern	unsigned	d_8to24table[256];

extern	int		registration_sequence;

void V_AddBlend(float r, float g, float b, float a, float* v_blend);

qboolean 	R_Init(void* hinstance, void* hWnd);
void	R_Shutdown(void);

void R_RenderView(refdef_t* fd);
void GL_ScreenShot_f(void);
void R_DrawAliasModel(entity_t* e, qboolean alpha);
void R_DrawBrushModel(entity_t* e);
void R_DrawSpriteModel(entity_t* e);
void R_DrawBeam(entity_t* e);
void R_DrawWorld(void);
void R_RenderDlights(void);
void R_DrawAlphaSurfaces(void);
void R_RenderBrushPoly(msurface_t* fa);
void R_InitParticleTexture(void);
void Draw_InitLocal(void);
void Draw_ShutdownLocal();
void GL_SubdivideSurface(msurface_t* fa);
qboolean R_CullBox(vec3_t mins, vec3_t maxs);
uint32_t R_RotateForEntity(entity_t* e, entitymodelview_t** modelview);
void R_MarkLeaves(void);

glpoly_t* WaterWarpPolyVerts(glpoly_t* p);
void EmitWaterPolys(msurface_t* fa);
void R_SetSky(char* name, float rotate, vec3_t axis);
void R_AddSkySurface(msurface_t* fa);
void R_ClearSkyBox(void);
void R_DrawSkyBox(void);
void R_MarkLights(dlight_t* light, int bit, mnode_t* node);

image_t*	Draw_FindPic(char* name);
void		Draw_GetPicSize(int* w, int* h, char* name);
void		Draw_Pic(int x, int y, char* name);
void		Draw_StretchPic(int x, int y, int w, int h, char* name);
void		Draw_Char(int x, int y, int c);
void		Draw_TileClear(int x, int y, int w, int h, char* name);
void		Draw_Fill(int x, int y, int w, int h, int c);
void		Draw_FillF(int x, int y, int w, int h, float r, float g, float b);
void		Draw_FadeScreen(void);
void		Draw_FadeScreenDirect(float r, float g, float b, float a);
void		Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte* data);
qboolean	Draw_IsRecording();
void		Draw_Flush();
void		Draw_Invalidate(); //marks that the Draw state has been disturbed, and it should be reset upon the next call. 

void	R_BeginFrame(float camera_separation);
void	R_SetPalette(const unsigned char* palette);
void	R_PushLightmapUpdates();

int		Draw_GetPalette(void);

void VK_ResampleTexture(unsigned* in, int inwidth, int inheight, unsigned* out, int outwidth, int outheight);

struct image_s* R_RegisterSkin(char* name);

void LoadPCX(const char* filename, byte** pic, byte** palette, int* width, int* height);
image_t* VK_CreatePic(const char* name, int width, int height, int layers, VkFormat format, VkImageViewType viewformat, VkImageLayout destinationLayout);
image_t* VK_LoadPic(const char* name, byte* pic, int width, int height, imagetype_t type, int bits, unsigned int* palette = nullptr);
image_t* VK_FindImage(const char* name, imagetype_t type);
void	VK_ChangeSampler(image_t* image, VkSampler sampler);
void	VK_FreeImage(image_t* image);
void	VK_TextureMode(char* string);
void	VK_ImageList_f(void);
void	LoadTGA(const char* name, byte** pic, int* width, int* height);
void	VK_UpdateSamplers();

void	VK_SetTexturePalette(unsigned palette[256]);

void	VK_InitImages(void);
void	VK_ShutdownImages(void);

void	VK_FreeUnusedImages(void);
void VK_CreateDepthBuffer();

void R_BeginRegistration(char* model);
struct model_s* R_RegisterModel(char* name);
void R_EndRegistration(void);

void VK_CreateSurfaceLightmap(msurface_t* surf);
void VK_EndBuildingLightmaps(void);
void VK_BeginBuildingLightmaps(model_t* m);
void VK_CreateLightmapImage();
void VK_DestroyLightmapImage();

void R_BuildSurfaces(model_t* mod);
void R_DrawSkySurfaces(msurface_t* firstchain);

void VK_InitParticles();
void VK_ShutdownParticles();
void VK_DrawParticles();

void VK_SyncFrame();

extern image_t* r_notexture;		// use for bad textures
extern image_t* r_particletexture;	// little dot for particles
extern image_t* r_depthbuffer;		// depth buffer for rendering, managed by dll in vk

typedef struct
{
	int         renderer;
	const char* renderer_string;
	const char* vendor_string;
	const char* version_string;
	const char* extensions_string;

	qboolean	allow_cds;
} vkconfig_t;

#define NUM_STAGING_BUFFERS 2
#define STAGING_BUFFER_SIZE 16 * 1024 * 1024

typedef struct
{
	VkBuffer		stagebuffers[NUM_STAGING_BUFFERS];
	vkallocinfo_t	stagebufferoffsets[NUM_STAGING_BUFFERS]; //need to remember the offsets for the mapped range. 
	VkCommandBuffer commandbuffers[NUM_STAGING_BUFFERS];
	VkFence			fences[NUM_STAGING_BUFFERS];
	uint64_t		semaphore_wait_values[NUM_STAGING_BUFFERS];
	unsigned int	stagenum;
	size_t			totalbyteswritten;
	qboolean		staging;
} vkstage_t;

//A physical device. 
typedef struct
{
	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures2 features;
	VkPhysicalDeviceVulkan13Features features13;

	uint32_t numextensions;
	VkExtensionProperties* extensions;

	//Memory heaps
	VkPhysicalDeviceMemoryProperties memory;

	//Queues
	uint32_t numqueuefamilies;
	VkQueueFamilyProperties* queuefamilies;

	//Surfaces
	VkSurfaceCapabilitiesKHR surfacecapabilities;

	uint32_t numsurfaceformats;
	VkSurfaceFormatKHR* surfaceformats;

	uint32_t numpresentmodes;
	VkPresentModeKHR* presentmodes;

	VkSurfaceFormatKHR surfaceformat;

} vkphysdevice_t;

//A logical device, which represents a vulkan instance on the physical device
typedef struct
{
	vkphysdevice_t* phys_device;
	VkDevice handle;

	//Should all of this be here? They're all associated with the logical device, but the structure is very cluttered as a result.
	// I guess that's what I get for doing this in C...
	
	//Queue families, needed for the swapchain. 
	uint32_t drawqueuefamily, presentqueuefamily;
	//Queue handles. 
	VkQueue drawqueue, presentqueue;
	qboolean sharedqueues;

	//Command pools. ATM there's two of them, one for rendering commands, and one for additional commands like texture uploads.
	VkCommandPool drawpool, resourcepool;

	//Pipelines
	VkPipeline screen_colored, screen_textured, world_lightmap, world_lightmap_unclamped, world_lightmap_only, brush_unlit;
	VkPipeline brush_unlit_warped, brush_unlit_warped_alpha, brush_lightmap, alias_vertexlit, alias_vertexlit_depthhack, alias_vertexlit_alpha;
	VkPipeline world_sky, sprite_textured, sprite_textured_alpha, particle_colored, beam_colored, sprite_textured_additive;

	//Samplers
	VkSampler nearest_sampler;
	VkSampler linear_sampler;
	VkSampler nearest_mipmap_nearest_sampler;
	VkSampler nearest_mipmap_linear_sampler;
	VkSampler linear_mipmap_nearest_sampler;
	VkSampler linear_mipmap_linear_sampler;

	//Descriptor sets with the projection_modelview_layout are always bound at slot 0, used by all shaders.
	VkDescriptorSetLayout projection_modelview_layout;
	VkDescriptorSetLayout modelview_layout;
	//Descriptor sets with the sampled_texture_layout are bound at slot 1, used by all textured shaders.
	VkDescriptorSetLayout sampled_texture_layout;
	VkDescriptorSetLayout dynamic_uniform_layout;
	VkDescriptorSetLayout storage_buffer_layout;

	//Memory pools
	//This memory is device local, and does not need to be host visible.
	vkmempool_t device_local_pool;
	//This memory can be device local, but is always host visible. 
	vkmempool_t host_visible_pool;
	//This memory is host visible, and device local if absolutely possible
	vkmempool_t host_visible_device_local_pool;

	//Stage for getting data to the GPU
	vkstage_t stage;

	//Semaphores for the swapchain
	VkSemaphore swap_acquire, swap_present;
	//Fence for synchronizing swapchain acquire
	VkFence complete_fence;

	//Descriptor pools
	//This pool is devoted to the projection matrix and modelviews for various models
	vkdpool_t misc_descriptor_pool;
	//This pool is devoted to images. Lots of images
	vkdpool_t image_descriptor_pool;
} vklogicaldevice_t;

typedef struct 
{
	VkInstance inst;
	VkSurfaceKHR surf;
	VkDebugUtilsMessengerEXT messenger;

	vkphysdevice_t phys_device;
	vklogicaldevice_t device;

	float inverse_intensity;
	qboolean fullscreen;

	int     prev_mode;

	unsigned char* d_16to8table;

	int lightmap_textures;

	int	currenttextures[2];
	int currenttmu;

	float camera_separation;
	qboolean stereo_enabled;

	unsigned char originalRedGammaTable[256];
	unsigned char originalGreenGammaTable[256];
	unsigned char originalBlueGammaTable[256];
} vkstate_t;

//Structure of the "globals" block uploaded to all shaders.
typedef struct
{
	vec3_t	origin;
	float pad1;
	vec3_t	color;
	float	intensity;
} vkdlight_t;

struct vkshaderglobal_t
{
	float intensity;
	float time;
	float modulate;
	int numlights;
	//float pad; //TODO: need to figure out the specific rules here. 
	vkdlight_t lights[MAX_DLIGHTS];
};

extern vkconfig_t  vk_config;
extern vkstate_t   vk_state;

extern model_t* r_worldmodel;

extern entity_t* currententity;
extern model_t* currentmodel;

extern vkshaderglobal_t* globals_ptr;

extern int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

//===================================================================
//Vulkan devices
//These will be called from the platform implementation of Vulkan

//Creates a device and all needed resources derived from the device.
//Returns !0 on error, but the game will probably have sys_errored by then. 
qboolean R_CreateDevice();

//Destroys a device and all its resources derived from it. 
void R_DestroyDevice();

//===================================================================
//Vulkan memory
void VK_CreateMemory(vklogicaldevice_t* device);
void VK_DestroyMemory(vklogicaldevice_t* device);

void VK_AllocatePool(vklogicaldevice_t* device, vkmempool_t* pool, size_t bytes, qboolean device_local_only, qboolean host_visible);
void VK_FreePool(vklogicaldevice_t* device, vkmempool_t* pool);

vkallocinfo_t VK_AllocateMemory(vkmempool_t* pool, VkMemoryRequirements requirements);
void VK_FreeMemory(vkmempool_t* pool, vkallocinfo_t* memory);

void VK_Memory_f();

//===================================================================
//Vulkan resources

//Creates all the resources, except the initial swapchain. Called from R_CreateDevice
void VK_CreateResources(vklogicaldevice_t* device);
//Destroys all resources, used when closing the game or changing modes. 
void VK_DestroyResources(vklogicaldevice_t* device);

//Creates all the pipelines.
void VK_CreatePipelines(vklogicaldevice_t* device);
//Destroys all the pipelines
void VK_DestroyPipelines(vklogicaldevice_t* device);

//Create a swapchain. Returns false if no error, true on error.
//Some errors are not fatal, and the game should continue running in their presence.
qboolean VK_CreateSwapchain();
//Destroys the swapchain
void VK_DestroySwapchain();
//Gets an image view for the current image, or NULL if one isn't available. Pass a VkSemaphore for it to signal on to acquire.
VkImage* VK_GetSwapchainImage(VkSemaphore signal, VkImageView* view);
//Presents the most recently acquired image. The present will wait for the semaphore wait, which should be signaled when all work is done.
void VK_Present(VkSemaphore wait);

//===================================================================
//Staging system

//Creates the staging system. This must be done only after the memory system is created.
void VK_CreateStage(vklogicaldevice_t* device);
//Destroys the staging system
void VK_DestroyStage(vklogicaldevice_t* device);

//Gets the command buffer for the current stage number, initializing it if it hasn't been set yet
//Making this call will cause the staging system to begin staging.
VkCommandBuffer VK_GetStageCommandBuffer(vkstage_t* stage);

//Starts staging data to image dest
void VK_StartStagingImageData(vkstage_t* stage, image_t* dest);
//Starts staging data to image dest, but only for one layer. Used to avoid having to stall the entire pipeline because one layer of the lightmap cube changed. 
void VK_StartStagingImageLayerData(vkstage_t* stage, image_t* dest, int layernum);
//Stages data to transfer to an image
void VK_StageImageData(vkstage_t* stage, image_t* dest, int miplevel, int width, int height, void* data);
//Stages data to transfer to a layered image
void VK_StageLayeredImageData(vkstage_t* stage, image_t* dest, int layer, int width, int height, void* data);
//Stages data to transfer to a subset of a layered image
void VK_StageLayeredImageDataSub(vkstage_t* stage, image_t* dest, int layer, int xoffs, int yoffs, int width, int height, void* data);
//Finishes staging data to the image
void VK_FinishStagingImage(vkstage_t* stage, image_t* dest);
//Finishes staging data to one layer of an image
void VK_FinishStagingImageLayer(vkstage_t* stage, image_t* dest, int layernum);

//Copies data from a host visible buffer into a device local buffer. This doesn't add any barriers automatically ATM. 
void VK_StageBufferData(vkstage_t* stage, VkBuffer dest, void* src, size_t srcoffset, size_t destoffset, size_t size);

//Returns true if there is data waiting to be staged.
qboolean VK_DataStaged(vkstage_t* stage);
//Gets a semaphore to wait on for the stage
//VkSemaphore VK_GetWaitSemaphore(vkstage_t* stage, uint64_t* wait_value);
//Flushes the staging buffer by queuing the command buffer. Increments the buffer number
void VK_FlushStage(vkstage_t* stage);
//Finishes the staging process. This must be called at the end of each frame
void VK_FinishStage(vkstage_t* stage);

//===================================================================
//Descriptor pools
//Each pool is built with fixed descriptor allocations. When an allocation fails, a new pool is created and linked in, and the allocation is tried from there

//Creates the initial link in a descriptor pool. pool->info must be initialized before calling this.
void VK_CreateDescriptorPool(vklogicaldevice_t* device, vkdpool_t* pool);
//Destroys all the descriptor pools that have been created in this pool.
void VK_DestroyDescriptorPool(vklogicaldevice_t* device, vkdpool_t* pool);

//Allocates new descriptors. Returns qfalse if no error, qtrue if an error has occurred. ATM does not validate if the layouts can be allocated from a pool. 
qboolean VK_AllocateDescriptor(vkdpool_t* pool, uint32_t count, VkDescriptorSetLayout* layouts, vkdescriptorset_t* results);

void VK_ResetPool(vkdpool_t* pool);

void VK_FreeDescriptors(uint32_t count, vkdescriptorset_t* descriptors);

//===================================================================
//Samplers

//Creates all the samplers
void VK_CreateSamplers(vklogicaldevice_t* device);
//Does some unknown task to all the samplers
void VK_DestroySamplers(vklogicaldevice_t* device);

//===================================================================
//Buffer builder

typedef struct
{
	byte* data;
	size_t currentsize;
	size_t byteswritten;

	//These are used when 
	vkmempool_t* pool;
	VkBuffer host_visible_buffer;
	vkallocinfo_t pool_allocation;
} bufferbuilder_t;

//Creates a buffer builder with a host-allocated buffer.
void R_CreateBufferBuilder(bufferbuilder_t* builder, size_t size);
//Creates a buffer builder with a buffer allocated out of a Vulkan memory pool. 
void R_CreateBufferBuilderFromPool(bufferbuilder_t* builder, vkmempool_t* pool, size_t size);
//Frees the buffer and any associated resources.
void R_FreeBufferBuilder(bufferbuilder_t* builder);

//Adds arbritary data to the buffer. Returns the current offset in the buffer. 
size_t R_BufferBuilderPut(bufferbuilder_t* builder, void* data, size_t size);
//Flushes the buffer builder, if mapped from an incoherent host visible pool
void R_BufferBuilderFlush(bufferbuilder_t* builder);

//===================================================================
//Thing related nonsense

void VK_InitThings();
void VK_ShutdownThings();
void VK_StartThings();
uint32_t VK_PushThing(entitymodelview_t** view);
void VK_FlushThingRanges();

extern vkdescriptorset_t modelview_descriptor;

/*
====================================================================

IMPORTED FUNCTIONS

====================================================================
*/

extern	refimport_t	ri;

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

bool 		VKimp_Init(void* hinstance, void* hWnd);
void		VKimp_Shutdown(void);
rserr_t    	VKimp_SetMode(unsigned int* pwidth, unsigned int* pheight, int mode, qboolean fullscreen);
void		VKimp_AppActivate(qboolean active);
void		VKimp_EnableLogging(qboolean enable);
void		VKimp_LogNewFrame(void);