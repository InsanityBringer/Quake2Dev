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

//Shader binaries, in headers to avoid clutter
//TODO: Maybe should use a macro to clean this up at least a little. 
#include "shaders/generic_colored_frag.h"
const VkShaderModuleCreateInfo generic_colored_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext,
	0, //flags,
	sizeof(generic_colored_frag), //codesize
	(uint32_t*)generic_colored_frag //pCode
};
VkShaderModule generic_colored_frag_module;

#include "shaders/screen_colored_vert.h"
const VkShaderModuleCreateInfo screen_colored_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(screen_colored_vert), //codeSize
	(uint32_t*)screen_colored_vert //pCode
};
VkShaderModule screen_colored_vert_module;

#include "shaders/screen_textured_frag.h"
const VkShaderModuleCreateInfo screen_textured_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(screen_textured_frag), //codeSize
	(uint32_t*)screen_textured_frag, //pCode
};
VkShaderModule screen_textured_frag_module;

#include "shaders/screen_textured_vert.h"
const VkShaderModuleCreateInfo screen_textured_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr,
	0,
	sizeof(screen_textured_vert),
	(uint32_t*)screen_textured_vert,
};
VkShaderModule screen_textured_vert_module;

//ref_vk requires the following vertex formats:
//screen: vec3 position, vec4 color, vec2 UVs
//world: vec3 position, vec2 UV, vec2 lightmapUV, int layer;
//alias: int index, vec2 UV. Actual vertex position is calculated at runtime from a storage buffer.

const VkVertexInputBindingDescription screen_vertex_binding =
{
	0, //binding
	sizeof(float) * 9, //stride
	VK_VERTEX_INPUT_RATE_VERTEX //inputRate
};

const VkVertexInputAttributeDescription screen_vertex_attributes[] =
{
	{
		0, //location
		0, //binding
		VK_FORMAT_R32G32B32_SFLOAT, //format
		0 //offset
	},
	{
		1,
		0,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		3 * sizeof(float)
	},
	{
		2,
		0,
		VK_FORMAT_R32G32_SFLOAT,
		7 * sizeof(float)
	}
};

const VkPipelineVertexInputStateCreateInfo screen_vertex_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	1, //vertexBindingDescriptionCount,
	&screen_vertex_binding, //pVertexBindingDescriptions
	sizeof(screen_vertex_attributes) / sizeof(screen_vertex_attributes[0]), //vertexAttributeDescriptionCount
	screen_vertex_attributes //pVertexAttributeDescriptions
};

/*typedef struct VkPipelineInputAssemblyStateCreateInfo {
    VkStructureType                            sType;
    const void*                                pNext;
    VkPipelineInputAssemblyStateCreateFlags    flags;
    VkPrimitiveTopology                        topology;
    VkBool32                                   primitiveRestartEnable;
} VkPipelineInputAssemblyStateCreateInfo;*/

const VkPipelineInputAssemblyStateCreateInfo triangle_input_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	nullptr, //pNext
	0, //flags,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, //topology
	VK_FALSE //primitiveRestartEnable
};

//This stands as a substitute for quads when rendering UI elements
const VkPipelineInputAssemblyStateCreateInfo triangle_strip_input_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	nullptr, //pNext
	0, //flags,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, //topology
	VK_FALSE //primitiveRestartEnable
};

//Single points, used for particles.
const VkPipelineInputAssemblyStateCreateInfo point_input_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	nullptr, //pNext
	0, //flags,
	VK_PRIMITIVE_TOPOLOGY_POINT_LIST, //topology
	VK_FALSE //primitiveRestartEnable
};

//These must be filled out at pipeline compile time, and therefore aren't consts. 
VkRect2D scissors_rect;
VkViewport viewport, viewport_depthhack;

/*typedef struct VkPipelineViewportStateCreateInfo {
	VkStructureType                       sType;
	const void* pNext;
	VkPipelineViewportStateCreateFlags    flags;
	uint32_t                              viewportCount;
	const VkViewport* pViewports;
	uint32_t                              scissorCount;
	const VkRect2D* pScissors;
} VkPipelineViewportStateCreateInfo;*/

const VkDynamicState viewport_dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

const VkPipelineDynamicStateCreateInfo viewport_dynamic_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	nullptr,
	0, //flags
	2, //dynamicStateCount,
	viewport_dynamic_states //pDynamicState
};

