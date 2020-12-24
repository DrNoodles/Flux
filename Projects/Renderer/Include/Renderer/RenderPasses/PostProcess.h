#pragma once

#include "Framework/FileService.h"
#include "Renderer/GpuTypes.h"
#include "Renderer/UniformBufferObjects.h"
#include "Renderer/VulkanService.h"


struct TextureData
{
	VkDescriptorImageInfo Texture; // todo make this a texture resource? decouple from hard resources is probs better
};

class PostProcessPass
{
	struct DescriptorResources
	{
		u32 ImageCount = 0;
		std::vector<VkDescriptorSet> DescriptorSets = {};
		std::vector<VkBuffer> UboBuffers = {};
		std::vector<VkDeviceMemory> UboBuffersMemory = {};
		VkDescriptorPool DescriptorPool = nullptr;

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

	struct DrawResources
	{
		// Client used
		MeshResource Quad;
		VkPipelineLayout PipelineLayout = nullptr;
		VkPipeline Pipeline = nullptr;

		// Private resources
		VkDescriptorSetLayout DescriptorSetLayout = nullptr;

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
			vkDestroyDescriptorSetLayout(device, DescriptorSetLayout, allocator);
		}
	};


public:
	PostProcessPass() = default;
	PostProcessPass(const std::string& shaderDir, VulkanService* vk) : _vulkan(vk)
	{
		// Create quad resources
		_screenQuadResources = CreateDrawResources(
			_vulkan->GetSwapchain().GetRenderPass(),
			shaderDir,
			_vulkan->LogicalDevice(), _vulkan->PhysicalDevice(), _vulkan->CommandPool(), _vulkan->GraphicsQueue());
	}
	void Destroy()
	{
		_descriptorResources.Destroy(_vulkan->LogicalDevice(), _vulkan->Allocator());
		_screenQuadResources.Destroy(_vulkan->LogicalDevice(), _vulkan->Allocator());
	}

	void CreateDescriptorResources(TextureData input)
	{
		_descriptorResources = CreateDescriptorResources(
			_screenQuadResources.DescriptorSetLayout,
			input.Texture,
			_vulkan->GetSwapchain().GetImageCount(),
			_vulkan->LogicalDevice(), _vulkan->PhysicalDevice());
	}

	void DestroyDescriptorResources()
	{
		_descriptorResources.Destroy(_vulkan->LogicalDevice(), _vulkan->Allocator());
	}

	void Draw(VkCommandBuffer commandBuffer, i32 imageIndex, const RenderOptions& ro)
	{
		// Update Ubo
		{
			//const auto& ro = _scene.GetRenderOptions();
			PostUbo ubo;

			ubo.ShowClipping = (i32)ro.ShowClipping;
			ubo.ExposureBias = ro.ExposureBias;

			ubo.EnableVignette = (i32)ro.Vignette.Enabled;
			ubo.VignetteColor = ro.Vignette.Color;
			ubo.VignetteInnerRadius = ro.Vignette.InnerRadius;
			ubo.VignetteOuterRadius = ro.Vignette.OuterRadius;

			ubo.EnableGrain = (i32)ro.Grain.Enabled;
			ubo.GrainStrength = ro.Grain.Strength;
			ubo.GrainColorStrength = ro.Grain.ColorStrength;
			ubo.GrainSize = ro.Grain.Size;


			void* data;
			const auto size = sizeof(ubo);
			vkMapMemory(_vulkan->LogicalDevice(), _descriptorResources.UboBuffersMemory[imageIndex], 0, size, 0, &data);
			memcpy(data, &ubo, size);
			vkUnmapMemory(_vulkan->LogicalDevice(), _descriptorResources.UboBuffersMemory[imageIndex]);
		}

		// Draw
		const MeshResource& mesh = _screenQuadResources.Quad;
		VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
		VkDeviceSize pOffsets = { 0 };

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _screenQuadResources.Pipeline);


		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, &pOffsets);
		vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			_screenQuadResources.PipelineLayout,
			0, 1,
			&_descriptorResources.DescriptorSets[imageIndex], 0, nullptr);

		vkCmdDrawIndexed(commandBuffer, (u32)mesh.IndexCount, 1, 0, 0, 0);
	}

