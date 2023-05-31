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

//vk_data.h
//include after vk_local.h
//Contains externs for important data like descriptor set layouts, and structures for all the vertex formats, uniform buffers, and push constant ranges. 

extern VkPipelineLayout screen_pipeline_layout;
extern VkPipelineLayout world_pipeline_layout;
extern VkPipelineLayout bmodel_pipeline_layout;
extern VkPipelineLayout alias_pipeline_layout;
extern VkPipelineLayout sky_pipeline_layout;
extern VkPipelineLayout particle_pipeline_layout;

//Screen vertex
typedef struct
{
	float x, y, z;
	float r, g, b, a;
	float u, v;
} screenvertex_t;

//World vertex
typedef struct
{
	vec3_t pos;
	float u, v, scroll_speed;
	float lu, lv;
	int lightmap_layer;
} worldvertex_t;

//Alias vertex
typedef struct
{
	vec3_t pos;
	float pad1;
	vec3_t normal;
	float pad2;
} aliasvert_t;

//Alias vertex index, the data actually contained in the vert buffer
typedef struct
{
	int index;
	float s, t;
} aliasindex_t;

//Particle. Mostly the same as the input data, but with color parameters rather than a palette index
struct vkparticle_t
{
	vec3_t pos;
	vec3_t color;
	float alpha;
};
