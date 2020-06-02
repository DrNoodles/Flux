#pragma once

#include "GpuTypes.h"
#include "VulkanHelpers.h"
#include "VulkanInitializers.h"
#include "Swapchain.h"

#include <Framework/CommonTypes.h>

#include <vulkan/vulkan.h>

#include <iostream>

using vkh = VulkanHelpers;

class ISurfaceBuilder
{
public:
	virtual ~ISurfaceBuilder() = default;
	virtual VkSurfaceKHR CreateSurface(VkInstance instance) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IVulkanServiceDelegate
{
public:
	virtual ~IVulkanServiceDelegate() = default;
	virtual VkExtent2D GetFramebufferSize() = 0;
	virtual VkExtent2D WaitTillFramebufferHasSize() = 0;
	virtual void NotifySwapchainUpdated(u32 width, u32 height, u32 numSwapchainImages) = 0;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//struct Device
//{
//	VkDevice _logicalDevice = nullptr;
//	VkPhysicalDevice _physicalDevice = nullptr;
//	// TODO Keep all the useful device queries here
//};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class VulkanService
{
public: // DATA ///////////////////////////////////////////////////////////////////////////////////////////////////////
	
private: // DATA //////////////////////////////////////////////////////////////////////////////////////////////////////
	
	// Dependencies
	IVulkanServiceDelegate* _delegate = nullptr;

	// Data
	bool _enableValidationLayers = false;
	bool _vsync = false;
	bool _msaaEnabled = false;
	VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	const size_t _maxFramesInFlight = 2;
	const std::vector<const char*> _validationLayers = { "VK_LAYER_KHRONOS_validation", };
	const std::vector<const char*> _physicalDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	// Core vulkan
	VkInstance _instance = nullptr;
	VkDebugUtilsMessengerEXT _debugMessenger = nullptr;
	VkPhysicalDevice _physicalDevice = nullptr;
	VkDevice _device = nullptr;
	VkCommandPool _commandPool = nullptr;

	VkQueue _graphicsQueue = nullptr;
	VkQueue _presentQueue = nullptr;

	std::unique_ptr<Swapchain> _swapchain = nullptr;
	
	VkSurfaceKHR _surface = nullptr;

	std::vector<VkCommandBuffer> _commandBuffers{};

	// Synchronization
	std::vector<VkSemaphore> _renderFinishedSemaphores{};
	std::vector<VkSemaphore> _imageAvailableSemaphores{};
	std::vector<VkFence> _inFlightFences{};
	std::vector<VkFence> _imagesInFlight{};

	size_t _currentFrame = 0;
	bool _swapchainInvalidated = false;

	
public: // METHODS ////////////////////////////////////////////////////////////////////////////////////////////////////
	VkDevice LogicalDevice() const { return _device; }
	VkInstance Instance() const { return _instance; }
	VkSurfaceKHR Surface() const { return _surface; }
	VkPhysicalDevice PhysicalDevice() const { return _physicalDevice; }
	VkCommandPool CommandPool() const { return _commandPool; }
	VkQueue GraphicsQueue() const { return _graphicsQueue; }
	VkQueue PresentQueue() const { return _presentQueue; }

	VkAllocationCallbacks* Allocator() const { return nullptr; }
	
	VkSampleCountFlagBits MsaaSamples() const { return _msaaSamples; }
	size_t MaxFramesInFlight() const { return _maxFramesInFlight; }
	
	const Swapchain& GetSwapchain() const { return *_swapchain; }
	
	const std::vector<VkCommandBuffer>& CommandBuffers() const { return _commandBuffers; }

	// Frame rendering
	const std::vector<VkSemaphore>& RenderFinishedSemaphores() const { return _renderFinishedSemaphores; }
	const std::vector<VkSemaphore>& ImageAvailableSemaphores() const { return _imageAvailableSemaphores; }
	const std::vector<VkFence>& InFlightFences() const { return _inFlightFences; }
	std::vector<VkFence>& ImagesInFlight() { return _imagesInFlight; }

	
	void InvalidateSwapchain() { _swapchainInvalidated = true; }


	VulkanService(bool enableValidationLayers, bool enableVsync, bool enableMsaa, IVulkanServiceDelegate* delegate,
	              ISurfaceBuilder* builder, const VkExtent2D framebufferSize)
	{
		assert(delegate);
		assert(builder);
		
		_delegate = delegate;
		_enableValidationLayers = enableValidationLayers;
		_vsync = enableVsync;
		_msaaEnabled = enableMsaa;

		InitVulkan(builder);
		InitVulkanSwapchainAndDependants(framebufferSize);

		std::cout << (_msaaEnabled ? "MSAA Enabled" : "MSAA Disabled") << std::endl;
		std::cout << (_vsync ? "VSync Enabled" : "VSync Disabled") << std::endl;
	}
	
	void Shutdown()
	{
		DestroyVulkanSwapchain();
		DestroyVulkan();
	}
	
	std::optional<std::tuple<u32,VkCommandBuffer>> StartFrame()
	{
		// Sync CPU-GPU
		vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], true, UINT64_MAX);

		// Aquire an image from the swap chain
		u32 imageIndex;
		VkResult result = vkAcquireNextImageKHR(_device, _swapchain->GetSwapchain(), UINT64_MAX, _imageAvailableSemaphores[_currentFrame],
			nullptr, &imageIndex);

		if (_swapchainInvalidated || result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapchain();
			return std::nullopt;
		}
		const auto isUsable = result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
		if (!isUsable)
		{
			throw std::runtime_error("Failed to acquire swapchain image!");
		}


		// If the image is still used by a previous frame, wait for it to finish!
		if (_imagesInFlight[imageIndex] != nullptr)
		{
			vkWaitForFences(_device, 1, &_imagesInFlight[imageIndex], true, UINT64_MAX);
		}

		// Mark the image as now being in use by this frame
		_imagesInFlight[imageIndex] = _inFlightFences[_currentFrame];


		
		// Start command buffer
		auto* commandBuffer = _commandBuffers[imageIndex];
		const auto beginInfo = vki::CommandBufferBeginInfo(0, nullptr);
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin recording command buffer");
		}
		
