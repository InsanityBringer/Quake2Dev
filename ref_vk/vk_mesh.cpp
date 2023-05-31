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

// vk_mesh.c: triangle model functions

#include "vk_local.h"
#include "vk_data.h"
#include "vk_math.h"

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = 
{
#include "anorms.h"
};

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h"
;

int fixvs = 0;

/*
** R_CullAliasModel
*/
static qboolean R_CullAliasModel(vec3_t bbox[8], entity_t* e)
{
	int i;
	vec3_t		mins, maxs;
	dmdl_t* paliashdr;
	vec3_t		vectors[3];
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t* pframe, * poldframe;
	vec3_t angles;

	paliashdr = (dmdl_t*)currentmodel->extradata;

	if ((e->frame >= paliashdr->num_frames) || (e->frame < 0))
	{
		ri.Con_Printf(PRINT_ALL, "R_CullAliasModel %s: no such frame %d\n",
			currentmodel->name, e->frame);
		e->frame = 0;
	}
	if ((e->oldframe >= paliashdr->num_frames) || (e->oldframe < 0))
	{
		ri.Con_Printf(PRINT_ALL, "R_CullAliasModel %s: no such oldframe %d\n",
			currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = (daliasframe_t*)((byte*)paliashdr +
		paliashdr->ofs_frames +
		e->frame * paliashdr->framesize);

	poldframe = (daliasframe_t*)((byte*)paliashdr +
		paliashdr->ofs_frames +
		e->oldframe * paliashdr->framesize);

	/*
	** compute axially aligned mins and maxs
	*/
	if (pframe == poldframe)
	{
		for (i = 0; i < 3; i++)
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255;
		}
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i] * 255;

			oldmins[i] = poldframe->translate[i];
			oldmaxs[i] = oldmins[i] + poldframe->scale[i] * 255;

			if (thismins[i] < oldmins[i])
				mins[i] = thismins[i];
			else
				mins[i] = oldmins[i];

			if (thismaxs[i] > oldmaxs[i])
				maxs[i] = thismaxs[i];
			else
				maxs[i] = oldmaxs[i];
		}
	}

	/*
	** compute a full bounding box
	*/
	for (i = 0; i < 8; i++)
	{
		vec3_t   tmp;

		if (i & 1)
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if (i & 2)
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if (i & 4)
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		VectorCopy(tmp, bbox[i]);
	}

	/*
	** rotate the bounding box
	*/
	VectorCopy(e->angles, angles);
	angles[YAW] = -angles[YAW];
	AngleVectors(angles, vectors[0], vectors[1], vectors[2]);

	for (i = 0; i < 8; i++)
	{
		vec3_t tmp;

		VectorCopy(bbox[i], tmp);

		bbox[i][0] = DotProduct(vectors[0], tmp);
		bbox[i][1] = -DotProduct(vectors[1], tmp);
		bbox[i][2] = DotProduct(vectors[2], tmp);

		VectorAdd(e->origin, bbox[i], bbox[i]);
	}

	{
		int p, f, aggregatemask = ~0;

		for (p = 0; p < 8; p++)
		{
			int mask = 0;

			for (f = 0; f < 4; f++)
			{
				float dp = DotProduct(frustum[f].normal, bbox[p]);

				if ((dp - frustum[f].dist) < 0)
				{
					mask |= (1 << f);
				}
			}

			aggregatemask &= mask;
		}

		if (aggregatemask)
		{
			return qtrue;
		}

		return qfalse;
	}
}

extern VkCommandBuffer currentcmdbuffer;

void R_MixShells(entitymodelview_t* modelview)
{
	VectorClear(modelview->color);
	if (currententity->flags & RF_SHELL_HALF_DAM)
	{
		modelview->color[0] = 0.56;
		modelview->color[1] = 0.59;
		modelview->color[2] = 0.45;
	}
	if (currententity->flags & RF_SHELL_DOUBLE)
	{
		modelview->color[0] = 0.9;
		modelview->color[1] = 0.7;
	}
	if (currententity->flags & RF_SHELL_RED)
		modelview->color[0] = 1.0;
	if (currententity->flags & RF_SHELL_GREEN)
		modelview->color[1] = 1.0;
	if (currententity->flags & RF_SHELL_BLUE)
		modelview->color[2] = 1.0;
}

static float RandomOffset()
{
	return ((rand() / (float)RAND_MAX) - .5f);
}

