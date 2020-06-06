#pragma once

#include "Renderer/GpuTypes.h"
#include "Renderer/VulkanService.h"

namespace OffScreen
{
	struct FramebufferResources
	{
		struct Attachment
		{
			VkImage Image;
			VkDeviceMemory ImageMemory;
			VkImageView ImageView;

			void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
			{
				vkFreeMemory(device, ImageMemory, allocator);
				vkDestroyImage(device, Image, allocator);
				vkDestroyImageView(device, ImageView, allocator);
				ImageMemory = nullptr;
				Image = nullptr;
				ImageView = nullptr;
			}
		};

		VkExtent2D Extent = {};
		VkFormat Format = {};
		std::vector<Attachment> Attachments = {};
		VkFramebuffer Framebuffer = nullptr;

		// Color extras so we can sample from shader
		VkSampler OutputSampler = nullptr;
		VkDescriptorImageInfo OutputDescriptor;
		
		
		void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
		{
			for (auto&& attachment : Attachments) {
				attachment.Destroy(device, allocator);
			}

			vkDestroyFramebuffer(device, Framebuffer, allocator);
			vkDestroySampler(device, OutputSampler, allocator);
		}
	};

	
	inline FramebufferResources CreateSceneOffscreenFramebuffer(VkExtent2D extent, VkFormat format, VkRenderPass renderPass, VkSampleCountFlagBits msaaSamples, VkDevice device, VkPhysicalDevice physicalDevice)
	{
		const u32 mipLevels = 1;
		const u32 layerCount = 1;
		const bool usingMsaa = msaaSamples > VK_SAMPLE_COUNT_1_BIT;

		std::vector<FramebufferResources::Attachment> attachments = {};

		
		// Create color attachment
		FramebufferResources::Attachment colorAttachment = {};
		{
			// Create color image and memory
			std::tie(colorAttachment.Image, colorAttachment.ImageMemory) = vkh::CreateImage2D(
				extent.width, extent.height,
				mipLevels,
				msaaSamples,
				format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // TODO change this when msaa on?
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				physicalDevice, device);

			// Create image view
			colorAttachment.ImageView = vkh::CreateImage2DView(
				colorAttachment.Image, 
				format, 
				VK_IMAGE_VIEW_TYPE_2D, 
				VK_IMAGE_ASPECT_COLOR_BIT,
				mipLevels, 
				layerCount, 
				device);

			// Store it
			attachments.push_back(colorAttachment);
		}

		
		// Create depth attachment
		FramebufferResources::Attachment depthAttachment = {};
		{
			const VkFormat depthFormat = vkh::FindDepthFormat(physicalDevice);

			// Create depth image and memory
			std::tie(depthAttachment.Image, depthAttachment.ImageMemory) = vkh::CreateImage2D(
				extent.width, extent.height,
				mipLevels,
				msaaSamples,
				depthFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
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

			// Store it
			attachments.push_back(depthAttachment);
		}

		
		// Create optional resolve attachment  -  when msaa is enabled
		FramebufferResources::Attachment resolveAttachment = {};
		if (usingMsaa)
		{
			// Create color image and memory
			std::tie(resolveAttachment.Image, resolveAttachment.ImageMemory) = vkh::CreateImage2D(
				extent.width, extent.height,
				mipLevels,
				VK_SAMPLE_COUNT_1_BIT,
				format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				physicalDevice, device);

			// Create image view
			resolveAttachment.ImageView = vkh::CreateImage2DView(
				resolveAttachment.Image, 
				format, 
				VK_IMAGE_VIEW_TYPE_2D, 
				VK_IMAGE_ASPECT_COLOR_BIT,
				mipLevels, 
				layerCount, 
				device);

			// Store the resolve goodness
			attachments.push_back(resolveAttachment);
		}


		// Collect the views of attachments for use in framebuffer
		std::vector<VkImageView> framebufferViews = {};
		for (auto& a : attachments) {
			framebufferViews.push_back(a.ImageView);
		}
		
		
		// Create framebuffer
		auto* framebuffer = vkh::CreateFramebuffer(device,
			extent.width, extent.height,
			framebufferViews,
			renderPass);


		// Color Sampler and co. so it can be sampled from a shader
		auto* sampler = vkh::CreateSampler(device);

		
		FramebufferResources res = {};
		res.Framebuffer = framebuffer;
		res.Extent = extent;
		res.Format = format;
		res.Attachments = attachments;
		res.OutputSampler = sampler;
		res.OutputDescriptor = usingMsaa
			? VkDescriptorImageInfo{ sampler, resolveAttachment.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			: VkDescriptorImageInfo{ sampler, colorAttachment.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

		return res;
	}
}