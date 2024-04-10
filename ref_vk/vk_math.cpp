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

#include <cmath>
#include <intrin.h>
#include "vk_local.h"
#include "vk_math.h"

//vk_math.c
//Math specifically for the Vulkan renderer, replacing calls like glOrtho. 

//Putting this here so it isn't completely cluttering vk_rmain.c
matrix_t ortho_matrix;
VkBuffer ortho_projection_buffer;
vkallocinfo_t ortho_memory;
vkdescriptorset_t ortho_descriptor_set;

projectionblock_t projection_block;
VkBuffer persp_projection_buffer;
vkallocinfo_t persp_memory;
vkdescriptorset_t persp_descriptor_set;

extern VkBuffer globals_buffer;

const mat4 mat4_identity = mat4::MakeIdentity();

#define DEG2RAD( a ) ( a * M_PI ) / 180.0F

void VK_CreatePerspBuffer()
{
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.size = sizeof(projectionblock_t);
	info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &info, NULL, &persp_projection_buffer));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, persp_projection_buffer, &reqs);

	persp_memory = VK_AllocateMemory(&vk_state.device.host_visible_pool, reqs);

	vkBindBufferMemory(vk_state.device.handle, persp_projection_buffer, persp_memory.memory, persp_memory.offset);

	/*matrix_t* buffer = (matrix_t*)&((byte*)vk_state.device.host_visible_pool.host_ptr)[persp_memory.offset];

	memcpy(buffer, persp_matrix, sizeof(matrix_t));

	if (vk_state.device.host_visible_pool.host_ptr_incoherent)
	{
		VkMappedMemoryRange range =
		{
			.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			.memory = vk_state.device.host_visible_pool.memory,
			.offset = ortho_memory.offset,
			.size = sizeof(matrix_t),
		};
		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}*/

	//Create and update a descriptor set that can be used for this buffer. 
	if (VK_AllocateDescriptor(&vk_state.device.misc_descriptor_pool, 1, &vk_state.device.projection_modelview_layout, &persp_descriptor_set))
	{
		ri.Sys_Error(ERR_FATAL, "Failed to create perspective projection descriptor set");
	}

	VkDescriptorBufferInfo bufferinfo = {};
	bufferinfo.buffer = persp_projection_buffer;
	bufferinfo.offset = 0;
	bufferinfo.range = sizeof(projectionblock_t);
	
	VkWriteDescriptorSet writeinfo = {};
	writeinfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeinfo.dstBinding = 0;
	writeinfo.dstSet = persp_descriptor_set.set;
	writeinfo.pBufferInfo = &bufferinfo;

	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);

	//can't update both of them at once, since they have different stage flags.
	bufferinfo.buffer = globals_buffer;
	bufferinfo.offset = 0;
	bufferinfo.range = sizeof(vkshaderglobal_t);
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeinfo.dstBinding = 1;
	writeinfo.dstSet = persp_descriptor_set.set;
	writeinfo.pBufferInfo = &bufferinfo;
	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);
}

void VK_CreateOrthoBuffer()
{
	VK_Ortho(ortho_matrix, 0, vid.width, 0, vid.height, -99999, 99999);

	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.size = sizeof(matrix_t);
	info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &info, NULL, &ortho_projection_buffer));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, ortho_projection_buffer, &reqs);

	ortho_memory = VK_AllocateMemory(&vk_state.device.host_visible_pool, reqs);

	vkBindBufferMemory(vk_state.device.handle, ortho_projection_buffer, ortho_memory.memory, ortho_memory.offset);

	matrix_t* buffer = (matrix_t*)&((byte*)vk_state.device.host_visible_pool.host_ptr)[ortho_memory.offset];

	memcpy(buffer, ortho_matrix, sizeof(matrix_t));

	if (vk_state.device.host_visible_pool.host_ptr_incoherent)
	{
		VkMappedMemoryRange range = {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = vk_state.device.host_visible_pool.memory;
		range.offset = ortho_memory.offset;
		range.size = sizeof(matrix_t);
		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}

	//Create and update a descriptor set that can be used for this buffer. 
	if (VK_AllocateDescriptor(&vk_state.device.misc_descriptor_pool, 1, &vk_state.device.projection_modelview_layout, &ortho_descriptor_set))
	{
		ri.Sys_Error(ERR_FATAL, "Failed to create ortho projection descriptor set");
	}

	VkDescriptorBufferInfo bufferinfo = {};
	bufferinfo.buffer = ortho_projection_buffer;
	bufferinfo.offset = 0;
	bufferinfo.range = sizeof(matrix_t);

	VkWriteDescriptorSet writeinfo = {};
	writeinfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeinfo.dstBinding = 0;
	writeinfo.dstSet = ortho_descriptor_set.set;
	writeinfo.pBufferInfo = &bufferinfo;

	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);

	//can't update both of them at once, since they have different stage flags.
	//TODO: This shouldn't be duplicated. 
	bufferinfo.buffer = globals_buffer;
	bufferinfo.offset = 0;
	bufferinfo.range = sizeof(vkshaderglobal_t);
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeinfo.dstBinding = 1;
	writeinfo.dstSet = ortho_descriptor_set.set;
	writeinfo.pBufferInfo = &bufferinfo;
	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);

	VK_CreatePerspBuffer();
}

