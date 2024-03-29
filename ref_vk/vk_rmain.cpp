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
#include "vk_local.h"
#include "vkw_win.h"
#include "vk_math.h"
#include "vk_data.h"

viddef_t	vid;

refimport_t	ri;

vkconfig_t  vk_config;
vkstate_t   vk_state;

model_t* r_worldmodel;

entity_t* currententity;
model_t* currentmodel;

mleaf_t* current_viewleaf;

cplane_t	frustum[4];

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_alias_polys;
int			c_num_light_pushes;

float		v_blend[4];			// final blending color

image_t* r_notexture;		// use for bad textures
image_t* r_particletexture;	// little dot for particles
image_t* r_depthbuffer;		// depth buffer for rendering, managed by dll in vk
image_t* r_whitetexture;	// white texture, used for colored shells. 
//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

//command buffers
int cmdbuffernum;
VkCommandBuffer cmdbuffers[NUM_STAGING_BUFFERS];
VkCommandBuffer presentcmdbuffers[NUM_STAGING_BUFFERS];
VkBuffer globals_buffer;
vkallocinfo_t globals_buffer_memory;
vkshaderglobal_t* globals_ptr;

cvar_t* r_lightlevel;
cvar_t* r_norefresh;
cvar_t* r_speeds;
cvar_t* r_nocull;
cvar_t* r_drawentities;
cvar_t* r_lerpmodels;

cvar_t* vk_mode;
cvar_t* vk_no_po2;
cvar_t* vk_round_down;
cvar_t* vk_picmip; //absolutely fucking useless but why not. 
cvar_t* vk_vsync;
cvar_t* vk_dynamiclightmode;
cvar_t* vk_lockpvs;
cvar_t* vk_modulate;
cvar_t* vk_lightmode;
cvar_t* vk_lightmap;
cvar_t* vk_fps;
cvar_t* vk_modelswell; //yeah this was stupid but I like stupid things
cvar_t* vk_debug;
cvar_t* vk_debug_amd;
cvar_t* vk_particle_size;
cvar_t* vk_particle_size_min;
cvar_t* vk_particle_size_max;
cvar_t* vk_muzzleflash;
cvar_t* vk_sync;
cvar_t* vk_texturemode;
cvar_t* vk_particle_mode;
cvar_t* vk_intensity;

cvar_t* vid_fullscreen;
cvar_t* vid_gamma;
cvar_t* vid_ref;

cvar_t* r_novis;

//horrid hack to estimate framerate
LARGE_INTEGER oldtime, timer_rate;

//If debug utilities are available, this will be set to true. 
bool debug_utils_available;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox(vec3_t mins, vec3_t maxs)
{
	int		i;

	if (r_nocull->value)
		return qfalse;

	for (i = 0; i < 4; i++)
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
			return qtrue;
	return qfalse;
}

uint32_t R_RotateForEntity(entity_t* e, entitymodelview_t** modelview)
{
	entitymodelview_t* newmodelview;
	uint32_t offset = VK_PushThing(&newmodelview);
	
	VectorCopy(e->origin, newmodelview->origin);
	VectorCopy(e->angles, newmodelview->rot);
	*modelview = newmodelview;

	return offset;
}

VkCommandBuffer currentcmdbuffer, currentpresentcmdbuffer;

void R_DrawNullModel()
{
}

constexpr int NUM_BEAM_SEGS = 6;
VkBuffer beam_vert_buffer;
vkallocinfo_t beam_vert_buffer_memory;

struct beamvert_t
{
	float x, y, z;
};

void R_InitBeams()
{
	//how many times have I written some code to make a buffer and some memory now?
	VkBufferCreateInfo bufferinfo = {};
	bufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferinfo.size = (NUM_BEAM_SEGS+1) * sizeof(beamvert_t) * 4;
	bufferinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &bufferinfo, nullptr, &beam_vert_buffer));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, beam_vert_buffer, &reqs);

	beam_vert_buffer_memory = VK_AllocateMemory(&vk_state.device.host_visible_device_local_pool, reqs);
	vkBindBufferMemory(vk_state.device.handle, beam_vert_buffer, beam_vert_buffer_memory.memory, beam_vert_buffer_memory.offset);

	beamvert_t* beam_data = (beamvert_t*)&((byte*)vk_state.device.host_visible_device_local_pool.host_ptr)[beam_vert_buffer_memory.offset];

	for (int i = 0; i < NUM_BEAM_SEGS; i++)
	{
		float yval = sin(i / (double)NUM_BEAM_SEGS * M_PI * 2.0) * .5;
		float zval = cos(i / (double)NUM_BEAM_SEGS * M_PI * 2.0) * .5;
		beam_data[0].x = 0; beam_data[0].y = yval; beam_data[0].z = zval;
		beam_data[1].x = 1; beam_data[1].y = yval; beam_data[1].z = zval;
		yval = sin((i+1) / (double)NUM_BEAM_SEGS * M_PI * 2.0) * .5;
		zval = cos((i+1) / (double)NUM_BEAM_SEGS * M_PI * 2.0) * .5;
		beam_data[2].x = 0; beam_data[2].y = yval; beam_data[2].z = zval;
		beam_data[3].x = 1; beam_data[3].y = yval; beam_data[3].z = zval;

		beam_data += 4;
	}
}

