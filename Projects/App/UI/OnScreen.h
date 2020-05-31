#pragma once

#include "Framework/FileService.h"
#include "Renderer/GpuTypes.h"
#include "Renderer/VulkanService.h"

namespace OnScreen
{
	// Used to render final result to the screen
	struct QuadResources
	{
		// Client used
		MeshResource Quad;
		std::vector<VkDescriptorSet> DescriptorSets = {};
		VkPipelineLayout PipelineLayout = nullptr;
		VkPipeline Pipeline = nullptr;

		// Private resources
		VkDescriptorSetLayout DescriptorSetLayout = nullptr;
		VkDescriptorPool DescriptorPool = nullptr;
		
		void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
		{
			//Quad
			vkDestroyBuffer(device, Quad.IndexBuffer, allocator);
			vkDestroyBuffer(device, Quad.VertexBuffer, allocator);
			vkFreeMemory(device, Quad.IndexBufferMemory, allocator);
			vkFreeMemory(device, Quad.VertexBufferMemory, allocator);

			vkDestroyPipeline(device, Pipeline, nullptr);
			vkDestroyPipelineLayout(device, PipelineLayout, nullptr);

			//vkFreeDescriptorSets(device, DescriptorPool, (u32)DescriptorSets.size(), DescriptorSets.data());
			vkDestroyDescriptorPool(device, DescriptorPool, allocator);
			vkDestroyDescriptorSetLayout(device, DescriptorSetLayout, allocator);
		}
	};

	inline QuadResources CreateQuadResources(const VkDescriptorImageInfo& screenMap, u32 imageCount, 
		const std::string& shaderDir, VulkanService& vk)
	{
		MeshResource quad;
		{
			Vertex topLeft       = {};
			Vertex bottomLeft    = {};
			Vertex bottomRight   = {};
			Vertex topRight      = {};
			
			topLeft.Pos          = {-1,-1,0};
			bottomLeft.Pos       = {-1, 1,0};
			bottomRight.Pos      = { 1, 1,0};
			topRight.Pos         = { 1,-1,0};
			
			topLeft.TexCoord     = {0,0};
			bottomLeft.TexCoord  = {0,1};
			bottomRight.TexCoord = {1,1};
			topRight.TexCoord    = {1,0};
			
			const std::vector<Vertex> vertices = {
				topLeft,
				bottomLeft,
				bottomRight,
				topRight,
			};
			
			const std::vector<u32> indices = { 0,1,3,3,1,2 };

			quad.IndexCount = indices.size();
			quad.VertexCount = vertices.size();

			std::tie(quad.IndexBuffer, quad.IndexBufferMemory) = vkh::CreateIndexBuffer(indices,
				vk.GraphicsQueue(), vk.CommandPool(), vk.PhysicalDevice(), vk.LogicalDevice());

			std::tie(quad.VertexBuffer, quad.VertexBufferMemory) = vkh::CreateVertexBuffer(vertices,
				vk.GraphicsQueue(), vk.CommandPool(), vk.PhysicalDevice(), vk.LogicalDevice());
		}

		
		VkDescriptorPool descPool;
		{
			const std::vector<VkDescriptorPoolSize> poolSizes = 
			{
				VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount }
			};
			descPool = vkh::CreateDescriptorPool(poolSizes, imageCount, vk.LogicalDevice());
		}


		VkDescriptorSetLayout descSetlayout;
		{
			descSetlayout = vkh::CreateDescriptorSetLayout(vk.LogicalDevice(),
				{
					vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
				});
		}

		
		std::vector<VkDescriptorSet> descSets;
		{
			descSets = vkh::AllocateDescriptorSets(imageCount, descSetlayout, descPool, vk.LogicalDevice());

			std::vector<VkWriteDescriptorSet> writes(descSets.size());
			for (size_t i = 0; i < descSets.size(); i++)
			{
				writes[i] = vki::WriteDescriptorSet(descSets[i], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &screenMap);
			}
			
			vkh::UpdateDescriptorSets(vk.LogicalDevice(), writes);
		}
		
		
		VkPipelineLayout pipelineLayout;
		{
			pipelineLayout = vkh::CreatePipelineLayout(vk.LogicalDevice(), { descSetlayout });
		}


