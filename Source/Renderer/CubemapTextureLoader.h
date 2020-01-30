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
// RAII container for pixel data
class TexelsRgbaF16
{
public:
	u32 Width{}, Height{}, /*Channels{},*/ MipLevels{};
	std::vector<float> Data;

	void Load(const std::string& path)
	{
		int texWidth, texHeight, texChannels;
		float* pData = stbi_loadf(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pData)
		{
			stbi_image_free(pData);
			throw std::runtime_error("Failed to load texture image: " + path);
		}

		const auto dataSize = (size_t)texWidth * (size_t)texHeight * 4; // We requested RGBA = 4bytes

		Data = std::vector<float>{ pData, pData + dataSize };
		stbi_image_free(pData);

		Width = (u32)texWidth;
		Height = (u32)texHeight;
		//Channels = (u32)texChannels;
		MipLevels = (u32)std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
	}
	//TexelsRgbaF16() = default;
	//// No copy, no move.
	//TexelsRgbaF16(const TexelsRgbaF16&) = delete;
	//TexelsRgbaF16(TexelsRgbaF16&&) = delete;
	//TexelsRgbaF16& operator=(const TexelsRgbaF16&) = delete;
	//TexelsRgbaF16& operator=(TexelsRgbaF16&&) = delete;
};



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RAII container for pixel data
class TexelsRgbaU8
{
public:
	u32 Width{}, Height{}, /*Channels{},*/ MipLevels{};
	std::vector<u8> Data;

	void Load(const std::string& path)
	{
		int texWidth, texHeight, texChannels;
		u8* pData = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pData)
		{
			stbi_image_free(pData);
			throw std::runtime_error("Failed to load texture image: " + path);
		}

		const auto dataSize = (size_t)texWidth * (size_t)texHeight * 4; // We requested RGBA = 4bytes

		Data = std::vector<u8>{ pData, pData + dataSize };
		stbi_image_free(pData);