void R_DrawBeam(entity_t* e)
{
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if (VectorNormalize(normalized_direction) == 0)
		return;

	PerpendicularVector(perpvec, normalized_direction);
	//VectorScale(perpvec, e->frame / 2, perpvec);

	entitymodelview_t* modelview;
	uint32_t offset = VK_PushThing(&modelview);

	float r = (d_8to24table[e->skinnum & 0xFF]) & 0xFF;
	float g = (d_8to24table[e->skinnum & 0xFF] >> 8) & 0xFF;
	float b = (d_8to24table[e->skinnum & 0xFF] >> 16) & 0xFF;

	r *= 1 / 255.0F;
	g *= 1 / 255.0F;
	b *= 1 / 255.0F;

	VectorSet(modelview->color, r, g, b);
	VectorCopy(origin, modelview->origin);
	VectorCopy(oldorigin, modelview->rot);
	VectorCopy(perpvec, modelview->move);
	modelview->backlerp = e->alpha;
	modelview->swell = e->frame;

	vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.beam_colored);
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alias_pipeline_layout, 3, 1, &modelview_descriptor.set, 1, &offset);
	VkDeviceSize whycantijustpassnullherewhenidontwanttouseoffsets = 0;
	vkCmdBindVertexBuffers(currentcmdbuffer, 0, 1, &beam_vert_buffer, &whycantijustpassnullherewhenidontwanttouseoffsets);
	vkCmdDraw(currentcmdbuffer, 4 * NUM_BEAM_SEGS, 1, 0, 0);
}

void R_ShutdownBeams()
{
	VK_FreeMemory(&vk_state.device.host_visible_device_local_pool, &beam_vert_buffer_memory);
	vkDestroyBuffer(vk_state.device.handle, beam_vert_buffer, nullptr);
}

/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel(entity_t* e)
{
	dsprite_t* psprite = (dsprite_t*)currentmodel->extradata;
	e->frame %= psprite->numframes;
	dsprframe_t* frame = &psprite->frames[e->frame];

	float alpha = 1.0f;
	if (e->flags & RF_TRANSLUCENT)
		alpha = e->alpha;


	entitymodelview_t* modelview;
	uint32_t offset = VK_PushThing(&modelview);

	VectorCopy(e->origin, modelview->origin);

	//disgusting, reusing fields of the modelview structure for different things for sprites. 
	float scale = currentmodel->radius;
	modelview->rot[0] = frame->origin_x * scale;
	modelview->rot[1] = frame->origin_y * scale;
	modelview->color[0] = frame->width * scale;
	modelview->color[1] = frame->height * scale;
	modelview->backlerp = alpha;

	if (alpha != 1.0f)
	{
		if (currentmodel->flags & MF_ADDITIVE)
			vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.sprite_textured_additive);
		else
			vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.sprite_textured_alpha);
	}
	else
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.sprite_textured);

	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alias_pipeline_layout, 1, 1, &currentmodel->skins[e->frame]->descriptor.set, 0, NULL);
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alias_pipeline_layout, 3, 1, &modelview_descriptor.set, 1, &offset);

	vkCmdDraw(currentcmdbuffer, 4, 1, 0, 0);
}

entity_t effect_ent, effect2_ent;
bool effect_visible, effect2_visible;
float muzzle_flash_detect_time;

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList(void)
{
	int		i;

	if (!r_drawentities->value)
		return;

	// draw non-transparent first
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];
		if (currententity->flags & RF_TRANSLUCENT)
			continue;	// solid

		if (currententity->flags & RF_BEAM)
		{
			R_DrawBeam(currententity);
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				R_DrawNullModel();
				continue;
			}
			switch (currentmodel->type)
			{
			case mod_alias:
				R_DrawAliasModel(currententity, qfalse);
				break;
			case mod_brush:
				R_DrawBrushModel(currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel(currententity);
				break;
			default:
				ri.Sys_Error(ERR_DROP, "Bad modeltype");
				break;
			}
		}
	}

	// draw transparent entities
	// we could sort these if it ever becomes a problem...
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];
		if (!(currententity->flags & RF_TRANSLUCENT))
			continue;	// solid

		if (currententity->flags & RF_BEAM)
		{
			R_DrawBeam(currententity);
		}
		else
		{
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel();
				continue;
			}
			switch (currentmodel->type)
			{
			case mod_alias:
				R_DrawAliasModel(currententity, qtrue);
				break;
			case mod_brush:
				R_DrawBrushModel(currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel(currententity);
				break;
			default:
				ri.Sys_Error(ERR_DROP, "Bad modeltype");
				break;
			}
		}
	}
}

void R_DrawEffects()
{
	if (effect_visible)
	{
		currententity = &effect_ent;

		if (currententity->flags & RF_BEAM)
		{
			R_DrawBeam(currententity);
		}
		else
		{
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel();
				return;
			}
			switch (currentmodel->type)
			{
			case mod_alias:
				R_DrawAliasModel(currententity, qtrue);
				break;
			case mod_brush:
				R_DrawBrushModel(currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel(currententity);
				break;
			default:
				ri.Sys_Error(ERR_DROP, "Bad modeltype");
				break;
			}
		}
		effect_visible = false;
	}

	if (effect2_visible)
	{
		currententity = &effect2_ent;

		if (currententity->flags & RF_BEAM)
		{
			R_DrawBeam(currententity);
		}
		else
		{
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel();
				return;
			}
			switch (currentmodel->type)
			{
			case mod_alias:
				R_DrawAliasModel(currententity, qtrue);
				break;
			case mod_brush:
				R_DrawBrushModel(currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel(currententity);
				break;
			default:
				ri.Sys_Error(ERR_DROP, "Bad modeltype");
				break;
			}
		}
		effect2_visible = false;
	}
}

