#pragma once


#include <Framework/CommonTypes.h>

#include <vulkan/vulkan.h>

#include <vector>

#include "VulkanHelpers.h"

using vkh = VulkanHelpers;

struct FramebufferDesc
{
	VkExtent2D Extent = {};
	VkFormat Format = {};
	VkSampleCountFlagBits MsaaSamples;
	VkRenderPass RenderPass{};
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FramebufferResources // NOTE: Not sure if i'm keeping this class - just thrown together for quick renderpasses
{
public:
	
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

	FramebufferDesc Desc = {};
	
	//VkExtent2D Extent = {};
	//VkFormat Format = {};
	std::vector<Attachment> Attachments = {};
	VkFramebuffer Framebuffer = nullptr;
	
	VkSampler OutputSampler = nullptr;
	VkDescriptorImageInfo OutputDescriptor = {};

	FramebufferResources(FramebufferDesc desc, VkDevice device, VkAllocationCallbacks* allocator, VkPhysicalDevice physicalDevice) :
		_device{ device }, _allocator{ allocator }, Desc{desc}
	{
		const u32 mipLevels = 1;
		const u32 layerCount = 1;
		const bool usingMsaa = desc.MsaaSamples > VK_SAMPLE_COUNT_1_BIT;

		Attachments = {};
		
		// Create color attachment
		Attachment colorAttachment = {};
		{
			// Create color image and memory
			std::tie(colorAttachment.Image, colorAttachment.ImageMemory) = vkh::CreateImage2D(
				desc.Extent.width, desc.Extent.height,
				mipLevels,
				desc.MsaaSamples,
				desc.Format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // TODO change this when msaa on?
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				physicalDevice, device);

			// Create image view
			colorAttachment.ImageView = vkh::CreateImage2DView(
				colorAttachment.Image, 
				desc.Format, 
				VK_IMAGE_VIEW_TYPE_2D, 
				VK_IMAGE_ASPECT_COLOR_BIT,
				mipLevels, 
				layerCount, 
				device);

			// Store it
			Attachments.push_back(colorAttachment);
		}

		
		// Create depth attachment
		Attachment depthAttachment = {};
		{
			const VkFormat depthFormat = vkh::FindDepthFormat(physicalDevice);

			// Create depth image and memory
			std::tie(depthAttachment.Image, depthAttachment.ImageMemory) = vkh::CreateImage2D(
				desc.Extent.width, desc.Extent.height,
				mipLevels,
				desc.MsaaSamples,
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
			Attachments.push_back(depthAttachment);
		}

		
		// Create optional resolve attachment  -  when msaa is enabled
		Attachment resolveAttachment = {};
		if (usingMsaa)
		{
			// Create color image and memory
			std::tie(resolveAttachment.Image, resolveAttachment.ImageMemory) = vkh::CreateImage2D(
				desc.Extent.width, desc.Extent.height,
				mipLevels,
				VK_SAMPLE_COUNT_1_BIT,
				desc.Format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				physicalDevice, device);

			// Create image view
			resolveAttachment.ImageView = vkh::CreateImage2DView(
				resolveAttachment.Image, 
				desc.Format, 
				VK_IMAGE_VIEW_TYPE_2D, 
				VK_IMAGE_ASPECT_COLOR_BIT,
				mipLevels, 
				layerCount, 
				device);

			// Store the resolve goodness
			Attachments.push_back(resolveAttachment);
		}


		// Collect the views of attachments for use in framebuffer
		std::vector<VkImageView> framebufferViews = {};
		for (auto& a : Attachments) {
			framebufferViews.push_back(a.ImageView);
		}
		
		
		// Create framebuffer
		auto* framebuffer = vkh::CreateFramebuffer(device,
			desc.Extent.width, desc.Extent.height,
			framebufferViews,
			desc.RenderPass);


		// Color Sampler and co. so it can be sampled from a shader
		auto* sampler = vkh::CreateSampler(device);


		Framebuffer = framebuffer;
		OutputSampler = sampler;
		OutputDescriptor = usingMsaa
			? VkDescriptorImageInfo{ sampler, resolveAttachment.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			: VkDescriptorImageInfo{ sampler, colorAttachment.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	}
	
	void Destroy()
	{
		for (auto&& attachment : Attachments) {
			attachment.Destroy(_device, _allocator);
		}

		vkDestroyFramebuffer(_device, Framebuffer, _allocator);
		vkDestroySampler(_device, OutputSampler, _allocator);
	}

	// TODO Make this less specific to the use case
	static FramebufferResources CreateSceneFramebuffer(VkExtent2D extent, VkFormat format, VkRenderPass renderPass, VkSampleCountFlagBits msaaSamples, VkDevice device, VkPhysicalDevice physicalDevice, VkAllocationCallbacks* allocator)
	{		
		FramebufferDesc desc = {};
		desc.Extent = extent;
		desc.Format = format;
		desc.MsaaSamples = msaaSamples;
		desc.RenderPass = renderPass;

		auto obj = FramebufferResources(desc, device, allocator, physicalDevice);
		return obj;
	}

	// TODO Make this less specific to the use case
	static FramebufferResources CreateShadowFramebuffer(VkExtent2D extent, VkRenderPass renderPass, VkDevice device, VkPhysicalDevice physicalDevice, VkAllocationCallbacks* allocator)
	{
		const u32 mipLevels = 1;
		const u32 layerCount = 1;
		const auto msaaSamples = VK_SAMPLE_COUNT_1_BIT;


		const VkFormat depthFormat = vkh::FindDepthFormat(physicalDevice);

		// Create depth attachment
		Attachment depthAttachment = {};
		{

			// Create depth image and memory
			std::tie(depthAttachment.Image, depthAttachment.ImageMemory) = vkh::CreateImage2D(
				extent.width, extent.height,
				mipLevels,
				msaaSamples,
				depthFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
		}


		// Create framebuffer
		auto* framebuffer = vkh::CreateFramebuffer(device, extent.width, extent.height,
			{ depthAttachment.ImageView }, renderPass);


		// Sampler so it can be sampled from a shader
		auto* sampler = vkh::CreateSampler(device,
			VK_FILTER_LINEAR, VK_FILTER_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

		FramebufferDesc desc = {};
		desc.Extent = extent;
		desc.Format = vkh::FindDepthFormat(physicalDevice);
		desc.MsaaSamples = msaaSamples;
		desc.RenderPass = renderPass;

		
		FramebufferResources res = {};
		res.Desc = desc;
		res.Framebuffer = framebuffer;
		res.Attachments = { depthAttachment };
		res.OutputSampler = sampler;
		res.OutputDescriptor = VkDescriptorImageInfo{ sampler, depthAttachment.ImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };

		res._device = device;
		res._allocator = allocator;
		
		return res;
	}
	
private:
	VkDevice _device = nullptr;
	VkAllocationCallbacks* _allocator = nullptr;

	FramebufferResources() = default;
	
};
