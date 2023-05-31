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
// VK_RSURF.C: surface-related refresh code
#include <algorithm>
#include <vector>

#include "vk_local.h"
#include "vk_data.h"

static vec3_t	modelorg;		// relative to viewpoint

qboolean lightmap_updates_need_push;

msurface_t* r_alpha_surfaces, * r_warp_surfaces, * r_sky_surfaces;

#define		DYNAMIC_LIGHT_WIDTH  128
#define		DYNAMIC_LIGHT_HEIGHT 128

#define		LIGHTMAP_BYTES 4

#define		BLOCK_WIDTH		128
#define		BLOCK_HEIGHT	128

#define		MAX_LIGHTMAPS	128

image_t* lightmap_image;

typedef struct
{
	int internal_format;
	int	current_lightmap_texture;

	msurface_t* lightmap_surfaces[MAX_LIGHTMAPS];

	int			allocated[BLOCK_WIDTH];

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte		lightmap_buffer[4 * BLOCK_WIDTH * BLOCK_HEIGHT];
} vklightmapstate_t;

vklightmapstate_t vk_lms;

void VK_CreateLightmapImage()
{
	lightmap_image = VK_CreatePic("***lightmap_image***", BLOCK_WIDTH, BLOCK_HEIGHT, MAX_LIGHTMAPS, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	lightmap_image->locked = qtrue;
	VK_ChangeSampler(lightmap_image, vk_state.device.linear_sampler);
}

void VK_DestroyLightmapImage()
{
	//VK_FreeImage(lightmap_image); //undone: freed when images are shut down
}

bool Mod_AreTexInfoCompatible(const mtexinfo_t* texa, const mtexinfo_t* texb)
{
	//If the flags or animation frame counts different, these two texinfos are never compatible
	if (texa->flags != texb->flags || texa->numframes != texb->numframes)
		return false;

	//Otherwise they're compatible if the images are identical.
	return texa->image == texb->image;
}

//Sort function to sort surfaces by their texture and flags. 
//This doesn't need to be sorted in any particular order, just grouped together. 
int Mod_CompareMSurfacePtr(const void* a, const void* b)
{
	msurface_t* surfa = *(msurface_t**)a;
	msurface_t* surfb = *(msurface_t**)b;

	//Create a unique sort code to ensure that all surfaces are sorted consistiently by their texinfo. The largest contributor is the image number, which will group all similar surfaces together.
	uint32_t sorta = (uint32_t)(surfa->texinfo->image - vktextures) * (MAX_GLTEXTURES * SURF_NODRAW) + surfa->texinfo->flags * MAX_GLTEXTURES + surfa->texinfo->numframes;
	uint32_t sortb = (uint32_t)(surfb->texinfo->image - vktextures) * (MAX_GLTEXTURES * SURF_NODRAW) + surfb->texinfo->flags * MAX_GLTEXTURES + surfb->texinfo->numframes;

	if (sorta > sortb)
		return 1;
	if (sorta < sortb)
		return -1;

	return 0;
}
/*int Mod_CompareMSurfacePtr(const void* a, const void* b)
{
	msurface_t* surfa = *(msurface_t**)a;
	msurface_t* surfb = *(msurface_t**)b;

	uint32_t sorta = (uint32_t)(surfa->texinfo);
	uint32_t sortb = (uint32_t)(surfb->texinfo);

	if (sorta > sortb)
		return 1;
	if (sorta < sortb)
		return -1;

	return 0;
}*/

//Sort function to sort leaves by their cluster
bool Mod_CompareMLeafPtr(mleaf_t*& leafa, mleaf_t*& leafb)
{
	if (leafa->cluster < leafb->cluster)
		return true;
	else
		return false;
}

void R_BeginLightmapUpdates()
{
	if (lightmap_updates_need_push)
		return;

	VK_StartStagingImageData(&vk_state.device.stage, lightmap_image);
	lightmap_updates_need_push = qtrue;
}

void R_PushLightmapUpdates()
{
	if (lightmap_updates_need_push)
	{
		lightmap_updates_need_push = qfalse;
		VK_FinishStagingImage(&vk_state.device.stage, lightmap_image);
	}
}

//modified from the gl version to emit to the vertex buffer. 
//ATM all vertices are just a massive triangle soup. The geometry will probably always be a triangle soup, but I may add an index buffer later for the sake of memory conservation. 
//Returns amount of vertices created. //good fan version, but don't want fan emulation penalty..
/*int VK_BuildPolygonFromSurface(msurface_t* fa, bufferbuilder_t* buffer)
{
	int			lindex, lnumverts;
	medge_t*	pedges, * r_pedge;
	int			vertpage;
	float*		vec;
	float		s, t;
	worldvertex_t vert;

	//This will never change, so it can be initialized once
	vert.lightmap_layer = fa->lightmaptexturenum;

	if (fa->texinfo->flags & SURF_NODRAW && !(fa->texinfo->flags & SURF_DRAWSKY))
		return 0;

	if (fa->texinfo->flags & SURF_FLOWING)
	{
		if (fa->flags & SURF_DRAWTURB) //why does msurface_t::flags and mtexinfo_t::flags both use smimilarly named constants??
			vert.scroll_speed = .5f;
		else
			vert.scroll_speed = 64.f / 40;
	}
	else
		vert.scroll_speed = 0;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	//currently hijacking vert.lu for alpha information
	qboolean special = fa->texinfo->flags & (SURF_TRANS66 | SURF_TRANS33 | SURF_WARP) ? qtrue : qfalse;
	float alpha = 1.0f;
	if (fa->texinfo->flags & SURF_TRANS66)
		alpha = 2.0f / 3;
	else if (fa->texinfo->flags & SURF_TRANS33)
		alpha = 1.0f / 3;

	int num_verts_emitted = 0;

	for (int i = 0; i < lnumverts; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}

		VectorCopy(vec, vert.pos);
		vert.u = (DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3]) / fa->texinfo->image->width;
		vert.v = (DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3]) / fa->texinfo->image->height;
		vert.lu = (DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3] - fa->texturemins[0] + fa->light_s * 16 + 8) / (BLOCK_WIDTH * 16);
		vert.lv = (DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3] - fa->texturemins[1] + fa->light_t * 16 + 8) / (BLOCK_HEIGHT * 16);
		if (special)
			vert.lu = alpha;
		R_BufferBuilderPut(buffer, &vert, sizeof(vert));

		num_verts_emitted++;
	}

	return num_verts_emitted;
}*/

int VK_BuildPolygonFromSurface(msurface_t* fa, bufferbuilder_t* buffer, bufferbuilder_t* indexbuffer, int& basevertex)
{
	int			lindex, lnumverts;
	medge_t* pedges, * r_pedge;
	int			vertpage;
	float* vec;
	float		s, t;
	worldvertex_t vert;

	//This will never change, so it can be initialized once
	vert.lightmap_layer = fa->lightmaptexturenum;

	if (fa->texinfo->flags & SURF_NODRAW && !(fa->texinfo->flags & SURF_DRAWSKY))
		return 0;

	if (fa->texinfo->flags & SURF_FLOWING)
	{
		if (fa->flags & SURF_DRAWTURB) //why does msurface_t::flags and mtexinfo_t::flags both use smimilarly named constants??
			vert.scroll_speed = .5f;
		else
			vert.scroll_speed = 64.f / 40;
	}
	else
		vert.scroll_speed = 0;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	//currently hijacking vert.lu for alpha information
	qboolean special = fa->texinfo->flags & (SURF_TRANS66 | SURF_TRANS33 | SURF_WARP) ? qtrue : qfalse;
	float alpha = 1.0f;
	if (fa->texinfo->flags & SURF_TRANS66)
		alpha = 2.0f / 3;
	else if (fa->texinfo->flags & SURF_TRANS33)
		alpha = 1.0f / 3;

	//Emit vertices
	int num_vertex_emitted = 0;
	for (int i = 0; i < lnumverts; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}

		VectorCopy(vec, vert.pos);
		vert.u = (DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3]) / fa->texinfo->image->width;
		vert.v = (DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3]) / fa->texinfo->image->height;
		vert.lu = (DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3] - fa->texturemins[0] + fa->light_s * 16 + 8) / (BLOCK_WIDTH * 16);
		vert.lv = (DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3] - fa->texturemins[1] + fa->light_t * 16 + 8) / (BLOCK_HEIGHT * 16);
		if (special)
			vert.lu = alpha;
		R_BufferBuilderPut(buffer, &vert, sizeof(vert));

		num_vertex_emitted++;
	}

	//TODO: Index buffer can probably be a std::vector, not a buffer builder.
	int hack = basevertex; R_BufferBuilderPut(indexbuffer, &hack, sizeof(hack));
	hack = basevertex + 2; R_BufferBuilderPut(indexbuffer, &hack, sizeof(hack));
	hack = basevertex + 1; R_BufferBuilderPut(indexbuffer, &hack, sizeof(hack));

	int num_indices_emitted = 3;
	int vec2, vec3;
	for (int i = 2; i < fa->numedges - 1; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		vec2 = basevertex + i + 1;
		vec3 = basevertex + i;

		/*if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			//vec2 = currentmodel->vertexes[r_pedge->v[0]].position;
			//vec3 = currentmodel->vertexes[r_pedge->v[1]].position;
			vec2 = basevertex + ((i - 1) * 2) - 1;
			vec3 = basevertex + (i * 2);
		}
		else
		{
			r_pedge = &pedges[-lindex];
			//vec2 = currentmodel->vertexes[r_pedge->v[1]].position;
			//vec3 = currentmodel->vertexes[r_pedge->v[0]].position;
			vec2 = basevertex + (i * 2);
			vec3 = basevertex + (i * 2) - 1;
		}*/

		num_indices_emitted += 3;

		R_BufferBuilderPut(indexbuffer, &basevertex, sizeof(hack));
		R_BufferBuilderPut(indexbuffer, &vec2, sizeof(hack));
		R_BufferBuilderPut(indexbuffer, &vec3, sizeof(hack));
	}

	basevertex += num_vertex_emitted;
	return num_indices_emitted;
}