/*
==================
R_SetMode
==================
*/
qboolean R_SetMode(void)
{
	rserr_t err;
	qboolean fullscreen;

	fullscreen = (qboolean)vid_fullscreen->value;

	vid_fullscreen->modified = qfalse;
	vk_mode->modified = qfalse;

	if ((err = VKimp_SetMode(&vid.width, &vid.height, vk_mode->value, fullscreen)) == rserr_ok)
	{
		vk_state.prev_mode = vk_mode->value;
	}
	else
	{
		if (err == rserr_invalid_fullscreen)
		{
			ri.Cvar_SetValue("vid_fullscreen", 0);
			vid_fullscreen->modified = qfalse;
			ri.Con_Printf(PRINT_ALL, "ref_vk::R_SetMode() - fullscreen unavailable in this mode\n");
			if ((err = VKimp_SetMode(&vid.width, &vid.height, vk_mode->value, qfalse)) == rserr_ok)
			{
				VK_CreateDepthBuffer();
				return qtrue;
			}
		}
		else if (err == rserr_invalid_mode)
		{
			ri.Cvar_SetValue("vk_mode", vk_state.prev_mode);
			vk_mode->modified = qfalse;
			ri.Con_Printf(PRINT_ALL, "ref_vk::R_SetMode() - invalid mode\n");
		}

		// try setting it back to something safe
		if ((err = VKimp_SetMode(&vid.width, &vid.height, vk_state.prev_mode, qfalse)) != rserr_ok)
		{
			ri.Con_Printf(PRINT_ALL, "ref_vk::R_SetMode() - could not revert to safe mode\n");
			return qfalse;
		}
	}
	VK_CreateDepthBuffer();
	return qtrue;
}

void R_Register()
{
	r_lightlevel = ri.Cvar_Get("r_lightlevel", "0", 0);
	r_norefresh = ri.Cvar_Get("r_norefresh", "0", 0);
	r_speeds = ri.Cvar_Get("r_speeds", "0", 0);
	r_nocull = ri.Cvar_Get("r_nocull", "0", 0);
	r_drawentities = ri.Cvar_Get("r_drawentities", "1", 0);
	r_lerpmodels = ri.Cvar_Get("r_lerpmodels", "1", 0);

	vk_mode = ri.Cvar_Get("vk_mode", "3", CVAR_ARCHIVE);
	vk_vsync = ri.Cvar_Get("vk_vsync", "0", CVAR_ARCHIVE);
	vk_picmip = ri.Cvar_Get("vk_picmip", "0", CVAR_ARCHIVE);
	vk_round_down = ri.Cvar_Get("vk_round_down", "0", CVAR_ARCHIVE);
	vk_no_po2 = ri.Cvar_Get("vk_no_po2", "0", CVAR_ARCHIVE);
	vk_dynamiclightmode = ri.Cvar_Get("vk_dynamiclightmode", "0", CVAR_ARCHIVE);
	vk_lockpvs = ri.Cvar_Get("vk_lockpvs", "0", 0);
	vk_modulate = ri.Cvar_Get("vk_modulate", "2.0", CVAR_ARCHIVE);
	vk_lightmode = ri.Cvar_Get("vk_lightmode", "1", CVAR_ARCHIVE);
	vk_lightmap = ri.Cvar_Get("vk_lightmap", "0", 0);
	vk_fps = ri.Cvar_Get("vk_fps", "0", 0);
	vk_modelswell = ri.Cvar_Get("vk_modelswell", "0", 0);
	vk_particle_size = ri.Cvar_Get("vk_particle_size", "9", CVAR_ARCHIVE);
	vk_particle_size_min = ri.Cvar_Get("vk_particle_size_min", "2", CVAR_ARCHIVE);
	vk_particle_size_max = ri.Cvar_Get("vk_particle_size_max", "100", CVAR_ARCHIVE);
	vk_muzzleflash = ri.Cvar_Get("vk_muzzleflash", "0", CVAR_ARCHIVE);
	vk_sync = ri.Cvar_Get("vk_sync", "0", CVAR_ARCHIVE);
	vk_texturemode = ri.Cvar_Get("vk_texturemode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE);
	vk_particle_mode = ri.Cvar_Get("vk_particle_mode", "1", CVAR_ARCHIVE);
	vk_intensity = ri.Cvar_Get("intensity", "2", 0);

	vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = ri.Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);
	vid_ref = ri.Cvar_Get("vid_ref", "soft", CVAR_ARCHIVE);

	r_novis = ri.Cvar_Get("r_novis", "0", 0);
}

