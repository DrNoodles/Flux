#pragma once

#include "Renderer/GpuTypes.h"
#include "Renderer/VulkanService.h"

// Temp file to encapsulate code related to generating a shadowmap

namespace ShadowMap
{
	struct ShadowmapDescriptorResources
	{
		u32 ImageCount = 0; 
		std::vector<VkDescriptorSet> DescriptorSets = {};
		std::vector<VkBuffer> UboBuffers = {};
		std::vector<VkDeviceMemory> UboBuffersMemory = {};
		VkDescriptorPool DescriptorPool = nullptr;


		static ShadowmapDescriptorResources Create(u32 imageCount, VkDescriptorSetLayout descSetlayout, VkDevice device, VkPhysicalDevice physicalDevice)
		{
			// Create uniform buffers
			const auto uboSize = sizeof(ShadowVertUbo);
			auto [uboBuffers, uboBuffersMemory]
				= vkh::CreateUniformBuffers(imageCount, uboSize, device, physicalDevice);

			
			// Create descriptor pool
			VkDescriptorPool descPool;
			{
				const std::vector<VkDescriptorPoolSize> poolSizes = {
					VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, imageCount}
				};
				descPool = vkh::CreateDescriptorPool(poolSizes, imageCount, device);
			}


			// Create descriptor sets
			std::vector<VkDescriptorSet> descSets;
			{
				descSets = vkh::AllocateDescriptorSets(imageCount, descSetlayout, descPool, device);
				
				for (size_t i = 0; i < imageCount; i++)
				{
					VkDescriptorBufferInfo bufferUboInfo = {};
					bufferUboInfo.buffer = uboBuffers[i];
					bufferUboInfo.offset = 0;
					bufferUboInfo.range = uboSize;

					std::vector<VkWriteDescriptorSet> writes
					{
						vki::WriteDescriptorSet(descSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &bufferUboInfo),
					};
					
					vkh::UpdateDescriptorSets(device, writes);
				}
			}


			
			ShadowmapDescriptorResources res;
			res.ImageCount = imageCount;
			res.UboBuffers = uboBuffers;
			res.UboBuffersMemory = uboBuffersMemory;
			res.DescriptorPool = descPool;
			res.DescriptorSets = descSets;
			return res;
		}