// Dear Future ISB,
//	 You've attempted to rewrite this to mesh at the cluster level twice. Both times, it has not had a significant effect in terms of drawcall reduction.
// If you're cringing at the amount of drawcalls you're currently emitting, try something more clever like switching to bindless. 
// The cluster level meshing hasn't been successful at all, even when you did manage to fix the bugs with similar textures not being merged. 
// I know you probably won't believe me, but please let the following screenshot convince you of the infeasibility of doing it.
// https://cdn.discordapp.com/attachments/684929771057971247/1111850708023640185/image.png
//
// Thank you for your consideration,
//  Current ISB. 
void R_BuildSurfaces(model_t* mod)
{
	//Initialize the vertex buffer for this model. 
	//This is currently using a fixed stride of 32 bytes per vertex, to simplify construction, even though it wastes data on sky and warp faces. 
	bufferbuilder_t worldVertexBuffer, worldIndexBuffer;
	R_CreateBufferBuilder(&worldVertexBuffer, 8 * 1024 * 1024);
	R_CreateBufferBuilder(&worldIndexBuffer, 2 * 1024 * 1024);

	int baseVertex = 0;
	int baseVertexNum = 0; //Maintains the current offset into the vertex buffer, for building the index buffer
	for (int i = 0; i < mod->numsurfaces; i++)
	{
		msurface_t* msurf = &mod->surfaces[i];
		int newVertices = VK_BuildPolygonFromSurface(msurf, &worldVertexBuffer, &worldIndexBuffer, baseVertexNum);
		msurf->basevertex = baseVertex;
		msurf->numvertices = newVertices;
		baseVertex += newVertices;
	}

	//Generate the vertex buffer for the level.

	VkBufferCreateInfo vertbufferinfo = {};
	vertbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertbufferinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vertbufferinfo.size = worldVertexBuffer.byteswritten;
	vertbufferinfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &vertbufferinfo, NULL, &mod->vertbuffer));

	vertbufferinfo.size = worldIndexBuffer.byteswritten;

	VK_CHECK(vkCreateBuffer(vk_state.device.handle, &vertbufferinfo, NULL, &mod->indexbuffer));

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(vk_state.device.handle, mod->vertbuffer, &reqs);

	mod->vertbuffermem = VK_AllocateMemory(&vk_state.device.device_local_pool, reqs);
	vkBindBufferMemory(vk_state.device.handle, mod->vertbuffer, mod->vertbuffermem.memory, mod->vertbuffermem.offset);

	vkGetBufferMemoryRequirements(vk_state.device.handle, mod->indexbuffer, &reqs);

	mod->indexbuffermem = VK_AllocateMemory(&vk_state.device.device_local_pool, reqs);
	vkBindBufferMemory(vk_state.device.handle, mod->indexbuffer, mod->indexbuffermem.memory, mod->indexbuffermem.offset);

	VK_StageBufferData(&vk_state.device.stage, mod->vertbuffer, worldVertexBuffer.data, 0, 0, worldVertexBuffer.byteswritten);
	VK_StageBufferData(&vk_state.device.stage, mod->indexbuffer, worldIndexBuffer.data, 0, 0, worldIndexBuffer.byteswritten);
	VkCommandBuffer cmdbuffer = VK_GetStageCommandBuffer(&vk_state.device.stage); //do this after staging the data since the stage command may have flipped the buffer

	VkMemoryBarrier2 barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT;

	VkDependencyInfo depinfo = {};
	depinfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depinfo.memoryBarrierCount = 1;
	depinfo.pMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(cmdbuffer, &depinfo);

	R_FreeBufferBuilder(&worldVertexBuffer);
	R_FreeBufferBuilder(&worldIndexBuffer);
}