qboolean R_Init(void* hinstance, void* hWnd)
{
	QueryPerformanceFrequency(&timer_rate);
	Draw_GetPalette();
	R_Register();

	ri.Con_Printf(PRINT_ALL, "ref_vk version: " REF_VERSION "\n");

	//Initialize the Volk loader. 
	VK_CHECK(volkInitialize());

	// initialize OS-specific parts of Vulkan
	if (!VKimp_Init(hinstance, hWnd))
	{
		return qtrue;
	}

	// set our "safe" modes
	vk_state.prev_mode = 3;

	// create the window and set up the context
	if (!R_SetMode())
	{
		ri.Con_Printf(PRINT_ALL, "ref_vk::R_Init() - could not R_SetMode()\n");
		return qtrue;
	}

	if (VK_CreateSwapchain())
	{
		ri.Con_Printf(PRINT_ALL, "ref_vk::R_Init() - initial VK_CreateSwapchain failed\n");
	}

	/*//Testing the stupid allocator.
	//Try 5 tightly packed allocations
	vkallocinfo_t allocations[5];
	VkMemoryRequirements reqs =
	{
		.size = 128,
		.alignment = 4
	};
	for (int i = 0; i < 5; i++)
	{
		allocations[i] = VK_AllocateMemory(&vk_state.device.device_local_pool, reqs);
	}

	//free two and four
	VK_FreeMemory(&vk_state.device.device_local_pool, &allocations[1]);
	VK_FreeMemory(&vk_state.device.device_local_pool, &allocations[3]);
	
	//free three and see if it merges. 
	VK_FreeMemory(&vk_state.device.device_local_pool, &allocations[2]);

	//test alignment, this should end up at 1024.
	reqs.size = 2048;
	reqs.alignment = 512;
	vkallocinfo_t aligned = VK_AllocateMemory(&vk_state.device.device_local_pool, reqs);

	reqs.alignment = 131072;
	vkallocinfo_t bigaligned = VK_AllocateMemory(&vk_state.device.device_local_pool, reqs);*/

	VkBufferCreateInfo globalsbufferinfo = {};
	globalsbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	globalsbufferinfo.size = sizeof(vkshaderglobal_t);
	globalsbufferinfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	globalsbufferinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &globalsbufferinfo, nullptr, &globals_buffer));

	VkMemoryRequirements reqs = {};
	vkGetBufferMemoryRequirements(vk_state.device.handle, globals_buffer, &reqs);

	globals_buffer_memory = VK_AllocateMemory(&vk_state.device.host_visible_device_local_pool, reqs);
	vkBindBufferMemory(vk_state.device.handle, globals_buffer, globals_buffer_memory.memory, globals_buffer_memory.offset);
	globals_ptr = (vkshaderglobal_t*)((byte*)vk_state.device.host_visible_device_local_pool.host_ptr) + globals_buffer_memory.offset;

	VkCommandBufferAllocateInfo cmdbufferinfo = {};
	cmdbufferinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdbufferinfo.commandPool = vk_state.device.drawpool;
	cmdbufferinfo.commandBufferCount = NUM_STAGING_BUFFERS;
	cmdbufferinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VK_CHECK(vkAllocateCommandBuffers(vk_state.device.handle, &cmdbufferinfo, cmdbuffers));

	if (!vk_state.device.sharedqueues)
	{
		cmdbufferinfo.commandPool = vk_state.device.presentpool;
		VK_CHECK(vkAllocateCommandBuffers(vk_state.device.handle, &cmdbufferinfo, presentcmdbuffers));
	}

	VK_CreateOrthoBuffer();
	VK_InitImages();
	R_InitParticleTexture();
	Draw_InitLocal();
	VK_CreateLightmapImage();
	VK_InitThings();
	VK_InitParticles();
	R_InitBeams();

	extern void VK_Norminfo_f(void);
	ri.Cmd_AddCommand("vk_memory", VK_Memory_f);
	ri.Cmd_AddCommand("vk_norminfo", VK_Norminfo_f);
	ri.Cmd_AddCommand("imagelist", VK_ImageList_f);
	ri.Cmd_AddCommand("modellist", Mod_Modellist_f);

	return qfalse;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown(void)
{
	
	ri.Cmd_RemoveCommand("modellist");
	//ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("imagelist");
	ri.Cmd_RemoveCommand("vk_memory");
	ri.Cmd_RemoveCommand("vk_norminfo");

	//Wait on the fence to ensure that the last frame is actually done
	VkResult res = vkWaitForFences(vk_state.device.handle, 1, &vk_state.device.complete_fence, VK_FALSE, 5000000000ull);

	if (res == VK_TIMEOUT)
		ri.Sys_Error(ERR_FATAL, "R_Shutdown: Timed out waiting on render fence");
	else if (res == VK_ERROR_DEVICE_LOST)
		ri.Sys_Error(ERR_FATAL, "R_Shutdown: Device lost waiting on render fence");

	//Wait on the fence, since resources in the staging buffer may be in use until its done. 
	//VK_FlushStage(&vk_state.device.stage); //undone. Draw_ calls potentially can stage new data
	vkResetFences(vk_state.device.handle, 1, &vk_state.device.complete_fence);

	vkFreeCommandBuffers(vk_state.device.handle, vk_state.device.drawpool, NUM_STAGING_BUFFERS, cmdbuffers);
	if (!vk_state.device.sharedqueues)
		vkFreeCommandBuffers(vk_state.device.handle, vk_state.device.presentpool, NUM_STAGING_BUFFERS, presentcmdbuffers);
	vkDestroyBuffer(vk_state.device.handle, globals_buffer, nullptr);
	VK_FreeMemory(&vk_state.device.host_visible_device_local_pool, &globals_buffer_memory);

	Mod_FreeAll();

	R_ShutdownBeams();
	Draw_ShutdownLocal();
	VK_ShutdownParticles();
	VK_ShutdownThings();
	VK_DestroyOrthoBuffer();
	VK_ShutdownImages();
	VK_DestroySwapchain();

	R_DestroyDevice();

	/*
	** shut down OS specific Vulkan stuff like contexts, etc.
	*/
	VKimp_Shutdown();
}

