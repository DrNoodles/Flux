#pragma once

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
		VkSampler ColorSampler = nullptr;

		VkDescriptorImageInfo OutputDescriptor;
		
		
		void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
		{
			for (auto&& attachment : Attachments) {
				attachment.Destroy(device, allocator);
			}

			vkDestroyFramebuffer(device, Framebuffer, allocator);
			vkDestroySampler(device, ColorSampler, allocator);
		}
	};

	
	inline FramebufferResources CreateSceneOffscreenFramebuffer(VkFormat format, VkRenderPass renderPass, VulkanService& vk)
	{
		const auto extent = vk.GetSwapchain().GetExtent();
		const auto msaaSamples = vk.MsaaSamples();
		const u32 mipLevels = 1;
		const u32 layerCount = 1;

		
		// Create color attachment resources
		FramebufferResources::Attachment colorAttachment = {};
		{
			// Create color image and memory
			std::tie(colorAttachment.Image, colorAttachment.ImageMemory) = vkh::CreateImage2D(
				extent.width, extent.height,
				mipLevels,
				msaaSamples,
				format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vk.PhysicalDevice(), vk.LogicalDevice());

			// Create image view
			colorAttachment.ImageView = vkh::CreateImage2DView(
				colorAttachment.Image, 
				format, 
				VK_IMAGE_VIEW_TYPE_2D, 
				VK_IMAGE_ASPECT_COLOR_BIT,
				mipLevels, 
				layerCount, 
				vk.LogicalDevice());
		}

		
		// Create depth attachment resources
		FramebufferResources::Attachment depthAttachment = {};
		{
			const VkFormat depthFormat = vkh::FindDepthFormat(vk.PhysicalDevice());

			// Create depth image and memory
			std::tie(depthAttachment.Image, depthAttachment.ImageMemory) = vkh::CreateImage2D(
				extent.width, extent.height,
				mipLevels,
				msaaSamples,
				depthFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vk.PhysicalDevice(), vk.LogicalDevice());

			// Create image view
			depthAttachment.ImageView = vkh::CreateImage2DView(
				depthAttachment.Image, 
				depthFormat, 
				VK_IMAGE_VIEW_TYPE_2D, 
				VK_IMAGE_ASPECT_DEPTH_BIT, 
				mipLevels, 
				layerCount, 
				vk.LogicalDevice());
		}


		// Create framebuffer
		auto* framebuffer = vkh::CreateFramebuffer(vk.LogicalDevice(),
			extent.width, extent.height,
			{ colorAttachment.ImageView, depthAttachment.ImageView },
			renderPass);


		// Color Sampler and co. so it can be sampled from a shader
		auto* colorSampler = vkh::CreateSampler(vk.LogicalDevice()); // This might need some TLC.

		
		FramebufferResources res = {};
		res.Framebuffer = framebuffer;
		res.Extent = extent;
		res.Format = format;
		res.Attachments = std::vector{ colorAttachment, depthAttachment };
		res.ColorSampler = colorSampler;
		res.OutputDescriptor = VkDescriptorImageInfo{ colorSampler, colorAttachment.ImageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

		return res;
	}
}