const VkPipelineViewportStateCreateInfo viewport_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
	nullptr, //pNext,
	0, //flags,
	1, //viewportCount
	&viewport, //pViewports
	1, //scissorCount
	&scissors_rect //pScissors
};

const VkPipelineViewportStateCreateInfo viewport_depthhack_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
	nullptr, //pNext,
	0, //flags,
	1, //viewportCount
	& viewport_depthhack, //pViewports
	1, //scissorCount
	& scissors_rect //pScissors
};

/*typedef struct VkPipelineRasterizationStateCreateInfo {
	VkStructureType                            sType;
	const void* pNext;
	VkPipelineRasterizationStateCreateFlags    flags;
	VkBool32                                   depthClampEnable;
	VkBool32                                   rasterizerDiscardEnable;
	VkPolygonMode                              polygonMode;
	VkCullModeFlags                            cullMode;
	VkFrontFace                                frontFace;
	VkBool32                                   depthBiasEnable;
	float                                      depthBiasConstantFactor;
	float                                      depthBiasClamp;
	float                                      depthBiasSlopeFactor;
	float                                      lineWidth;
} VkPipelineRasterizationStateCreateInfo;*/

const VkPipelineRasterizationStateCreateInfo rasterization_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	nullptr,
	0, //flags,
	VK_FALSE, //depthClampEnable
	VK_FALSE, //rasterizerDiscardEnable,
	VK_POLYGON_MODE_FILL, //polygonMode,
	VK_CULL_MODE_BACK_BIT, //cullMode
	VK_FRONT_FACE_COUNTER_CLOCKWISE, //frontFace,
	VK_FALSE, //depthBiasEnable
	0, //depthBiasConstantFactor
	0, //depthBiasClamp
	0, //depthBiasSlopeFactor
	1.0f //lineWidth
};

const VkPipelineRasterizationStateCreateInfo rasterization_info_wires =
{
	VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	nullptr,
	0, //flags,
	VK_FALSE, //depthClampEnable
	VK_FALSE, //rasterizerDiscardEnable,
	VK_POLYGON_MODE_LINE, //polygonMode,
	VK_CULL_MODE_BACK_BIT, //cullMode
	VK_FRONT_FACE_COUNTER_CLOCKWISE, //frontFace,
	VK_FALSE, //depthBiasEnable
	0, //depthBiasConstantFactor
	0, //depthBiasClamp
	0, //depthBiasSlopeFactor
	1.0f //lineWidth
};

//Alias winding flips the faces, just change the culling rather than bother with fixing the winding
const VkPipelineRasterizationStateCreateInfo rasterization_info_alias =
{
	VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	nullptr,
	0, //flags,
	VK_FALSE, //depthClampEnable
	VK_FALSE, //rasterizerDiscardEnable,
	VK_POLYGON_MODE_FILL, //polygonMode,
	VK_CULL_MODE_FRONT_BIT, //cullMode
	VK_FRONT_FACE_COUNTER_CLOCKWISE, //frontFace,
	VK_FALSE, //depthBiasEnable
	0, //depthBiasConstantFactor
	0, //depthBiasClamp
	0, //depthBiasSlopeFactor
	1.0f //lineWidth
};

/*typedef struct VkPipelineMultisampleStateCreateInfo {
    VkStructureType                          sType;
    const void*                              pNext;
    VkPipelineMultisampleStateCreateFlags    flags;
    VkSampleCountFlagBits                    rasterizationSamples;
    VkBool32                                 sampleShadingEnable;
    float                                    minSampleShading;
    const VkSampleMask*                      pSampleMask;
    VkBool32                                 alphaToCoverageEnable;
    VkBool32                                 alphaToOneEnable;
} VkPipelineMultisampleStateCreateInfo;*/

const VkPipelineMultisampleStateCreateInfo no_multisample_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
	nullptr, //pNext,
	0, //flags
	VK_SAMPLE_COUNT_1_BIT, //rasterizationSamples
};

/*
typedef struct VkPipelineDepthStencilStateCreateInfo {
    VkStructureType                           sType;
    const void*                               pNext;
    VkPipelineDepthStencilStateCreateFlags    flags;
    VkBool32                                  depthTestEnable;
    VkBool32                                  depthWriteEnable;
    VkCompareOp                               depthCompareOp;
    VkBool32                                  depthBoundsTestEnable;
    VkBool32                                  stencilTestEnable;
    VkStencilOpState                          front;
    VkStencilOpState                          back;
    float                                     minDepthBounds;
    float                                     maxDepthBounds;
} VkPipelineDepthStencilStateCreateInfo;*/

