#pragma once


#include <Framework/FileService.h>
#include <Renderer/Renderer.h>

#include <vulkan/vulkan.h>

#include <tuple>

#include "UiPresenter.h"


namespace UiPresenterHelpers
{
		
	struct PostPassResources
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
	
	struct FramebufferAttachmentResources
	{
		VkImage Image;
		VkDeviceMemory ImageMemory;
		VkImageView ImageView;

		void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
		{
			vkFreeMemory(device, ImageMemory, allocator);
			vkDestroyImage(device, Image, allocator);
			vkDestroyImageView(device, ImageView, allocator);
		}
	};
	
	struct FramebufferResources
	{
		VkExtent2D Extent = {};
		VkFormat Format = {};
		std::vector<FramebufferAttachmentResources> Attachments = {};
		VkFramebuffer Framebuffer = nullptr;

		void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
		{
			for (auto && attachment : Attachments)
			{
				attachment.Destroy(device, allocator);
			}

			vkDestroyFramebuffer(device, Framebuffer, allocator);
		}
	};


	inline FramebufferResources CreateSceneOffscreenFramebuffer(VkImageView outputImageView, VkRenderPass renderPass, VulkanService& vk)
	{
		const auto format = VK_FORMAT_R16G16B16A16_SFLOAT;
		const auto extent = vk.SwapchainExtent();

		
		// Create color attachment resources
		FramebufferAttachmentResources colorAttachment = {};
		{
			const u32 mipLevels = 1;
			const u32 layerCount = 1;
			
			// Create color image and memory
			std::tie(colorAttachment.Image, colorAttachment.ImageMemory) = vkh::CreateImage2D(
				extent.width, extent.height,
				mipLevels,
				vk.MsaaSamples(),
				format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vk.PhysicalDevice(), vk.LogicalDevice());

			// Create image view
			colorAttachment.ImageView = vkh::CreateImage2DView(colorAttachment.Image, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, layerCount, vk.LogicalDevice());
		}

		
		// Create depth attachment resources
		FramebufferAttachmentResources depthAttachment = {};
		std::tie(depthAttachment.Image, depthAttachment.ImageMemory, depthAttachment.ImageView) = vkh::CreateDepthResources(extent, vk.MsaaSamples(), vk.LogicalDevice(), vk.PhysicalDevice());



		// Create framebuffer
		VkFramebuffer framebuffer = vkh::CreateFramebuffer(vk.LogicalDevice(), 
			extent.width,extent.height,
			{ colorAttachment.ImageView, depthAttachment.ImageView, outputImageView }, 
			renderPass);
		
		FramebufferResources res = {};
		res.Framebuffer = framebuffer;
		res.Extent = extent;
		res.Format = format;
		res.Attachments = { colorAttachment, depthAttachment };
		
		return res;
	}


	inline PostPassResources CreatePostPassResources(const VkDescriptorImageInfo& screenMap, u32 imageCount, 
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
			pipelineCI.renderPass = vk.SwapchainRenderPass();
			pipelineCI.layout = pipelineLayout;

			
			if (VK_SUCCESS != vkCreateGraphicsPipelines(vk.LogicalDevice(), nullptr, 1, &pipelineCI, nullptr, &pipeline))
			{
				throw std::runtime_error("Failed to create pipeline");
			}


			// Cleanup
			vkDestroyShaderModule(vk.LogicalDevice(), vertShaderStage.module, nullptr);
			vkDestroyShaderModule(vk.LogicalDevice(), fragShaderStage.module, nullptr);
		}


		PostPassResources res = {};
		res.Quad = quad;
		res.DescriptorSets = descSets;
		res.DescriptorSetLayout = descSetlayout;
		res.PipelineLayout = pipelineLayout;
		res.Pipeline = pipeline;
		res.DescriptorPool = descPool;
		
		return res;
	}

	inline TextureResource CreateScreenTexture(u32 width, u32 height, VulkanService& vk)
	{
		const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
		const u32 mipLevels = 1;
		const u32 layerCount = 1;


		VkImage image;
		VkDeviceMemory memory;
		std::tie(image, memory) = vkh::CreateImage2D(width, height, mipLevels,
			VK_SAMPLE_COUNT_1_BIT,
			format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | // Used in offscreen framebuffer
			//VK_IMAGE_USAGE_TRANSFER_SRC_BIT |     // Need to convert layout to attachment optimal in prep for framebuffer writing
			VK_IMAGE_USAGE_SAMPLED_BIT,           // Framebuffer result is used in later shader pass

			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vk.PhysicalDevice(), vk.LogicalDevice(),
			layerCount);

		
		// Transition image layout
		{
			const auto cmdBuf = vkh::BeginSingleTimeCommands(vk.CommandPool(), vk.LogicalDevice());

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = mipLevels;

			vkh::TransitionImageLayout(cmdBuf, image,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

			vkh::EndSingeTimeCommands(cmdBuf, vk.CommandPool(), vk.GraphicsQueue(), vk.LogicalDevice());
		}


		VkImageView view = vkh::CreateImage2DView(image, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, layerCount, vk.LogicalDevice());

		
		VkSampler sampler = vkh::CreateSampler(vk.LogicalDevice());

		
		return TextureResource{ vk.LogicalDevice(), width, height, mipLevels, layerCount, image, memory, view, sampler, format };
	}
}
