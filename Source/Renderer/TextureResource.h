#pragma once

#include "VulkanHelpers.h"

#include <Shared/CommonTypes.h>

#include <vulkan/vulkan.h>

#include <cassert>
#include <stbi/stb_image.h>

using vkh = VulkanHelpers;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TextureResource final
{
public:
	// Lifetime
	TextureResource() = delete;
	TextureResource(VkDevice device, u32 width, u32 height, u32 mipLevels, VkImage image, VkDeviceMemory memory, 
		VkImageView view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		assert(device);
		assert(image);
		assert(memory);
		assert(view);
		assert(sampler);

		_device = device;
		_width = width;
		_height = height;
		_mipLevels = mipLevels;
		_image = image;
		_memory = memory;
		_descriptorImageInfo.imageView = view;
		_descriptorImageInfo.sampler = sampler;
		_descriptorImageInfo.imageLayout = layout;
	}
	// No copy
	TextureResource(const TextureResource&) = delete;
	TextureResource& operator=(const TextureResource&) = delete;
	// Move
	TextureResource(TextureResource&& other) noexcept
	{
		if (this != &other)
		{
			_width = other._width;
			_height = other._height;
			_mipLevels = other._mipLevels;
			
			_device       = other._device;
			_image        = other._image;
			_memory       = other._memory;
			_descriptorImageInfo = other._descriptorImageInfo;
			other._device = nullptr;
			other._image = nullptr;
			other._memory = nullptr;
			other._descriptorImageInfo = VkDescriptorImageInfo{};
		}
	}
	TextureResource& operator=(TextureResource&& other) noexcept
	{
		if (this != &other)
		{
			_width = other._width;
			_height = other._height;
			_mipLevels = other._mipLevels;

			_device = other._device;
			_image = other._image;
			_memory = other._memory;
			_descriptorImageInfo = other._descriptorImageInfo;
			other._device = nullptr;
			other._image = nullptr;
			other._memory = nullptr;
			other._descriptorImageInfo = VkDescriptorImageInfo{};
		}
		return *this;
	}
	~TextureResource()
	{
		if (_device)
		{
			vkDestroySampler(_device, _descriptorImageInfo.sampler, nullptr);
			vkDestroyImageView(_device, _descriptorImageInfo.imageView, nullptr);
			vkDestroyImage(_device, _image, nullptr);
			vkFreeMemory(_device, _memory, nullptr);
			_device = nullptr;
		}
	}

	// Getters
	u32 Width() const { return _width; }
	u32 Height() const { return _height; }
	u32 MipLevels() const { return _mipLevels; }
	const VkImage& Image() const { return _image; }
	const VkDeviceMemory& Memory() const { return _memory; }
	const VkDescriptorImageInfo& DescriptorImageInfo() const { return _descriptorImageInfo; }
	
private:
	VkDevice _device;

	u32 _width{};
	u32 _height{};
	u32 _mipLevels{};
	//u32 _layerCount{};
	VkImage _image{};
	VkDeviceMemory _memory{};
	VkDescriptorImageInfo _descriptorImageInfo{};
};





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class EquirectangularTextureResourceLoader
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



	static TextureResource LoadFromPath(const std::string& path, const std::string& shaderDir, 
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
		CreateSrcImageView();
		CreateSrcSampler();

		CreateColorAttachment();
		CreateSubpassDependencies();
		//VkRenderPass renderPass = CreateRenderPass(device, physicalDevice, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32B32A32_SFLOAT);
		CreateOffscreenFramebuffer();

		CreateDescriptorSetLayout();
		CreateDescriptorPool();
		
		LoadShaders(shaderDir);
		//CreatePipelineLayout();
		//CreatePipeline();

		Render();

		OptimiseImageForRead();

		// Cleanup


		VkImage dstImage;
		VkDeviceMemory dstMemory;
		VkImageView dstView;
		u32 dstMipLevels, dstWidth, dstHeight;
		VkSampler dstSampler;
		VkImageLayout dstLayout;

		return TextureResource(device, dstWidth, dstHeight, dstMipLevels, dstImage, dstMemory, dstView, dstSampler, dstLayout);
	}

private:
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
		std::tie(textureImage, textureImageMemory) = vkh::CreateImage(
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

		return { textureImage, textureImageMemory };
	}
	static void CreateSrcImageView(){}
	static void CreateSrcSampler(){}

	static void CreateColorAttachment(){}
	static void CreateSubpassDependencies() {};;
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
	static void CreateOffscreenFramebuffer(){}

	static void CreateDescriptorSetLayout(){}
	static void CreateDescriptorPool(){}

	static void LoadShaders(const std::string& cs){}

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
	static void Render(){}

	static void OptimiseImageForRead(){}

	static void CleanUp(){}
};














