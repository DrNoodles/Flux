#pragma once

#include "VulkanHelpers.h"

#include <Framework/CommonTypes.h>

#include <stbi/stb_image.h>
#include <vulkan/vulkan.h>

using vkh = VulkanHelpers;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TextureResource final
{
public:
	// Lifetime
	TextureResource() = delete;

	TextureResource(VkDevice device, u32 width, u32 height, u32 mipLevels, u32 layerCount, VkImage image, 
	                VkDeviceMemory memory, VkImageView view, VkSampler sampler, VkFormat format,
	                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		// TODO reenable asserts!
		/*assert(device);
		assert(image);
		assert(memory);
		assert(view);
		assert(sampler);*/

		_device = device;
		_width = width;
		_height = height;
		_mipLevels = mipLevels;
		_layerCount = layerCount;
		_format = format;
		
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
			_layerCount = other._layerCount;
			_format = other._format;
			
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
			_layerCount = other._layerCount;
			_format = other._format;

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
	inline u32 Width() const { return _width; }
	inline u32 Height() const { return _height; }
	inline u32 MipLevels() const { return _mipLevels; }
	inline u32 LayerCount() const { return _layerCount; }
	inline const VkImage& Image() const { return _image; }
	inline const VkDeviceMemory& Memory() const { return _memory; }
	inline const VkDescriptorImageInfo& DescriptorImageInfo() const { return _descriptorImageInfo; }
	
private:
	VkDevice _device;

	u32 _width{};
	u32 _height{};
	u32 _mipLevels{};
	u32 _layerCount{};
	VkFormat _format{};
	VkImage _image{};
	VkDeviceMemory _memory{};
	VkDescriptorImageInfo _descriptorImageInfo{};
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

		u32 layerCount = 1;

		VkImage image;
		VkDeviceMemory memory;
		u32 mipLevels, width, height;
		const auto format = VK_FORMAT_R8G8B8A8_UNORM;

		std::tie(image, memory, mipLevels, width, height)
			= CreateTextureImage(path, transferPool, transferQueue, physicalDevice, device);
		const auto view = vkh::CreateImage2DView(image, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, layerCount, device);
		const auto sampler = CreateTextureSampler(mipLevels, device);
		const auto layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		return TextureResource(device, width, height, mipLevels, layerCount, image, memory, view, sampler, format, layout);
	}

private:

	// TODO Move this to VulkanHelpers and add support for n layers, and pass in a command buffer for external management
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
		std::tie(textureImage, textureImageMemory) = vkh::CreateImage2D(texWidth, texHeight,
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