boolean renderingReady;
VkImage* colorimg;

//=======================================================================

int SignbitsForPlane(cplane_t* out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}


void R_SetFrustum(void)
{
	int		i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector(frustum[0].normal, vup, vpn, -(90 - r_newrefdef.fov_x / 2));
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector(frustum[1].normal, vup, vpn, 90 - r_newrefdef.fov_x / 2);
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector(frustum[2].normal, vright, vpn, 90 - r_newrefdef.fov_y / 2);
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector(frustum[3].normal, vright, vpn, -(90 - r_newrefdef.fov_y / 2));

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane(&frustum[i]);
	}
}

//=======================================================================

/*
===============
R_SetShaderGlobals
===============
*/
void R_SetShaderGlobals()
{
	globals_ptr->intensity = vk_intensity->value;
	globals_ptr->time = r_newrefdef.time;
	globals_ptr->modulate = vk_modulate->value;

	if (vk_state.device.host_visible_device_local_pool.host_ptr_incoherent)
	{
		VkMappedMemoryRange range = {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = globals_buffer_memory.memory;
		range.offset = globals_buffer_memory.offset;
		range.size = sizeof(vkshaderglobal_t);
		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame(void)
{
	int i;
	mleaf_t* leaf;

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vpn, vright, vup);

	// current viewcluster
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		current_viewleaf = leaf = Mod_PointInLeaf(r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy(r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);
			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2))
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy(r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);
			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2))
				r_viewcluster2 = leaf->cluster;
		}
	}

	for (i = 0; i < 4; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_polys = 0;
	c_alias_polys = 0;

	// clear out the portion of the screen that the NOWORLDMODEL defines
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		//this can't be an actual clear command, seemingly, due to the use of secondary command buffers. The validation messages throw an error if I try and it doesn't work.
		//I can't find the specific part of the spec mentioning this, though, some help would be nice there.
		//Instead, use draw to do a fill
		Draw_FillF(r_newrefdef.x, r_newrefdef.y, r_newrefdef.width, r_newrefdef.height, 0.3f, 0.3f, 0.3f);
	}
}

void MYgluPerspective(double fovy, double aspect, double zNear, double zFar)
{
	double xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	xmin += -(2 * vk_state.camera_separation) / zNear;
	xmax += -(2 * vk_state.camera_separation) / zNear;

	VK_Perspective(xmin, xmax, ymin, ymax, zNear, zFar);
}

qboolean drawing3d;

/*
=============
R_Setup3DFrame
=============
*/
void R_Setup3DFrame(void)
{
	//TODO: this is needed for the player view. 
	/*//
	// set up viewport
	//
	int x = floor(r_newrefdef.x * vid.width / vid.width);
	int x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	int y = floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	int y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	int w = x2 - x;
	int h = y - y2;

	qglViewport(x, y2, w, h);*/

	//
	// set up projection matrix
	//
	float screenaspect = (float)r_newrefdef.width / r_newrefdef.height;
	//	yfov = 2*atan((float)r_newrefdef.height/r_newrefdef.width)*180/M_PI;
	MYgluPerspective(r_newrefdef.fov_y, screenaspect, 4, 4096);

	VK_UpdateCamera(r_newrefdef.vieworg, r_newrefdef.viewangles);

	//Bind the projection and modelview block
	//TODO: This shouldn't need to change for each pipeline layout. 
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, world_pipeline_layout, 0, 1, &persp_descriptor_set.set, 0, NULL);

	VkViewport viewport;
	viewport.x = viewport.y = 0;
	viewport.width = vid.width;
	viewport.height = vid.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissors;
	scissors.offset.x = scissors.offset.y = 0;
	scissors.extent.width = vid.width;
	scissors.extent.height = vid.height;

	vkCmdSetViewport(currentcmdbuffer, 0, 1, &viewport);
	vkCmdSetScissor(currentcmdbuffer, 0, 1, &scissors);
	drawing3d = qtrue;
}

/*
@@@@@@@@@@@@@@@@@@@@@
R_RenderFrame

@@@@@@@@@@@@@@@@@@@@@
*/
void R_RenderFrame(refdef_t* fd)
{
	if (r_norefresh->value)
		return;

	r_newrefdef = *fd;

	//Make sure the draw subsystem knows its state has been disturbed.
	//In practice, Draw funcs should rarely be called before RenderFrame, but it can happen with player setup.
	Draw_Invalidate();
	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
		ri.Sys_Error(ERR_DROP, "R_RenderView: NULL worldmodel");

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
		c_num_light_pushes = 0;
	}

	//mark polygons lit by dynamic lights
	R_PushDlights();

	R_SetupFrame();
	R_SetFrustum();
	VK_StartThings();
	R_Setup3DFrame();
	R_MarkLeaves();
	R_DrawWorld();
	R_DrawEntitiesOnList();
	VK_DrawParticles();

	R_DrawAlphaSurfaces();
	R_DrawEffects();

	R_PushLightmapUpdates();

	if (r_newrefdef.blend[3] != 0)
		Draw_FadeScreenDirect(r_newrefdef.blend[0], r_newrefdef.blend[1], r_newrefdef.blend[2], r_newrefdef.blend[3]);
}

