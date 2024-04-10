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

//Simple matrix type is used for data that goes to vulkan.
//In theory on practically all compiles a mat4 should be able to provide data for a mat4 in a shader, but it's weird since it's a complex type.
typedef float matrix_t[16];

class mat4
{
	float values[16];
public:
	mat4()
	{
		memset(values, 0, sizeof(values));
	}

	mat4& operator= (const mat4& other)
	{
		memcpy(values, other.values, sizeof(values));
		return *this;
	}

	const float& operator[] (int index) const
	{
		return values[index];
	}

	float& operator[] (int index)
	{
		return values[index];
	}

	friend mat4& operator* (mat4& left, const mat4& other);
	mat4& operator*= (const mat4& other);

	void RotateAroundX(float ang);
	void RotateAroundY(float ang);
	void RotateAroundZ(float ang);
	void Scale(float x, float y, float z);
	void Translate(float x, float y, float z);

	//Honestly I could stick this complex class into the projection block structure,
	//and the layout will be correct, but just in case..
	void ToMatrixt(matrix_t mat)
	{
		memcpy(mat, values, sizeof(matrix_t));
	}

	static mat4 MakeIdentity();
	static mat4 MakeScale(float x, float y, float z);
	static mat4 MakeRotateX(float ang);
	static mat4 MakeRotateY(float ang);
	static mat4 MakeRotateZ(float ang);
	//static mat4 MakeRotate(float ang, float x, float y, float z);
	static mat4 MakeTranslate(float x, float y, float z);
};

extern const mat4 mat4_identity;

typedef struct
{
	matrix_t projection;
	matrix_t modelview;
	vec3_t pos[3]; //sky needs the position directly, blargh
	float pad;
} projectionblock_t;

extern vkdescriptorset_t ortho_descriptor_set;
extern vkdescriptorset_t persp_descriptor_set;

//Create the ortho projection matrix, an associated uniform buffer, and the needed descriptor. With Quake 2 the video size should never change, so this can be precomputed. 
void VK_CreateOrthoBuffer();
void VK_DestroyOrthoBuffer();
void VK_Ortho(matrix_t matrix, float left, float right, float top, float bottom, float znear, float zfar);
void VK_Perspective(float left, float right, float top, float bottom, float znear, float zfar);
void VK_UpdateCamera(float* pos, float* rot);
