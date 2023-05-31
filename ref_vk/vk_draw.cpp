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
//draw.c

#include "vk_local.h"
#include "vk_data.h"
#include "vk_math.h"

image_t* draw_chars;

qboolean drawrecording;
bool drawinvalidated;
bool stretchraw_reentrancycheck;
VkPipeline lastpipeline; //avoid changing pipeline binding too much, just to be safe. 
VkDescriptorSet lastdescriptorset;

VkBuffer vertbufferobj;
vkallocinfo_t vertbuffermem;
screenvertex_t* vertbuffer;
int numdrawvertices;

byte* stretchraw_buffer;
int laststretchraw_size;

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal(void)
{
	// load console characters (don't bilerp characters)
	draw_chars = VK_FindImage("pics/conchars.pcx", it_pic);

	VkBufferCreateInfo bufferinfo = {};
	bufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferinfo.size = 1048576 * 2;
	bufferinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &bufferinfo, NULL, &vertbufferobj));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, vertbufferobj, &reqs);

	vertbuffermem = VK_AllocateMemory(&vk_state.device.host_visible_device_local_pool, reqs);
	VK_CHECK(vkBindBufferMemory(vk_state.device.handle, vertbufferobj, vertbuffermem.memory, vertbuffermem.offset));
}

void Draw_ShutdownLocal()
{
	vkDestroyBuffer(vk_state.device.handle, vertbufferobj, NULL);
	if (stretchraw_buffer)
		free(stretchraw_buffer);
}

qboolean Draw_IsRecording()
{
	return drawrecording;
}

void Draw_Invalidate()
{
	if (drawrecording)
		drawinvalidated = true;
}

void Draw_Flush()
{
	if (drawrecording)
	{
		if (vk_state.device.host_visible_device_local_pool.host_ptr_incoherent)
		{
			VkMappedMemoryRange range = {};
			range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			range.memory = vk_state.device.host_visible_device_local_pool.memory;
			range.offset = vertbuffermem.offset;
			range.size = sizeof(screenvertex_t) * numdrawvertices;
			vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
		}

		drawrecording = qfalse;
	}
	drawinvalidated = false;
	stretchraw_reentrancycheck = false;
}

extern boolean renderingReady;
extern VkCommandBuffer currentcmdbuffer;
/*
===============
Draw_Begin

If a command buffer isn't being recorded, this allocates a new one and begins recording. 
===============
*/
void Draw_Begin()
{
	//HACK: Quake 2 draws the pinging servers dialog without calling BeginFrame, triggering errors. 
	//If a render pass hasn't been started yet, start one. 
	if (!renderingReady)
	{
		//ri.Con_Printf(PRINT_ALL, "Warning: Draw_Begin called outside render pass\n");
		R_BeginFrame(0.0f);
	}

	if (!drawrecording)
	{
		lastpipeline = VK_NULL_HANDLE;
		lastdescriptorset = VK_NULL_HANDLE;
		numdrawvertices = 0;

		drawrecording = qtrue;

		byte* mem = (byte*)vk_state.device.host_visible_device_local_pool.host_ptr;
		vertbuffer = (screenvertex_t*)(mem + vertbuffermem.offset);

		vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screen_pipeline_layout, 0, 1, &ortho_descriptor_set.set, 0, NULL);
		VkDeviceSize why = 0;
		vkCmdBindVertexBuffers(currentcmdbuffer, 0, 1, &vertbufferobj, &why);

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
	}
	else if (drawinvalidated)
	{
		vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screen_pipeline_layout, 0, 1, &ortho_descriptor_set.set, 0, NULL);
		VkDeviceSize why = 0;
		vkCmdBindVertexBuffers(currentcmdbuffer, 0, 1, &vertbufferobj, &why);

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
	}
}