private:
	DrawResources _screenQuadResources;
	DescriptorResources _descriptorResources;
	VulkanService* _vulkan = nullptr;

	static DrawResources CreateDrawResources(VkRenderPass renderPass, const std::string& shaderDir, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue cmdQueue)
	{
		auto msaaSamples = VK_SAMPLE_COUNT_1_BIT;

		// Create the quad mesh resource that we'll render to on screen
		auto quad = [&]() -> MeshResource
		{
			Vertex topLeft = {};
			Vertex bottomLeft = {};
			Vertex bottomRight = {};
			Vertex topRight = {};

			topLeft.Pos = { -1,-1,0 };
			bottomLeft.Pos = { -1, 1,0 };
			bottomRight.Pos = { 1, 1,0 };
			topRight.Pos = { 1,-1,0 };

			topLeft.TexCoord = { 0,0 };
			bottomLeft.TexCoord = { 0,1 };
			bottomRight.TexCoord = { 1,1 };
			topRight.TexCoord = { 1,0 };

			const std::vector<Vertex> vertices = {
				topLeft,
				bottomLeft,
				bottomRight,
				topRight,
			};

			const std::vector<u32> indices = { 0,1,3,3,1,2 };


			MeshResource screenQuad;
			screenQuad.IndexCount = indices.size();
			screenQuad.VertexCount = vertices.size();

			std::tie(screenQuad.IndexBuffer, screenQuad.IndexBufferMemory) = vkh::CreateIndexBuffer(indices,
				cmdQueue, cmdPool, physicalDevice, device);

			std::tie(screenQuad.VertexBuffer, screenQuad.VertexBufferMemory) = vkh::CreateVertexBuffer(vertices,
				cmdQueue, cmdPool, physicalDevice, device);

			return screenQuad;
		}();


		VkDescriptorSetLayout descSetlayout = vkh::CreateDescriptorSetLayout(device, {
				vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
				vki::DescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
			});

		VkPipelineLayout pipelineLayout = vkh::CreatePipelineLayout(device, { descSetlayout });

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
			multisampleState.rasterizationSamples = msaaSamples;
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
			vertShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(vertPath), device);
			vertShaderStage.pName = "main";

			VkPipelineShaderStageCreateInfo fragShaderStage = {};
			fragShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(fragPath), device);
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
			pipelineCI.renderPass = renderPass;
			pipelineCI.layout = pipelineLayout;


			if (VK_SUCCESS != vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCI, nullptr, &pipeline))
			{
				throw std::runtime_error("Failed to create pipeline");
			}


			// Cleanup
			vkDestroyShaderModule(device, vertShaderStage.module, nullptr);
			vkDestroyShaderModule(device, fragShaderStage.module, nullptr);
		}


		DrawResources res = {};
		res.Quad = quad;
		res.DescriptorSetLayout = descSetlayout;
		res.PipelineLayout = pipelineLayout;
		res.Pipeline = pipeline;

		return res;
	}

	static DescriptorResources CreateDescriptorResources(VkDescriptorSetLayout descSetlayout, const VkDescriptorImageInfo& screenMap,
		u32 imageCount, VkDevice device, VkPhysicalDevice physicalDevice)
	{
		// Create uniform buffers
		const auto uboSize = sizeof(PostUbo);
		auto [uboBuffers, uboBuffersMemory]
			= vkh::CreateUniformBuffers(imageCount, uboSize, device, physicalDevice);


		// Create descriptor pool
		VkDescriptorPool descPool;
		{
			const std::vector<VkDescriptorPoolSize> poolSizes = {
				VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount},
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
					vki::WriteDescriptorSet(descSets[i], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &screenMap),
					vki::WriteDescriptorSet(descSets[i], 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &bufferUboInfo),
				};

				vkh::UpdateDescriptorSets(device, writes);
			}
		}

		DescriptorResources res;
		res.ImageCount = imageCount;
		res.UboBuffers = uboBuffers;
		res.UboBuffersMemory = uboBuffersMemory;
		res.DescriptorPool = descPool;
		res.DescriptorSets = descSets;
		return res;
	}

};

