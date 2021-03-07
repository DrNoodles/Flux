#pragma once

#include "Renderer/LowLevel/GpuTypes.h"
#include "Renderer/LowLevel/VulkanService.h"
#include "Renderer/LowLevel/RenderableMesh.h"

#include <Framework/FileService.h>




class DirectionalShadowRenderPass
{
public:
	struct PushConstants
	{
		glm::mat4 ShadowMatrix;
	};
	

private:
	VkPipeline _pipeline = nullptr;
	VkPipelineLayout _pipelineLayout = nullptr;
	VkRenderPass _renderPass = nullptr;

public:
	DirectionalShadowRenderPass() = default;
	DirectionalShadowRenderPass(const std::string& shaderDir, VulkanService& vk)
	{
		_renderPass = CreateRenderPass(vk);

		// Pipeline Layout
		{
			VkPushConstantRange pushConstantRange = {};
			pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			pushConstantRange.size = sizeof(PushConstants);
			pushConstantRange.offset = 0;
			_pipelineLayout = vkh::CreatePipelineLayout(vk.LogicalDevice(), {}, { pushConstantRange });
		}
		
		_pipeline = CreatePipeline(shaderDir, _renderPass, _pipelineLayout, vk);
	}

	void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
	{
		vkDestroyPipeline(device, _pipeline, allocator);
		vkDestroyRenderPass(device, _renderPass, allocator);
	}

	VkRenderPass GetRenderPass() const { return _renderPass; }
	
	void Draw(VkCommandBuffer commandBuffer, VkRect2D renderArea, const SceneRendererPrimitives& scene, const glm::mat4& lightSpaceMatrix,
		const std::vector<std::unique_ptr<RenderableMesh>>& renderables,
		const std::vector<std::unique_ptr<MeshResource>>& meshes) const
	{
		const auto viewport = vki::Viewport(renderArea);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);
		
		const float depthBiasConstant = 1.0f;
		const float depthBiasSlope = 1.f;
		
		vkCmdSetDepthBias(commandBuffer, depthBiasConstant, 0, depthBiasSlope);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

		const VkDeviceSize offsets[] = { 0 };
		const auto size = sizeof(PushConstants);
		PushConstants pushConstants{};
		
		for (const auto& object : scene.Objects)
		{
			pushConstants.ShadowMatrix = lightSpaceMatrix * object.Transform;
			
			const auto& renderable = *renderables[object.RenderableId.Value()];
			const auto& mesh = *meshes[renderable.MeshId.Value()];


			const VkBuffer vertexBuffers[] = { mesh.VertexBuffer };

			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdPushConstants(commandBuffer, _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, size, &pushConstants);
			vkCmdDrawIndexed(commandBuffer, (u32)mesh.IndexCount, 1, 0, 0, 0);
		}
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
		rasterizationState.cullMode = VK_CULL_MODE_NONE; // TODO Enable culling once this renderpass works
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationState.flags = 0;
		rasterizationState.depthClampEnable = VK_FALSE;
		rasterizationState.lineWidth = 1.0f;
		rasterizationState.depthBiasEnable = true; // Enabled in dynamic state

		/*VkPipelineColorBlendAttachmentState blendAttachmentState = {};
		blendAttachmentState.colorWriteMask = 0xf;
		blendAttachmentState.blendEnable = VK_FALSE;*/

		VkPipelineColorBlendStateCreateInfo colorBlendState = {};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendState.attachmentCount = 0;
		colorBlendState.pAttachments = nullptr;//&blendAttachmentState;

		VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = true;
		depthStencilState.depthWriteEnable = true;
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

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_DEPTH_BIAS };

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
};