		return std::tuple<u32, VkCommandBuffer>{ imageIndex, commandBuffer };
	}

	void EndFrame(u32 imageIndex, VkCommandBuffer commandBuffer)
	{
		// End recording
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to end recording command buffer");
		}

		
		// Execute command buffer with the image as an attachment in the framebuffer
		const uint32_t waitCount = 1; // waitSemaphores and waitStages arrays sizes must match as they're matched by index
		VkSemaphore waitSemaphores[waitCount] = { _imageAvailableSemaphores[_currentFrame] };
		VkPipelineStageFlags waitStages[waitCount] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		const uint32_t signalCount = 1;
		VkSemaphore signalSemaphores[signalCount] = { _renderFinishedSemaphores[_currentFrame] };

		VkSubmitInfo submitInfo = {};
		{
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];
			// cmdbuf that binds the swapchain image we acquired as color attachment
			submitInfo.waitSemaphoreCount = waitCount;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.signalSemaphoreCount = signalCount;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}

		vkResetFences(_device, 1, &_inFlightFences[_currentFrame]);

		if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to submit Draw Command Buffer");
		}


		// Return the image to the swap chain for presentation
		std::array<VkSwapchainKHR, 1> swapchains = { _swapchain->GetSwapchain() };
		VkPresentInfoKHR presentInfo = {};
		{
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;
			presentInfo.swapchainCount = (u32)swapchains.size();
			presentInfo.pSwapchains = swapchains.data();
			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr;
		}

		VkResult result = vkQueuePresentKHR(_presentQueue, &presentInfo);
		if (_swapchainInvalidated || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			RecreateSwapchain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed ot present swapchain image!");
		}

		_currentFrame = (_currentFrame + 1) % _maxFramesInFlight;
	}


private: // METHODS ///////////////////////////////////////////////////////////////////////////////////////////////////

	void InitVulkan(ISurfaceBuilder* builder)
	{
		auto* instance = vkh::CreateInstance(_enableValidationLayers, _validationLayers);
		
		if (_enableValidationLayers) {
			_debugMessenger = vkh::SetupDebugMessenger(instance);
		}

		auto* surface = builder->CreateSurface(instance);

		auto [physicalDevice, maxMsaaSamples] = vkh::PickPhysicalDevice(_physicalDeviceExtensions, instance, surface);
		
		auto [device, graphicsQueue, presentQueue]
			= vkh::CreateLogicalDevice(physicalDevice, surface, _validationLayers, _physicalDeviceExtensions);

		auto* commandPool = vkh::CreateCommandPool(vkh::FindQueueFamilies(physicalDevice, surface), device);

		
		// Set em. Done like this to enforce the correct initialization order above 
		_instance = instance;
		_surface = surface;
		_physicalDevice = physicalDevice;
		_msaaSamples =  _msaaEnabled ? maxMsaaSamples : VK_SAMPLE_COUNT_1_BIT;;
		_device = device;
		_graphicsQueue = graphicsQueue;
		_presentQueue = presentQueue;
		_commandPool = commandPool;
	}

	void DestroyVulkan() const
	{
		vkDestroyCommandPool(_device, _commandPool, nullptr);
		vkDestroyDevice(_device, nullptr);
		if (_enableValidationLayers) { vkh::DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr); }
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyInstance(_instance, nullptr);
	}

	void InitVulkanSwapchainAndDependants(const VkExtent2D& framebufferSize)
	{
		_swapchain = std::make_unique<Swapchain>(_device, _physicalDevice, _surface, framebufferSize, _msaaSamples, _vsync);

		_commandBuffers = vkh::AllocateCommandBuffers(_swapchain->GetImageCount(), _commandPool, _device);
		
		// TODO Break CreateSyncObjects() method so we can recreate the parts that are dependend on num swapchainImages
		auto [renderFinishedSemaphores, imageAvailableSemaphores, inFlightFences, imagesInFlight]
			= vkh::CreateSyncObjects(_maxFramesInFlight, _swapchain->GetImageCount(), _device);

		_renderFinishedSemaphores = renderFinishedSemaphores;
		_imageAvailableSemaphores = imageAvailableSemaphores;
		_inFlightFences = inFlightFences;
		_imagesInFlight = imagesInFlight;
	}
	void DestroyVulkanSwapchain()
	{

		for (auto& x : _inFlightFences) { vkDestroyFence(_device, x, nullptr); }
		for (auto& x : _renderFinishedSemaphores) { vkDestroySemaphore(_device, x, nullptr); }
		for (auto& x : _imageAvailableSemaphores) { vkDestroySemaphore(_device, x, nullptr); }
		
		vkFreeCommandBuffers(_device, _commandPool, (uint32_t)_commandBuffers.size(), _commandBuffers.data());

		_swapchain = nullptr; // RAII cleanup
		
	}
	void RecreateSwapchain()
	{
		_swapchainInvalidated = false;
		
		const auto size = _delegate->WaitTillFramebufferHasSize();
		
		vkDeviceWaitIdle(_device);
		
		DestroyVulkanSwapchain();
		InitVulkanSwapchainAndDependants(size);

		_delegate->NotifySwapchainUpdated(size.width, size.height, _swapchain->GetImageCount());
	}
};
