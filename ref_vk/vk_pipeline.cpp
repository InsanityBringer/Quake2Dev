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

#include "vk_local.h"

//vk_pipeline.c
//Handles everything relating to shaders, pipelines, descriptor sets, and vertex attributes. 

//There's waaaay too much data, so while I'm loathe to do this due to it hiding information, I'm putting this in a header for now
//TODO: There should be a better way to do this that isn't as cluttered. 
#include "vk_pipeline_data.h"

VkPipelineLayout screen_pipeline_layout;
VkPipelineLayout world_pipeline_layout;
VkPipelineLayout bmodel_pipeline_layout;
VkPipelineLayout alias_pipeline_layout;
VkPipelineLayout sky_pipeline_layout;
VkPipelineLayout particle_pipeline_layout;
//Internal pipeline handles, since they're all generated at once. The values in vklogicaldevice_t will be assigned from this pool. 
int num_pipelines;
VkPipeline pipelines[32];

void VK_CreateDescriptorSets(vklogicaldevice_t* device)
{
	/*typedef struct VkDescriptorSetLayoutCreateInfo {
    VkStructureType                        sType;
    const void*                            pNext;
    VkDescriptorSetLayoutCreateFlags       flags;
    uint32_t                               bindingCount;
    const VkDescriptorSetLayoutBinding*    pBindings;
} VkDescriptorSetLayoutCreateInfo;*/

	VkDescriptorSetLayoutCreateInfo projection_modelview_info =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		2, //bindingCount
		globals_layout_bindings //pBindings
	};
	
	VkDescriptorSetLayoutCreateInfo modelview_info =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		&modelview_layout_binding,
	};	

	VkDescriptorSetLayoutCreateInfo texture_info =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		&texture_layout_binding,
	};
	
	VkDescriptorSetLayoutCreateInfo dynamic_uniform_layout_info =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		&dynamic_uniform_layout_binding,
	};
	
	VkDescriptorSetLayoutCreateInfo storage_layout_info =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		&alias_storage_layout_binding,
	};

	VK_CHECK(vkCreateDescriptorSetLayout(device->handle, &projection_modelview_info, NULL, &device->projection_modelview_layout));
	VK_CHECK(vkCreateDescriptorSetLayout(device->handle, &texture_info, NULL, &device->sampled_texture_layout));
	VK_CHECK(vkCreateDescriptorSetLayout(device->handle, &dynamic_uniform_layout_info, NULL, &device->dynamic_uniform_layout));
	VK_CHECK(vkCreateDescriptorSetLayout(device->handle, &storage_layout_info, NULL, &device->storage_buffer_layout));
	VK_CHECK(vkCreateDescriptorSetLayout(device->handle, &modelview_info, NULL, &device->modelview_layout));

	VkDescriptorSetLayout screen_layouts[] = { device->projection_modelview_layout, device->sampled_texture_layout };
	VkDescriptorSetLayout world_layouts[] = { device->projection_modelview_layout, device->sampled_texture_layout, device->sampled_texture_layout };
	VkDescriptorSetLayout thing_layouts[] = { device->projection_modelview_layout, device->sampled_texture_layout, device->sampled_texture_layout, device->modelview_layout };
	VkDescriptorSetLayout alias_layouts[] = { device->projection_modelview_layout, device->sampled_texture_layout, device->storage_buffer_layout, device->modelview_layout };
	VkDescriptorSetLayout sky_layouts[] = { device->projection_modelview_layout, device->sampled_texture_layout };

	VkPipelineLayoutCreateInfo screen_pipeline_layout_info =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		2, //setLayoutCount
		screen_layouts, //pSetLayouts
		1, //pushConstantRangeCount
		& shared_push_constant_range, //pPushConstantRanges
	};

	VK_CHECK(vkCreatePipelineLayout(device->handle, &screen_pipeline_layout_info, NULL, &screen_pipeline_layout));

	VkPipelineLayoutCreateInfo world_pipeline_layout_info =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		3,
		world_layouts,
		1, //pushConstantRangeCount
		&shared_push_constant_range, //pPushConstantRanges
	};

	VK_CHECK(vkCreatePipelineLayout(device->handle, &world_pipeline_layout_info, NULL, &world_pipeline_layout));

	VkPipelineLayoutCreateInfo bmodel_pipeline_layout_info =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		4,
		thing_layouts,
		1, //pushConstantRangeCount
		& shared_push_constant_range, //pPushConstantRanges
	};

	VK_CHECK(vkCreatePipelineLayout(device->handle, &bmodel_pipeline_layout_info, NULL, &bmodel_pipeline_layout));

	VkPipelineLayoutCreateInfo alias_pipeline_layout_info =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		4,
		alias_layouts,
		1, //pushConstantRangeCount
		& shared_push_constant_range, //pPushConstantRanges
	};

	VK_CHECK(vkCreatePipelineLayout(device->handle, &alias_pipeline_layout_info, NULL, &alias_pipeline_layout));

	VkPipelineLayoutCreateInfo sky_pipeline_layout_info =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		2,
		sky_layouts,
		1, //pushConstantRangeCount
		& shared_push_constant_range, //pPushConstantRanges
	};

	VK_CHECK(vkCreatePipelineLayout(device->handle, &sky_pipeline_layout_info, NULL, &sky_pipeline_layout));

	VkPipelineLayoutCreateInfo particle_pipeline_layout_info =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		&device->projection_modelview_layout,
		1, //pushConstantRangeCount
		& shared_push_constant_range, //pPushConstantRanges
	};

	VK_CHECK(vkCreatePipelineLayout(device->handle, &particle_pipeline_layout_info, NULL, &particle_pipeline_layout));
}

