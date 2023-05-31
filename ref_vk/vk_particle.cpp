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

//vk_particle.cpp
//Particles! Not much to this file, but didn't want to clutter rmain any more than it already is.

#include "vk_local.h"
#include "vk_data.h"
#include "vk_math.h"

//Write here to actually update the particles that will be drawn in the given frame. 
vkparticle_t* particle_buffer;

VkBuffer particle_buffer_handle;
vkallocinfo_t particle_buffer_memory;

void VK_InitParticles()
{
	//how many times have I written some code to make a buffer and some memory now?
	VkBufferCreateInfo bufferinfo = {};
	bufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferinfo.size = MAX_PARTICLES * sizeof(vkparticle_t);
	bufferinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &bufferinfo, nullptr, &particle_buffer_handle));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, particle_buffer_handle, &reqs);

	particle_buffer_memory = VK_AllocateMemory(&vk_state.device.host_visible_device_local_pool, reqs);
	vkBindBufferMemory(vk_state.device.handle, particle_buffer_handle, particle_buffer_memory.memory, particle_buffer_memory.offset);

	particle_buffer = (vkparticle_t*)&((byte*)vk_state.device.host_visible_device_local_pool.host_ptr)[particle_buffer_memory.offset];
}

void VK_ShutdownParticles()
{
	vkDestroyBuffer(vk_state.device.handle, particle_buffer_handle, nullptr);
	VK_FreeMemory(&vk_state.device.host_visible_device_local_pool, &particle_buffer_memory);
}

extern VkCommandBuffer currentcmdbuffer;

struct vkparticlesizes_t
{
	float basesize;
	float min, max;
	int mode;
};

void VK_DrawParticles()
{
	unsigned char color[4];

	if (r_newrefdef.num_particles >= MAX_PARTICLES)
		r_newrefdef.num_particles = MAX_PARTICLES - 1;
	for (int i = 0; i < r_newrefdef.num_particles; i++)
	{
		particle_t* p = &r_newrefdef.particles[i];
		vkparticle_t* vp = &particle_buffer[i];
		VectorCopy(p->origin, vp->pos);
		*(int*)color = d_8to24table[p->color];

		vp->color[0] = color[0] / 255.0f;
		vp->color[1] = color[1] / 255.0f;
		vp->color[2] = color[2] / 255.0f;
		vp->alpha = p->alpha;
	}

	if (vk_state.device.host_visible_device_local_pool.host_ptr_incoherent)
	{
		VkMappedMemoryRange range = {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = particle_buffer_memory.memory;
		range.offset = particle_buffer_memory.offset;
		range.size = MAX_PARTICLES * sizeof(vkparticle_t);
		vkFlushMappedMemoryRanges(vk_state.device.handle, 1, &range);
	}

	VkDeviceSize whywouldineedanoffsethereanyways = 0;
	vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.particle_colored);
	vkCmdBindVertexBuffers(currentcmdbuffer, 0, 1, &particle_buffer_handle, &whywouldineedanoffsethereanyways);
	//vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particle_pipeline_layout, 0, 1, &persp_descriptor_set.set, 0, nullptr);

	//Particle sizes are specified in terms of their size in pixels at 640x480, when viewed from 64 units away. 
	float particlesizescale = vid.height / 480.0f;

	vkparticlesizes_t sizes;
	sizes.basesize = vk_particle_size->value * 64 * particlesizescale;
	sizes.min = vk_particle_size_min->value * particlesizescale;
	sizes.max = vk_particle_size_max->value * particlesizescale;
	sizes.mode = vk_particle_mode->value;

	vkCmdPushConstants(currentcmdbuffer, particle_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vkparticlesizes_t), &sizes);

	vkCmdDraw(currentcmdbuffer, r_newrefdef.num_particles, 1, 0, 0);
}