void R_DrawAliasModel(entity_t* e, qboolean alpha)
{
	int			i;
	dmdl_t* paliashdr;
	float		an;
	vec3_t		bbox[8];
	image_t* skin;
	entitymodelview_t* modelview;
	VkViewport viewport;

	if (!(e->flags & RF_WEAPONMODEL))
	{
		if (R_CullAliasModel(bbox, e))
			return;
	}

	if (e->flags & RF_WEAPONMODEL)
	{
		//if (r_lefthand->value == 2)
		//	return;

		viewport.x = viewport.y = 0;
		viewport.width = vid.width;
		viewport.height = vid.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 0.3f;

		vkCmdSetViewport(currentcmdbuffer, 0, 1, &viewport);

		//Check for muzzle flash hack
		if (vk_muzzleflash->value && currentmodel->flashtable)
		{
			for (i = 0; i < currentmodel->num_flashes; i++)
			{
				flashinfo_t& flash = currentmodel->flashtable[i];
				if (e->frame == flash.frame && e->backlerp <= flash.start_backlerp && e->backlerp > flash.end_backlerp)
				{
					vec3_t forward, side, up;
					AngleVectors(e->angles, forward, side, up);
					VectorCopy(e->origin, effect_ent.origin);
					VectorMA(effect_ent.origin, flash.offset[0] + RandomOffset(), side, effect_ent.origin);
					VectorMA(effect_ent.origin, flash.offset[2] + RandomOffset(), up, effect_ent.origin);
					VectorMA(effect_ent.origin, flash.offset[1] + RandomOffset(), forward, effect_ent.origin);
					effect_ent.alpha = 0.8f;
					effect_ent.flags |= RF_TRANSLUCENT;
					effect_ent.model = &flash_model;
					effect_visible = true;

					if (flash.doubleoffsets)
					{
						VectorCopy(e->origin, effect2_ent.origin);
						VectorMA(effect2_ent.origin, flash.offset2[0] + RandomOffset(), side, effect2_ent.origin);
						VectorMA(effect2_ent.origin, flash.offset2[2] + RandomOffset(), up, effect2_ent.origin);
						VectorMA(effect2_ent.origin, flash.offset2[1] + RandomOffset(), forward, effect2_ent.origin);
						effect2_ent.alpha = 0.8f;
						effect2_ent.flags |= RF_TRANSLUCENT;
						effect2_ent.model = &flash_model;
						effect2_visible = true;
					}

					break;
				}
			}
		}
	}

	paliashdr = (dmdl_t*)currentmodel->extradata;

	c_alias_polys++;

	//e->angles[PITCH] = -e->angles[PITCH];	// sigh. [ISB] pitch hack done in shader
	uint32_t bufferoffset = R_RotateForEntity(e, &modelview);
	//e->angles[PITCH] = -e->angles[PITCH];	// sigh.

	// select skin
	if (currententity->skin)
		skin = currententity->skin;	// custom player skin
	else
	{
		if (currententity->skinnum >= MAX_MD2SKINS)
			skin = currentmodel->skins[0];
		else
		{
			skin = currentmodel->skins[currententity->skinnum];
			if (!skin)
				skin = currentmodel->skins[0];
		}
	}
	if (!skin)
		skin = r_notexture;	// fallback...

	modelview->lightscale = 1;
	modelview->swell = 0;
	if (currententity->flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
	{
		R_MixShells(modelview);
		modelview->swell = POWERSUIT_SCALE;
		modelview->lightscale = 0;
		skin = r_whitetexture;
	}
	else if (currententity->flags & RF_FULLBRIGHT)
	{
		for (i = 0; i < 3; i++)
			modelview->color[i] = 1.0;
	}
	else
	{
		R_LightPoint(currententity->origin, modelview->color);

		// player lighting hack for communication back to server
		// big hack!
		if (currententity->flags & RF_WEAPONMODEL)
		{
			// pick the greatest component, which should be the same
			// as the mono value returned by software
			if (modelview->color[0] > modelview->color[1])
			{
				if (modelview->color[0] > modelview->color[2])
					r_lightlevel->value = 150 * modelview->color[0];
				else
					r_lightlevel->value = 150 * modelview->color[2];
			}
			else
			{
				if (modelview->color[1] > modelview->color[2])
					r_lightlevel->value = 150 * modelview->color[1];
				else
					r_lightlevel->value = 150 * modelview->color[2];
			}

		}

		/*if (gl_monolightmap->string[0] != '0')
		{
			float s = shadelight[0];

			if (s < shadelight[1])
				s = shadelight[1];
			if (s < shadelight[2])
				s = shadelight[2];

			shadelight[0] = s;
			shadelight[1] = s;
			shadelight[2] = s;
		}*/
	}

	if (currententity->flags & RF_MINLIGHT)
	{
		for (i = 0; i < 3; i++)
			if (modelview->color[i] > 0.1)
				break;
		if (i == 3)
		{
			modelview->color[0] = 0.1;
			modelview->color[1] = 0.1;
			modelview->color[2] = 0.1;
		}
	}

	if (currententity->flags & RF_GLOW)
	{	// bonus items will pulse with time
		float	scale;
		float	min;

		scale = 0.1 * sin(r_newrefdef.time * 7);
		for (i = 0; i < 3; i++)
		{
			min = modelview->color[i] * 0.8;
			modelview->color[i] += scale;
			if (modelview->color[i] < min)
				modelview->color[i] = min;
		}
	}

	if ((currententity->frame >= paliashdr->num_frames)
		|| (currententity->frame < 0))
	{
		ri.Con_Printf(PRINT_ALL, "R_DrawAliasModel %s: no such frame %d\n",
			currentmodel->name, currententity->frame);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if ((currententity->oldframe >= paliashdr->num_frames)
		|| (currententity->oldframe < 0))
	{
		ri.Con_Printf(PRINT_ALL, "R_DrawAliasModel %s: no such oldframe %d\n",
			currentmodel->name, currententity->oldframe);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	VkDeviceSize stillhere = 0;

	if (!r_lerpmodels->value)
		currententity->backlerp = 0;

	modelview->frame_offset = currententity->frame * paliashdr->num_xyz;
	modelview->old_frame_offset = currententity->oldframe * paliashdr->num_xyz;
	modelview->backlerp = currententity->backlerp;
	modelview->swell += vk_modelswell->value;

	vec3_t delta;
	vec3_t vectors[3];
	// move should be the delta back to the previous frame * backlerp
	VectorSubtract(currententity->oldorigin, currententity->origin, delta);
	AngleVectors(currententity->angles, vectors[0], vectors[1], vectors[2]);

	modelview->move[0] = DotProduct(delta, vectors[0]);	// forward
	modelview->move[1] = -DotProduct(delta, vectors[1]);	// left
	modelview->move[2] = DotProduct(delta, vectors[2]);	// up

	if (currententity->flags & RF_TRANSLUCENT)
		modelview->alpha = currententity->alpha;
	else
		modelview->alpha = 1.0f;

	if (alpha)
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.alias_vertexlit_alpha);
	else
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.alias_vertexlit);
	vkCmdBindVertexBuffers(currentcmdbuffer, 0, 1, &currentmodel->vertbuffer, &stillhere);
	//vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alias_pipeline_layout, 0, 1, &persp_descriptor_set.set, 0, NULL);
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alias_pipeline_layout, 1, 1, &skin->descriptor.set, 0, NULL);
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alias_pipeline_layout, 2, 1, &currentmodel->alias_descriptorset, 0, NULL);
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alias_pipeline_layout, 3, 1, &modelview_descriptor.set, 1, &bufferoffset);

	vkCmdDraw(currentcmdbuffer, paliashdr->num_tris * 3, 1, 0, 0);

	if (e->flags & RF_WEAPONMODEL)
	{
		viewport.maxDepth = 1.0f;

		vkCmdSetViewport(currentcmdbuffer, 0, 1, &viewport);
	}

}

