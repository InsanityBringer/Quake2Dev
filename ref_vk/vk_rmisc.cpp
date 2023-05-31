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
// r_misc.c

#include "vk_local.h"

/*
==================
R_InitParticleTexture
==================
*/
byte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};
/*{
	{0,0,0,1,1,0,0,0},
	{0,1,1,1,1,1,1,0},
	{0,1,1,1,1,1,1,0},
	{1,1,1,1,1,1,1,1},
	{1,1,1,1,1,1,1,1},
	{0,1,1,1,1,1,1,0},
	{0,1,1,1,1,1,1,0},
	{0,0,0,1,1,0,0,0},
};*/

void R_InitParticleTexture(void)
{
	int		x, y;
	byte	data[8][8][4];

	//
	// particle texture
	//
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}
	r_particletexture = VK_LoadPic("***particle***", (byte*)data, 8, 8, it_sprite, 32);
	r_particletexture->locked = qtrue;

	//
	// also use this for bad textures, but without alpha
	//
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = dottexture[x & 3][y & 3] * 255;
			data[y][x][1] = 0; // dottexture[x&3][y&3]*255;
			data[y][x][2] = 0; //dottexture[x&3][y&3]*255;
			data[y][x][3] = 255;
		}
	}
	r_notexture = VK_LoadPic("***r_notexture***", (byte*)data, 8, 8, it_wall, 32);
	r_notexture->locked = qtrue;

	//
	// white texture
	//
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = 255;
		}
	}
	r_whitetexture = VK_LoadPic("***white***", (byte*)data, 8, 8, it_pic, 32);
	r_whitetexture->locked = qtrue;
}

void VK_CreateDepthBuffer()
{
	if (r_depthbuffer)
		VK_FreeImage(r_depthbuffer);

	r_depthbuffer = VK_CreatePic("***r_depthbuffer***",
		vid.width,
		vid.height,
		1,
		VK_FORMAT_D32_SFLOAT,
		VK_IMAGE_VIEW_TYPE_2D,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR);
	r_depthbuffer->locked = qtrue;
}