const VkPipelineDepthStencilStateCreateInfo no_depth_test_state =
{
	VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
	nullptr, //pNext,
	0, //flags,
	VK_FALSE //depthTestEnable
};

const VkPipelineDepthStencilStateCreateInfo full_depth_test_state =
{
	VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
	nullptr, //pNext,
	0, //flags,
	VK_TRUE, //depthTestEnable
	VK_TRUE, //depthWriteEnable
	VK_COMPARE_OP_LESS_OR_EQUAL //depthCompareOp
};

const VkPipelineDepthStencilStateCreateInfo depth_test_no_write_state =
{
	VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
	nullptr, //pNext,
	0, //flags,
	VK_TRUE, //depthTestEnable
	VK_FALSE, //depthWriteEnable
	VK_COMPARE_OP_LESS_OR_EQUAL //depthCompareOp
};

/*typedef struct VkPipelineColorBlendAttachmentState 
{
	VkBool32                 blendEnable;
	VkBlendFactor            srcColorBlendFactor;
	VkBlendFactor            dstColorBlendFactor;
	VkBlendOp                colorBlendOp;
	VkBlendFactor            srcAlphaBlendFactor;
	VkBlendFactor            dstAlphaBlendFactor;
	VkBlendOp                alphaBlendOp;
	VkColorComponentFlags    colorWriteMask;
} VkPipelineColorBlendAttachmentState;*/


const VkPipelineColorBlendAttachmentState color_attachment_no_blend_state =
{
	VK_FALSE, //blendEnable
	VK_BLEND_FACTOR_ZERO, //srcColorBlendFactor
	VK_BLEND_FACTOR_ZERO, //dstColorBlendFactor
	VK_BLEND_OP_ADD, //colorBlendOp
	VK_BLEND_FACTOR_ZERO, //srcAlphaBlendFactor
	VK_BLEND_FACTOR_ZERO, //dstAlphaBlendFactor
	VK_BLEND_OP_ADD, //alphaBlendOp
	VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |  VK_COLOR_COMPONENT_A_BIT //colorWriteMask
};

/*typedef struct VkPipelineColorBlendStateCreateInfo {
    VkStructureType                               sType;
    const void*                                   pNext;
    VkPipelineColorBlendStateCreateFlags          flags;
    VkBool32                                      logicOpEnable;
    VkLogicOp                                     logicOp;
    uint32_t                                      attachmentCount;
    const VkPipelineColorBlendAttachmentState*    pAttachments;
    float                                         blendConstants[4];
} VkPipelineColorBlendStateCreateInfo;*/

const VkPipelineColorBlendStateCreateInfo no_blend_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	VK_FALSE, //logicOpEnable
	VK_LOGIC_OP_CLEAR, //logicOp
	1, //attachmentCount
	&color_attachment_no_blend_state,
};

const VkPipelineColorBlendAttachmentState color_attachment_normal_blend_state =
{
	VK_TRUE, //blendEnable
	VK_BLEND_FACTOR_SRC_ALPHA, //srcColorBlendFactor
	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, //dstColorBlendFactor
	VK_BLEND_OP_ADD, //colorBlendOp
	VK_BLEND_FACTOR_SRC_ALPHA, //srcAlphaBlendFactor
	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, //dstAlphaBlendFactor
	VK_BLEND_OP_ADD, //alphaBlendOp
	VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT //colorWriteMask
};

const VkPipelineColorBlendStateCreateInfo normal_blend_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	VK_FALSE, //logicOpEnable
	VK_LOGIC_OP_CLEAR, //logicOp
	1, //attachmentCount
	&color_attachment_normal_blend_state,
};

//here just in case I decide to support vk_flashblend
const VkPipelineColorBlendAttachmentState color_attachment_additive_blend_state =
{
	VK_TRUE, //blendEnable
	VK_BLEND_FACTOR_SRC_ALPHA, //srcColorBlendFactor
	VK_BLEND_FACTOR_ONE, //dstColorBlendFactor
	VK_BLEND_OP_ADD, //colorBlendOp
	VK_BLEND_FACTOR_SRC_ALPHA, //srcAlphaBlendFactor
	VK_BLEND_FACTOR_ONE, //dstAlphaBlendFactor
	VK_BLEND_OP_ADD, //alphaBlendOp
	VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT //colorWriteMask
};