void VK_DestroyOrthoBuffer()
{
	vkDestroyBuffer(vk_state.device.handle, ortho_projection_buffer, NULL);
	vkDestroyBuffer(vk_state.device.handle, persp_projection_buffer, NULL);

	VK_FreeMemory(&vk_state.device.host_visible_pool, &ortho_memory);
	VK_FreeMemory(&vk_state.device.host_visible_pool, &persp_memory);
}

void VK_Ortho(matrix_t matrix, float left, float right, float top, float bottom, float znear, float zfar)
{
	memset(matrix, 0, sizeof(float) * 16);
	matrix[0] = 2 / (right - left);
	matrix[5] = 2 / (bottom - top);
	matrix[10] = 1 / (zfar - znear);
	matrix[12] = (left + right) / (left - right);
	matrix[13] = (bottom + top) / (top - bottom);
	matrix[14] = -znear / (zfar - znear);
	matrix[15] = 1;
}

void VK_Perspective(float left, float right, float top, float bottom, float znear, float zfar)
{
	projectionblock_t* block = (projectionblock_t*)&((byte*)vk_state.device.host_visible_pool.host_ptr)[persp_memory.offset];
	memset(block->projection, 0, sizeof(float) * 16);
	block->projection[0] = 2 * znear / (right - left);

	block->projection[5] = 2 * znear / (bottom - top);

	block->projection[8] = (right + left) / (right - left);
	block->projection[9] = (top + bottom) / (top - bottom);
	block->projection[10] = zfar / (zfar - znear);
	block->projection[11] = 1;

	block->projection[14] = -znear * zfar / (zfar - znear);
}