//[ISB] ew
extern void R_SetCacheState(msurface_t* surf);
extern void R_BuildLightMap(msurface_t* surf, byte* dest, int stride);

static void R_CheckLightmapUpdate(msurface_t* surf)
{
	//Don't even bother if the surface is unlit.
	if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
		return;

	qboolean needs_update = qfalse;
	qboolean dynamic = qfalse;
	int map;
	for (map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++)
	{
		if (r_newrefdef.lightstyles[surf->styles[map]].white != surf->cached_light[map])
		{
			needs_update = qtrue;
			break;
		}
	}

	//Unlike the Quake 2 GL renderer, this needs to also account for the frame afterwards, to erase the dynamic light.
	//A downside of not being able to change vertex data on the fly, so I can't change the lightmap layer used. 
	//if (surf->dlightframe == r_framecount || surf->dlightframe == (r_framecount - 1))
	if (surf->dlightframe != 0)
	{
		needs_update = qtrue;
		dynamic = qtrue;
	}

	if (needs_update)
	{
		R_BeginLightmapUpdates();

		static unsigned	temp[128 * 128];
		int			smax, tmax;

		//if ((surf->styles[map] >= 32 || surf->styles[map] == 0) && (surf->dlightframe != r_framecount || surf->dlightframe != (r_framecount-1)))
		smax = (surf->extents[0] >> 4) + 1;
		tmax = (surf->extents[1] >> 4) + 1;

		//build the lightmap
		R_BuildLightMap(surf, (byte*)temp, smax * 4);
		//[ISB] I don't know if it'll cause issues elsewhere, only one way to find out, but use the cache for any lightstyle.
		//GL is constantly recreating the dynamic lightmap any frame where the lightstyle isn't at the default state. This makes it only upload on changes. 
		if (!dynamic /*&& (surf->styles[map] >= 32 || surf->styles[map] == 0)*/)
			R_SetCacheState(surf);

		//add the new lightmap to the image
		//VK_StageLayeredImageData(&vk_state.device.stage, lightmap_image, surf->lightmaptexturenum, BLOCK_WIDTH, BLOCK_HEIGHT, vk_lms.lightmap_buffer);
		VK_StageLayeredImageDataSub(&vk_state.device.stage, lightmap_image, surf->lightmaptexturenum, surf->light_s, surf->light_t, smax, tmax, temp);
		c_num_light_pushes++;

		//This is a bit hacky, but the surface needs to be updated later to erase the dynamic light. 
		if (dynamic && surf->dlightframe != r_framecount)
			surf->dlightframe = 0;
	}
}

