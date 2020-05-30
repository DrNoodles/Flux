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
			= vkh::CreateSwapchain(device, physicalDevice, surface, framebufferSize, vsync);

		auto swapchainImageViews = vkh::CreateImageViews(swapchainImages, swapchainImageFormat, VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, device);

		auto [colorImage, colorImageMemory, colorImageView]
			= vkh::CreateColorResources(swapchainImageFormat, swapchainExtent, msaa, device, physicalDevice);

		auto [depthImage, depthImageMemory, depthImageView]
			= vkh::CreateDepthResources(swapchainExtent, msaa, device, physicalDevice);

		auto* renderPass = vkh::CreateSwapchainRenderPass(msaa, swapchainImageFormat, device, physicalDevice);

		auto swapchainFramebuffers
			= vkh::CreateSwapchainFramebuffer(device, colorImageView, depthImageView, swapchainImageViews,
				swapchainExtent, renderPass);

		_imageCount = swapchainImages.size();
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
};