void VK_UpdateCamera(float* pos, float* rot)
{
	projectionblock_t* block = (projectionblock_t*)&((byte*)vk_state.device.host_visible_pool.host_ptr)[persp_memory.offset];

	mat4 modelview = mat4_identity;
	modelview.RotateAroundX(-90);
	modelview.RotateAroundZ(90);
	modelview.RotateAroundX(rot[2]);
	modelview.RotateAroundY(rot[0]);
	modelview.RotateAroundZ(-rot[1]);
	modelview.Translate(-pos[0], -pos[1], -pos[2]);

	modelview.ToMatrixt(block->modelview);

	//Was trying to avoid sending this, but the sky shader needs it to calculate a direction.
	memcpy(block->pos, pos, sizeof(vec3_t));

	/*(block->x = pos[0];
	block->y = pos[1];
	block->z = pos[2];

	block->rotx = rot[0];
	block->roty = rot[1];
	block->rotz = rot[2];*/

	if (vk_state.device.host_visible_pool.host_ptr_incoherent)
	{
		VkMappedMemoryRange range = {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = vk_state.device.host_visible_pool.memory;
		range.offset = persp_memory.offset;
		range.size = sizeof(projectionblock_t);
		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}
}

/*void Mat4Rotate(matrix_t mat, float angle, float x, float y, float z)
{
	matrix_t rotmat = { 0 };
	rotmat[15] = 1;
	angle = DEG2RAD(angle);
	float c = cos(angle);
	float s = sin(angle);
	float xx = x * x;
	float xy = x * y;
	float xz = x * z;
	float yy = y * y;
	float yz = y * z;
	float zz = z * z;
	float xs = x * s;
	float ys = y * s;
	float zs = z * s;
	float oneminusc = (1 - c);

	float length = sqrt(x * x + y * y + z * z);
	//don't do anything on a null vector. 
	if (length == 0)
		return;

	if (std::abs(length - 1) > 1e-4)
	{
		x /= length; y /= length; z /= length;
	}

	rotmat[0] = xx * oneminusc + c;
	rotmat[1] = xy * oneminusc + zs;
	rotmat[2] = xz * oneminusc - ys;

	rotmat[4] = xy * oneminusc - zs;
	rotmat[5] = yy * oneminusc + c;
	rotmat[6] = yz * oneminusc + xs;

	rotmat[8] = xz * oneminusc + ys;
	rotmat[9] = yz * oneminusc - xs;
	rotmat[10] = zz * oneminusc + c;

	Mat4Multiply(mat, rotmat);
}*/

mat4& operator*(mat4& left, const mat4& other)
{
	left *= other;
	return left;
}

mat4& mat4::operator*=(const mat4& right)
{
	//The same 4 columns are always used, so load them first.
	__m128 columns[4];
	__m128 rightvalue[4];
	__m128 res;
	columns[0] = _mm_loadu_ps(&values[0]);
	columns[1] = _mm_loadu_ps(&values[4]);
	columns[2] = _mm_loadu_ps(&values[8]);
	columns[3] = _mm_loadu_ps(&values[12]);

	//Each column will broadcast 4 values from the right matrix and multiply each column by it, 
	// and then sum up the resultant vectors to form a result column. 
	for (int i = 0; i < 4; i++)
	{
		rightvalue[0] = _mm_mul_ps(_mm_load_ps1(&right.values[i * 4 + 0]), columns[0]);
		rightvalue[1] = _mm_mul_ps(_mm_load_ps1(&right.values[i * 4 + 1]), columns[1]);
		rightvalue[2] = _mm_mul_ps(_mm_load_ps1(&right.values[i * 4 + 2]), columns[2]);
		rightvalue[3] = _mm_mul_ps(_mm_load_ps1(&right.values[i * 4 + 3]), columns[3]);

		//From some quick profiling this seems slightly faster than nesting all the adds. 
		res = _mm_add_ps(rightvalue[0], rightvalue[1]);
		res = _mm_add_ps(res, rightvalue[2]);
		res = _mm_add_ps(res, rightvalue[3]);

		_mm_storeu_ps(&values[i * 4], res);
	}

	return *this;
}

void mat4::RotateAroundX(float ang)
{
	*this *= mat4::MakeRotateX(ang);
}

void mat4::RotateAroundY(float ang)
{
	*this *= mat4::MakeRotateY(ang);
}

void mat4::RotateAroundZ(float ang)
{
	*this *= mat4::MakeRotateZ(ang);
}

void mat4::Scale(float x, float y, float z)
{
	*this *= mat4::MakeScale(x, y, z);
}

void mat4::Translate(float x, float y, float z)
{
	*this *= mat4::MakeTranslate(x, y, z);
}

mat4 mat4::MakeIdentity()
{
	mat4 mat;
	mat.values[0] = mat.values[5] = mat.values[10] = mat.values[15] = 1;
	return mat;
}

mat4 mat4::MakeScale(float x, float y, float z)
{
	mat4 mat;
	mat.values[0] = x;
	mat.values[5] = y;
	mat.values[10] = z;
	mat.values[15] = 1;
	return mat;
}

mat4 mat4::MakeRotateX(float ang)
{
	mat4 mat;
	ang = DEG2RAD(ang);
	mat.values[0] = mat.values[15] = 1;
	mat.values[5] = cos(ang);
	mat.values[6] = -sin(ang);
	mat.values[9] = sin(ang);
	mat.values[10] = cos(ang);
	return mat;
}

mat4 mat4::MakeRotateY(float ang)
{
	mat4 mat;
	ang = DEG2RAD(ang);
	mat.values[5] = mat.values[15] = 1;
	mat.values[0] = cos(ang);
	mat.values[2] = sin(ang);
	mat.values[8] = -sin(ang);
	mat.values[10] = cos(ang);
	return mat;
}

mat4 mat4::MakeRotateZ(float ang)
{
	mat4 mat;
	ang = DEG2RAD(ang);
	mat.values[10] = mat.values[15] = 1;
	mat.values[0] = cos(ang);
	mat.values[1] = sin(ang);
	mat.values[4] = -sin(ang);
	mat.values[5] = cos(ang);
	return mat;
}

mat4 mat4::MakeTranslate(float x, float y, float z)
{
	mat4 mat = mat4_identity;

	mat.values[12] = x;
	mat.values[13] = y;
	mat.values[14] = z;

	return mat;
}