extern VkCommandBuffer currentcmdbuffer;

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
image_t* R_TextureAnimation(mtexinfo_t* tex)
{
	int		c;

	if (!tex->next)
		return tex->image;

	c = currententity->frame % tex->numframes;
	while (c)
	{
		tex = tex->next;
		c--;
	}

	return tex->image;
}

/*
===============
R_DrawWarpSurfaces

Draws non-transparent warp surfaces. Should be done right after rendering the solid faces of the world or a brush model
===============
*/
void R_DrawWarpSurfaces()
{
	//I would figure pipeline compatibility would make the camera block stay bound, but the validation layers complain about it. 
	//vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bmodel_pipeline_layout, 0, 1, &persp_descriptor_set.set, 0, NULL);
	vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.brush_unlit_warped);

	for (msurface_t* chain = r_warp_surfaces; chain; chain = chain->texturechain)
	{
		//two rebinds per chain? ew
		vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bmodel_pipeline_layout, 1, 1, &chain->texinfo->image->descriptor.set, 0, NULL);

		//This replicates the context the chain was added in, since it can be part of a moved bmodel. 
		vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bmodel_pipeline_layout, 3, 1, &modelview_descriptor.set, 1, &chain->modelview_offset);
		vkCmdDrawIndexed(currentcmdbuffer, chain->numvertices, 1, chain->basevertex, 0, 0);

		c_brush_polys++;
	}

	r_warp_surfaces = NULL;
}

