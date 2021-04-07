#pragma once

#include <Framework/CommonTypes.h>
#include <vulkan/vulkan.h>

#include "VulkanService.h"

#include <vector>

using vkh = VulkanHelpers;


struct FramebufferAttachmentDesc
{
	VkFormat Format = VK_FORMAT_R8G8B8A8_UNORM;
	VkImageAspectFlagBits Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	VkClearValue ClearColor = {};
	VkSampleCountFlagBits MultisampleCount = VK_SAMPLE_COUNT_1_BIT;

	static FramebufferAttachmentDesc CreateColor(VkFormat format, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, VkClearColorValue clearColor = { {0,0,0,1} })
	{
		FramebufferAttachmentDesc color{};
		color.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		color.Format = format;
		color.ClearColor = VkClearValue{ .color = clearColor };
		color.MultisampleCount = samples;
		return color;
	}

	static FramebufferAttachmentDesc CreateDepth(VkFormat format, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, VkClearDepthStencilValue clearColor = { 1, 0 })
	{
		FramebufferAttachmentDesc depth{};
		depth.Aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		depth.Format = format;
		depth.ClearColor = VkClearValue{ .depthStencil = clearColor };
		depth.MultisampleCount = samples;
		return depth;
	}
	
	static FramebufferAttachmentDesc CreateStencil(VkFormat format, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, VkClearDepthStencilValue clearColor = { 1, 0 })
	{
		FramebufferAttachmentDesc depth{};
		depth.Aspect = VK_IMAGE_ASPECT_STENCIL_BIT;
		depth.Format = format;
		depth.ClearColor = VkClearValue{ .depthStencil = clearColor };
		depth.MultisampleCount = samples;
		return depth;
	}
	
	static FramebufferAttachmentDesc CreateResolve(VkFormat format)
	{
		FramebufferAttachmentDesc resolve{};
		resolve.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		resolve.Format = format;
		resolve.MultisampleCount = VK_SAMPLE_COUNT_1_BIT;
		return resolve;
	}
};	

struct FramebufferDesc
{
	VkExtent2D Extent{};
	std::vector<FramebufferAttachmentDesc> Attachments;
	u8 OutputAttachmentIndex = 0;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FramebufferResources // NOTE: Not sure if i'm keeping this class - just thrown together for quick renderpasses
{
private:// Types
	struct Attachment
	{
		FramebufferAttachmentDesc Desc;
		VkImage Image{};
		VkDeviceMemory ImageMemory{};
		VkImageView ImageView{};

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

private:// Data
	VulkanService& _vk;

public: // Data
	// Todo make private and provide getters
	FramebufferDesc Desc = {};
	VkFramebuffer Framebuffer = nullptr;
	std::vector<Attachment> Attachments = {};
	VkImage OutputImage = nullptr;
	VkDescriptorImageInfo OutputDescriptor = {};
	std::vector<VkClearValue> ClearValues = {};

public: // Methods

	FramebufferResources(const FramebufferDesc& desc, VkRenderPass renderPass, VulkanService& vk) :
      _vk(vk), Desc{desc}
	{
		assert(desc.OutputAttachmentIndex < desc.Attachments.size());
		
		const u32 mipLevels = 1;
		const u32 layerCount = 1;

		auto usageFromAspect = [](VkImageAspectFlagBits aspect) -> VkImageUsageFlags
		{
			VkImageUsageFlags usageFlags =
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			if (aspect & VK_IMAGE_ASPECT_COLOR_BIT)
				usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			else if (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
				usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			return usageFlags;
		};			
		
		for (const auto& attachmentDesc : desc.Attachments)
		{
			Attachment attachment;
			attachment.Desc = attachmentDesc;

			// Create image
			std::tie(attachment.Image, attachment.ImageMemory) = vkh::CreateImage2D(
            desc.Extent.width, desc.Extent.height,
            mipLevels,
            attachmentDesc.MultisampleCount,
            attachmentDesc.Format,
            VK_IMAGE_TILING_OPTIMAL,
            usageFromAspect(attachmentDesc.Aspect),
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            _vk.PhysicalDevice(), _vk.LogicalDevice());

			// Create image view
			attachment.ImageView = vkh::CreateImage2DView(
            attachment.Image,
            attachmentDesc.Format,
            VK_IMAGE_VIEW_TYPE_2D,
            attachmentDesc.Aspect,
            mipLevels,
            layerCount,
            _vk.LogicalDevice());

			Attachments.emplace_back(attachment);
		}
		

		// Collect the views of attachments for use in framebuffer
		std::vector<VkImageView> framebufferViews = {};
		for (auto& a : Attachments) {
			framebufferViews.push_back(a.ImageView);
		}
		
		
		// Create framebuffer
		auto* framebuffer = vkh::CreateFramebuffer(_vk.LogicalDevice(),
         desc.Extent.width, desc.Extent.height,
         framebufferViews,
         renderPass);


		// Color Sampler and co. so it can be sampled from a shader
		auto* sampler = vkh::CreateSampler(_vk.LogicalDevice());

		Framebuffer = framebuffer;
		OutputImage = Attachments[desc.OutputAttachmentIndex].Image;
		OutputDescriptor = VkDescriptorImageInfo{
			sampler,
         Attachments[desc.OutputAttachmentIndex].ImageView,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      };

		const auto clearCount = (u32)Attachments.size();
		ClearValues.resize(clearCount);
		for (size_t i = 0; i < clearCount; i++)
		{
			ClearValues[i] = Attachments[i].Desc.ClearColor;
		}
	}
	
	void Destroy()
	{
		for (auto&& attachment : Attachments) {
			attachment.Destroy(_vk.LogicalDevice(), _vk.Allocator());
		}

		vkDestroyFramebuffer(_vk.LogicalDevice(), Framebuffer, _vk.Allocator());
		vkDestroySampler(_vk.LogicalDevice(), OutputDescriptor.sampler, _vk.Allocator());
	}
};