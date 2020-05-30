#pragma once

#include "VulkanHelpers.h"

using vkh = VulkanHelpers;

// RAII container of swapchain components
class Swapchain
{
public:
private:
	// Dependency
	VkDevice _device = nullptr;

	VkSwapchainKHR _swapchain = nullptr;
	VkFormat _imageFormat= {};
	VkExtent2D _extent = {};
	
	std::vector<VkFramebuffer> _framebuffers = {};
	std::vector<VkImage> _images = {};
	std::vector<VkImageView> _imageViews = {};
	u32 _imageCount = 0;

	// Color image Swapchain attachment - one instance paired with each swapchain instance for use in the framebuffer
	VkImage _colorImage = nullptr;
	VkDeviceMemory _colorImageMemory = nullptr;
	VkImageView _colorImageView = nullptr;

	// Depth image Swapchain attachment - one instance paired with each swapchain instance for use in the framebuffer
	VkImage _depthImage = nullptr;
	VkDeviceMemory _depthImageMemory = nullptr;
	VkImageView _depthImageView = nullptr;

	VkRenderPass _renderPass = nullptr;
	
public:
	inline VkSwapchainKHR GetSwapchain() const                       { return _swapchain; }
	inline VkRenderPass GetRenderPass() const                        { return _renderPass; }
	inline const std::vector<VkFramebuffer>& GetFramebuffers() const { return _framebuffers; }
	inline u32 GetImageCount() const                                 { return _imageCount; }
	inline VkExtent2D GetExtent() const                              { return _extent; }

	Swapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const VkExtent2D& framebufferSize, 
		VkSampleCountFlagBits msaa, bool vsync)
		: _device(device)
	{
		assert(_device);

		auto [swapchain, swapchainImages, swapchainImageFormat, swapchainExtent]
			= CreateSwapchain(device, physicalDevice, surface, framebufferSize, vsync);

		auto swapchainImageViews = vkh::CreateImageViews(swapchainImages, swapchainImageFormat, VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, device);

		auto [colorImage, colorImageMemory, colorImageView]
			= vkh::CreateColorResources(swapchainImageFormat, swapchainExtent, msaa, device, physicalDevice);

		auto [depthImage, depthImageMemory, depthImageView]
			= vkh::CreateDepthResources(swapchainExtent, msaa, device, physicalDevice);

		auto* renderPass = CreateSwapchainRenderPass(msaa, swapchainImageFormat, device, physicalDevice);

		auto swapchainFramebuffers
			= CreateSwapchainFramebuffer(device, colorImageView, depthImageView, swapchainImageViews,
				swapchainExtent, renderPass);

		_imageCount = (u32)swapchainImages.size();
		_swapchain = swapchain;
		_images = std::move(swapchainImages);
		_imageFormat = swapchainImageFormat;
		_extent = swapchainExtent;
		_imageViews = std::move(swapchainImageViews);
		_colorImage = colorImage;
		_colorImageMemory = colorImageMemory;
		_colorImageView = colorImageView;
		_depthImage = depthImage;
		_depthImageMemory = depthImageMemory;
		_depthImageView = depthImageView;
		_renderPass = renderPass;
		_framebuffers = std::move(swapchainFramebuffers);
	};
	
	// No copying as we're an RAII container
	Swapchain(const Swapchain&) = delete;
	Swapchain& operator=(const Swapchain&) = delete;
	
	// Default move is sufficient as members are trivially moved
	Swapchain(Swapchain&&) = default; 
	Swapchain& operator=(Swapchain&&) = default; 

	~Swapchain()
	{
		assert(_device);
		for (auto* fb : _framebuffers) { vkDestroyFramebuffer(_device, fb, nullptr); }

		vkDestroyImageView(_device, _colorImageView, nullptr);
		vkDestroyImage(_device, _colorImage, nullptr);
		vkFreeMemory(_device, _colorImageMemory, nullptr);

		vkDestroyImageView(_device, _depthImageView, nullptr);
		vkDestroyImage(_device, _depthImage, nullptr);
		vkFreeMemory(_device, _depthImageMemory, nullptr);

		vkDestroyRenderPass(_device, _renderPass, nullptr);
		for (auto& x : _imageViews) { vkDestroyImageView(_device, x, nullptr); }
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	}