void VK_DestroyDescriptorSets(vklogicaldevice_t* device)
{
	vkDestroyPipelineLayout(device->handle, screen_pipeline_layout, NULL);
	vkDestroyPipelineLayout(device->handle, world_pipeline_layout, NULL);
	vkDestroyPipelineLayout(device->handle, bmodel_pipeline_layout, NULL);
	vkDestroyPipelineLayout(device->handle, alias_pipeline_layout, NULL);
	vkDestroyPipelineLayout(device->handle, sky_pipeline_layout, NULL);
	vkDestroyPipelineLayout(device->handle, particle_pipeline_layout, NULL);
	vkDestroyDescriptorSetLayout(device->handle, device->projection_modelview_layout, NULL);
	vkDestroyDescriptorSetLayout(device->handle, device->sampled_texture_layout, NULL);
	vkDestroyDescriptorSetLayout(device->handle, device->dynamic_uniform_layout, NULL);
	vkDestroyDescriptorSetLayout(device->handle, device->storage_buffer_layout, NULL);
	vkDestroyDescriptorSetLayout(device->handle, device->modelview_layout, NULL);
}

void VK_CreatePipelines(vklogicaldevice_t* device)
{
	VK_CHECK(vkCreateShaderModule(device->handle, &generic_colored_frag_info, NULL, &generic_colored_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &screen_colored_vert_info, NULL, &screen_colored_vert_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &screen_textured_frag_info, NULL, &screen_textured_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &screen_textured_vert_info, NULL, &screen_textured_vert_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &world_lightmap_vert_info, NULL, &world_lightmap_vert_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &world_lightmap_frag_info, NULL, &world_lightmap_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &world_lightmap_unclamped_frag_info, NULL, &world_lightmap_unclamped_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &world_lightmap_only_frag_info, NULL, &world_lightmap_only_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &world_unlit_vert_info, NULL, &world_unlit_vert_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &world_unlit_frag_info, NULL, &world_unlit_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &world_unlit_warped_frag_info, NULL, &world_unlit_warped_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &brush_lightmap_vert_info, NULL, &brush_lightmap_vert_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &alias_vertexlit_vert_info, NULL, &alias_vertexlit_vert_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &alias_vertexlit_frag_info, NULL, &alias_vertexlit_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &world_sky_vert_info, NULL, &world_sky_vert_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &world_sky_frag_info, NULL, &world_sky_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &sprite_textured_vert_info, NULL, &sprite_textured_vert_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &sprite_textured_frag_info, NULL, &sprite_textured_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &particle_vert_info, NULL, &particle_vert_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &particle_frag_info, NULL, &particle_frag_module));
	VK_CHECK(vkCreateShaderModule(device->handle, &beam_vert_info, NULL, &beam_vert_module));

	VkPipelineShaderStageCreateInfo screen_colored_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr, 
			0,
			VK_SHADER_STAGE_VERTEX_BIT, //stage
			screen_colored_vert_module, //module
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT, //stage
			generic_colored_frag_module, //module
			"main",
		},
	};

	VkPipelineShaderStageCreateInfo screen_textured_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			screen_textured_vert_module,
			"main", 
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			screen_textured_frag_module,
			"main",
		},
	};

	VkPipelineShaderStageCreateInfo world_lightmap_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			world_lightmap_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			world_lightmap_frag_module,
			"main",
		},
	};

	VkPipelineShaderStageCreateInfo world_lightmap_unclamped_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			world_lightmap_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			world_lightmap_unclamped_frag_module,
			"main",
		},
	};

	VkPipelineShaderStageCreateInfo world_lightmap_only_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			world_lightmap_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			world_lightmap_only_frag_module,
			"main",
		},
	};
	
	VkPipelineShaderStageCreateInfo brush_unlit_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			world_unlit_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			world_unlit_frag_module,
			"main",
		},
	};
	
	VkPipelineShaderStageCreateInfo brush_unlit_warped_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			world_unlit_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			world_unlit_warped_frag_module,
			"main",
		},
	};

	VkPipelineShaderStageCreateInfo brush_lightmap_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			brush_lightmap_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			world_lightmap_unclamped_frag_module,
			"main",
		},
	};
	
	VkPipelineShaderStageCreateInfo alias_vertexlit_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			alias_vertexlit_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			alias_vertexlit_frag_module,
			"main",
		},
	};
	
	VkPipelineShaderStageCreateInfo world_sky_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			world_sky_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			world_sky_frag_module,
			"main",
		},
	};
	
	VkPipelineShaderStageCreateInfo sprite_textured_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			sprite_textured_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			sprite_textured_frag_module,
			"main",
		},
	};
	
	VkPipelineShaderStageCreateInfo particle_shader_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			particle_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			particle_frag_module,
			"main",
		},
	};
	
	VkPipelineShaderStageCreateInfo beam_shader_info[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			beam_vert_module,
			"main",
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			generic_colored_frag_module,
			"main",
		},
	};

	VK_CreateDescriptorSets(device);

	VkFormat colorformat = vk_state.phys_device.surfaceformat.format;
	VkPipelineRenderingCreateInfo renderingInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		nullptr, //pNext
		0, //viewmask
		1, //colorAttachmentCount
		&colorformat, //pColorAttachmentFormats
		VK_FORMAT_D32_SFLOAT, //depthAttachmentFormat
	};

	//Update the viewport and scissors rects, now that a window has been created. 
	viewport.x = viewport.y = 0; viewport.width = vid.width; viewport.height = vid.height; viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
	scissors_rect.offset.x = scissors_rect.offset.y = 0; scissors_rect.extent.width = vid.width; scissors_rect.extent.height = vid.height;
	viewport_depthhack = viewport; viewport_depthhack.maxDepth *= .3f;

	//The big ol array that contains all the pipelines that are needed
	//TODO: anything to clean up this code would be appreciated.
	VkGraphicsPipelineCreateInfo pipelineInfo[] =
	{
		{ //screen_colored
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			&renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			screen_colored_info, //pStages
			&screen_vertex_info, //pVertexInputState
			&triangle_strip_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			&viewport_info, //pViewportState
			&rasterization_info, //pRasterizationState
			&no_multisample_info, //pMultisampleState
			&no_depth_test_state, //pDepthStencilState
			&normal_blend_info, //pColorBlendState,
			& viewport_dynamic_info, //pDynamicState
			screen_pipeline_layout, //layout,
		},
		{ //screen_textured
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			screen_textured_info, //pStages
			& screen_vertex_info, //pVertexInputState
			& triangle_strip_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& no_depth_test_state, //pDepthStencilState
			& normal_blend_info, //pColorBlendState,
			& viewport_dynamic_info, //pDynamicState
			screen_pipeline_layout, //layout,
		},
		{ //world_lightmap
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			world_lightmap_info, //pStages
			& world_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& full_depth_test_state, //pDepthStencilState
			& no_blend_info, //pColorBlendState,
			&viewport_dynamic_info, //pDynamicState
			world_pipeline_layout, //layout,
		},
		{ //world_lightmap_unclamped
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			world_lightmap_unclamped_info, //pStages
			& world_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& full_depth_test_state, //pDepthStencilState
			& no_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			world_pipeline_layout, //layout
		},
		{ //world_lightmap_only
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			world_lightmap_only_info, //pStages
			& world_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& full_depth_test_state, //pDepthStencilState
			& no_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			world_pipeline_layout, //layout
		},
		{ //brush_unlit
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			brush_unlit_info, //pStages
			& world_unlit_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& depth_test_no_write_state, //pDepthStencilState
			& normal_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			bmodel_pipeline_layout, //layout
		},
		{ //brush_warped
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			brush_unlit_warped_info, //pStages
			& world_unlit_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& full_depth_test_state, //pDepthStencilState
			& no_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			bmodel_pipeline_layout, //layout
		},
		{ //brush_warped_alpha
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			brush_unlit_warped_info, //pStages
			& world_unlit_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& depth_test_no_write_state, //pDepthStencilState
			& normal_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			bmodel_pipeline_layout, //layout
		},
		{ //brush_lightmap
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			brush_lightmap_info, //pStages
			& world_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& full_depth_test_state, //pDepthStencilState
			& no_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			bmodel_pipeline_layout, //layout
		},
		{ //alias_vertexlit
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			alias_vertexlit_info, //pStages
			& alias_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info_alias, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& full_depth_test_state, //pDepthStencilState
			& no_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			alias_pipeline_layout, //layout
		},
		{ //alias_vertexlit_depthhack
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			alias_vertexlit_info, //pStages
			& alias_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_depthhack_info, //pViewportState
			& rasterization_info_alias, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& full_depth_test_state, //pDepthStencilState
			& no_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			alias_pipeline_layout, //layout
		},
		{ //alias_vertexlit_alpha
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			alias_vertexlit_info, //pStages
			& alias_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info_alias, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& depth_test_no_write_state, //pDepthStencilState
			& additive_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			alias_pipeline_layout, //layout
		},
		{ //world_sky
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			world_sky_info, //pStages
			& world_sky_vertex_info, //pVertexInputState
			& triangle_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& depth_test_no_write_state, //pDepthStencilState
			& no_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			sky_pipeline_layout, //layout
		},
		{ //sprite_textured
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			sprite_textured_info, //pStages
			& sprite_textured_vertex_info, //pVertexInputState
			& triangle_strip_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& full_depth_test_state, //pDepthStencilState
			& no_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			alias_pipeline_layout, //layout
		},
		{ //sprite_textured_alpha
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			sprite_textured_info, //pStages
			& sprite_textured_vertex_info, //pVertexInputState
			& triangle_strip_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& depth_test_no_write_state, //pDepthStencilState
			& normal_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			alias_pipeline_layout, //layout
		},
		{ //particle_colored
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			particle_shader_info, //pStages
			& particle_vertex_info, //pVertexInputState
			& point_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& depth_test_no_write_state, //pDepthStencilState
			& normal_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			particle_pipeline_layout, //layout
		},
		{ //beam_colored
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			beam_shader_info, //pStages
			& beam_vertex_info, //pVertexInputState
			& triangle_strip_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& depth_test_no_write_state, //pDepthStencilState
			& normal_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			alias_pipeline_layout, //layout
		},
		{ //sprite_textured_additive
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			& renderingInfo, //pNext
			0, //flags
			2, //stageCount,
			sprite_textured_info, //pStages
			& sprite_textured_vertex_info, //pVertexInputState
			& triangle_strip_input_info, //pInputAssemblyState
			nullptr, //pTessellationState,
			& viewport_info, //pViewportState
			& rasterization_info, //pRasterizationState
			& no_multisample_info, //pMultisampleState
			& depth_test_no_write_state, //pDepthStencilState
			& additive_blend_info, //pColorBlendState
			& viewport_dynamic_info, //pDynamicState
			alias_pipeline_layout, //layout
		},
	};

	num_pipelines = sizeof(pipelineInfo) / sizeof(pipelineInfo[0]);
	VK_CHECK(vkCreateGraphicsPipelines(device->handle, VK_NULL_HANDLE, num_pipelines, pipelineInfo, NULL, pipelines));

	//Assign the pipelines from the big pipeline array
	device->screen_colored = pipelines[0];
	device->screen_textured = pipelines[1];
	device->world_lightmap = pipelines[2];
	device->world_lightmap_unclamped = pipelines[3];
	device->world_lightmap_only = pipelines[4];
	device->brush_unlit = pipelines[5];
	device->brush_unlit_warped = pipelines[6];
	device->brush_unlit_warped_alpha = pipelines[7];
	device->brush_lightmap = pipelines[8];
	device->alias_vertexlit = pipelines[9];
	device->alias_vertexlit_depthhack = pipelines[10];
	device->alias_vertexlit_alpha = pipelines[11];
	device->world_sky = pipelines[12];
	device->sprite_textured = pipelines[13];
	device->sprite_textured_alpha = pipelines[14];
	device->particle_colored = pipelines[15];
	device->beam_colored = pipelines[16];
	device->sprite_textured_additive = pipelines[17];

	vkDestroyShaderModule(device->handle, screen_textured_vert_module, NULL);
	vkDestroyShaderModule(device->handle, screen_textured_frag_module, NULL);
	vkDestroyShaderModule(device->handle, screen_colored_vert_module, NULL);
	vkDestroyShaderModule(device->handle, generic_colored_frag_module, NULL);
	vkDestroyShaderModule(device->handle, world_lightmap_vert_module, NULL);
	vkDestroyShaderModule(device->handle, world_lightmap_frag_module, NULL);
	vkDestroyShaderModule(device->handle, world_lightmap_unclamped_frag_module, NULL);
	vkDestroyShaderModule(device->handle, world_lightmap_only_frag_module, NULL);
	vkDestroyShaderModule(device->handle, world_unlit_vert_module, NULL);
	vkDestroyShaderModule(device->handle, world_unlit_frag_module, NULL);
	vkDestroyShaderModule(device->handle, world_unlit_warped_frag_module, NULL);
	vkDestroyShaderModule(device->handle, brush_lightmap_vert_module, NULL);
	vkDestroyShaderModule(device->handle, alias_vertexlit_vert_module, NULL);
	vkDestroyShaderModule(device->handle, alias_vertexlit_frag_module, NULL);
	vkDestroyShaderModule(device->handle, world_sky_vert_module, NULL);
	vkDestroyShaderModule(device->handle, world_sky_frag_module, NULL);
	vkDestroyShaderModule(device->handle, sprite_textured_vert_module, NULL);
	vkDestroyShaderModule(device->handle, sprite_textured_frag_module, NULL);
	vkDestroyShaderModule(device->handle, particle_vert_module, NULL);
	vkDestroyShaderModule(device->handle, particle_frag_module, NULL);
	vkDestroyShaderModule(device->handle, beam_vert_module, NULL);
}

void VK_DestroyPipelines(vklogicaldevice_t* device)
{
	for (int i = 0; i < num_pipelines; i++)
	{
		vkDestroyPipeline(device->handle, pipelines[i], NULL);
	}
	VK_DestroyDescriptorSets(device);
}
