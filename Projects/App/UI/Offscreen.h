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
		//VkRenderPass RenderPass = nullptr;

		//VkDescriptorImageInfo OutputDescriptor() const { return 
		
		void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
		{
			for (auto&& attachment : Attachments) {
				attachment.Destroy(device, allocator);
			}

			vkDestroyFramebuffer(device, Framebuffer, allocator);
			//vkDestroyRenderPass(device, RenderPass, allocator);
		}
	};

	inline VkRenderPass CreateSceneRenderPass(VkFormat format, VulkanService& vk)
	{
		auto physicalDevice = vk.PhysicalDevice();
		auto device = vk.LogicalDevice();
		auto msaaSamples = vk.MsaaSamples();
		auto usingMsaa = msaaSamples > VK_SAMPLE_COUNT_1_BIT;
		
		// Color attachment
		VkAttachmentDescription colorAttachmentDesc = {};
		{
			colorAttachmentDesc.format = format;
			colorAttachmentDesc.samples = msaaSamples;
			colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // what to do with color/depth data before rendering
			colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // what to do with color/depth data after rendering
			colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // not using stencil
			colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
			colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// memory layout after renderpass
		}
		VkAttachmentReference colorAttachmentRef = {};
		{
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}


		// Depth attachment  -  multisample depth doesn't need to be resolved as it won't be displayed
		VkAttachmentDescription depthAttachmentDesc = {};
		{
			depthAttachmentDesc.format = vkh::FindDepthFormat(physicalDevice);
			depthAttachmentDesc.samples = msaaSamples;
			depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // not used after drawing
			depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 
			depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // memory layout after renderpass
		}
		VkAttachmentReference depthAttachmentRef = {};
		{
			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		// Associate color and depth attachements with a subpass
		VkSubpassDescription subpassDesc = {};
		{
			subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDesc.colorAttachmentCount = 1;
			subpassDesc.pColorAttachments = &colorAttachmentRef;
			subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;
			subpassDesc.pResolveAttachments = nullptr;
		}


		// Set subpass dependency for the implicit external subpass to wait for the swapchain to finish reading from it
		VkSubpassDependency subpassDependency = {};
		{
			subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL; // implicit subpass before render
			subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpassDependency.srcAccessMask = 0;
			subpassDependency.dstSubpass = 0; // this pass
			subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}


		// Create render pass
		std::vector<VkAttachmentDescription> attachments = {
			colorAttachmentDesc,
			depthAttachmentDesc,
		};

		VkRenderPassCreateInfo renderPassCI = {};
		{
			renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassCI.attachmentCount = (uint32_t)attachments.size();
			renderPassCI.pAttachments = attachments.data();
			renderPassCI.subpassCount = 1;
			renderPassCI.pSubpasses = &subpassDesc;
			renderPassCI.dependencyCount = 1;
			renderPassCI.pDependencies = &subpassDependency;
		}

		VkRenderPass renderPass;
		if (vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass");
		}

		return renderPass;
	}
	
	
	inline FramebufferResources CreateSceneOffscreenFramebuffer(VkFormat format, VkRenderPass renderPass, VulkanService& vk)
	{
		const auto extent = vk.GetSwapchain().GetExtent();


		// Create color attachment resources
		FramebufferResources::Attachment colorAttachment = {};
		{
			const u32 mipLevels = 1;
			const u32 layerCount = 1;

			// Create color image and memory
			std::tie(colorAttachment.Image, colorAttachment.ImageMemory) = vkh::CreateImage2D(
				extent.width, extent.height,
				mipLevels,
				vk.MsaaSamples(),
				format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vk.PhysicalDevice(), vk.LogicalDevice());

			// Create image view
			colorAttachment.ImageView = vkh::CreateImage2DView(colorAttachment.Image, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, layerCount, vk.LogicalDevice());
		}


		// Create depth attachment resources
		FramebufferResources::Attachment depthAttachment = {};
		std::tie(depthAttachment.Image, depthAttachment.ImageMemory, depthAttachment.ImageView) = vkh::CreateDepthResources(extent, vk.MsaaSamples(), vk.LogicalDevice(), vk.PhysicalDevice());


		// Create framebuffer
		auto* framebuffer = vkh::CreateFramebuffer(vk.LogicalDevice(),
			extent.width, extent.height,
			{ colorAttachment.ImageView, depthAttachment.ImageView },
			renderPass);

		FramebufferResources res = {};
		res.Framebuffer = framebuffer;
		res.Extent = extent;
		res.Format = format;
		res.Attachments = std::vector{ colorAttachment, depthAttachment };

		return res;
	}

	
	//inline TextureResource CreateScreenTexture(u32 width, u32 height, VulkanService& vk)
	//{
	//	const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
	//	const u32 mipLevels = 1;
	//	const u32 layerCount = 1;


	//	auto [image, memory] = vkh::CreateImage2D(width, height, mipLevels,
	//		VK_SAMPLE_COUNT_1_BIT,
	//		format,
	//		VK_IMAGE_TILING_OPTIMAL,
	//		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | // Used in offscreen framebuffer
	//		//VK_IMAGE_USAGE_TRANSFER_SRC_BIT |     // Need to convert layout to attachment optimal in prep for framebuffer writing
	//		VK_IMAGE_USAGE_SAMPLED_BIT,           // Framebuffer result is used in later shader pass

	//		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	//		vk.PhysicalDevice(), vk.LogicalDevice(),
	//		layerCount);

	//	
	//	// Transition image layout
	//	{
	//		auto* cmdBuf = vkh::BeginSingleTimeCommands(vk.CommandPool(), vk.LogicalDevice());

	//		VkImageSubresourceRange subresourceRange = {};
	//		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//		subresourceRange.baseArrayLayer = 0;
	//		subresourceRange.layerCount = 1;
	//		subresourceRange.baseMipLevel = 0;
	//		subresourceRange.levelCount = mipLevels;

	//		vkh::TransitionImageLayout(cmdBuf, image,
	//			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

	//		vkh::EndSingeTimeCommands(cmdBuf, vk.CommandPool(), vk.GraphicsQueue(), vk.LogicalDevice());
	//	}


	//	VkImageView view = vkh::CreateImage2DView(image, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, layerCount, vk.LogicalDevice());

	//	
	//	VkSampler sampler = vkh::CreateSampler(vk.LogicalDevice());

	//	
	//	return TextureResource{ vk.LogicalDevice(), width, height, mipLevels, layerCount, image, memory, view, sampler, format };
	//}
	
}