void VK_SyncFrame()
{
	VkResult res = vkWaitForFences(vk_state.device.handle, 1, &vk_state.device.complete_fence, VK_TRUE, 5000000000ull);
	if (res == VK_TIMEOUT)
		ri.Sys_Error(ERR_FATAL, "Timed out waiting on render fence");
	else if (res == VK_ERROR_DEVICE_LOST)
		ri.Sys_Error(ERR_FATAL, "Device lost waiting on render fence");
}

void R_BeginFrame(float camera_separation)
{
	if (renderingReady)
	{
		ri.Con_Printf(PRINT_ALL, "R_BeginFrame called recursively");
		return;
	}

	//this is always done since changing vk_sync crashes display drivers. Not sure why..
	//TODO: this is a bit brute-forcey, a fence isn't strictly needed here, but here to ensure everything is done before starting again
	VkResult res = vkWaitForFences(vk_state.device.handle, 1, &vk_state.device.complete_fence, VK_TRUE, 5000000000ull);
	if (res == VK_TIMEOUT)
		ri.Sys_Error(ERR_FATAL, "Timed out waiting on render fence");
	else if (res == VK_ERROR_DEVICE_LOST)
		ri.Sys_Error(ERR_FATAL, "Device lost waiting on render fence");

	//Wait on the fence, since resources in the staging buffer may be in use until its done. 
	//VK_FlushStage(&vk_state.device.stage); //undone. Draw_ calls potentially can stage new data
	vkResetFences(vk_state.device.handle, 1, &vk_state.device.complete_fence);

	if (vk_mode->modified)
	{
		//Wait for the device to idle, will this sync swapchain present?
		vkDeviceWaitIdle(vk_state.device.handle);

		vk_mode->modified = qfalse;

		//With an instance already created, R_SetMode will simply resize things.
		if (!R_SetMode())
		{
			ri.Sys_Error(ERR_FATAL, "R_BeginFrame: R_SetMode failed!");
		}

		//Recreate the swapchain so it is the correct size. 
		VK_CreateSwapchain();

		VK_DestroyOrthoBuffer();
		VK_CreateOrthoBuffer();
	}

	if (vk_vsync->modified)
	{
		//Wait for the device to idle, will this sync swapchain present?
		vkDeviceWaitIdle(vk_state.device.handle);

		VK_CreateSwapchain();
		vk_vsync->modified = qfalse;
	}

	if (vk_texturemode->modified)
	{
		VK_UpdateSamplers();
		vk_texturemode->modified = qfalse;
	}

	R_SetShaderGlobals();
	currentcmdbuffer = cmdbuffers[cmdbuffernum];

	VK_CHECK(vkResetCommandBuffer(currentcmdbuffer, 0));
	if (!vk_state.device.sharedqueues)
	{
		currentpresentcmdbuffer = presentcmdbuffers[cmdbuffernum];
		VK_CHECK(vkResetCommandBuffer(currentpresentcmdbuffer, 0));
	}

	VkImageView colorview;
	colorimg = VK_GetSwapchainImage(vk_state.device.swap_acquire, &colorview);
	if (!colorimg || *colorimg == VK_NULL_HANDLE)
	{
		//wait, if you can't render, this won't be seen...
		ri.Con_Printf(PRINT_ALL, "R_BeginFrame: VK_GetSwapchainImage failed to return");
		renderingReady = false;
		return;
	}

	//At this point, the acquire 

	VkCommandBufferBeginInfo begininfo = {};
	begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begininfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(currentcmdbuffer, &begininfo));
	if (!vk_state.device.sharedqueues)
	{
		VK_CHECK(vkBeginCommandBuffer(currentpresentcmdbuffer, &begininfo));
	}

	//Before submitting the rendering start, need to transition the swapchain image. 
	VkImageMemoryBarrier2 swapchainbarrier = {};
	swapchainbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	swapchainbarrier.image = *colorimg;
	swapchainbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainbarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	swapchainbarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	swapchainbarrier.srcAccessMask = VK_ACCESS_2_NONE;
	swapchainbarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	swapchainbarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	swapchainbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	swapchainbarrier.subresourceRange.baseArrayLayer = 0;
	swapchainbarrier.subresourceRange.baseMipLevel = 0;
	swapchainbarrier.subresourceRange.layerCount = 1;
	swapchainbarrier.subresourceRange.levelCount = 1;

	VkDependencyInfo barrierinfo = {};
	barrierinfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	barrierinfo.imageMemoryBarrierCount = 1;
	barrierinfo.pImageMemoryBarriers = &swapchainbarrier;

	vkCmdPipelineBarrier2(currentcmdbuffer, &barrierinfo);

	VkRenderingAttachmentInfo colorinfo = {};
	colorinfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorinfo.clearValue.color.float32[0] = 0.0f;
	colorinfo.clearValue.color.float32[1] = 0.0f;
	colorinfo.clearValue.color.float32[2] = vk_debug_amd->value ? 0.0f : 0.3f;
	colorinfo.clearValue.color.float32[3] = 1.0f;
	colorinfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorinfo.imageView = colorview;
	colorinfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorinfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthinfo = {};
	depthinfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthinfo.clearValue.depthStencil.depth = 1.0f;
	depthinfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depthinfo.imageView = r_depthbuffer->viewhandle;
	depthinfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthinfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderinfo = {};
	renderinfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderinfo.layerCount = 1;
	renderinfo.colorAttachmentCount = 1;
	renderinfo.pColorAttachments = &colorinfo;
	renderinfo.pDepthAttachment = &depthinfo;
	renderinfo.renderArea.offset.x = 0;
	renderinfo.renderArea.offset.y = 0;
	renderinfo.renderArea.extent.width = vid.width;
	renderinfo.renderArea.extent.height = vid.height;
	//renderinfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

	vkCmdBeginRendering(currentcmdbuffer, &renderinfo);

	renderingReady = true;
}

