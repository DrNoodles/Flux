#pragma once 

#include "VulkanHelpers.h"
#include "VulkanInitializers.h"
#include "TextureResource.h"
#include "Texels.h"

#include <Shared/CommonTypes.h>
#include <Shared/FileService.h>

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <algorithm>
#include <iostream>

using vkh = VulkanHelpers;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class EquirectangularCubemapLoader
{
public:
	static TextureResource LoadFromPath(const std::string& path, const MeshResource& skyboxMesh, const std::string& shaderDir, VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		Inputs in = {};
		in.Path = path;
		in.ShaderDir = shaderDir;
		in.SkyboxMesh = skyboxMesh;
		in.TransferPool = transferPool;
		in.TransferQueue = transferQueue;
		in.PhysicalDevice = physicalDevice;
		in.Device = device;
		
		const u32 cubemapRes = 2048;
		const auto texelFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
		const auto targetFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
		const VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		
		auto texels = TexelsRgbaF32();
		texels.Load(path);


		// Load src equirectangular texture
		TextureResource srcTexture = [&in, &texels, &texelFormat, &targetFormat, &finalLayout]()
		{
			VkImage image;
			VkDeviceMemory memory;
			
			std::tie(image, memory) = CreateSrcImage(in, texels, texelFormat, targetFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			const auto view = vkh::CreateImage2DView(image, targetFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, in.Device);
			const auto sampler = vkh::CreateSampler(in.Device,
				VK_FILTER_LINEAR, VK_FILTER_LINEAR,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

			return TextureResource(in.Device, texels.Width(), texels.Height(), 1, 1,
				image, memory, view, sampler, targetFormat, finalLayout);
		}();


		// Create dst cubemap
		TextureResource dstCubemap = [&in, &texels, &texelFormat, &targetFormat, &finalLayout, &cubemapRes]()
		{
			VkImage image;
			VkDeviceMemory memory;

			const auto mipLevels = 1;
			const auto layerCount = 6;
			
			std::tie(image, memory) = vkh::CreateImage2D(cubemapRes, cubemapRes, mipLevels,
				VK_SAMPLE_COUNT_1_BIT,
				targetFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // copy to it and sample it in shaders
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				in.PhysicalDevice, in.Device, layerCount,
				VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
			
			const auto view = vkh::CreateImage2DView(image, targetFormat, 
				VK_IMAGE_VIEW_TYPE_CUBE, 
				VK_IMAGE_ASPECT_COLOR_BIT, 
				mipLevels, layerCount, in.Device);
			
			const auto sampler = vkh::CreateSampler(in.Device,
				VK_FILTER_LINEAR, VK_FILTER_LINEAR,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
			
			return TextureResource(in.Device, cubemapRes, cubemapRes, mipLevels, layerCount,
				image, memory, view, sampler, targetFormat, finalLayout);
		}();
		

		// Create Descriptor Pool
		VkDescriptorPool descPool = vkh::CreateDescriptorPool(
			{ VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} }, 
			1, in.Device
		);


		// Create Descriptor Set Layout
		VkDescriptorSetLayout descSetLayout = vkh::CreateDescriptorSetLayout(in.Device, { 
			vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) }
		);


		// Create Descriptor Set
		VkDescriptorSet descSet;
		{
			descSet = vkh::AllocateDescriptorSets(1, descSetLayout, descPool, in.Device)[0]; // NOTE: [0]

			auto write = vki::WriteDescriptorSet(descSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0,
				&srcTexture.DescriptorImageInfo());

			vkh::UpdateDescriptorSets(in.Device, { write });
		}


		// Create Pipeline Layout
		VkPipelineLayout pipelineLayout;
		{
			VkPushConstantRange pushConst = {};
			pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			pushConst.size = sizeof(PushConstants);
			pushConst.offset = 0;
			pipelineLayout = vkh::CreatePipelineLayout(in.Device, { descSetLayout }, { pushConst });
		}
		

		// Create RenderPass
		VkRenderPass renderPass = CreateRenderPass(in.Device, targetFormat);


		// Create RenderTarget
		RenderTarget renderTarget = {};
		{
			std::tie(renderTarget.Image, renderTarget.Memory) = vkh::CreateImage2D(
				cubemapRes, cubemapRes, 1, VK_SAMPLE_COUNT_1_BIT, 
				targetFormat, 
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // render to it, copy from it
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
				in.PhysicalDevice, in.Device);

			renderTarget.View = vkh::CreateImage2DView(renderTarget.Image, targetFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, in.Device);

			renderTarget.Framebuffer = vkh::CreateFramebuffer(in.Device, cubemapRes, cubemapRes, { renderTarget.View }, renderPass, 1);
		}

		
		// Create Pipeline
		VkPipeline pipeline;
		{
			const auto vertPath = shaderDir + "Cubemap.vert.spv";
			const auto fragPath = shaderDir + "CubemapFromEquirectangular.frag.spv";
			std::vector<VkVertexInputAttributeDescription> vertAttrDesc(1);
			{
				// Pos
				vertAttrDesc[0].binding = 0;
				vertAttrDesc[0].location = 0;
				vertAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
				vertAttrDesc[0].offset = offsetof(Vertex, Pos);
			}
			pipeline = CreatePipeline(in.Device, pipelineLayout, renderPass, vertPath, fragPath, vertAttrDesc);
		}


		// Render cubemap
		RenderCubemap(in, renderPass, pipeline, pipelineLayout, descSet, dstCubemap, renderTarget);


		// Cleanup
		vkDestroyRenderPass(device, renderPass, nullptr);
		renderTarget.Destroy(device);
		vkDestroyDescriptorPool(device, descPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		
		
		return dstCubemap;
	}
	
private:
	//static constexpr f64 PI = 3.1415926535897932384626433;

	struct PushConstants
	{
		glm::mat4 Mvp{};
	};
	struct Inputs
	{
		std::string Path{};
		std::string ShaderDir{};
		VkCommandPool TransferPool{};
		VkQueue TransferQueue{};
		VkPhysicalDevice PhysicalDevice{};
		VkDevice Device{};
		MeshResource SkyboxMesh{};
	};
	struct RenderTarget
	{
		VkImage Image;
		VkImageView View;
		VkDeviceMemory Memory;
		VkFramebuffer Framebuffer;

		void Destroy(VkDevice device)
		{
			vkDestroyFramebuffer(device, Framebuffer, nullptr);
			vkFreeMemory(device, Memory, nullptr);
			vkDestroyImageView(device, View, nullptr);
			vkDestroyImage(device, Image, nullptr);

			Image = nullptr;
			View = nullptr;
			Memory = nullptr;
			Framebuffer = nullptr;
		}
	};
	
	static std::tuple<VkImage, VkDeviceMemory> CreateSrcImage(const Inputs& in,
		const TexelsRgbaF32& texels, VkFormat texelFormat, VkFormat targetFormat, VkImageLayout targetLayout)
	{
		const u32 mipLevels = 1;
		const u32 arrayLayers = 1;
		const auto subresourceRange = vki::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, arrayLayers);
		const auto cmdBuffer = vkh::BeginSingleTimeCommands(in.TransferPool, in.Device);


		// Create staging buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		{
			std::tie(stagingBuffer, stagingBufferMemory) = vkh::CreateBuffer(
				texels.DataSize(),
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage flags
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // property flags
				in.Device, in.PhysicalDevice);


			// Copy texels from system mem to GPU staging buffer
			void* data;
			vkMapMemory(in.Device, stagingBufferMemory, 0, texels.DataSize(), 0, &data);
			memcpy(data, texels.Data().data(), texels.DataSize());
			vkUnmapMemory(in.Device, stagingBufferMemory);
		}

		
		// Create intermediate buffer to load 32 bit texture to gpu mem
		VkImage intermediateImage;
		VkDeviceMemory intermediateImageMemory;
		{
			std::tie(intermediateImage, intermediateImageMemory) = vkh::CreateImage2D(
				texels.Width(), texels.Height(),
				mipLevels,
				VK_SAMPLE_COUNT_1_BIT,
				texelFormat, 
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // Copy to then from
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				in.PhysicalDevice, in.Device,
				arrayLayers);
		}

		
		// Transition intermediate image's layout to optimal for copying to it from the staging buffer
		{
			vkh::TransitionImageLayout(cmdBuffer, intermediateImage,
				VK_IMAGE_LAYOUT_UNDEFINED, // from
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
				subresourceRange);
		}


		// Copy texels from staging buffer to intermediate image buffer		
		{
			const auto bufferCopyRegion = vki::BufferImageCopy(0, 0, 0,
				vki::ImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1),
				vki::Offset3D(0, 0, 0),
				vki::Extent3D(texels.Width(), texels.Height(), 1));

			// Copy the cube map faces from the staging buffer to the optimal tiled image
			vkCmdCopyBufferToImage(
				cmdBuffer,
				stagingBuffer,
				intermediateImage,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &bufferCopyRegion
			);
		}


		// Create dst image buffer so we can blit to the desired format
		VkImage dstImage;
		VkDeviceMemory dstImageMemory;
		{
			std::tie(dstImage, dstImageMemory) = vkh::CreateImage2D(
				texels.Width(), texels.Height(),
				mipLevels,
				VK_SAMPLE_COUNT_1_BIT,
				targetFormat, // format
				VK_IMAGE_TILING_OPTIMAL, // tiling
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, //usage flags
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //memory flags
				in.PhysicalDevice, in.Device,
				arrayLayers);// array layers for cubemap
		}


		// Convert format  - achieved via blitting from source to dest image
		{
			vkh::TransitionImageLayout(cmdBuffer, intermediateImage,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // from
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // to
				subresourceRange);
			
			vkh::TransitionImageLayout(cmdBuffer, dstImage,
				VK_IMAGE_LAYOUT_UNDEFINED, // from
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
				subresourceRange);
			
			vkh::BlitSrcToDstImage(cmdBuffer, intermediateImage, dstImage, texels.Width(), texels.Height(), subresourceRange);
		}

		
		// Change texture image layout to shader read after all faces have been copied
		vkh::TransitionImageLayout(
			cmdBuffer,
			dstImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			targetLayout,
			subresourceRange);


		// Execute commands
		vkh::EndSingeTimeCommands(cmdBuffer, in.TransferPool, in.TransferQueue, in.Device);


		// Cleanup unneeded resources - now buffer has executed!
		vkDestroyImage(in.Device, intermediateImage, nullptr);
		vkFreeMemory(in.Device, intermediateImageMemory, nullptr);
		vkFreeMemory(in.Device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(in.Device, stagingBuffer, nullptr);

		return { dstImage, dstImageMemory };
	}

	static VkRenderPass CreateRenderPass(VkDevice device, VkFormat format)
	{
		// Define attachments
		VkAttachmentDescription colorAttachmentDesc = {};
		{
			colorAttachmentDesc.format = format;
			colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}


		// Define Subpass
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkSubpassDescription subpassDescription = {};
		{
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorReference;
		}


		// Define dependencies for layout transitions
		std::vector<VkSubpassDependency> dependencies(2);
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		return vkh::CreateRenderPass(device, { colorAttachmentDesc }, { subpassDescription }, { dependencies });
	}
	
	static VkPipeline CreatePipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, 
		const std::string& vertPath, const std::string& fragPath,
		const std::vector<VkVertexInputAttributeDescription>& vertAttrDesc)
	{
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
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleState.flags = 0;

		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables.data();
		dynamicState.dynamicStateCount = (u32)dynamicStateEnables.size();
		dynamicState.flags = 0;


		// Vertex Input  -  Define the format of the vertex data passed to the vert shader
		VkVertexInputBindingDescription vertBindingDesc = Vertex::BindingDescription();

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

		VkPipeline pipeline;
		if (VK_SUCCESS != vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCI, nullptr, &pipeline))
		{
			throw std::runtime_error("Failed to create pipeline");
		}


		// Cleanup
		vkDestroyShaderModule(device, vertShaderStage.module, nullptr);
		vkDestroyShaderModule(device, fragShaderStage.module, nullptr);


		return pipeline;
	}

	static void RenderCubemap(const Inputs& in,
		VkRenderPass renderPass, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descSet,
		TextureResource& targetCube, RenderTarget& renderTarget)
	{
		std::vector<VkClearValue> clearValues(1);
		clearValues[0].color = { 0.0f, 0.0f, 0.2f, 0.0f };

		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(renderPass, renderTarget.Framebuffer,
			vki::Rect2D(0, 0, targetCube.Width(), targetCube.Height()),
			clearValues);


		const auto perspective = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f);
		
		/*glm::mat4 captureViews[] = // From Ogl code
		{
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};*/
		
		// +X -X +Y -Y +Z -Z | right, left, up, down, front, back
		std::array<glm::mat4, 6> matrices =
		{
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};
		for (auto& m : matrices)
		{
			m = glm::scale(m, { 1,-1,1 });
			m = glm::rotate(m, glm::radians(90.f), { 0,1,0 });
		}
		
		const auto cmdBuf = vkh::BeginSingleTimeCommands(in.TransferPool, in.Device);


		// Init viewport and scissor
		VkViewport viewport = vki::Viewport(0, 0, (float)targetCube.Width(), (float)targetCube.Height(), 0.0f, 1.0f);
		VkRect2D scissor = vki::Rect2D(0, 0, targetCube.Width(), targetCube.Height());
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		
		const auto targetCubeSubresRange = vki::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 
			0, targetCube.MipLevels(), 0, targetCube.LayerCount());
		const auto renderTargetSubresRange = vki::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		
		// Change image layout for all cubemap faces to transfer destination
		vkh::TransitionImageLayout(cmdBuf,
			targetCube.Image(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			targetCubeSubresRange);


		// Stop. Render time!
		for (u32 mip = 0; mip < targetCube.MipLevels(); mip++)
		{
			for (u32 face = 0; face < 6; face++)
			{
				viewport.width = f32(targetCube.Width() * std::pow(0.5f, mip));
				viewport.height = f32(targetCube.Height() * std::pow(0.5f, mip));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);


				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				{
					// Update shader push constant block
					PushConstants pushBlock{};
					pushBlock.Mvp = perspective * matrices[face];

					vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
						sizeof(PushConstants), &pushBlock);

					vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
					vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSet, 0, nullptr);

					VkDeviceSize offsets[1] = { 0 };
					vkCmdBindVertexBuffers(cmdBuf, 0, 1, &in.SkyboxMesh.VertexBuffer, offsets);
					vkCmdBindIndexBuffer(cmdBuf, in.SkyboxMesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(cmdBuf, (u32)in.SkyboxMesh.IndexCount, 1, 0, 0, 0);
				}
				vkCmdEndRenderPass(cmdBuf);


				// Prep to copy from render target to cubemap
				vkh::TransitionImageLayout(cmdBuf,
					renderTarget.Image,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, renderTargetSubresRange);

				
				// Copy from renderTarget to cube face
				{
					VkImageCopy copyRegion = {};
					copyRegion.srcSubresource = vki::ImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1);
					copyRegion.srcOffset = vki::Offset3D(0, 0, 0);
					copyRegion.dstSubresource = vki::ImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, mip, face, 1);
					copyRegion.dstOffset = vki::Offset3D(0, 0, 0);
					copyRegion.extent = vki::Extent3D(u32(viewport.width), u32(viewport.height), 1);

					vkCmdCopyImage(cmdBuf,
						renderTarget.Image,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						targetCube.Image(),
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&copyRegion);
				}


				// Transform framebuffer color attachment back 
				vkh::TransitionImageLayout(
					cmdBuf,
					renderTarget.Image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, renderTargetSubresRange);
			}
		}


		// Prep target cube layout for use in shaders <3
		vkh::TransitionImageLayout(
			cmdBuf,
			targetCube.Image(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			targetCubeSubresRange);

		vkh::EndSingeTimeCommands(cmdBuf, in.TransferPool, in.TransferQueue, in.Device);
	}
};