///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TextureResourceHelpers
{
public:
	static TextureResource LoadTexture(const std::string& path, VkCommandPool transferPool, VkQueue transferQueue, 
		VkPhysicalDevice physicalDevice, VkDevice device)
	{
		// TODO Pull the texture library out of the CreateTextureImage, just work on an TextureDefinition struct that
		// has an array of pixels and width, height, channels, etc
		
		VkImage image;
		VkDeviceMemory memory;
		u32 mipLevels, width, height;

		std::tie(image, memory, mipLevels, width, height)
			= CreateTextureImage(path, transferPool, transferQueue, physicalDevice, device);
		const auto view = vkh::CreateImageView(image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, device);
		const auto sampler = CreateTextureSampler(mipLevels, device);
		const auto layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		return TextureResource(device, width, height, mipLevels, image, memory, view, sampler, layout);
	}

private:

	// Preconditions: image layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	// Postconditions: image layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 
	static void GenerateMipmaps(VkImage image, VkFormat format, uint32_t texWidth, uint32_t texHeight,
	                            uint32_t mipLevels, VkCommandPool transferPool, VkQueue transferQueue,
	                            VkDevice device, VkPhysicalDevice physicalDevice)
	{
		// Check if device supports linear blitting
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
		{
			throw std::runtime_error("Texture image format does not support linear blitting!");
		}


		auto commandBuffer = vkh::BeginSingleTimeCommands(transferPool, device);

		VkImageMemoryBarrier barrier = {};
		{
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0; // Defined later
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
		}

		auto srcMipWidth = (int32_t)texWidth;
		auto srcMipHeight = (int32_t)texHeight;

		for (uint32_t i = 1; i < mipLevels; i++)
		{
			const uint32_t srcMipLevel = i - 1;
			const uint32_t dstMipLevel = i;
			const int32_t dstMipWidth = srcMipWidth > 1 ? srcMipWidth / 2 : 1;
			const int32_t dstMipHeight = srcMipHeight > 1 ? srcMipHeight / 2 : 1;


			// Transition layout of src mip to TRANSFER_SRC_OPTIMAL (Note: dst mip is already TRANSFER_DST_OPTIMAL)
			barrier.subresourceRange.baseMipLevel = srcMipLevel;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr, // mem barriers
				0, nullptr, // buffer barriers
				1, &barrier); // image barriers


	// Blit the smaller image to the dst 
			VkImageBlit blit = {};
			{
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { srcMipWidth, srcMipHeight, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = srcMipLevel;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;

				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { dstMipWidth, dstMipHeight, 1 };
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = dstMipLevel;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;
			}
			vkCmdBlitImage(commandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);


			// Transition layout of the src mip to optimal shader readible (we don't need to read it again)
			barrier.subresourceRange.baseMipLevel = srcMipLevel;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr, // mem barriers
				0, nullptr, // buffer barriers
				1, &barrier); // image barriers


	// Halve mip dimensions in prep for next loop iteration 
			if (srcMipWidth > 1) srcMipWidth /= 2;
			if (srcMipHeight > 1) srcMipHeight /= 2;
		}


		// Transition the final mip to be optimal for reading by shader (wasn't processed in the loop)
		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // still dst from precondition
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr, // mem barriers
			0, nullptr, // buffer barriers
			1, &barrier); // image barriers

		vkh::EndSingeTimeCommands(commandBuffer, transferPool, transferQueue, device);
	}

	static std::tuple<VkImage, VkDeviceMemory, uint32_t, uint32_t, uint32_t> CreateTextureImage(
		const std::string& path, VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice,
		VkDevice device)
	{
		// Load texture from file into system mem
		int texWidth, texHeight, texChannels;
		unsigned char* texels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!texels)
		{
			stbi_image_free(texels);
			throw std::runtime_error("Failed to load texture image: " + path);
		}

		const VkDeviceSize imageSize = (uint64_t)texWidth * (uint64_t)texHeight * 4; // RGBA = 4bytes
		const uint32_t mipLevels = (uint32_t)std::floor(std::log2(std::max(texWidth, texHeight))) + 1;

		// Create staging buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		std::tie(stagingBuffer, stagingBufferMemory) = vkh::CreateBuffer(
			imageSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage flags
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // property flags
			device, physicalDevice);


		// Copy texels from system mem to GPU staging buffer
		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, texels, imageSize);
		vkUnmapMemory(device, stagingBufferMemory);


		// Free loaded image from system mem
		stbi_image_free(texels);


		// Create image buffer
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
		std::tie(textureImage, textureImageMemory) = vkh::CreateImage(texWidth, texHeight,
			mipLevels,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R8G8B8A8_UNORM, // format
			VK_IMAGE_TILING_OPTIMAL, // tiling
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT, //usageflags
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //propertyflags
			physicalDevice, device);


		// Transition image layout to optimal for copying to it from the staging buffer
		vkh::TransitionImageLayout(textureImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_UNDEFINED, // from
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
			mipLevels,
			transferPool, transferQueue, device);


		// Copy texels from staging buffer to image buffer
		vkh::CopyBufferToImage(stagingBuffer, textureImage, texWidth, texHeight, transferPool, transferQueue, device);


		GenerateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_UNORM, texWidth, texHeight, mipLevels,
			transferPool, transferQueue, device, physicalDevice);


		// Destroy the staging buffer
		vkFreeMemory(device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);

		return { textureImage, textureImageMemory, mipLevels, texWidth, texHeight };
	}

	static VkSampler CreateTextureSampler(uint32_t mipLevels, VkDevice device)
	{
		VkSamplerCreateInfo samplerCI = {};
		{
			samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCI.magFilter = VK_FILTER_LINEAR;
			samplerCI.minFilter = VK_FILTER_LINEAR;
			samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCI.anisotropyEnable = VK_TRUE;
			samplerCI.maxAnisotropy = 16;
			samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // applied with addressMode is clamp
			samplerCI.unnormalizedCoordinates = VK_FALSE; // false addresses tex coord via [0,1), true = [0,dimensionSize]
			samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCI.mipLodBias = 0;
			samplerCI.minLod = 0;
			samplerCI.maxLod = (float)mipLevels;
		}

		VkSampler textureSampler;
		if (VK_SUCCESS != vkCreateSampler(device, &samplerCI, nullptr, &textureSampler))
		{
			throw std::runtime_error("Failed to create texture sampler");
		}

		return textureSampler;
	}
};