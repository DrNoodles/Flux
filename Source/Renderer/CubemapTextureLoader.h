#pragma once 

#include "VulkanHelpers.h"
#include "TextureResource.h"

#include <Shared/CommonTypes.h>

#include <vulkan/vulkan.h>
#include <stbi/stb_image.h>

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <iostream>

using vkh = VulkanHelpers;



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Container for pixel data
class Texels
{
public:
	virtual ~Texels() = default;

	inline u32 Width() const { return _width; }
	inline u32 Height() const { return _height; }
	inline u8 Channels() const { return _channels; }
	//inline u8 BytesPerPixel() const { return _bytesPerPixel; }
	//inline u8 BytesPerChannel() const { return _bytesPerChannel; }
	inline size_t DataSize() const { return _dataSize; }
	inline const std::vector<u8>& Data() const { return _data; }

	void Load(const std::string& path)
	{
		_data = LoadType(path, _dataSize, _width, _height, _channels/*, _bytesPerChannel*/);
		
		//_mipLevels = (u8)std::floor(std::log2(std::max(_width, _height))) + 1;
		//_bytesPerPixel = _bytesPerChannel * _channels;
	}

	inline static u8 CalcMipLevels(u32 width, u32 height) { return (u8)std::floor(std::log2(std::max(width, height))) + 1; }

protected:
	virtual std::vector<u8> LoadType(const std::string& path,
		size_t& outDataSize,
		u32& outWidth,
		u32& outHeight,
		u8& outChannels/*,
		u8& outBytesPerChannel*/) = 0;

private:
	// Data
	u32 _width{};
	u32 _height{};
	u8 _channels{};
	//u8 _bytesPerPixel = {};
	//u8 _bytesPerChannel = {};
	
