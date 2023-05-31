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

//This really should be vk_sky.cpp

#include "vk_local.h"
#include "vk_data.h"
#include "vk_math.h"

image_t* skyimage;
extern VkCommandBuffer currentcmdbuffer;

float spinspeed;
vec3_t spinaxis;

typedef struct
{
	vec3_t axis;
	float speed; 
} pushspin_t;

void R_DrawSkySurfaces(msurface_t* firstchain)
{
	vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.world_sky);
	//vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sky_pipeline_layout, 0, 1, &persp_descriptor_set.set, 0, NULL);
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sky_pipeline_layout, 1, 1, &skyimage->descriptor.set, 0, NULL);

	pushspin_t spininfo;
	if (spinspeed == 0)
	{
		spininfo.speed = 0;
		spinaxis[0] = 1.0f;
		spinaxis[1] = spinaxis[2] = 0.0f;
	}
	else
	{
		spininfo.speed = r_newrefdef.time * spinspeed;
		VectorCopy(spinaxis, spininfo.axis);
	}

	vkCmdPushConstants(currentcmdbuffer, sky_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushspin_t), &spininfo);

	for (msurface_t* chain = firstchain; chain; chain = chain->texturechain)
	{
		vkCmdDrawIndexed(currentcmdbuffer, chain->numvertices, 1, chain->basevertex, 0, 0);

		c_brush_polys++;
	}
}

void R_TransposeTop(int width, int height, uint32_t* data)
{
	uint32_t* temp = (uint32_t*)malloc(width * height * sizeof(uint32_t));
	if (!temp)
	{
		ri.Sys_Error(ERR_FATAL, "what are you doing that you're running out of memory on a 256 kilobyte allocation anyways?");
		return;
	}

	memcpy(temp, data, width * height * sizeof(uint32_t));

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			data[x * width + y] = temp[y * width + (height - x - 1)];
		}
	}

	free(temp);
}

void R_TransposeBottom(int width, int height, uint32_t* data)
{
	uint32_t* temp = (uint32_t*)malloc(width * height * sizeof(uint32_t));
	if (!temp)
	{
		ri.Sys_Error(ERR_FATAL, "what are you doing that you're running out of memory on a 256 kilobyte allocation anyways?");
		return;
	}

	memcpy(temp, data, width * height * sizeof(uint32_t));

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			data[x * width + y] = temp[(width - y - 1) * width + x];
		}
	}

	free(temp);
}

/*
============
R_SetSky
============
*/
// 3dstudio environment map names
const char* suf[6] = { "rt", "lf", "up", "dn", "bk", "ft" };
void R_SetSky(char* name, float rotate, vec3_t axis)
{
	VK_SyncFrame(); //TODO: I gotta see if I can do this in a less brute-force at every propblem point manner
	if (skyimage)
		VK_FreeImage(skyimage);

	//Load the first image to figure out the resolution
	int width, height;
	uint32_t* buffer = NULL;

	char pathname[MAX_QPATH];
	Com_sprintf(pathname, sizeof(pathname), "env/%s%s.tga", name, suf[0]);

	LoadTGA(pathname, (byte**)&buffer, &width, &height);

	skyimage = VK_CreatePic("***sky***", width, height, 6, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	skyimage->type = it_sky;

	VK_StartStagingImageData(&vk_state.device.stage, skyimage);

	spinspeed = rotate;
	VectorCopy(axis, spinaxis);

	for (int i = 0; i < 6; i++)
	{
		if (i != 0)
		{
			if (buffer)
				free(buffer);

			Com_sprintf(pathname, sizeof(pathname), "env/%s%s.tga", name, suf[i]);
			int local_width, local_height;
			LoadTGA(pathname, (byte**)&buffer, &local_width, &local_height);

			if (i == 2)
			{
				R_TransposeTop(width, height, buffer);
			}
			else if (i == 3)
			{
				R_TransposeBottom(width, height, buffer);
			}

			if (local_width != width || local_height != height)
			{
				free(buffer);
				ri.Sys_Error(ERR_DROP, "R_SetSky: Sky sizes don't match");
			}
		}

		VK_StageLayeredImageData(&vk_state.device.stage, skyimage, i, width, height, buffer);
	}

	free(buffer);
	VK_FinishStagingImage(&vk_state.device.stage, skyimage);
}
