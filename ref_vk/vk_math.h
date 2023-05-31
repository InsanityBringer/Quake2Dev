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

typedef float matrix_t[16];

typedef struct
{
	matrix_t projection;
	float x, y, z, x1;
	float rotx, roty, rotz, x2;
} projectionblock_t;

extern vkdescriptorset_t ortho_descriptor_set;
extern vkdescriptorset_t persp_descriptor_set;

//Create the ortho projection matrix, an associated uniform buffer, and the needed descriptor. With Quake 2 the video size should never change, so this can be precomputed. 
void VK_CreateOrthoBuffer();
void VK_DestroyOrthoBuffer();
void VK_Ortho(matrix_t matrix, float left, float right, float top, float bottom, float znear, float zfar);
void VK_Perspective(float left, float right, float top, float bottom, float znear, float zfar);
void VK_UpdateCamera(float* pos, float* rot);
