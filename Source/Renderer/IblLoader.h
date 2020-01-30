#pragma once 


#include "VulkanHelpers.h"
#include "TextureResource.h"

#include <Shared/CommonTypes.h>

#include <vulkan/vulkan.h>
#include <stbi/stb_image.h>

#include <stdexcept>
#include <algorithm>
#include <cmath>

using vkh = VulkanHelpers;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct IblTextureResources
{
	TextureResource EnvironmentCubemap;
	TextureResource IrradianceCubemap;
	TextureResource PrefilterCubemap;
	TextureResource BrdfLut;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IblLoader
{
public:
	// RAII container for pixel data
	class Texels
	{
	public:
		u32 Width{}, Height{}, /*Channels{},*/ MipLevels{};
		std::vector<float> Data;

		explicit Texels(const std::string& path)
		{
			int texWidth, texHeight, texChannels;
			float* data = stbi_loadf(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
			if (!data)
			{
				stbi_image_free(data);
				throw std::runtime_error("Failed to load texture image: " + path);
			}

			const auto dataSize = (size_t)texWidth * (size_t)texHeight * 4; // We requested RGBA = 4bytes

			Data = std::vector<float>{ data, data + dataSize };
			Width = (u32)texWidth;
			Height = (u32)texHeight;
			//Channels = (u32)texChannels;
			MipLevels = (u32)std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
		}
		Texels() = delete;
		// No copy, no move.
		Texels(const Texels&) = delete;
		Texels(Texels&&) = delete;
		Texels& operator=(const Texels&) = delete;
		Texels& operator=(Texels&&) = delete;
		~Texels()
		{
			stbi_image_free(Data.data());
		}
	};

	// TODO Make private
	static TextureResource LoadCubemapFromPath(const std::string& path, const std::string& shaderDir,
		VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		VkImage srcImage;
		VkDeviceMemory srcMemory;
		VkImageView srcView;
		VkSampler srcSampler;
		//u32 srcMipLevels, srcWidth, srcHeight;
		VkImageLayout srcLayout;
		const auto srcTexels = Texels{ path };

		std::tie(srcImage, srcMemory) = CreateSrcImage(srcTexels, device, physicalDevice, transferPool, transferQueue);
		CreateSrcImageView(srcImage, device);
		CreateSrcSampler(device);

		CreateColorAttachment(device);
		CreateSubpassDependencies(device);
		//VkRenderPass renderPass = CreateRenderPass(device, physicalDevice, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32B32A32_SFLOAT);
		CreateOffscreenFramebuffer(device);

		CreateDescriptorPool(device);
		CreateDescriptorSetLayout(device);

		//CreatePipelineLayout(device);
		LoadShaders(shaderDir, device);
		//CreatePipeline(device);

		Render(device);

		OptimiseImageForRead(device);

		// Cleanup


		VkImage dstImage;
		VkDeviceMemory dstMemory;
		VkImageView dstView;
		u32 dstWidth, dstHeight;
		u32 dstMipLevels;
		u32 dstLayerCount;

		VkSampler dstSampler;
		VkImageLayout dstLayout;


		return TextureResource(device, dstWidth, dstHeight, dstMipLevels, dstLayerCount, dstImage, dstMemory, dstView, dstSampler, dstLayout);
	}

	static IblTextureResources LoadIblFromPath(const std::string& equirectangularHdrPath, const std::string& shaderDir, 
		VkCommandPool commandPool, VkQueue graphicsQueue, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		auto environmentCubemap = LoadCubemapFromPath(equirectangularHdrPath, shaderDir, commandPool, graphicsQueue, physicalDevice, device);
		auto irradianceCubemap = LoadCubemapFromPath(equirectangularHdrPath, shaderDir, commandPool, graphicsQueue, physicalDevice, device); //CreateIrradianceFromEnvCubemap();
		auto prefilterCubemap = LoadCubemapFromPath(equirectangularHdrPath, shaderDir, commandPool, graphicsQueue, physicalDevice, device); //CreatePrefilterFromEnvCubemap();
		auto brdfLut = LoadCubemapFromPath(equirectangularHdrPath, shaderDir, commandPool, graphicsQueue, physicalDevice, device); //CreateBrdfLutFromEnvCubemap();

		IblTextureResources iblRes
		{
			std::move(environmentCubemap),
			std::move(irradianceCubemap),
			std::move(prefilterCubemap),
			std::move(brdfLut),
		};
		return iblRes;
	}


private:

#pragma region LoadCubemapFromPath

	

	static std::tuple<VkImage, VkDeviceMemory> CreateSrcImage(const Texels& texels,
		VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool transferPool, VkQueue transferQueue)
	{
		// Create staging buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		std::tie(stagingBuffer, stagingBufferMemory) = vkh::CreateBuffer(
			texels.Data.size(),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage flags
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // property flags
			device, physicalDevice);


		// Copy texels from system mem to GPU staging buffer
		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, texels.Data.size(), 0, &data);
		memcpy(data, texels.Data.data(), texels.Data.size());
		vkUnmapMemory(device, stagingBufferMemory);


		// Create image buffer
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
		std::tie(textureImage, textureImageMemory) = vkh::CreateImage2D(
			texels.Width, texels.Height,
			texels.MipLevels,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R16G16B16A16_SFLOAT, // format
			VK_IMAGE_TILING_OPTIMAL, // tiling
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, //usageflags
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //propertyflags
			physicalDevice, device);


		// Transition image layout to optimal for copying to it from the staging buffer
		vkh::TransitionImageLayout(textureImage,
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_LAYOUT_UNDEFINED, // from
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
			texels.MipLevels,
			transferPool, transferQueue, device);

		// Copy texels from staging buffer to image buffer
		vkh::CopyBufferToImage(stagingBuffer, textureImage, texels.Width, texels.Height, transferPool, transferQueue, device);

		return { textureImage, textureImageMemory };
	}
	static VkImageView CreateSrcImageView(VkImage image, VkDevice device)
	{
		return vkh::CreateImageView(image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, device);
	}
	static void CreateSrcSampler(VkDevice device)
	{

	}

	static void CreateColorAttachment(VkDevice device) {}
	static void CreateSubpassDependencies(VkDevice device) {}
	//static VkRenderPass CreateRenderPass(VkDevice device, VkPhysicalDevice physicalDevice, VkSampleCountFlagBits msaaSamples, VkFormat format)
	//{
	//	// Color attachment
	//	VkAttachmentDescription colorAttachmentDesc = {};
	//	{
	//		colorAttachmentDesc.format = swapchainFormat;
	//		colorAttachmentDesc.samples = msaaSamples;
	//		colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // what to do with color/depth data before rendering
	//		colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // what to do with color/depth data after rendering
	//		colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // not using stencil
	//		colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//		colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // mem layout before renderpass
	//		colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // memory layout after renderpass
	//	}
	//	VkAttachmentReference colorAttachmentRef = {};
	//	{
	//		colorAttachmentRef.attachment = 0;
	//		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//	}


	//	// Depth attachment  -  multisample depth doesn't need to be resolved as it won't be displayed
	//	VkAttachmentDescription depthAttachmentDesc = {};
	//	{
	//		depthAttachmentDesc.format = FindDepthFormat(physicalDevice);
	//		depthAttachmentDesc.samples = msaaSamples;
	//		depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	//		depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // not used after drawing
	//		depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 
	//		depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//		depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//		depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	//	}
	//	VkAttachmentReference depthAttachmentRef = {};
	//	{
	//		depthAttachmentRef.attachment = 1;
	//		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	//	}


	//	// Color resolve attachment  -  used to resolve multisampled image into one that can be displayed
	//	VkAttachmentDescription colorAttachmentResolveDesc = {};
	//	{
	//		colorAttachmentResolveDesc.format = swapchainFormat;
	//		colorAttachmentResolveDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	//		colorAttachmentResolveDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	//		colorAttachmentResolveDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	//		colorAttachmentResolveDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	//		colorAttachmentResolveDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//		colorAttachmentResolveDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//		colorAttachmentResolveDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//	}
	//	VkAttachmentReference colorAttachmentResolveRef = {};
	//	{
	//		colorAttachmentResolveRef.attachment = 2;
	//		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//	}


	//	// Associate color and depth attachements with a subpass
	//	VkSubpassDescription subpassDesc = {};
	//	{
	//		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	//		subpassDesc.colorAttachmentCount = 1;
	//		subpassDesc.pColorAttachments = &colorAttachmentRef;
	//		subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;
	//		subpassDesc.pResolveAttachments = &colorAttachmentResolveRef;
	//	}


	//	// Set subpass dependency for the implicit external subpass to wait for the swapchain to finish reading from it
	//	VkSubpassDependency subpassDependency = {};
	//	{
	//		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL; // implicit subpass before render
	//		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	//		subpassDependency.srcAccessMask = 0;
	//		subpassDependency.dstSubpass = 0; // this pass
	//		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	//		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	//	}


	//	// Create render pass
	//	std::array<VkAttachmentDescription, 3> attachments = {
	//		colorAttachmentDesc,
	//		depthAttachmentDesc,
	//		colorAttachmentResolveDesc
	//	};

	//	VkRenderPassCreateInfo renderPassCI = {};
	//	{
	//		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	//		renderPassCI.attachmentCount = (uint32_t)attachments.size();
	//		renderPassCI.pAttachments = attachments.data();
	//		renderPassCI.subpassCount = 1;
	//		renderPassCI.pSubpasses = &subpassDesc;
	//		renderPassCI.dependencyCount = 1;
	//		renderPassCI.pDependencies = &subpassDependency;
	//	}

	//	VkRenderPass renderPass;
	//	if (vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass) != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed to create render pass");
	//	}

	//	return renderPass;
	//}
	static void CreateOffscreenFramebuffer(VkDevice device) {}

	static void CreateDescriptorSetLayout(VkDevice device) {}
	static void CreateDescriptorPool(VkDevice device) {}

	static void LoadShaders(const std::string& cs, VkDevice device) {}

	//static VkPipelineLayout CreatePipelineLayout(VkDevice device)
	//{
	//	VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
	//	{
	//		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	//		pipelineLayoutCI.setLayoutCount = 1;
	//		pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;
	//		pipelineLayoutCI.pushConstantRangeCount = 0;
	//		pipelineLayoutCI.pPushConstantRanges = nullptr;
	//	}

	//	VkPipelineLayout pipelineLayout = nullptr;
	//	if (vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout) != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed to Create Pipeline Layout!");
	//	}
	//}
	//static VkPipeline CreatePipeline(VkDevice device)
	//{
	//	// Create the Pipeline  -  Finally!...
	//	VkGraphicsPipelineCreateInfo graphicsPipelineCI = {};
	//	{
	//		graphicsPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	//		// Programmable
	//		graphicsPipelineCI.stageCount = (uint32_t)shaderStageCIs.size();
	//		graphicsPipelineCI.pStages = shaderStageCIs.data();

	//		// Fixed function
	//		graphicsPipelineCI.pVertexInputState = &vertexInputCI;
	//		graphicsPipelineCI.pInputAssemblyState = &inputAssemblyCI;
	//		graphicsPipelineCI.pViewportState = &viewportCI;
	//		graphicsPipelineCI.pRasterizationState = &rasterizationCI;
	//		graphicsPipelineCI.pMultisampleState = &multisampleCI;
	//		graphicsPipelineCI.pDepthStencilState = &depthStencilCI;
	//		graphicsPipelineCI.pColorBlendState = &colorBlendCI;
	//		graphicsPipelineCI.pDynamicState = nullptr;

	//		graphicsPipelineCI.layout = pipelineLayout;

	//		graphicsPipelineCI.renderPass = renderPass;
	//		graphicsPipelineCI.subpass = 0;

	//		graphicsPipelineCI.basePipelineHandle = VK_NULL_HANDLE; // is our pipeline derived from another?
	//		graphicsPipelineCI.basePipelineIndex = -1;
	//	}
	//	VkPipeline pipeline;
	//	if (vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCI, nullptr, &pipeline) != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed to create Pipeline");
	//	}
	//}
	static void Render(VkDevice device) {}

	static void OptimiseImageForRead(VkDevice device) {}

	static void CleanUp(VkDevice device) {}
	
	#pragma endregion


	static TextureResource CreateIrradianceFromEnvCubemap() { throw; }

	static TextureResource CreatePrefilterFromEnvCubemap() { throw; }

	static TextureResource CreateBrdfLutFromEnvCubemap() { throw; }

};