char perfbuffer[256];

//Unlike GL, this doesn't need to be owned by the platform specific code. 
void R_EndFrame()
{
	if (!renderingReady)
		return;

	if (drawing3d)
	{
		VK_FlushThingRanges();
		drawing3d = qfalse;
	}

	if (vk_fps->value) //shitty FPS estimate
	{
		LARGE_INTEGER newtime;
		QueryPerformanceCounter(&newtime);
		uint64_t delta = newtime.QuadPart - oldtime.QuadPart;
		oldtime = newtime;

		double time = delta / (double)timer_rate.QuadPart;
		double fps = 1.0 / time;

		snprintf(perfbuffer, sizeof(perfbuffer) - 1, "fps: %.2f", fps);
		perfbuffer[sizeof(perfbuffer) - 1] = '\0';

		int startx = vid.width - 160;
		int len = strlen(perfbuffer);
		for (int i = 0; i < len; i++)
		{
			Draw_Char(startx, 0, perfbuffer[i]);
			startx += 8;
		}
	}

	if (r_speeds->value)
	{
		snprintf(perfbuffer, sizeof(perfbuffer) - 1, "bpoly: %4u apoly %4u pushes %4u", c_brush_polys, c_alias_polys, c_num_light_pushes);
		int startx = 0;
		int len = strlen(perfbuffer);
		for (int i = 0; i < len; i++)
		{
			Draw_Char(startx, 32, perfbuffer[i]);
			startx += 8;
		}
	}

	if (Draw_IsRecording())
	{
		Draw_Flush();
	}

	renderingReady = false;

	vkCmdEndRendering(currentcmdbuffer);
	//Only flush the stage now. 
	VK_FlushStage(&vk_state.device.stage);

	//Transition the color image into present layout, when needed. 
	VkImageMemoryBarrier2 swapchainbarrier = {};
	swapchainbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	swapchainbarrier.image = *colorimg;
	swapchainbarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	swapchainbarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	swapchainbarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	swapchainbarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	swapchainbarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	swapchainbarrier.dstAccessMask = VK_ACCESS_2_NONE;
	swapchainbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	swapchainbarrier.subresourceRange.baseArrayLayer = 0;
	swapchainbarrier.subresourceRange.baseMipLevel = 0;
	swapchainbarrier.subresourceRange.layerCount = 1;
	swapchainbarrier.subresourceRange.levelCount = 1;

	//If shared queues aren't used, release the image from the rendering command buffer here. 
	if (!vk_state.device.sharedqueues)
	{
		swapchainbarrier.srcQueueFamilyIndex = vk_state.device.drawqueuefamily;
		swapchainbarrier.dstQueueFamilyIndex = vk_state.device.presentqueuefamily;
	}

	VkDependencyInfo barrierinfo = {};
	barrierinfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	barrierinfo.imageMemoryBarrierCount = 1;
	barrierinfo.pImageMemoryBarriers = &swapchainbarrier;

	vkCmdPipelineBarrier2(currentcmdbuffer, &barrierinfo);

	//Acquire the image in the present command buffer
	if (!vk_state.device.sharedqueues)
	{
		swapchainbarrier = {};
		swapchainbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		swapchainbarrier.image = *colorimg;
		swapchainbarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		swapchainbarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		swapchainbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		swapchainbarrier.subresourceRange.baseArrayLayer = 0;
		swapchainbarrier.subresourceRange.baseMipLevel = 0;
		swapchainbarrier.subresourceRange.layerCount = 1;
		swapchainbarrier.subresourceRange.levelCount = 1;
		swapchainbarrier.srcQueueFamilyIndex = vk_state.device.drawqueuefamily;
		swapchainbarrier.dstQueueFamilyIndex = vk_state.device.presentqueuefamily;

		vkCmdPipelineBarrier2(currentpresentcmdbuffer, &barrierinfo);

		VK_CHECK(vkEndCommandBuffer(currentpresentcmdbuffer));
	}

	VK_CHECK(vkEndCommandBuffer(currentcmdbuffer));

	VkSemaphoreSubmitInfo waitinfo = {};
	waitinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitinfo.semaphore = vk_state.device.swap_acquire;
	waitinfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

	VkSemaphoreSubmitInfo signalinfo = {};
	signalinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	if (vk_state.device.sharedqueues)
		signalinfo.semaphore = vk_state.device.swap_present;
	else
		signalinfo.semaphore = vk_state.device.swap_present_pre;
	//signalinfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

	VkCommandBufferSubmitInfo bufferinfo = {};
	bufferinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	bufferinfo.commandBuffer = currentcmdbuffer;

	VkSubmitInfo2 submitinfo = {};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitinfo.waitSemaphoreInfoCount = 1;
	submitinfo.pWaitSemaphoreInfos = &waitinfo;
	submitinfo.signalSemaphoreInfoCount = 1;
	submitinfo.pSignalSemaphoreInfos = &signalinfo;
	submitinfo.commandBufferInfoCount = 1;
	submitinfo.pCommandBufferInfos = &bufferinfo;

	if (vk_state.device.sharedqueues)
	{
		VK_CHECK(vkQueueSubmit2(vk_state.device.drawqueue, 1, &submitinfo, vk_state.device.complete_fence));
	}
	else
	{
		VK_CHECK(vkQueueSubmit2(vk_state.device.drawqueue, 1, &submitinfo, VK_NULL_HANDLE));
	}

	if (!vk_state.device.sharedqueues)
	{
		VkSemaphoreSubmitInfo signalinfopost = {};
		signalinfopost.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalinfopost.semaphore = vk_state.device.swap_present;
		//signalinfopost.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

		VkCommandBufferSubmitInfo bufferinfopost = {};
		bufferinfopost.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		bufferinfopost.commandBuffer = currentpresentcmdbuffer;

		submitinfo.waitSemaphoreInfoCount = 1;
		submitinfo.pWaitSemaphoreInfos = &signalinfo;
		submitinfo.signalSemaphoreInfoCount = 1;
		submitinfo.pSignalSemaphoreInfos = &signalinfopost;
		submitinfo.commandBufferInfoCount = 1;
		submitinfo.pCommandBufferInfos = &bufferinfopost;

		VK_CHECK(vkQueueSubmit2(vk_state.device.presentqueue, 1, &submitinfo, vk_state.device.complete_fence));
	}

	/*VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitinfo = {};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &currentcmdbuffer;
	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &vk_state.device.swap_present;
	submitinfo.waitSemaphoreCount = 1;
	submitinfo.pWaitSemaphores = &vk_state.device.swap_acquire;
	submitinfo.pWaitDstStageMask = &dstStageMask;

	VK_CHECK(vkQueueSubmit(vk_state.device.drawqueue, 1, &submitinfo, vk_state.device.complete_fence));*/

	//Hack specifically for OBS. If I don't sync before present, the recording goes berserk. 
	if (vk_sync->value)
	{
		//TODO: this is a bit brute-forcey, a fence isn't strictly needed here, but here to ensure everything is done before starting again
		VkResult res = vkWaitForFences(vk_state.device.handle, 1, &vk_state.device.complete_fence, VK_TRUE, 5000000000ull);
		if (res == VK_TIMEOUT)
			ri.Sys_Error(ERR_FATAL, "Timed out waiting on render fence");
		else if (res == VK_ERROR_DEVICE_LOST)
			ri.Sys_Error(ERR_FATAL, "Device lost waiting on render fence");
	}

	VK_Present(vk_state.device.swap_present);

	VK_FinishStage(&vk_state.device.stage);
}