		Width = (u32)texWidth;
		Height = (u32)texHeight;
		//Channels = (u32)texChannels;
		MipLevels = (u32)std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
	}
	//TexelsRgbaU8() = default;
	//// No copy, no move.
	//TexelsRgbaU8(const TexelsRgbaU8&) = delete;
	//TexelsRgbaU8(TexelsRgbaU8&&) = delete;
	//TexelsRgbaU8& operator=(const TexelsRgbaU8&) = delete;
	//TexelsRgbaU8& operator=(TexelsRgbaU8&&) = delete;
};



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CubemapTextureLoader
{
public:
	static TextureResource LoadFromPath(const std::array<std::string, 6>& sidePaths, const std::string& shaderDir,
		VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice, VkDevice device)
	{

		VkImage image;
		VkDeviceMemory memory;
		const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		std::array<TexelsRgbaU8, 6> texels{};
		for (size_t i = 0; i < 6; i++)
		{
			texels[i].Load(sidePaths[i]);
			texels[i].MipLevels = 1; // override mips as we aren't using em for the cubemap
		}

		std::tie(image, memory) = CreateImage(texels, layout, device, physicalDevice, transferPool, transferQueue);
		const auto view = CreateImageView(device, VK_FORMAT_R8G8B8A8_UNORM, image);
		const auto sampler = CreateSampler(device);

		
		return TextureResource(device, texels[0].Width, texels[0].Height, texels[0].MipLevels, 6, 
			image, memory, view, sampler, layout);
	}

private:
	static std::tuple<VkImage, VkDeviceMemory> CreateImage(const std::array<TexelsRgbaU8, 6>& texels, 
		VkImageLayout targetLayout, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool transferPool, 
		VkQueue transferQueue)
	{
		const auto format = VK_FORMAT_R8G8B8A8_UNORM;
		const u32 mipLevels = 1;
		const u32 cubeSides = 6;
		const auto faceTexelDataSize = texels[0].Data.size();
		const auto faceTexelWidth = texels[0].Width;
		const auto faceTexelHeight = texels[0].Height;

		// Create staging buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		std::tie(stagingBuffer, stagingBufferMemory) = vkh::CreateBuffer(
			faceTexelDataSize * cubeSides,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage flags
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // property flags
			device, physicalDevice);


		// Copy texels from system mem to GPU staging buffer
		for (size_t i = 0; i < texels.size(); i++)
		{
			const VkDeviceSize offset = i * faceTexelDataSize;
			void* data;
			
			vkMapMemory(device, stagingBufferMemory, offset, faceTexelDataSize, 0, &data);
			memcpy(data, texels[i].Data.data(), faceTexelDataSize);
			vkUnmapMemory(device, stagingBufferMemory);
		}


		// Create image buffer
		VkImage cubemapTextureImage;
		VkDeviceMemory cubemapTextureImageMemory;
		std::tie(cubemapTextureImage, cubemapTextureImageMemory) = vkh::CreateImage2D(
			faceTexelWidth, faceTexelHeight,
			mipLevels, // no mips //texels.MipLevels,
			VK_SAMPLE_COUNT_1_BIT,
			format, // format
			VK_IMAGE_TILING_OPTIMAL, // tiling
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, //usage flags
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //memory flags
			physicalDevice, device,
			cubeSides,// array layers for cubemap
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT); // flags


		// Transition image layout to optimal for copying to it from the staging buffer
		const auto cmdBuffer = vkh::BeginSingleTimeCommands(transferPool, device);
		VkImageSubresourceRange subresourceRange = {};
		{
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = mipLevels;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = cubeSides;
		}
		vkh::TransitionImageLayout(cmdBuffer, cubemapTextureImage,
			VK_IMAGE_LAYOUT_UNDEFINED, // from
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
			subresourceRange);
		

		// Copy texels from staging buffer to image buffer		
		// Setup buffer copy regions for each face including all of it's miplevels
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		for (u32 face = 0; face < 6; face++)
		{
			const VkDeviceSize offset = face * faceTexelDataSize;
			//for (u32 level = 0; level < cubeMap.mipLevels; level++)
			{
				// Calculate offset into staging buffer for the current mip level and face
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = 0;
				bufferCopyRegion.imageSubresource.baseArrayLayer = face;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = faceTexelWidth;
				bufferCopyRegion.imageExtent.height = faceTexelHeight;
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;
				bufferCopyRegions.push_back(bufferCopyRegion);
			}
		}

		// Copy the cube map faces from the staging buffer to the optimal tiled image
		vkCmdCopyBufferToImage(
			cmdBuffer,
			stagingBuffer,
			cubemapTextureImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			u32(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);

		
		// Change texture image layout to shader read after all faces have been copied
		vkh::TransitionImageLayout(
			cmdBuffer,
			cubemapTextureImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			targetLayout,
			subresourceRange);


		// Execute commands
		vkh::EndSingeTimeCommands(cmdBuffer, transferPool, transferQueue, device);


		// Cleanup
		vkFreeMemory(device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		
		return { cubemapTextureImage, cubemapTextureImageMemory };
	}
	static VkImageView CreateImageView(VkDevice device, VkFormat format, VkImage image)
	{
		VkImageView imageView;

		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		createInfo.format = format;
		createInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 6;  // cube sides

		if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image views");
		}

		return imageView;
	}
	static VkSampler CreateSampler(VkDevice device)
	{
		VkSamplerCreateInfo samplerCI = {};
		{
			samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCI.magFilter = VK_FILTER_LINEAR;
			samplerCI.minFilter = VK_FILTER_LINEAR;
			samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		
			samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // applied with addressMode is clamp
			samplerCI.unnormalizedCoordinates = VK_FALSE; // false addresses tex coord via [0,1), true = [0,dimensionSize]
			samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCI.mipLodBias = 0;
			samplerCI.minLod = 0;
			samplerCI.maxLod = (float)1;

			samplerCI.anisotropyEnable = VK_TRUE; // TODO Query device to find max if anisotrophy is supported
			samplerCI.maxAnisotropy = 16; // TODO Query device to find max anisotrophy
			/*if (vulkanDevice->features.samplerAnisotropy)
			{
				sampler.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
				sampler.anisotropyEnable = VK_TRUE;
			}*/
		}

		VkSampler textureSampler;
		if (VK_SUCCESS != vkCreateSampler(device, &samplerCI, nullptr, &textureSampler))
		{
			throw std::runtime_error("Failed to create cubemap sampler");
		}

		return textureSampler;
	}
};