const VkPipelineColorBlendStateCreateInfo additive_blend_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	VK_FALSE, //logicOpEnable
	VK_LOGIC_OP_CLEAR, //logicOp
	1, //attachmentCount
	& color_attachment_additive_blend_state,
};

/*typedef struct VkDescriptorSetLayoutBinding {
    uint32_t              binding;
    VkDescriptorType      descriptorType;
    uint32_t              descriptorCount;
    VkShaderStageFlags    stageFlags;
    const VkSampler*      pImmutableSamplers;
} VkDescriptorSetLayoutBinding;*/

const VkDescriptorSetLayoutBinding globals_layout_bindings[]
{
	{
		0, //binding
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, //descriptorType
		1, //descriptorCount
		VK_SHADER_STAGE_VERTEX_BIT //stageFlags
	},
	{
		1, //binding
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, //descriptorType
		1, //descriptorCount
		VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT //stageFlags
	}
};

const VkDescriptorSetLayoutBinding modelview_layout_binding =
{
	0,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
	1,
	VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
};

const VkDescriptorSetLayoutBinding dynamic_uniform_layout_binding =
{
	0,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
	1,
	VK_SHADER_STAGE_VERTEX_BIT
};

const VkDescriptorSetLayoutBinding alias_storage_layout_binding =
{
	0,
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	1,
	VK_SHADER_STAGE_VERTEX_BIT
};

const VkDescriptorSetLayoutBinding texture_layout_binding =
{
	0,
	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	1,
	VK_SHADER_STAGE_FRAGMENT_BIT
};

//ref_vk requires XXX pipelines:
//screen_colored: Simple colored polygons. uses screen vertex attributes. uses projection/modelview descriptors at slot 0.
//screen_textured: Simple textured polygons. uses screen vertex attributes. uses projection/modelview descriptors at slot 0. uses combined image sampler at slot 1.
//world_lightmap: Lightmapped 3d polygons. Uses world vertex attributes. uses projection/modelview descriptors at slot 0. Uses combined image sampler at slot 2. Uses combined image sampler (lightmap array) at slot 3. 
// Fragment shader push constant has the modulate value.
//world_lightmap_unclamped: Same as world_lightmap, but different fragment shader allows overbrights. 
//world_warp: Same as world_lightmap, but no lightmap descriptor. Warps the world
//world_sky: Skybox. Uses world vertex attributes, but most properties are ignored. uses projection_modeview descriptors at slot 0. Uses combined image sampler at slot 2, cubemap array texture. 
//alias_vertexlit: Vertex lit 3d polygons. uses alias vertex attributes. uses projection/modelview descriptors at slot 0. Uses vertex storage buffer at slot 1. Uses combined image sampler at slot 2.
//laser_colored: Laser beams. TBD
//sprite_textured: Sprites. TBD. 

//brush model specific data.

#include "shaders/world_lightmap_frag.h"
const VkShaderModuleCreateInfo world_lightmap_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr,
	0,
	sizeof(world_lightmap_frag),
	world_lightmap_frag,
	
};
VkShaderModule world_lightmap_frag_module;

#include "shaders/world_lightmap_unclamped_frag.h"
const VkShaderModuleCreateInfo world_lightmap_unclamped_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, 
	0,
	sizeof(world_lightmap_unclamped_frag),
	world_lightmap_unclamped_frag,
	
};
VkShaderModule world_lightmap_unclamped_frag_module;

#include "shaders/world_lightmap_only_frag.h"
const VkShaderModuleCreateInfo world_lightmap_only_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr,
	0,
	sizeof(world_lightmap_only_frag),
	world_lightmap_only_frag,
	
};
VkShaderModule world_lightmap_only_frag_module;

#include "shaders/world_lightmap_vert.h"
const VkShaderModuleCreateInfo world_lightmap_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr,
	0,
	sizeof(world_lightmap_vert),
	world_lightmap_vert,
};
VkShaderModule world_lightmap_vert_module;

const VkVertexInputBindingDescription world_vertex_binding =
{
	0, //binding
	sizeof(float) * 9, //stride
	VK_VERTEX_INPUT_RATE_VERTEX, //inputRate
};