void VK_Norminfo_f(void)
{
	float maxdot = -99999;
	float mindot = 99999;
	int minvec = 0, maxvec = 0;
	for (int i = 0; i < NUMVERTEXNORMALS; i++)
	{
		ri.Con_Printf(PRINT_ALL, "%d: (%f, %f, %f): %.2f\n", i, r_avertexnormals[i][0], r_avertexnormals[i][1], r_avertexnormals[i][2], r_avertexnormal_dots[0][i]);

		if (r_avertexnormal_dots[0][i] > maxdot)
		{
			maxdot = r_avertexnormal_dots[0][i];
			maxvec = i;
		}

		if (r_avertexnormal_dots[0][i] < mindot)
		{
			mindot = r_avertexnormal_dots[0][i];
			minvec = i;
		}
	}

	ri.Con_Printf(PRINT_ALL, "\nMin dot: %.2f(%d), max dot: %.2f(%d)\n", mindot, minvec, maxdot, maxvec);
}

void Mod_MeshAliasModel(model_t* model)
{
	bufferbuilder_t vertexbuffer;
	bufferbuilder_t indexbuffer;

	dmdl_t* aliashdr = (dmdl_t*)model->extradata;

	R_CreateBufferBuilder(&vertexbuffer, sizeof(aliasvert_t) * aliashdr->num_xyz);
	R_CreateBufferBuilder(&indexbuffer, sizeof(aliasindex_t) * aliashdr->num_tris * 3);

	//Build the vertex storage buffer
	for (int framenum = 0; framenum < aliashdr->num_frames; framenum++)
	{
		daliasframe_t* frame = (daliasframe_t*)((byte*)aliashdr + aliashdr->ofs_frames + framenum * aliashdr->framesize);
		
		for (int vertnum = 0; vertnum < aliashdr->num_xyz; vertnum++)
		{
			dtrivertx_t* invert = &frame->verts[vertnum];
			aliasvert_t vert;
			vert.pos[0] = invert->v[0] * frame->scale[0] + frame->translate[0];
			vert.pos[1] = invert->v[1] * frame->scale[1] + frame->translate[1];
			vert.pos[2] = invert->v[2] * frame->scale[2] + frame->translate[2];
			vert.normal[0] = r_avertexnormals[invert->lightnormalindex][0];
			vert.normal[1] = r_avertexnormals[invert->lightnormalindex][1];
			vert.normal[2] = r_avertexnormals[invert->lightnormalindex][2];

			R_BufferBuilderPut(&vertexbuffer, &vert, sizeof(vert));
		}

	}

	VkBufferCreateInfo vertbufferinfo = {};
	vertbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertbufferinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vertbufferinfo.size = vertexbuffer.byteswritten;
	vertbufferinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &vertbufferinfo, NULL, &model->alias_storagebuffer));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, model->alias_storagebuffer, &reqs);

	model->alias_storagebuffer_memory = VK_AllocateMemory(&vk_state.device.device_local_pool, reqs);
	vkBindBufferMemory(vk_state.device.handle, model->alias_storagebuffer, model->alias_storagebuffer_memory.memory, model->alias_storagebuffer_memory.offset);

	VK_StageBufferData(&vk_state.device.stage, model->alias_storagebuffer, vertexbuffer.data, 0, 0, vertexbuffer.byteswritten);

	dtriangle_t* triangle = (dtriangle_t*)((byte*)aliashdr + aliashdr->ofs_tris);
	dstvert_t* uvs = (dstvert_t*)((byte*)aliashdr + aliashdr->ofs_st);
	//Build the index vertex buffer
	for (int trinum = 0; trinum < aliashdr->num_tris; trinum++)
	{
		for (int i = 0; i < 3; i++)
		{
			aliasindex_t index;
			index.index = triangle->index_xyz[i];
			index.s = uvs[triangle->index_st[i]].s / (float)aliashdr->skinwidth;
			index.t = uvs[triangle->index_st[i]].t / (float)aliashdr->skinheight;

			R_BufferBuilderPut(&indexbuffer, &index, sizeof(index));
		}
		triangle++;
	}

	VkBufferCreateInfo indexbufferinfo = {};
	indexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexbufferinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	indexbufferinfo.size = indexbuffer.byteswritten;
	indexbufferinfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &indexbufferinfo, NULL, &model->vertbuffer));

	vkGetBufferMemoryRequirements(vk_state.device.handle, model->vertbuffer, &reqs);

	model->vertbuffermem = VK_AllocateMemory(&vk_state.device.device_local_pool, reqs);
	vkBindBufferMemory(vk_state.device.handle, model->vertbuffer, model->vertbuffermem.memory, model->vertbuffermem.offset);

	VK_StageBufferData(&vk_state.device.stage, model->vertbuffer, indexbuffer.data, 0, 0, indexbuffer.byteswritten);

	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(&vk_state.device.stage); //do this after staging the data since the stage command may have flipped the buffer

	VkMemoryBarrier2 barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

	VkDependencyInfo depinfo = {};
	depinfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depinfo.memoryBarrierCount = 1;
	depinfo.pMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(cmdbuffer, &depinfo);

	if (model->alias_descriptorset == VK_NULL_HANDLE)
	{
		vkdescriptorset_t bufferdescriptor;
		if (VK_AllocateDescriptor(&vk_state.device.misc_descriptor_pool, 1, &vk_state.device.storage_buffer_layout, &bufferdescriptor))
			ri.Sys_Error(ERR_FATAL, "Mod_MeshAliasModel: Failed to allocate descriptor for storage buffer");

		model->alias_descriptorset = bufferdescriptor.set;
	}

	VkDescriptorBufferInfo bufferdescriptorinfo = {};
	bufferdescriptorinfo.buffer = model->alias_storagebuffer;
	bufferdescriptorinfo.offset = 0;
	bufferdescriptorinfo.range = vertexbuffer.byteswritten;

	VkWriteDescriptorSet writeinfo = {};
	writeinfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeinfo.descriptorCount = 1;
	writeinfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeinfo.dstBinding = 0;
	writeinfo.dstSet = model->alias_descriptorset;
	writeinfo.pBufferInfo = &bufferdescriptorinfo;

	vkUpdateDescriptorSets(vk_state.device.handle, 1, &writeinfo, 0, NULL);

	R_FreeBufferBuilder(&vertexbuffer);
	R_FreeBufferBuilder(&indexbuffer);
}
