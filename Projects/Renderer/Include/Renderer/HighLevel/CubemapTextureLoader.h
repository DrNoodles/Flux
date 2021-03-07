#pragma once 

#include "Renderer/LowLevel/VulkanHelpers.h"
#include "Renderer/LowLevel/VulkanInitializers.h"
#include "Renderer/LowLevel/TextureResource.h"
#include "Texels.h"

#include <Framework/CommonTypes.h>

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <algorithm>

using vkh = VulkanHelpers;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	static TextureResource LoadFromFacePaths(const std::array<std::string, 6>& sidePaths, CubemapFormat sourceFormat, 
		VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice, 
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


		auto [image, memory] = CreateAndLoadCubemapImage(texels, texelFormat, finalFormat, finalLayout, device, physicalDevice, transferPool, transferQueue);

		auto* view = vkh::CreateImage2DView(image, finalFormat, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_ASPECT_COLOR_BIT, 1, 6, device);
		auto* sampler = CreateSampler(device);
		
		return TextureResource(device, texels[0]->Width(), texels[0]->Height(), 1/*miplevels*/, 6, 
			image, memory, view, sampler, finalFormat, finalLayout);
	}

private:
	static std::tuple<VkImage, VkDeviceMemory> CreateAndLoadCubemapImage(
		const std::array<std::unique_ptr<Texels>, 6>& texels, VkFormat texelFormat,
		VkFormat desiredFormat, VkImageLayout targetLayout, 
		VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool transferPool, VkQueue transferQueue)
	{
		const u32 mipLevels = 1;
		const u32 cubeSides = 6;
		const auto faceTexelDataSize = texels[0]->DataSize();
		const auto faceTexelWidth = texels[0]->Width();
		const auto faceTexelHeight = texels[0]->Height();

		
		// Create staging buffer
		auto [stagingBuffer, stagingBufferMemory] = vkh::CreateBuffer(
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

		auto* cmdBuffer = vkh::BeginSingleTimeCommands(transferPool, device);
		const auto subresourceRange = vki::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, cubeSides);
		
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
				const VkDeviceSize bufferOffset = face * faceTexelDataSize;
				//for (u32 level = 0; level < cubeMap.mipLevels; level++)
				{
					// Calculate offset into staging buffer for the current mip level and face
					auto bufferCopyRegion = vki::BufferImageCopy(bufferOffset, 0, 0,
						vki::ImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, face, 1),
						vki::Offset3D(0, 0, 0), 
						vki::Extent3D(faceTexelWidth, faceTexelHeight, 1));
					
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
			auto [newCubemapImage, newCubemapImageMemory] = vkh::CreateImage2D(
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

			
			vkh::TransitionImageLayout(cmdBuffer, cubemapTextureImage,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // from
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // to
				subresourceRange);

			
			vkh::TransitionImageLayout(cmdBuffer, newCubemapImage,
				VK_IMAGE_LAYOUT_UNDEFINED, // from
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
				subresourceRange);

			
			vkh::BlitSrcToDstImage(cmdBuffer, cubemapTextureImage, newCubemapImage, faceTexelWidth, faceTexelHeight, subresourceRange);

			
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
	
	static VkSampler CreateSampler(VkDevice device)
	{
		VkSamplerCreateInfo samplerCI = {};
		{
			samplerCI.pNext = nullptr;
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