/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Char(int x, int y, int num)
{
	int				row, col;
	float			frow, fcol, size;

	Draw_Begin();
	num &= 255;

	if ((num & 127) == 32)
		return;		// space

	if (y <= -8)
		return;			// totally off screen

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	if (lastpipeline != vk_state.device.screen_textured)
	{
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.screen_textured);
		lastpipeline = vk_state.device.screen_textured;
	}

	//this is ridiculous, rebinding for every fucking character, but the performance seems acceptable ATM. 
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screen_pipeline_layout, 1, 1, &draw_chars->descriptor.set, 0, NULL);

	vertbuffer->x = x; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = fcol; vertbuffer->v = frow;
	vertbuffer++;

	vertbuffer->x = x; vertbuffer->y = y + 8; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = fcol; vertbuffer->v = frow + size;
	vertbuffer++;

	vertbuffer->x = x + 8; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = fcol + size; vertbuffer->v = frow;
	vertbuffer++;

	vertbuffer->x = x + 8; vertbuffer->y = y + 8; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = fcol + size; vertbuffer->v = frow + size;
	vertbuffer++;

	vkCmdDraw(currentcmdbuffer, 4, 1, numdrawvertices, 0);

	numdrawvertices += 4;

	if (numdrawvertices > 40000)
	{
		ri.Con_Printf(PRINT_ALL, "we're in a world of hurt now");
	}
}

/*
=============
Draw_FindPic
=============
*/
image_t* Draw_FindPic(char* name)
{
	image_t* gl;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
		gl = VK_FindImage(fullname, it_pic);
	}
	else
		gl = VK_FindImage(name + 1, it_pic);

	return gl;
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize(int* w, int* h, char* pic)
{
	image_t* gl;

	gl = Draw_FindPic(pic);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}
	*w = gl->width;
	*h = gl->height;
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic(int x, int y, int w, int h, char* pic)
{
	Draw_Begin();
	image_t* gl = Draw_FindPic(pic);
	if (!gl)
	{
		ri.Con_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (lastpipeline != vk_state.device.screen_textured)
	{
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.screen_textured);
		lastpipeline = vk_state.device.screen_textured;
	}

	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screen_pipeline_layout, 1, 1, &gl->descriptor.set, 0, NULL);

	vertbuffer->x = x; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sl; vertbuffer->v = gl->tl;
	vertbuffer++;

	vertbuffer->x = x; vertbuffer->y = y + h; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sl; vertbuffer->v = gl->th;
	vertbuffer++;

	vertbuffer->x = x + w; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sh; vertbuffer->v = gl->tl;
	vertbuffer++;

	vertbuffer->x = x + w; vertbuffer->y = y + h; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sh; vertbuffer->v = gl->th;
	vertbuffer++;

	vkCmdDraw(currentcmdbuffer, 4, 1, numdrawvertices, 0);

	numdrawvertices += 4;

	if (numdrawvertices > 40000)
	{
		ri.Con_Printf(PRINT_ALL, "we're in a world of hurt now");
	}
}