		void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
		{
			for (u32 i = 0; i < ImageCount; i++)
			{
				vkDestroyBuffer(device, UboBuffers[i], allocator);
				vkFreeMemory(device, UboBuffersMemory[i], allocator);
			}
			vkDestroyDescriptorPool(device, DescriptorPool, allocator);
			ImageCount = 0;
		}
	};
	
	struct ShadowmapDrawResources
	{
		VkExtent2D Size = {};
		VkPipelineLayout PipelineLayout = nullptr;
		VkPipeline Pipeline = nullptr;
		VkRenderPass RenderPass = nullptr;
		FramebufferResources Framebuffer = {};
		VkDescriptorSetLayout DescriptorSetLayout = nullptr;

		ShadowmapDrawResources() = default;
		ShadowmapDrawResources(VkExtent2D size, const std::string& shaderDir, VulkanService& vk)
		{
			Size = size;
			RenderPass = CreateRenderPass(vk);
			Framebuffer = CreateFramebuffer(size, RenderPass, vk);
			DescriptorSetLayout = vkh::CreateDescriptorSetLayout(vk.LogicalDevice(), {
				vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
				});
			PipelineLayout = vkh::CreatePipelineLayout(vk.LogicalDevice(), { DescriptorSetLayout });
			Pipeline = CreatePipeline(shaderDir, RenderPass, PipelineLayout, vk);
		}

		void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
		{
			vkDestroyPipeline(device, Pipeline, allocator);
			vkDestroyPipelineLayout(device, PipelineLayout, allocator);
			vkDestroyRenderPass(device, RenderPass, allocator);
			vkDestroyDescriptorSetLayout(device, DescriptorSetLayout, allocator);
			Framebuffer.Destroy(device, allocator);
		}

	private:
		static VkPipeline CreatePipeline(const std::string& shaderDir, VkRenderPass renderPass,
			VkPipelineLayout pipelineLayout, VulkanService& vk)
		{
			auto* device = vk.LogicalDevice();
			const auto vertPath = shaderDir + "Shadowmap.vert.spv";
			//const auto fragPath = shaderDir + "Shadowmap.frag.spv";
			
			// Shaders
			VkPipelineShaderStageCreateInfo vertShaderStage = {};
			vertShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(vertPath), device);
			vertShaderStage.pName = "main";

			//VkPipelineShaderStageCreateInfo fragShaderStage = {};
			//fragShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			//fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			//fragShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(fragPath), device);
			//fragShaderStage.pName = "main";
			
			std::vector<VkPipelineShaderStageCreateInfo> shaderStages{ vertShaderStage };
			

			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
			inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssemblyState.flags = 0;
			inputAssemblyState.primitiveRestartEnable = VK_FALSE;

			VkPipelineRasterizationStateCreateInfo rasterizationState = {};
			rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationState.cullMode = VK_CULL_MODE_NONE; // TODO Enable culling once this renderpass works!
			rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizationState.flags = 0;
			rasterizationState.depthClampEnable = VK_FALSE;
			rasterizationState.lineWidth = 1.0f;
			//rasterizationState.depthBiasEnable = true; // TODO Enable and control dynamically via VK_DYNAMIC_STATE_SCISSOR

			/*VkPipelineColorBlendAttachmentState blendAttachmentState = {};
			blendAttachmentState.colorWriteMask = 0xf;
			blendAttachmentState.blendEnable = VK_FALSE;*/

			VkPipelineColorBlendStateCreateInfo colorBlendState = {};
			colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendState.attachmentCount = 0;
			colorBlendState.pAttachments = nullptr;//&blendAttachmentState;

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
			multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampleState.flags = 0;

			// TODO These can be static as shadowmap dims are fixed
			std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

			VkPipelineDynamicStateCreateInfo dynamicState = {};
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.pDynamicStates = dynamicStateEnables.data();
			dynamicState.dynamicStateCount = (u32)dynamicStateEnables.size();
			dynamicState.flags = 0;


			// Vertex Input  -  Define the format of the vertex data passed to the vert shader
			VkVertexInputBindingDescription vertBindingDesc = VertexHelper::BindingDescription();
			//auto vertAttrDesc = VertexHelper::AttributeDescriptions();
			std::vector<VkVertexInputAttributeDescription> vertAttrDesc(1);
			{
				// Pos
				vertAttrDesc[0].binding = 0;
				vertAttrDesc[0].location = 0;
				vertAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
				vertAttrDesc[0].offset = offsetof(Vertex, Pos);
			}

			VkPipelineVertexInputStateCreateInfo vertexInputState = {};
			vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputState.vertexBindingDescriptionCount = 1;
			vertexInputState.pVertexBindingDescriptions = &vertBindingDesc;
			vertexInputState.vertexAttributeDescriptionCount = (u32)vertAttrDesc.size();
			vertexInputState.pVertexAttributeDescriptions = vertAttrDesc.data();
			

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
			pipelineCI.renderPass = renderPass;
			pipelineCI.layout = pipelineLayout;


			VkPipeline pipeline;
			if (VK_SUCCESS != vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCI, nullptr, &pipeline))
			{
				throw std::runtime_error("Failed to create pipeline");
			}

			// Cleanup
			vkDestroyShaderModule(device, vertShaderStage.module, nullptr);
			//vkDestroyShaderModule(device, fragShaderStage.module, nullptr);

			return pipeline;
		}

		static VkRenderPass CreateRenderPass(VulkanService& vk)
		{
			const VkFormat depthFormat = vkh::FindDepthFormat(vk.PhysicalDevice());


			// Define attachments
			VkAttachmentDescription depthAttachDesc = {};
			{
				depthAttachDesc.format = depthFormat;
				depthAttachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
				depthAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				depthAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				depthAttachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				depthAttachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				depthAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				depthAttachDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}


			// Define Subpass
			VkAttachmentReference depthAttachRef = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
			VkSubpassDescription subpassDescription = {};
			{
				subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpassDescription.colorAttachmentCount = 0;	
				subpassDescription.pDepthStencilAttachment = &depthAttachRef;
			}


			// Define dependencies for layout transitions
			std::vector<VkSubpassDependency> dependencies(2);
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			return vkh::CreateRenderPass(vk.LogicalDevice(), { depthAttachDesc }, { subpassDescription }, dependencies);
		}

		static FramebufferResources CreateFramebuffer(VkExtent2D extent, VkRenderPass renderPass, VulkanService& vk)
		{
			const u32 mipLevels = 1;
			const u32 layerCount = 1;
			const auto msaaSamples = VK_SAMPLE_COUNT_1_BIT;

			auto* physicalDevice = vk.PhysicalDevice();
			auto* device = vk.LogicalDevice();

			const VkFormat depthFormat = vkh::FindDepthFormat(physicalDevice);

			// Create depth attachment
			FramebufferResources::Attachment depthAttachment = {};
			{

				// Create depth image and memory
				std::tie(depthAttachment.Image, depthAttachment.ImageMemory) = vkh::CreateImage2D(
					extent.width, extent.height,
					mipLevels,
					msaaSamples,
					depthFormat,
					VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					physicalDevice, device);

				// Create image view
				depthAttachment.ImageView = vkh::CreateImage2DView(
					depthAttachment.Image,
					depthFormat,
					VK_IMAGE_VIEW_TYPE_2D,
					VK_IMAGE_ASPECT_DEPTH_BIT,
					mipLevels,
					layerCount,
					device);
			}


			// Create framebuffer
			auto* framebuffer = vkh::CreateFramebuffer(device, extent.width, extent.height,
				{ depthAttachment.ImageView }, renderPass);


			// Sampler so it can be sampled from a shader
			auto* sampler = vkh::CreateSampler(device,
				VK_FILTER_LINEAR, VK_FILTER_LINEAR,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);


			FramebufferResources res = {};
			res.Framebuffer = framebuffer;
			res.Extent = extent;
			res.Format = depthFormat;
			res.Attachments = { depthAttachment };
			res.OutputSampler = sampler;
			res.OutputDescriptor = VkDescriptorImageInfo{ sampler, depthAttachment.ImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };
			
			return res;
		}
	};
}