void R_DebugNodeWalk(mnode_t* node, uint32_t offset)
{
	if (!node) return;

	if (node->contents != -1)
	{
		//A leaf, convert to it
		mleaf_t* pleaf = (mleaf_t*)node;

		//surface to mark
		msurface_t** mark = pleaf->firstmarksurface;
		//number of surfaces to mark
		int c = pleaf->nummarksurfaces;

		//do we have any surfaces to mark?
		if (c)
		{
			do
			{
				//As I've learned, a surf can exist in multiple leaves apparently, so we need to mark them here
				//but don't allow a surface to get their lightmap updated twice. 
				if ((*mark)->visframe != r_framecount)
				{
					//mark them by setting visframe to the current framecount. This indicates that they are to be drawn
					(*mark)->visframe = r_framecount;
					R_CheckLightmapUpdate(*mark);
				}
				mark++;
			} while (--c);
		}

		/*for (int i = 0; i < pleaf->numsurfacechains; i++)
		{
			surfacechain_t* chainptr = &pleaf->firstsurfacechain[i];
			if (chainptr < &currentmodel->chains[0] || chainptr >= &currentmodel->chains[currentmodel->numsurfacechains])
				printf("chain ptr out of range");
			chainptr->visframe = r_framecount;
		}*/

		return;
	}

	R_DebugNodeWalk(node->children[0], offset);
	R_DebugNodeWalk(node->children[1], offset);
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel(entity_t* e)
{
	if (currentmodel->nummodelsurfaces == 0)
		return;

	currententity = e;

	vec3_t		mins, maxs;
	qboolean	rotated;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = qtrue;
		for (int i = 0; i < 3; i++)
		{
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	}
	else
	{
		rotated = qfalse;
		VectorAdd(e->origin, currentmodel->mins, mins);
		VectorAdd(e->origin, currentmodel->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	// calculate dynamic lighting for bmodel
	if (!vk_dynamiclightmode->value != 2)
	{
		dlight_t* lt = r_newrefdef.dlights;
		for (int k = 0; k < r_newrefdef.num_dlights; k++, lt++)
		{
			R_MarkLights(lt, 1 << k, currentmodel->nodes + currentmodel->firstnode);
		}
	}

	entitymodelview_t* modelview;
	uint32_t bufferoffset = R_RotateForEntity(e, &modelview);

	float modulate = vk_modulate->value;

	VkDeviceSize dontyouthinkimdoingtoomanybinds = 0;
	vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.brush_lightmap);
	vkCmdBindVertexBuffers(currentcmdbuffer, 0, 1, &r_worldmodel->vertbuffer, &dontyouthinkimdoingtoomanybinds);
	vkCmdBindIndexBuffer(currentcmdbuffer, r_worldmodel->indexbuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdPushConstants(currentcmdbuffer, bmodel_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(modulate), &modulate);
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bmodel_pipeline_layout, 2, 1, &lightmap_image->descriptor.set, 0, NULL);
	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bmodel_pipeline_layout, 3, 1, &modelview_descriptor.set, 1, &bufferoffset);

	//R_DebugNodeWalk(&r_worldmodel->nodes[currentmodel->firstnode], bufferoffset);

	msurface_t* surf = &currentmodel->surfaces[currentmodel->firstmodelsurface];

	for (int i = 0; i < currentmodel->nummodelsurfaces; i++, surf++)
	{
		R_CheckLightmapUpdate(surf);
		surf->modelview_offset = bufferoffset;

		if (surf->texinfo->flags & SURF_NODRAW)
			continue;

		//sky surface, mark as such and drop
		if (surf->texinfo->flags & SURF_SKY)
		{	// just adds to visible sky bounds
			surf->texturechain = r_sky_surfaces;
			r_sky_surfaces = surf;
		}
		//transparent texture, to be rendered later
		else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
		{	// add to the translucent chain
			surf->texturechain = r_alpha_surfaces;
			r_alpha_surfaces = surf;
		}
		else if (surf->flags & SURF_DRAWTURB)
		{
			surf->texturechain = r_warp_surfaces;
			r_warp_surfaces = surf;
		}
		//Normal surface
		else
		{
			image_t* image = R_TextureAnimation(surf->texinfo);
			vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, world_pipeline_layout, 1, 1, &image->descriptor.set, 0, NULL);
			vkCmdDrawIndexed(currentcmdbuffer, surf->numvertices, 1, surf->basevertex, 0, 0);
			c_brush_polys++;
		}
	}

	R_DrawWarpSurfaces();
}