const VkVertexInputAttributeDescription world_vertex_attributes[] =
{
	{ //position
		0, //location
		0, //binding
		VK_FORMAT_R32G32B32_SFLOAT, //format
		0 //offset
	},
	{ //uv
		1,
		0,
		VK_FORMAT_R32G32_SFLOAT,
		3 * sizeof(float)
	},
	{ //scroll_speed
		2,
		0,
		VK_FORMAT_R32_SFLOAT,
		5 * sizeof(float)
	},
	{ //lightmap uv
		3,
		0,
		VK_FORMAT_R32G32_SFLOAT,
		6 * sizeof(float)
	},
	{ //lightmap layer
		4,
		0,
		VK_FORMAT_R32_UINT,
		8 * sizeof(float)
	}
};

/*typedef struct VkPipelineVertexInputStateCreateInfo {
	VkStructureType                             sType;
	const void*                                 pNext;
	VkPipelineVertexInputStateCreateFlags       flags;
	uint32_t                                    vertexBindingDescriptionCount;
	const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
	uint32_t                                    vertexAttributeDescriptionCount;
	const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
} VkPipelineVertexInputStateCreateInfo;*/

const VkPipelineVertexInputStateCreateInfo world_vertex_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	nullptr,
	0,
	1,
	&world_vertex_binding,
	sizeof(world_vertex_attributes) / sizeof(world_vertex_attributes[0]),
	world_vertex_attributes
};

/*typedef struct VkPushConstantRange {
    VkShaderStageFlags    stageFlags;
    uint32_t              offset;
    uint32_t              size;
} VkPushConstantRange;
*/

//All pipeline layouts use this, as the validation layers seem to treat binding a pipeline with incompatible push constants as disturbing the entire pipeline state. 
const VkPushConstantRange shared_push_constant_range =
{
	VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	0,
	sizeof(float) * 4,
};

//unlit brush model data. 

#include "shaders/world_unlit_frag.h"
const VkShaderModuleCreateInfo world_unlit_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(world_unlit_frag), //codeSize
	world_unlit_frag, //pCode
};
VkShaderModule world_unlit_frag_module;

#include "shaders/world_unlit_warped_frag.h"
const VkShaderModuleCreateInfo world_unlit_warped_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(world_unlit_warped_frag),
	world_unlit_warped_frag,

};
VkShaderModule world_unlit_warped_frag_module;

#include "shaders/world_unlit_vert.h"
const VkShaderModuleCreateInfo world_unlit_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(world_unlit_vert),
	world_unlit_vert,
};
VkShaderModule world_unlit_vert_module;

const VkVertexInputAttributeDescription world_unlit_vertex_attributes[] =
{
	{ //position
		0,
		0,
		VK_FORMAT_R32G32B32_SFLOAT,
		0
	},
	{ //uv
		1,
		0,
		VK_FORMAT_R32G32_SFLOAT,
		3 * sizeof(float)
	},
	{ //scroll speed
		2,
		0,
		VK_FORMAT_R32_SFLOAT,
		5 * sizeof(float)
	},
	{ //alpha
		3,
		0,
		VK_FORMAT_R32_SFLOAT,
		6 * sizeof(float)
	}
};

const VkPipelineVertexInputStateCreateInfo world_unlit_vertex_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	nullptr,
	0,
	1,
	& world_vertex_binding,
	sizeof(world_unlit_vertex_attributes) / sizeof(world_unlit_vertex_attributes[0]),
	world_unlit_vertex_attributes
};

//stuff for inline bmodels
#include "shaders/brush_lightmap_vert.h"
const VkShaderModuleCreateInfo brush_lightmap_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(brush_lightmap_vert),
	brush_lightmap_vert,
};
VkShaderModule brush_lightmap_vert_module;

//Alias model specific data
#include "shaders/alias_vertexlit_vert.h"
const VkShaderModuleCreateInfo alias_vertexlit_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(alias_vertexlit_vert),
	alias_vertexlit_vert,
};
VkShaderModule alias_vertexlit_vert_module;

#include "shaders/alias_vertexlit_frag.h"
const VkShaderModuleCreateInfo alias_vertexlit_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(alias_vertexlit_frag),
	alias_vertexlit_frag,
};
VkShaderModule alias_vertexlit_frag_module;

const VkVertexInputBindingDescription alias_vertex_binding =
{
	0, //binding
	sizeof(int) + sizeof(float) * 2, //stride
	VK_VERTEX_INPUT_RATE_VERTEX, //inputRate
};

