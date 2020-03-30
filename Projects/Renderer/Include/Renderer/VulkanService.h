#pragma once

#include "GpuTypes.h"
#include "VulkanHelpers.h"
#include "VulkanInitializers.h"

#include <Framework/CommonTypes.h>

#include <vulkan/vulkan.h>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IVulkanServiceDelegate
{
public:
	virtual ~IVulkanServiceDelegate() = default;
	virtual VkSurfaceKHR CreateSurface(VkInstance instance) const = 0;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class VulkanService
{
public:
	VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	const size_t _maxFramesInFlight = 2;
	const std::vector<const char*> _validationLayers = { "VK_LAYER_KHRONOS_validation", };
	const std::vector<const char*> _physicalDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkInstance _instance = nullptr;
	VkDebugUtilsMessengerEXT _debugMessenger = nullptr;
	VkPhysicalDevice _physicalDevice = nullptr;
	VkDevice _device = nullptr;
	VkCommandPool _commandPool = nullptr;

	VkQueue _graphicsQueue = nullptr;
	VkQueue _presentQueue = nullptr;

	VkSurfaceKHR _surface = nullptr; // ?

	
	VulkanService(bool enableValidationLayers/*, IVulkanServiceDelegate* delegate*/) /*:_delegate{delegate}*/
	{
		_enableValidationLayers = enableValidationLayers;
	}

	/*void InitVulkan()
	{
		_instance = vkh::CreateInstance(_enableValidationLayers, _validationLayers);

		if (_enableValidationLayers)
		{
			_debugMessenger = vkh::SetupDebugMessenger(_instance);
		}

		_surface = _delegate->CreateSurface(_instance);

		std::tie(_physicalDevice, _msaaSamples) = vkh::PickPhysicalDevice(_physicalDeviceExtensions, _instance, _surface);

		std::tie(_device, _graphicsQueue, _presentQueue)
			= vkh::CreateLogicalDevice(_physicalDevice, _surface, _validationLayers, _physicalDeviceExtensions);

		_commandPool = vkh::CreateCommandPool(vkh::FindQueueFamilies(_physicalDevice, _surface), _device);
	}*/

	/*void DestroyVulkan()
	{
		vkDestroyCommandPool(_device, _commandPool, nullptr);
		vkDestroyDevice(_device, nullptr);
		if (_enableValidationLayers) { vkh::DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr); }
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyInstance(_instance, nullptr);
	}*/

	//void InitSwapchain()
	//{
		/*_swapchain = vkh::CreateSwapchain({ (uint32_t)width, (uint32_t)height }, _physicalDevice, _surface, _device,
			_swapchainImages, _swapchainImageFormat, _swapchainExtent);

		_swapchainImageViews = vkh::CreateImageViews(_swapchainImages, _swapchainImageFormat, VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, _device);

		std::tie(_colorImage, _colorImageMemory, _colorImageView)
			= vkh::CreateColorResources(_swapchainImageFormat, _swapchainExtent, _msaaSamples,
				_commandPool, _graphicsQueue, _device, _physicalDevice);

		std::tie(_depthImage, _depthImageMemory, _depthImageView)
			= vkh::CreateDepthResources(_swapchainExtent, _msaaSamples, _commandPool, _graphicsQueue, _device,
				_physicalDevice);


		_renderPass = vkh::CreateSwapchainRenderPass(_msaaSamples, _swapchainImageFormat, _device, _physicalDevice);*/
	//}

	/*void DestroySwapchain()
	{
		
	}*/
	
	//std::optional<u32> StartFrame()
	//{
	//	// Sync CPU-GPU
	//	vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], true, UINT64_MAX);

	//	// Aquire an image from the swap chain
	//	u32 imageIndex;
	//	VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame],
	//		nullptr, &imageIndex);

	//	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	//	{
	//		RecreateSwapchain();
	//		return std::nullopt;
	//	}
	//	const auto isUsable = result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
	//	if (!isUsable)
	//	{
	//		throw std::runtime_error("Failed to acquire swapchain image!");
	//	}


	//	// If the image is still used by a previous frame, wait for it to finish!
	//	if (_imagesInFlight[imageIndex] != nullptr)
	//	{
	//		vkWaitForFences(_device, 1, &_imagesInFlight[imageIndex], true, UINT64_MAX);
	//	}

	//	// Mark the image as now being in use by this frame
	//	_imagesInFlight[imageIndex] = _inFlightFences[_currentFrame];

	//	return imageIndex;
	//}

	//void EndFrame(u32 imageIndex)
	//{
	//	// Execute command buffer with the image as an attachment in the framebuffer
	//	const uint32_t waitCount = 1; // waitSemaphores and waitStages arrays sizes must match as they're matched by index
	//	VkSemaphore waitSemaphores[waitCount] = { _imageAvailableSemaphores[_currentFrame] };
	//	VkPipelineStageFlags waitStages[waitCount] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	//	const uint32_t signalCount = 1;
	//	VkSemaphore signalSemaphores[signalCount] = { _renderFinishedSemaphores[_currentFrame] };

	//	VkSubmitInfo submitInfo = {};
	//	{
	//		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	//		submitInfo.commandBufferCount = 1;
	//		submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];
	//		// cmdbuf that binds the swapchain image we acquired as color attachment
	//		submitInfo.waitSemaphoreCount = waitCount;
	//		submitInfo.pWaitSemaphores = waitSemaphores;
	//		submitInfo.pWaitDstStageMask = waitStages;
	//		submitInfo.signalSemaphoreCount = signalCount;
	//		submitInfo.pSignalSemaphores = signalSemaphores;
	//	}

	//	vkResetFences(_device, 1, &_inFlightFences[_currentFrame]);

	//	if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed to submit Draw Command Buffer");
	//	}


	//	// Return the image to the swap chain for presentation
	//	std::array<VkSwapchainKHR, 1> swapchains = { _swapchain };
	//	VkPresentInfoKHR presentInfo = {};
	//	{
	//		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	//		presentInfo.waitSemaphoreCount = 1;
	//		presentInfo.pWaitSemaphores = signalSemaphores;
	//		presentInfo.swapchainCount = (uint32_t)swapchains.size();
	//		presentInfo.pSwapchains = swapchains.data();
	//		presentInfo.pImageIndices = &imageIndex;
	//		presentInfo.pResults = nullptr;
	//	}

	//	VkResult result = vkQueuePresentKHR(_presentQueue, &presentInfo);
	//	if (FramebufferResized || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	//	{
	//		FramebufferResized = false;
	//		RecreateSwapchain();
	//	}
	//	else if (result != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed ot present swapchain image!");
	//	}

	//	_currentFrame = (_currentFrame + 1) % _maxFramesInFlight;
	//}
	
	bool _enableValidationLayers = false;
private:
	IVulkanServiceDelegate* _delegate = nullptr;
};