/*
================
R_DrawAlphaSurfaces

Draw water surfaces and windows.
The BSP tree is waled front to back, so unwinding the chain
of alpha_surfaces will draw back to front, giving proper ordering.
================
*/
void R_DrawAlphaSurfaces(void)
{
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	VkDeviceSize reallyshouldactuallyusetheoffsetfeature = 0;

	vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.brush_unlit_warped);
	vkCmdBindVertexBuffers(currentcmdbuffer, 0, 1, &r_worldmodel->vertbuffer, &reallyshouldactuallyusetheoffsetfeature);

	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bmodel_pipeline_layout, 2, 1, &lightmap_image->descriptor.set, 0, NULL);

	VkPipeline lastpipeline = VK_NULL_HANDLE;
	for (msurface_t* chain = r_alpha_surfaces; chain; chain = chain->texturechain)
	{
		if (chain->texinfo->flags & SURF_WARP && lastpipeline != vk_state.device.brush_unlit_warped_alpha)
		{
			vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.brush_unlit_warped_alpha);
			lastpipeline = vk_state.device.brush_unlit_warped_alpha;
		}
		else if (!(chain->texinfo->flags & SURF_WARP) && lastpipeline != vk_state.device.brush_unlit)
		{
			vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.brush_unlit);
			lastpipeline = vk_state.device.brush_unlit;
		}
		//two rebinds per chain? ew
		vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bmodel_pipeline_layout, 1, 1, &chain->texinfo->image->descriptor.set, 0, NULL);

		//This replicates the context the chain was added in, since it can be part of a moved bmodel. 
		vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bmodel_pipeline_layout, 3, 1, &modelview_descriptor.set, 1, &chain->modelview_offset);
		vkCmdDrawIndexed(currentcmdbuffer, chain->numvertices, 1, chain->basevertex, 0, 0);

		c_brush_polys++;
	}

	r_alpha_surfaces = NULL;
}

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode(mnode_t* node)
{
	int			c, side, sidebit;
	cplane_t* plane;
	msurface_t* surf, ** mark;
	mleaf_t* pleaf;
	float		dot;
	image_t* image;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	//Cull the face if outside the view frustrum
	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	//if a leaf node, draw stuff
	if (node->contents != -1)
	{
		//A leaf, convert to it
		pleaf = (mleaf_t*)node;

		// check for door connected areas
		if (r_newrefdef.areabits)
		{
			if (!(r_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7))))
				return;		// not visible
		}

		//surface to mark
		mark = pleaf->firstmarksurface;
		//number of surfaces to mark
		c = pleaf->nummarksurfaces;

		//do we have any surfaces to mark?
		if (c)
		{
			do
			{
				//As I've learned, a surf can exist in multiple leaves apparently, so we need to mark them here
				//but don't allow a surface to get their lightmap updated twice. 
				if ((*mark)->visframe != r_framecount)
				{
					//mark them by setting visframe to the current framecount. This indicates that they are to be drawn
					(*mark)->visframe = r_framecount;
					R_CheckLightmapUpdate(*mark);
				}
				mark++;
			} while (--c);
		}

		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct(modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
	{
		side = 0;
		sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	// recurse down the children, front side first
	R_RecursiveWorldNode(node->children[side]);

	for (c = node->numsurfaces, surf = r_worldmodel->surfaces + node->firstsurface; c; c--, surf++)
	{
		//if not marked, skip it
		if (surf->visframe != r_framecount)
			continue;

		//cull backfaces
		if ((surf->flags & SURF_PLANEBACK) != sidebit)
			continue;		// wrong side

		surf->modelview_offset = 0; //TODO: probably not needed, since surfaces shouldn't be part of both submodels and the main world model. 

		//sky surface, mark as such and drop
		if (surf->texinfo->flags & SURF_SKY)
		{	// just adds to visible sky bounds
			surf->texturechain = r_sky_surfaces;
			r_sky_surfaces = surf;
		}
		//transparent texture, to be rendered later
		else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
		{	// add to the translucent chain
			surf->texturechain = r_alpha_surfaces;
			r_alpha_surfaces = surf;
		}
		else if (surf->flags & SURF_DRAWTURB)
		{
			surf->texturechain = r_warp_surfaces;
			r_warp_surfaces = surf;
		}
		//Normal surface
		else
		{
			if (surf->texinfo->flags & SURF_NODRAW)
				continue;

			image = R_TextureAnimation(surf->texinfo);
			vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, world_pipeline_layout, 1, 1, &image->descriptor.set, 0, NULL);
			vkCmdDrawIndexed(currentcmdbuffer, surf->numvertices, 1, surf->basevertex, 0, 0);
			c_brush_polys++;
		}
	}

	// recurse down the back side
	R_RecursiveWorldNode(node->children[!side]);

	// draw stuff

	/*surfacechain_t* chaintest = node->firstsurfacechain;
	for (int j = 0; j < node->numsurfacechains; j++, chaintest++)
	{
		if (chaintest->visframe != r_framecount)
			continue;

		if (chaintest->texinfo->flags & (SURF_TRANS66 | SURF_TRANS33))
		{
			chaintest->chain = r_alpha_surfaces;
			chaintest->modelview_offset = 0; //buffer offset is always 0 for worldmodel. 
			r_alpha_surfaces = chaintest;
			continue;
		}

		if (chaintest->texinfo->flags & SURF_WARP)
		{
			chaintest->chain = r_warp_surfaces;
			chaintest->modelview_offset = 0; //buffer offset is always 0 for worldmodel. 
			r_warp_surfaces = chaintest;
			continue;
		}

		if (chaintest->texinfo->flags & SURF_SKY)
		{
			chaintest->chain = r_sky_surfaces;
			chaintest->modelview_offset = 0; //buffer offset is always 0 for worldmodel. 
			r_sky_surfaces = chaintest;
			continue;
		}

		if (chaintest->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
			continue;

		//Bind the image for this draw chain
		image_t* image = R_TextureAnimation(chaintest->texinfo);
		vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, world_pipeline_layout, 1, 1, &image->descriptor.set, 0, NULL);
		vkCmdDraw(currentcmdbuffer, chaintest->count, 1, chaintest->offset, 0);

		c_brush_polys++;
	}*/
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld(void)
{
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	r_alpha_surfaces = NULL;

	currentmodel = r_worldmodel;

	VectorCopy(r_newrefdef.vieworg, modelorg); //so front/back checks work

	entity_t ent;
	// auto cycle the world frame for texture animation
	memset(&ent, 0, sizeof(ent));
	ent.frame = (int)(r_newrefdef.time * 2);
	currententity = &ent;

	float modulate = vk_modulate->value;
	vkCmdPushConstants(currentcmdbuffer, world_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(modulate), &modulate);

	vkCmdBindDescriptorSets(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, world_pipeline_layout, 2, 1, &lightmap_image->descriptor.set, 0, NULL);
	VkDeviceSize candobetter = 0;
	vkCmdBindVertexBuffers(currentcmdbuffer, 0, 1, &currentmodel->vertbuffer, &candobetter);
	vkCmdBindIndexBuffer(currentcmdbuffer, currentmodel->indexbuffer, 0, VK_INDEX_TYPE_UINT32);

	if (vk_lightmap->value)
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.world_lightmap_only);
	else if (vk_lightmode->value)
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.world_lightmap_unclamped);
	else
		vkCmdBindPipeline(currentcmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.device.world_lightmap);

	R_RecursiveWorldNode(r_worldmodel->nodes);

	//DEBUG
	/*extern model_t	mod_inline[];

	for (int i = 1; i < r_worldmodel->numsubmodels; i++)
	{
		model_t* hackmod = &mod_inline[i];

		if (hackmod->firstnode < 0)
			continue;

		R_DebugNodeWalk(&r_worldmodel->nodes[hackmod->firstnode]);
	}*/

	//Draw non-transparent postrender surfaces here, since they write to the depth buffer. 

	R_DrawWarpSurfaces();
	R_DrawSkySurfaces(r_sky_surfaces);
	r_sky_surfaces = NULL;
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
void R_MarkLeaves(void)
{
	byte* vis;
	byte	fatvis[MAX_MAP_LEAFS / 8];
	//renamed lleaf (local leaf) to remind that the actual data content is just a leaf
	mnode_t* lleaf;
	int		i, c;
	mleaf_t* leaf;
	int		cluster;

	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2 && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if (vk_lockpvs->value)
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	//if novis is enabled, mark everything
	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i = 0; i < r_worldmodel->numleafs; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i = 0; i < r_worldmodel->numnodes; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS(r_viewcluster, r_worldmodel);
	// may have to combine two clusters because of solid water boundaries
	if (r_viewcluster2 != r_viewcluster)
	{
		memcpy(fatvis, vis, (r_worldmodel->numleafs + 7) / 8);
		vis = Mod_ClusterPVS(r_viewcluster2, r_worldmodel);
		c = (r_worldmodel->numleafs + 31) / 32;
		for (i = 0; i < c; i++)
			((int*)fatvis)[i] |= ((int*)vis)[i];
		vis = fatvis;
	}

	//run through and mark all leaves
	for (i = 0, leaf = r_worldmodel->leafs; i < r_worldmodel->numleafs; i++, leaf++)
	{
		cluster = leaf->cluster;
		if (cluster == -1)
			continue;
		//if (cluster == r_viewcluster)
		//	current_viewleaf = leaf;


		if (vis[cluster >> 3] & (1 << (cluster & 7)))
		//if (cluster == r_viewcluster)
		{
			//cast to node here so we can iterate through each leaf's parents. 
			lleaf = (mnode_t*)leaf;
			do
			{
				//already visible
				if (lleaf->visframe == r_visframecount)
					break;
				//set visible
				lleaf->visframe = r_visframecount;
				lleaf = lleaf->parent;
			} while (lleaf);
		}
	}
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

static void LM_InitBlock(void)
{
	memset(vk_lms.allocated, 0, sizeof(vk_lms.allocated));
}

static void LM_UploadBlock(qboolean dynamic)
{
	int texture;
	int height = 0;

	if (dynamic)
	{
		texture = 0;
	}
	else
	{
		texture = vk_lms.current_lightmap_texture;
	}

	//GL_Bind(gl_state.lightmap_textures + texture);
	//qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (dynamic)
	{
		int i;

		for (i = 0; i < BLOCK_WIDTH; i++)
		{
			if (vk_lms.allocated[i] > height)
				height = vk_lms.allocated[i];
		}

		//I swear to god you better have initialized a transfer in the stage. 

		/*qglTexSubImage2D(GL_TEXTURE_2D,
			0,
			0, 0,
			BLOCK_WIDTH, height,
			GL_LIGHTMAP_FORMAT,
			GL_UNSIGNED_BYTE,
			gl_lms.lightmap_buffer);*/
	}
	else
	{
		//I swear to god you better have initialized a transfer in the stage.
		VK_StageLayeredImageData(&vk_state.device.stage, lightmap_image, texture, BLOCK_WIDTH, BLOCK_HEIGHT, vk_lms.lightmap_buffer);
		/*qglTexImage2D(GL_TEXTURE_2D,
			0,
			gl_lms.internal_format,
			BLOCK_WIDTH, BLOCK_HEIGHT,
			0,
			GL_LIGHTMAP_FORMAT,
			GL_UNSIGNED_BYTE,
			gl_lms.lightmap_buffer);*/

		if (++vk_lms.current_lightmap_texture == MAX_LIGHTMAPS)
			ri.Sys_Error(ERR_DROP, "LM_UploadBlock() - MAX_LIGHTMAPS exceeded\n");
	}
}

// returns a texture number and the position inside it
static qboolean LM_AllocBlock(int w, int h, int* x, int* y)
{
	int		i, j;
	int		best, best2;

	best = BLOCK_HEIGHT;

	for (i = 0; i < BLOCK_WIDTH - w; i++)
	{
		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (vk_lms.allocated[i + j] >= best)
				break;
			if (vk_lms.allocated[i + j] > best2)
				best2 = vk_lms.allocated[i + j];
		}
		if (j == w)
		{	// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > BLOCK_HEIGHT)
		return qfalse;

	for (i = 0; i < w; i++)
		vk_lms.allocated[*x + i] = best + h;

	return qtrue;
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void VK_CreateSurfaceLightmap(msurface_t* surf)
{
	int		smax, tmax;
	byte* base;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
	{
		LM_UploadBlock(qfalse);
		LM_InitBlock();
		if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
		{
			ri.Sys_Error(ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed\n", smax, tmax);
		}
	}

	surf->lightmaptexturenum = vk_lms.current_lightmap_texture;

	base = vk_lms.lightmap_buffer;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES;

	R_SetCacheState(surf);
	R_BuildLightMap(surf, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
}

/*
==================
GL_BeginBuildingLightmaps

==================
*/
void VK_BeginBuildingLightmaps(model_t* m)
{
	static lightstyle_t	lightstyles[MAX_LIGHTSTYLES];

	memset(vk_lms.allocated, 0, sizeof(vk_lms.allocated));

	r_framecount = 1;		// no dlightcache


	/*
	** setup the base lightstyles so the lightmaps won't have to be regenerated
	** the first time they're seen
	*/
	for (int i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		lightstyles[i].rgb[0] = 1;
		lightstyles[i].rgb[1] = 1;
		lightstyles[i].rgb[2] = 1;
		lightstyles[i].white = 3;
	}
	r_newrefdef.lightstyles = lightstyles;

	vk_state.lightmap_textures = 0; //[ISB] ref_vk doesn't need to reserve a texture for dynamic lights. 
	vk_lms.current_lightmap_texture = 0;

	VK_StartStagingImageData(&vk_state.device.stage, lightmap_image);
}

/*
=======================
GL_EndBuildingLightmaps
=======================
*/
void VK_EndBuildingLightmaps(void)
{
	LM_UploadBlock(qfalse);

	VK_FinishStagingImage(&vk_state.device.stage, lightmap_image);
}