const VkVertexInputAttributeDescription alias_vertex_attributes[] =
{
	{ //index
		0, //location
		0, //binding
		VK_FORMAT_R32_SINT, //format
		0 //offset
	},
	{ //uv
		1,
		0,
		VK_FORMAT_R32G32_SFLOAT,
		sizeof(int)
	},
};

const VkPipelineVertexInputStateCreateInfo alias_vertex_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	nullptr,
	0,
	1,
	&alias_vertex_binding,
	sizeof(alias_vertex_attributes) / sizeof(alias_vertex_attributes[0]),
	alias_vertex_attributes
};

//Sky nonsense
#include "shaders/world_sky_vert.h"
const VkShaderModuleCreateInfo world_sky_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(world_sky_vert),
	world_sky_vert,
};
VkShaderModule world_sky_vert_module;

#include "shaders/world_sky_frag.h"
const VkShaderModuleCreateInfo world_sky_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(world_sky_frag),
	world_sky_frag,
};
VkShaderModule world_sky_frag_module;

const VkVertexInputAttributeDescription world_sky_vertex_attributes[] =
{
	0, //location
	0, //binding
	VK_FORMAT_R32G32B32_SFLOAT, //format
	0 //offset
};

const VkPipelineVertexInputStateCreateInfo world_sky_vertex_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	nullptr,
	0,
	1,
	&world_vertex_binding,
	sizeof(world_sky_vertex_attributes) / sizeof(world_sky_vertex_attributes[0]),
	world_sky_vertex_attributes
};

//sprite nonsense
#include "shaders/sprite_textured_vert.h"
const VkShaderModuleCreateInfo sprite_textured_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(sprite_textured_vert),
	sprite_textured_vert,
};
VkShaderModule sprite_textured_vert_module;

#include "shaders/sprite_textured_frag.h"
const VkShaderModuleCreateInfo sprite_textured_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(sprite_textured_frag),
	sprite_textured_frag,
};
VkShaderModule sprite_textured_frag_module;

const VkPipelineVertexInputStateCreateInfo sprite_textured_vertex_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	nullptr,
	0,
	0,
	nullptr,
	0,
	nullptr
};

//particle nonsense
#include "shaders/particle_vert.h"
const VkShaderModuleCreateInfo particle_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(particle_vert),
	particle_vert,
};
VkShaderModule particle_vert_module;

#include "shaders/particle_frag.h"
const VkShaderModuleCreateInfo particle_frag_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(particle_frag),
	particle_frag,
};
VkShaderModule particle_frag_module;

const VkVertexInputBindingDescription particle_vertex_binding =
{
	0, //binding
	sizeof(float) * 7, //stride
	VK_VERTEX_INPUT_RATE_VERTEX, //inputRate
};

const VkVertexInputAttributeDescription particle_vertex_attributes[] =
{
	{ //index
		0, //location
		0, //binding
		VK_FORMAT_R32G32B32_SFLOAT, //format
		0 //offset
	},
	{ //color
		1, //location
		0, //binding
		VK_FORMAT_R32G32B32A32_SFLOAT, //format
		sizeof(float) * 3 //offset
	},
};

const VkPipelineVertexInputStateCreateInfo particle_vertex_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	nullptr,
	0,
	1,
	&particle_vertex_binding,
	sizeof(particle_vertex_attributes) / sizeof(particle_vertex_attributes[0]),
	particle_vertex_attributes
};

//beam nonsense
#include "shaders/beam_vert.h"
const VkShaderModuleCreateInfo beam_vert_info =
{
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	nullptr, //pNext
	0, //flags
	sizeof(beam_vert),
	beam_vert,
};
VkShaderModule beam_vert_module;

const VkVertexInputBindingDescription beam_vertex_binding =
{
	0, //binding
	sizeof(float) * 3, //stride
	VK_VERTEX_INPUT_RATE_VERTEX, //inputRate
};

const VkVertexInputAttributeDescription beam_vertex_attributes[] =
{
	0, //location
	0, //binding
	VK_FORMAT_R32G32B32_SFLOAT, //format
	0 //offset
};

const VkPipelineVertexInputStateCreateInfo beam_vertex_info =
{
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	nullptr,
	0,
	1,
	&beam_vertex_binding,
	1,
	beam_vertex_attributes
};