private:

	static std::tuple<VkSwapchainKHR, std::vector<VkImage>, VkFormat, VkExtent2D>
	CreateSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
	                const VkExtent2D& framebufferSize, bool vsync)
	{
		const SwapChainSupportDetails deets = vkh::QuerySwapChainSupport(physicalDevice, surface);

		const auto surfaceFormat = vkh::ChooseSwapSurfaceFormat(deets.Formats);
		const auto presentMode = vkh::ChooseSwapPresentMode(deets.PresentModes, vsync);
		const auto extent = vkh::ChooseSwapExtent(framebufferSize, deets.Capabilities);

		// Image count
		uint32_t minImageCount = deets.Capabilities.minImageCount + 1; // 1 extra image to avoid waiting on driver
		const auto maxImageCount = deets.Capabilities.maxImageCount;
		const auto maxImageCountExists = maxImageCount != 0;
		if (maxImageCountExists && minImageCount > maxImageCount)
		{
			minImageCount = maxImageCount;
		}


		// Create swap chain info
		VkSwapchainCreateInfoKHR info = {};
		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		info.surface = surface;
		info.minImageCount = minImageCount;
		info.imageFormat = surfaceFormat.format;
		info.imageColorSpace = surfaceFormat.colorSpace;
		info.imageExtent = extent;
		info.imageArrayLayers = 1;
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //VK_IMAGE_USAGE_TRANSFER_DST_BIT for post processing buffer
		info.preTransform = deets.Capabilities.currentTransform; // transform image before showing it?
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // how alpha is treated when blending with other windows
		info.presentMode = presentMode;
		info.clipped = VK_TRUE; // true means we don't care about pixels obscured by other windows
		info.oldSwapchain = nullptr;

		
		// Specify how to use swap chain images across multiple queue families
		QueueFamilyIndices indicies = vkh::FindQueueFamilies(physicalDevice, surface);
		const uint32_t queueCount = 2;
		// TODO Code smell: will break as more are added to indicies?
		uint32_t queueFamiliesIndices[queueCount] = { indicies.GraphicsFamily.value(), indicies.PresentFamily.value() };
		if (indicies.GraphicsFamily != indicies.PresentFamily)
		{
			info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			info.queueFamilyIndexCount = queueCount;
			info.pQueueFamilyIndices = queueFamiliesIndices;
		}
		else
		{
			info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // prefereable with best performance
			info.queueFamilyIndexCount = 0;
			info.pQueueFamilyIndices = nullptr;
		}


		// Create swap chain
		VkSwapchainKHR swapchain;
		if (vkCreateSwapchainKHR(device, &info, nullptr, &swapchain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain!");
		}


		// Retrieve swapchain images
		std::vector<VkImage> swapchainImages;
		u32 imageCount;
		vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
		swapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

		return { swapchain, std::move(swapchainImages), surfaceFormat.format, extent };
	}
	
	static std::vector<VkFramebuffer>
	CreateSwapchainFramebuffer(VkDevice device, VkImageView colorImageView, VkImageView depthImageView,
	                           const std::vector<VkImageView>& swapchainImageViews, const VkExtent2D& swapchainExtent,
	                           VkRenderPass renderPass)
	{
		std::vector<VkFramebuffer> swapchainFramebuffers{ swapchainImageViews.size() };

		for (size_t i = 0; i < swapchainImageViews.size(); ++i)
		{
			std::vector<VkImageView> attachments = { colorImageView, depthImageView, swapchainImageViews[i] };

			swapchainFramebuffers[i]
				= vkh::CreateFramebuffer(device, swapchainExtent.width, swapchainExtent.height, attachments, renderPass);
		}

		return swapchainFramebuffers;
	}

	static VkRenderPass
	CreateSwapchainRenderPass(VkSampleCountFlagBits msaaSamples, VkFormat swapchainFormat,
	                                     VkDevice device, VkPhysicalDevice physicalDevice)
	{
		// Color attachment
		VkAttachmentDescription colorAttachmentDesc = {};
		{
			colorAttachmentDesc.format = swapchainFormat;
			colorAttachmentDesc.samples = msaaSamples;
			colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // what to do with color/depth data before rendering
			colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // what to do with color/depth data after rendering
			colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // not using stencil
			colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // mem layout before renderpass
			colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // memory layout after renderpass
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
			depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		VkAttachmentReference depthAttachmentRef = {};
		{
			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}


		// Color resolve attachment  -  used to resolve multisampled image into one that can be displayed
		VkAttachmentDescription colorAttachmentResolveDesc = {};
		{
			colorAttachmentResolveDesc.format = swapchainFormat;
			colorAttachmentResolveDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachmentResolveDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachmentResolveDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachmentResolveDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachmentResolveDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachmentResolveDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachmentResolveDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}
		VkAttachmentReference colorAttachmentResolveRef = {};
		{
			colorAttachmentResolveRef.attachment = 2;
			colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}


		// Associate color and depth attachements with a subpass
		VkSubpassDescription subpassDesc = {};
		{
			subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDesc.colorAttachmentCount = 1;
			subpassDesc.pColorAttachments = &colorAttachmentRef;
			subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;
			subpassDesc.pResolveAttachments = &colorAttachmentResolveRef;
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
			colorAttachmentResolveDesc
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
	
};