/*
=============
Draw_StretchPicDirect

For internal use, takes a pointer directly to a image. 
=============
*/
void Draw_StretchPicDirect(int x, int y, int w, int h, image_t* gl)
{
	Draw_Begin();

	if (lastpipeline != vk_state.device.screen_textured)
	{
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.screen_textured);
		lastpipeline = vk_state.device.screen_textured;
	}

	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screen_pipeline_layout, 1, 1, &gl->descriptor.set, 0, NULL);

	vertbuffer->x = x; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sl; vertbuffer->v = gl->tl;
	vertbuffer++;

	vertbuffer->x = x; vertbuffer->y = y + h; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sl; vertbuffer->v = gl->th;
	vertbuffer++;

	vertbuffer->x = x + w; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sh; vertbuffer->v = gl->tl;
	vertbuffer++;

	vertbuffer->x = x + w; vertbuffer->y = y + h; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sh; vertbuffer->v = gl->th;
	vertbuffer++;

	vkCmdDraw(currentcmdbuffer, 4, 1, numdrawvertices, 0);

	numdrawvertices += 4;

	if (numdrawvertices > 40000)
	{
		ri.Con_Printf(PRINT_ALL, "we're in a world of hurt now");
	}
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic(int x, int y, char* pic)
{
	Draw_Begin();
	image_t* gl = Draw_FindPic(pic);
	if (!gl)
	{
		ri.Con_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (lastpipeline != vk_state.device.screen_textured)
	{
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.screen_textured);
		lastpipeline = vk_state.device.screen_textured;
	}

	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screen_pipeline_layout, 1, 1, &gl->descriptor.set, 0, NULL);

	vertbuffer->x = x; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sl; vertbuffer->v = gl->tl;
	vertbuffer++;

	vertbuffer->x = x; vertbuffer->y = y + gl->height; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sl; vertbuffer->v = gl->th;
	vertbuffer++;

	vertbuffer->x = x + gl->width; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sh; vertbuffer->v = gl->tl;
	vertbuffer++;

	vertbuffer->x = x + gl->width; vertbuffer->y = y + gl->height; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = vertbuffer->a = 1.0f;
	vertbuffer->u = gl->sh; vertbuffer->v = gl->th;
	vertbuffer++;

	vkCmdDraw(currentcmdbuffer, 4, 1, numdrawvertices, 0);

	numdrawvertices += 4;

	if (numdrawvertices > 40000)
	{
		ri.Con_Printf(PRINT_ALL, "we're in a world of hurt now");
	}
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear(int x, int y, int w, int h, char* pic)
{
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill(int x, int y, int w, int h, int c)
{
	Draw_Begin();

	union
	{
		unsigned	c;
		byte		v[4];
	} color;

	if ((unsigned)c > 255)
		ri.Sys_Error(ERR_FATAL, "Draw_Fill: bad color");

	color.c = d_8to24table[c];

	float r = color.v[0] / 255.0f;
	float g = color.v[1] / 255.0f;
	float b = color.v[2] / 255.0f;

	if (lastpipeline != vk_state.device.screen_colored)
	{
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.screen_colored);
		lastpipeline = vk_state.device.screen_colored;
	}

	vertbuffer->x = x; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = 1.0f;
	vertbuffer++;

	vertbuffer->x = x; vertbuffer->y = y + h; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = 1.0f;
	vertbuffer++;

	vertbuffer->x = x + w; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = 1.0f;
	vertbuffer++;

	vertbuffer->x = x + w; vertbuffer->y = y + h; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = 1.0f;
	vertbuffer++;

	vkCmdDraw(currentcmdbuffer, 4, 1, numdrawvertices, 0);

	numdrawvertices += 4;

	if (numdrawvertices > 40000)
	{
		ri.Con_Printf(PRINT_ALL, "we're in a world of hurt now");
	}
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color specified directly. For internal use. 
=============
*/
void Draw_FillF(int x, int y, int w, int h, float r, float g, float b)
{
	Draw_Begin();

	if (lastpipeline != vk_state.device.screen_colored)
	{
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.screen_colored);
		lastpipeline = vk_state.device.screen_colored;
	}

	vertbuffer->x = x; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = 1.0f;
	vertbuffer++;

	vertbuffer->x = x; vertbuffer->y = y + h; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = 1.0f;
	vertbuffer++;

	vertbuffer->x = x + w; vertbuffer->y = y; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = 1.0f;
	vertbuffer++;

	vertbuffer->x = x + w; vertbuffer->y = y + h; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = 1.0f;
	vertbuffer++;

	vkCmdDraw(currentcmdbuffer, 4, 1, numdrawvertices, 0);

	numdrawvertices += 4;

	if (numdrawvertices > 40000)
	{
		ri.Con_Printf(PRINT_ALL, "we're in a world of hurt now");
	}
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen(void)
{
	Draw_Begin();

	if (lastpipeline != vk_state.device.screen_colored)
	{
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.screen_colored);
		lastpipeline = vk_state.device.screen_colored;
	}

	vertbuffer->x = 0; vertbuffer->y = 0; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = 0.0f; vertbuffer->a = 0.8f;
	vertbuffer++;

	vertbuffer->x = 0; vertbuffer->y = vid.height; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = 0.0f; vertbuffer->a = 0.8f;
	vertbuffer++;

	vertbuffer->x = 0 + vid.width; vertbuffer->y = 0; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = 0.0f; vertbuffer->a = 0.8f;
	vertbuffer++;

	vertbuffer->x = vid.width; vertbuffer->y = vid.height; vertbuffer->z = 0;
	vertbuffer->r = vertbuffer->g = vertbuffer->b = 0.0f; vertbuffer->a = 0.8f;
	vertbuffer++;

	vkCmdDraw(currentcmdbuffer, 4, 1, numdrawvertices, 0);

	numdrawvertices += 4;

	if (numdrawvertices > 40000)
	{
		ri.Con_Printf(PRINT_ALL, "we're in a world of hurt now");
	}
}