	std::vector<u8> _data = {};
	size_t _dataSize = {}; // The total size of the buffer
	
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TexelsRgbaF32 final : public Texels
{
protected:
	std::vector<u8> LoadType(const std::string& path,
		size_t& outDataSize,
		u32& outWidth,
		u32& outHeight,
		u8& outChannels/*,
		u8& outBytesPerChannel*/) override
	{
		int width, height, channelsInImage;
		const auto desiredChannels = (u8)STBI_rgb_alpha;

		const auto bytesPerChannel = sizeof(f32);
		f32* pData = stbi_loadf(path.c_str(), &width, &height, &channelsInImage, desiredChannels);

		if (!pData)
		{
			stbi_image_free(pData);
			throw std::runtime_error("Failed to load texture image: " + path);
		}

		outWidth = (u32)width;
		outHeight = (u32)height;
		outChannels = (u8)desiredChannels;
		outDataSize = (size_t)width * (size_t)height * outChannels * bytesPerChannel;
		
		auto data = std::vector<u8>{ (u8*)pData, (u8*)pData + outDataSize };

		stbi_image_free(pData);

		return data;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TexelsRgbaU8 final : public Texels
{
protected:
	std::vector<u8> LoadType(const std::string& path,
		size_t& outDataSize,
		u32& outWidth,
		u32& outHeight,
		u8& outChannels/*,
		u8& outBytesPerChannel*/) override
	{
		int width, height, channelsInImage;
		const auto desiredChannels = (u8)STBI_rgb_alpha;

		const auto bytesPerChannel = sizeof(u8);
		u8* pData = stbi_load(path.c_str(), &width, &height, &channelsInImage, desiredChannels);

		if (!pData)
		{
			stbi_image_free(pData);
			throw std::runtime_error("Failed to load texture image: " + path);
		}

		outWidth = (u32)width;
		outHeight = (u32)height;
		outChannels = (u8)desiredChannels;
		outDataSize = (size_t)width * (size_t)height * outChannels * bytesPerChannel;

		auto data = std::vector<u8>{ (u8*)pData, (u8*)pData + outDataSize };

		stbi_image_free(pData);

		return data;
	}
};

enum class CubemapFormat : u8
{
	RGBA_F32,
	RGBA_U8,
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CubemapTextureLoader
{
	
public:
	// Each map corresponds to the following cube faces +X, -X, +Y, -Y, +Z, -Z.
	static TextureResource LoadFromPath(const std::array<std::string, 6>& sidePaths, CubemapFormat sourceFormat, 
		const std::string& shaderDir, VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice, 
		VkDevice device)
	{
		VkFormat texelFormat;
		std::array<std::unique_ptr<Texels>, 6> texels{};
		
		switch (sourceFormat)
		{
		case CubemapFormat::RGBA_F32:
			texelFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			for (size_t i = 0; i < 6; i++)
			{
				auto t = std::make_unique<TexelsRgbaF32>();
				t->Load(sidePaths[i]);
				texels[i] = std::move(t); 
			}
			break;
		case CubemapFormat::RGBA_U8:
			texelFormat = VK_FORMAT_R8G8B8A8_UNORM;
			for (size_t i = 0; i < 6; i++)
			{
				auto t = std::make_unique<TexelsRgbaU8>();
				t->Load(sidePaths[i]);
				texels[i] = std::move(t);
			}
			break;
		default:
			throw std::invalid_argument("Unsupported cubemap foramt");
		}

		
		const auto finalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
		const VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


		VkImage image;
		VkDeviceMemory memory;
		
		std::tie(image, memory) = CreateImage(texels, texelFormat, finalFormat, finalLayout, 
			device, physicalDevice, transferPool, transferQueue);
		const auto view = CreateImageView(device, finalFormat, image);
		const auto sampler = CreateSampler(device);

		return TextureResource(device, texels[0]->Width(), texels[0]->Height(), 1/*miplevels*/, 6, 
			image, memory, view, sampler, finalLayout);
	}

private:
	static std::tuple<VkImage, VkDeviceMemory> CreateImage(
		const std::array<std::unique_ptr<Texels>, 6>& texels,
		VkFormat texelFormat,
		VkFormat desiredFormat,
		VkImageLayout targetLayout, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool transferPool, 
		VkQueue transferQueue)
	{
		const u32 mipLevels = 1;
		const u32 cubeSides = 6;
		const auto faceTexelDataSize = texels[0]->DataSize();
		const auto faceTexelWidth = texels[0]->Width();
		const auto faceTexelHeight = texels[0]->Height();

		
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
			memcpy(data, texels[i]->Data().data(), faceTexelDataSize);
			vkUnmapMemory(device, stagingBufferMemory);
		}

		const bool needsFormatConversion = texelFormat != desiredFormat;

		// Create image buffer
		VkImage cubemapTextureImage;
		VkDeviceMemory cubemapTextureImageMemory;
		{
			const auto usageFlags = needsFormatConversion
				? VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT // copy to, then from
				: VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // copy to, then use in descSet
			
			std::tie(cubemapTextureImage, cubemapTextureImageMemory) = vkh::CreateImage2D(
				faceTexelWidth, faceTexelHeight,
				mipLevels,
				VK_SAMPLE_COUNT_1_BIT,
				texelFormat, // format
				VK_IMAGE_TILING_OPTIMAL, // tiling
				usageFlags, //usage flags
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //memory flags
				physicalDevice, device,
				cubeSides,// array layers for cubemap
				VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT); // flags
		}

		const auto cmdBuffer = vkh::BeginSingleTimeCommands(transferPool, device);

		VkImageSubresourceRange subresourceRange = {};
		{
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = mipLevels;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = cubeSides;
		}
		
		// Transition image layout to optimal for copying to it from the staging buffer
		{
			vkh::TransitionImageLayout(cmdBuffer, cubemapTextureImage,
				VK_IMAGE_LAYOUT_UNDEFINED, // from
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
				subresourceRange);
		}

		
		// Copy texels from staging buffer to image buffer		
		// Setup buffer copy regions for each face including all of it's miplevels
		{
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
		}
		

		
		// If we need to convert the format, we'll need another image buffer
		
		if (needsFormatConversion)
		{
			// Create another image buffer so we can convert to the desired format
			VkImage newCubemapImage;
			VkDeviceMemory newCubemapImageMemory;
			{
				std::tie(newCubemapImage, newCubemapImageMemory) = vkh::CreateImage2D(
					faceTexelWidth, faceTexelHeight,
					mipLevels,
					VK_SAMPLE_COUNT_1_BIT,
					desiredFormat, // format
					VK_IMAGE_TILING_OPTIMAL, // tiling
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, //usage flags
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //memory flags
					physicalDevice, device,
					cubeSides,// array layers for cubemap
					VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT); // flags
			}

			{
				vkh::TransitionImageLayout(cmdBuffer, cubemapTextureImage,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // from
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // to
					subresourceRange);
			}
			{
				vkh::TransitionImageLayout(cmdBuffer, newCubemapImage,
					VK_IMAGE_LAYOUT_UNDEFINED, // from
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
					subresourceRange);
			}
			vkh::ChangeFormat(cmdBuffer, cubemapTextureImage, newCubemapImage, faceTexelWidth, faceTexelHeight, subresourceRange);

			
			// Change texture image layout to shader read after all faces have been copied
			vkh::TransitionImageLayout(
				cmdBuffer,
				newCubemapImage,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				targetLayout,
				subresourceRange);


			// Execute commands
			vkh::EndSingeTimeCommands(cmdBuffer, transferPool, transferQueue, device);


			// Cleanup unneeded resources - now buffer has executed!
			vkDestroyImage(device, cubemapTextureImage, nullptr);
			vkFreeMemory(device, cubemapTextureImageMemory, nullptr);
			vkFreeMemory(device, stagingBufferMemory, nullptr);
			vkDestroyBuffer(device, stagingBuffer, nullptr);

			return { newCubemapImage, newCubemapImageMemory };
		}
		else
		{
			// Change texture image layout to shader read after all faces have been copied
			vkh::TransitionImageLayout(
				cmdBuffer,
				cubemapTextureImage,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				targetLayout,
				subresourceRange);


			// Execute commands
			vkh::EndSingeTimeCommands(cmdBuffer, transferPool, transferQueue, device);


			// Cleanup unneeded resources - now buffer has executed!
			vkFreeMemory(device, stagingBufferMemory, nullptr);
			vkDestroyBuffer(device, stagingBuffer, nullptr);
			
			return { cubemapTextureImage, cubemapTextureImageMemory };
		}
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