		VkPipeline pipeline;
		{
			const auto vertPath = shaderDir + "Post.vert.spv";
			const auto fragPath = shaderDir + "Post.frag.spv";

			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
			inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssemblyState.flags = 0;
			inputAssemblyState.primitiveRestartEnable = VK_FALSE;

			VkPipelineRasterizationStateCreateInfo rasterizationState = {};
			rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationState.cullMode = VK_CULL_MODE_NONE;
			rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizationState.flags = 0;
			rasterizationState.depthClampEnable = VK_FALSE;
			rasterizationState.lineWidth = 1.0f;

			VkPipelineColorBlendAttachmentState blendAttachmentState = {};
			blendAttachmentState.colorWriteMask = 0xf;
			blendAttachmentState.blendEnable = VK_FALSE;

			VkPipelineColorBlendStateCreateInfo colorBlendState = {};
			colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendState.attachmentCount = 1;
			colorBlendState.pAttachments = &blendAttachmentState;

			VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
			depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilState.depthTestEnable = VK_FALSE;
			depthStencilState.depthWriteEnable = VK_FALSE;
			depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;

			VkPipelineViewportStateCreateInfo viewportState = {};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;
			viewportState.flags = 0;

			VkPipelineMultisampleStateCreateInfo multisampleState = {};
			multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleState.rasterizationSamples = vk.MsaaSamples();
			multisampleState.flags = 0;

			std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

			VkPipelineDynamicStateCreateInfo dynamicState = {};
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.pDynamicStates = dynamicStateEnables.data();
			dynamicState.dynamicStateCount = (u32)dynamicStateEnables.size();
			dynamicState.flags = 0;


			// Vertex Input  -  Define the format of the vertex data passed to the vert shader
			VkVertexInputBindingDescription vertBindingDesc = VertexHelper::BindingDescription();
			std::vector<VkVertexInputAttributeDescription> vertAttrDesc(2);
			{
				// Pos
				vertAttrDesc[0].binding = 0;
				vertAttrDesc[0].location = 0;
				vertAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
				vertAttrDesc[0].offset = offsetof(Vertex, Pos);
				// TexCoord
				vertAttrDesc[1].binding = 0;
				vertAttrDesc[1].location = 1;
				vertAttrDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
				vertAttrDesc[1].offset = offsetof(Vertex, TexCoord);
			}
			
			VkPipelineVertexInputStateCreateInfo vertexInputState = {};
			vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputState.vertexBindingDescriptionCount = 1;
			vertexInputState.pVertexBindingDescriptions = &vertBindingDesc;
			vertexInputState.vertexAttributeDescriptionCount = (u32)vertAttrDesc.size();
			vertexInputState.pVertexAttributeDescriptions = vertAttrDesc.data();

			// Shaders
			VkPipelineShaderStageCreateInfo vertShaderStage = {};
			vertShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(vertPath), vk.LogicalDevice());
			vertShaderStage.pName = "main";

			VkPipelineShaderStageCreateInfo fragShaderStage = {};
			fragShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(fragPath), vk.LogicalDevice());
			fragShaderStage.pName = "main";
			std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{ vertShaderStage, fragShaderStage };


			// Create the pipeline
			VkGraphicsPipelineCreateInfo pipelineCI = {};
			pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineCI.pInputAssemblyState = &inputAssemblyState;
			pipelineCI.pRasterizationState = &rasterizationState;
			pipelineCI.pColorBlendState = &colorBlendState;
			pipelineCI.pMultisampleState = &multisampleState;
			pipelineCI.pViewportState = &viewportState;
			pipelineCI.pDepthStencilState = &depthStencilState;
			pipelineCI.pDynamicState = &dynamicState;
			pipelineCI.pVertexInputState = &vertexInputState;
			pipelineCI.stageCount = (u32)shaderStages.size();
			pipelineCI.pStages = shaderStages.data();
			pipelineCI.renderPass = vk.GetSwapchain().GetRenderPass();
			pipelineCI.layout = pipelineLayout;

			
			if (VK_SUCCESS != vkCreateGraphicsPipelines(vk.LogicalDevice(), nullptr, 1, &pipelineCI, nullptr, &pipeline))
			{
				throw std::runtime_error("Failed to create pipeline");
			}


			// Cleanup
			vkDestroyShaderModule(vk.LogicalDevice(), vertShaderStage.module, nullptr);
			vkDestroyShaderModule(vk.LogicalDevice(), fragShaderStage.module, nullptr);
		}


		QuadResources res = {};
		res.Quad = quad;
		res.DescriptorSets = descSets;
		res.DescriptorSetLayout = descSetlayout;
		res.PipelineLayout = pipelineLayout;
		res.Pipeline = pipeline;
		res.DescriptorPool = descPool;
		
		return res;
	}
};