void Draw_FadeScreenDirect(float r, float g, float b, float a)
{
	Draw_Begin();

	if (lastpipeline != vk_state.device.screen_colored)
	{
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.screen_colored);
		lastpipeline = vk_state.device.screen_colored;
	}

	vertbuffer->x = 0; vertbuffer->y = 0; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = a;
	vertbuffer++;

	vertbuffer->x = 0; vertbuffer->y = vid.height; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = a;
	vertbuffer++;

	vertbuffer->x = 0 + vid.width; vertbuffer->y = 0; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = a;
	vertbuffer++;

	vertbuffer->x = vid.width; vertbuffer->y = vid.height; vertbuffer->z = 0;
	vertbuffer->r = r; vertbuffer->g = g;  vertbuffer->b = b; vertbuffer->a = a;
	vertbuffer++;

	vkCmdDraw(currentcmdbuffer, 4, 1, numdrawvertices, 0);

	numdrawvertices += 4;

	if (numdrawvertices > 40000)
	{
		ri.Con_Printf(PRINT_ALL, "we're in a world of hurt now");
	}
}

//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
extern uint32_t	r_rawpalette[256];

image_t* frameimage;
int lastcols = -1, lastrows = -1;

void Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte* data)
{
	//This is absolutely not safe to call more than once per frame, but Quake 2 never does. Sanity check just to be safe though.
	if (stretchraw_reentrancycheck)
		return;

	if (!frameimage || lastcols != cols || lastrows != rows)
	{
		ri.Con_Printf(PRINT_DEVELOPER, "Draw_StretchRaw allocated a new picture");
		if (frameimage)
			VK_FreeImage(frameimage);

		frameimage = VK_CreatePic("***stretchframe***", cols, rows, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		if (!frameimage)
			ri.Sys_Error(ERR_FATAL, "Draw_StretchRaw: Failed to allocate image for movie framebuffer");

		//check that the currently allocated buffer is sized correctly
		size_t buffersize = cols * rows * 4;
		if (laststretchraw_size < buffersize)
		{
			if (stretchraw_buffer)
				free(stretchraw_buffer);

			stretchraw_buffer = (byte*)malloc(buffersize);
			if (!stretchraw_buffer)
			{
				ri.Sys_Error(ERR_FATAL, "Draw_StretchRaw: Failed to allocate buffer for framebuffer upload");
			}
		}

		lastrows = rows;
		lastcols = cols;
	}

	stretchraw_reentrancycheck = true;
	uint32_t* ptr = (uint32_t*)stretchraw_buffer;
	byte* dataptr = data;
	for (int j = 0; j < rows; j++)
	{
		for (int i = 0; i < cols; i++)
		{
			*ptr = r_rawpalette[*dataptr];
			ptr++; dataptr++;
		}
	}

	VK_StartStagingImageData(&vk_state.device.stage, frameimage);
	VK_StageImageData(&vk_state.device.stage, frameimage, 0, cols, rows, stretchraw_buffer);
	VK_FinishStagingImage(&vk_state.device.stage, frameimage);

	Draw_StretchPicDirect(x, y, w, h, frameimage);
}