/*
=============
R_SetPalette
=============
*/
uint32_t r_rawpalette[256];

void R_SetPalette(const unsigned char* palette)
{
	int		i;

	byte* rp = (byte*)r_rawpalette;

	if (palette)
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = palette[i * 3 + 0];
			rp[i * 4 + 1] = palette[i * 3 + 1];
			rp[i * 4 + 2] = palette[i * 3 + 2];
			rp[i * 4 + 3] = 0xff;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = d_8to24table[i] & 0xff;
			rp[i * 4 + 1] = (d_8to24table[i] >> 8) & 0xff;
			rp[i * 4 + 2] = (d_8to24table[i] >> 16) & 0xff;
			rp[i * 4 + 3] = 0xff;
		}
	}
	//GL_SetTexturePalette(r_rawpalette); //no color tables in vk atm.

	//qglClearColor(0, 0, 0, 0);
	//qglClear(GL_COLOR_BUFFER_BIT);
	//qglClearColor(0, 0, 0.05, 0.5);
}

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
refexport_t GetRefAPI(refimport_t rimp)
{
	refexport_t	re;

	ri = rimp;

	re.api_version = API_VERSION;

	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = Draw_FindPic;
	re.SetSky = R_SetSky;
	re.EndRegistration = R_EndRegistration;

	re.RenderFrame = R_RenderFrame;

	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawChar = Draw_Char;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;
	re.DrawFadeScreen = Draw_FadeScreen;

	re.DrawStretchRaw = Draw_StretchRaw;

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;

	re.CinematicSetPalette = R_SetPalette;
	re.BeginFrame = R_BeginFrame;
	re.EndFrame = R_EndFrame;

	re.AppActivate = VKimp_AppActivate;
	
	Swap_Init();

	return re;
}

#ifndef REF_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error(const char* error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start(argptr, error);
	vsnprintf(text, 1024, error, argptr);
	text[1023] = '\0';
	va_end(argptr);

	ri.Sys_Error(ERR_FATAL, "%s", text);
}

void Com_Printf(const char* fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start(argptr, fmt);
	vsnprintf(text, 1024, fmt, argptr);
	text[1023] = '\0';
	va_end(argptr);

	ri.Con_Printf(PRINT_ALL, "%s", text);
}

#endif
