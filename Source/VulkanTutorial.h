#pragma once

#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <fstream>
#include <array>

#include "Globals.h"
#include "Types.h"

#define OUT // syntax helper



class VulkanTutorial
{
public:
	std::string ShaderDir{};
	bool FramebufferResized = false;

	void Run()
	{
		InitWindow();
		InitVulkan(g_enableValidationLayers);
		MainLoop();
		CleanUp(g_enableValidationLayers);
	}

private:

	GLFWwindow* _window = nullptr;
	VkInstance _instance = nullptr;
	VkSurfaceKHR _surface = nullptr;
	VkDebugUtilsMessengerEXT _debugMessenger = nullptr;
	VkPhysicalDevice _physicalDevice = nullptr;
	VkDevice _device = nullptr;
	VkQueue _graphicsQueue = nullptr;
	VkQueue _presentQueue = nullptr;

	VkSwapchainKHR _swapchain = nullptr;
	std::vector<VkFramebuffer> _swapchainFramebuffers{};
	std::vector<VkImage> _swapchainImages{};
	std::vector<VkImageView> _swapchainImageViews{};
	VkFormat _swapchainImageFormat{};
	VkExtent2D _swapchainExtent{};

	VkRenderPass _renderPass = nullptr;
	VkPipeline _pipeline = nullptr;
	VkPipelineLayout _pipelineLayout = nullptr;

	VkCommandPool _commandPool = nullptr;
	std::vector<VkCommandBuffer> _commandBuffers{};

	// Synchronization
	std::vector<VkSemaphore> _renderFinishedSemaphores{};
	std::vector<VkSemaphore> _imageAvailableSemaphores{};
	std::vector<VkFence> _inFlightFences{};
	std::vector<VkFence> _framesInFlight{};
	size_t _currentFrame = 0;



	void InitWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't use opengl
	//	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // we'll handle resizing
		_window = glfwCreateWindow(g_width, g_height, "Vulkan", nullptr, nullptr);

		glfwSetWindowUserPointer(_window, this);
		glfwSetWindowSizeCallback(_window, FramebufferResizeCallback);
	}


	void InitVulkan(bool enableValidationLayers)
	{
		_instance = CreateInstance(enableValidationLayers);

		if (enableValidationLayers)
		{
			_debugMessenger = SetupDebugMessenger(_instance);
		}

		_surface = CreateSurface(_instance, _window);

		_physicalDevice = PickPhysicalDevice(_instance, _surface);

		std::tie(_device, _graphicsQueue, _presentQueue) =
			CreateLogicalDevice(_physicalDevice, _surface, g_validationLayers, g_physicalDeviceExtensions);

		_commandPool = CreateCommandPool(FindQueueFamilies(_physicalDevice, _surface), _device);



		int width, height;
		glfwGetFramebufferSize(_window, &width, &height);
		VkExtent2D windowSize{ width, height };
		_swapchain = CreateSwapchain(windowSize, _physicalDevice, _surface, _device,
			OUT _swapchainImages, OUT _swapchainImageFormat, OUT _swapchainExtent);

		_swapchainImageViews = CreateImageViews(_swapchainImages, _swapchainImageFormat, _device);

		_renderPass = CreateRenderPass(_swapchainImageFormat, _device);

		std::tie(_pipeline, _pipelineLayout) = CreateGraphicsPipeline(ShaderDir, _renderPass, _device, _swapchainExtent);

		_swapchainFramebuffers = CreateFramebuffer(_device, _renderPass, _swapchainExtent, _swapchainImageViews);

		_commandBuffers = CreateCommandBuffers((uint32_t)_swapchainImages.size(), _commandPool, _device, _renderPass,
			_swapchainExtent, _pipeline, _swapchainFramebuffers);



		std::tie(_renderFinishedSemaphores, _imageAvailableSemaphores, _inFlightFences, _framesInFlight) =
			CreateSyncObjects(g_maxFramesInFlight, _swapchainImages.size(), _device);
	}


	void DrawFrame()
	{
		// Sync CPU-GPU
		vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], true, UINT64_MAX);

		// Aquire an image from the swap chain
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame],
			nullptr, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapcain();
			return;
		}
		const auto isUsable = result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
		if (!isUsable)
		{
			throw std::runtime_error("Failed to acquire swapchain image!");
		}

		// If the image is still used by a previous frame, wait for it to finish!
		if (_framesInFlight[imageIndex] != nullptr)
		{
			vkWaitForFences(_device, 1, &_framesInFlight[imageIndex], true, UINT64_MAX);
		}

		// Mark the image as now being in use by this frame
		_framesInFlight[imageIndex] = _inFlightFences[_currentFrame];

		// Execute the command buffer with the image as an attachment in the framebuffer
		const uint32_t waitCount = 1; // waitSemaphores and waitStages arrays sizes must match as they're matched by index
		VkSemaphore waitSemaphores[waitCount] = { _imageAvailableSemaphores[_currentFrame] };
		VkPipelineStageFlags waitStages[waitCount] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		const uint32_t signalCount = 1;
		VkSemaphore signalSemaphores[signalCount] = { _renderFinishedSemaphores[_currentFrame] };

		VkSubmitInfo submitInfo = {};
		{
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &_commandBuffers[imageIndex]; // cmdbuf that binds the swapchain image we acquired as color attachment
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
		std::array<VkSwapchainKHR, 1> swapchains = { _swapchain };
		VkPresentInfoKHR presentInfo = {};
		{
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;
			presentInfo.swapchainCount = (uint32_t)swapchains.size();
			presentInfo.pSwapchains = swapchains.data();
			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr;
		}

		result = vkQueuePresentKHR(_presentQueue, &presentInfo);
		if (FramebufferResized || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			FramebufferResized = false;
			RecreateSwapcain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed ot present swapchain image!");
		}

		_currentFrame = (_currentFrame + 1) % g_maxFramesInFlight;
	}


	void MainLoop()
	{
		while (!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();
			DrawFrame();
		}

		vkDeviceWaitIdle(_device);
	}


	void CleanUp(bool enableValidationLayers)
	{
		CleanupSwapchain();

		for (auto& x : _inFlightFences) { vkDestroyFence(_device, x, nullptr); }
		for (auto& x : _renderFinishedSemaphores) { vkDestroySemaphore(_device, x, nullptr); }
		for (auto& x : _imageAvailableSemaphores) { vkDestroySemaphore(_device, x, nullptr); }

		vkDestroyCommandPool(_device, _commandPool, nullptr);
		vkDestroyDevice(_device, nullptr);
		if (enableValidationLayers) { DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr); }
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyInstance(_instance, nullptr);
		glfwDestroyWindow(_window);
		glfwTerminate();
	}


	void CleanupSwapchain()
	{
		vkFreeCommandBuffers(_device, _commandPool, (uint32_t)_commandBuffers.size(), _commandBuffers.data());
		for (auto& x : _swapchainFramebuffers) { vkDestroyFramebuffer(_device, x, nullptr); }
		vkDestroyPipeline(_device, _pipeline, nullptr);
		vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
		vkDestroyRenderPass(_device, _renderPass, nullptr);
		for (auto& x : _swapchainImageViews) { vkDestroyImageView(_device, x, nullptr); }
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	}


	void RecreateSwapcain()
	{

		int width, height;
		glfwGetFramebufferSize(_window, &width, &height);

		// This handles a minimized window. Wait until it has size > 0
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(_window, &width, &height);
			glfwWaitEvents();
		}


		vkDeviceWaitIdle(_device);


		CleanupSwapchain();


		VkExtent2D windowSize{ width, height };
		_swapchain = CreateSwapchain(windowSize, _physicalDevice, _surface, _device,
			OUT _swapchainImages, OUT _swapchainImageFormat, OUT _swapchainExtent);

		_swapchainImageViews = CreateImageViews(_swapchainImages, _swapchainImageFormat, _device);

		_renderPass = CreateRenderPass(_swapchainImageFormat, _device);

		std::tie(_pipeline, _pipelineLayout) = CreateGraphicsPipeline(ShaderDir, _renderPass, _device, _swapchainExtent);

		_swapchainFramebuffers = CreateFramebuffer(_device, _renderPass, _swapchainExtent, _swapchainImageViews);

		_commandBuffers = CreateCommandBuffers((uint32_t)_swapchainImages.size(), _commandPool, _device, _renderPass,
			_swapchainExtent, _pipeline, _swapchainFramebuffers);
	}


	#pragma region InitVulkan static helpers

	[[nodiscard]] static VkInstance CreateInstance(bool enableValidationLayers)
	{
		VkInstance instance = nullptr;

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;


		// Create info

		VkInstanceCreateInfo createInfo = {};
		createInfo.pNext = nullptr;
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;


		// Add validation layers?
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
		if (enableValidationLayers)
		{
			if (!CheckValidationLayerSupport())
			{
				throw std::runtime_error("Validation layers requested but not available");
			}

			createInfo.enabledLayerCount = uint32_t(g_validationLayers.size());
			createInfo.ppEnabledLayerNames = g_validationLayers.data();

			// Create debug helper to report messages for the creation/destruction of vkInstance
			PopulateDebugUtilsMessengerCreateInfoEXT(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}


		// Extensions
		auto requiredExtensions = GetRequiredExtensions(enableValidationLayers);
		createInfo.enabledExtensionCount = uint32_t(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();

		std::cout << "Required extensions\n";
		for (const auto& ext : requiredExtensions)
		{
			std::cout << "\t" << ext << std::endl;
		}

		// Query Vulkan Extensions
		uint32_t vkExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, nullptr);
		std::vector<VkExtensionProperties> vkExtensions(vkExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, vkExtensions.data());
		std::cout << "All supported vulkan extensions\n";
		for (const auto& ext : vkExtensions)
		{
			std::cout << "\t" << ext.specVersion << "\t" << ext.extensionName << std::endl;
		}

		// Confirm all glfw extensions are supported by vulkan
		const auto allSupported = [&]()
		{
			for (const auto& requiredExt : requiredExtensions)
			{
				auto isFound = false;
				for (auto& vkExt : vkExtensions)
				{
					if (strcmp(requiredExt, vkExt.extensionName) == 0)
					{
						isFound = true;
						break;
					}
				}
				if (!isFound) return false;
			}
			return true;
		}();

		if (!allSupported)
		{
			throw std::runtime_error{ "Vulkan doesn't support all extensions found by glfw" };
		}
		else
		{
			std::cout << "All glfw vulkan extensions are supported\n";
		}


		// Create the instance!

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		{
			throw std::runtime_error{ "Failed to create Vulkan Instance" };
		}

		return instance;
	}
	static bool CheckValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers{ layerCount };
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* requestedLayer : g_validationLayers)
		{
			bool layerFound = false;

			for (const auto& availableLayer : availableLayers)
			{
				if (strcmp(requestedLayer, availableLayer.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}

		return true;
	}
	static std::vector<const char*> GetRequiredExtensions(bool enableValidationLayers)
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}


	[[nodiscard]] static VkSurfaceKHR CreateSurface(VkInstance instance, GLFWwindow* window)
	{
		VkSurfaceKHR surface;
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface");
		}
		return surface;
	}


	[[nodiscard]] static VkDebugUtilsMessengerEXT SetupDebugMessenger(VkInstance instance)
	{
		VkDebugUtilsMessengerEXT debugMessenger = nullptr;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugUtilsMessengerCreateInfoEXT(createInfo);

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to setup debug messenger extension");
		}

		return debugMessenger;
	}
	static void PopulateDebugUtilsMessengerCreateInfoEXT(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = nullptr;
	}
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
		}
		return VK_FALSE;
	}


	[[nodiscard]] static VkPhysicalDevice PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
	{
		VkPhysicalDevice physicalDevice = nullptr;

		// Query available devices
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0)
		{
			throw std::runtime_error("Failed to find GPUs with Vulkan support.");
		}
		std::vector<VkPhysicalDevice> devices{ deviceCount };
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());


		// Use the first suitable physical device
		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device, surface))
			{
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == nullptr)
		{
			throw std::runtime_error("Failed to find a suitable GPU");
		}

		return physicalDevice;
	}
	static bool IsDeviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
	{
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);

		// TODO Run this on surface book and write something that selects the more powerful gpu - when attached!
		std::cout << "PhysicalDevice Name:" << properties.deviceName << " Type:" << properties.deviceType << std::endl;

		const QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
		const bool extensionsSupported = CheckPhysicalDeviceExtensionSupport(physicalDevice);

		bool swapChainAdequate = false;
		if (extensionsSupported) // CodeSmell: logical coupling of swap chain presence and extensionsSupported
		{
			auto swapChainSupport = QuerySwapChainSupport(physicalDevice, surface);
			swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
		}

		return indices.IsComplete() && extensionsSupported && swapChainAdequate;
	}
	static bool CheckPhysicalDeviceExtensionSupport(VkPhysicalDevice physicalDevice)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions{ g_physicalDeviceExtensions.begin(), g_physicalDeviceExtensions.end() };

		for (const auto& availableExt : availableExtensions)
		{
			requiredExtensions.erase(availableExt.extensionName);
		}

		return requiredExtensions.empty();
	}


	[[nodiscard]] static std::tuple<VkDevice, VkQueue, VkQueue> CreateLogicalDevice(
		VkPhysicalDevice physicalDevice,
		VkSurfaceKHR surface,
		const std::vector<const char*>& validationLayers,
		const std::vector<const char*>& physicalDeviceExtensions)
	{
		// Create QueueCreateInfo for each queue family

		QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			// Specify the queues to be created
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}


		// Specify used device features
		VkPhysicalDeviceFeatures deviceFeatures = {};


		// Prepare logical device create info
		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
		createInfo.enabledExtensionCount = (uint32_t)physicalDeviceExtensions.size();
		createInfo.ppEnabledExtensionNames = physicalDeviceExtensions.data();


		// Create the logical device and queues
		VkDevice device;
		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device.");
		}

		VkQueue graphicsQueue;
		vkGetDeviceQueue(device, indices.GraphicsFamily.value(), 0, &graphicsQueue);

		VkQueue presentQueue;
		vkGetDeviceQueue(device, indices.PresentFamily.value(), 0, &presentQueue);

		return { device, graphicsQueue, presentQueue };
	}


	[[nodiscard]] static VkSwapchainKHR
		CreateSwapchain(const VkExtent2D& windowSize, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device,
			std::vector<VkImage>& OUTswapchainImages, VkFormat& OUTswapchainImageFormat, VkExtent2D& OUTswapchainExtent)
		// TODO Remove OUT params, use tuple return
	{
		const SwapChainSupportDetails deets = QuerySwapChainSupport(physicalDevice, surface);

		const auto surfaceFormat = ChooseSwapSurfaceFormat(deets.Formats);
		const auto presentMode = ChooseSwapPresentMode(deets.PresentModes);
		const auto extent = ChooseSwapExtent(windowSize, deets.Capabilities);

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
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//VK_IMAGE_USAGE_TRANSFER_DST_BIT for post processing buffer
		info.preTransform = deets.Capabilities.currentTransform; // transform image before showing it?
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // how alpha is treated when blending with other windows
		info.presentMode = presentMode;
		info.clipped = VK_TRUE; // true means we don't care about pixels obscured by other windows
		info.oldSwapchain = nullptr;

		// Specify how to use swap chain images across multiple queue families
		QueueFamilyIndices indicies = FindQueueFamilies(physicalDevice, surface);
		const uint32_t queueCount = 2;
		// Code smell: will break as more are added to indicies
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
		uint32_t imageCount;
		vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
		OUTswapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapchain, &imageCount, OUTswapchainImages.data());
		OUTswapchainExtent = extent;
		OUTswapchainImageFormat = surfaceFormat.format;

		return swapchain;
	}
	static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		assert(!availableFormats.empty());

		for (const auto& f : availableFormats)
		{
			if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return f;
			}
		}

		return availableFormats[0];
	}
	static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
	{
		for (const auto& mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) // triple buffering
			{
				return mode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR; // use the only required format as fallback
	}
	static VkExtent2D ChooseSwapExtent(const VkExtent2D& windowSize, const VkSurfaceCapabilitiesKHR& capabilities)
	{
		const auto& c = capabilities;

		// Use currentExtent if it has not been set to the "override" value of max uint32
		const auto useExistingCurrentExtent = c.currentExtent.width != UINT32_MAX;
		if (useExistingCurrentExtent)
		{
			return c.currentExtent;
		}

		// Find the biggest extent possible
		VkExtent2D e = windowSize;
		e.width = std::max(c.minImageExtent.width, std::min(c.maxImageExtent.width, e.width));
		e.height = std::max(c.minImageExtent.height, std::min(c.maxImageExtent.height, e.height));

		return e;
	}


	[[nodiscard]] static std::vector<VkImageView> CreateImageViews(const std::vector<VkImage>& swapchainImages, VkFormat format,
		VkDevice device)
	{
		std::vector<VkImageView> imageViews{ swapchainImages.size() };

		for (size_t i = 0; i < swapchainImages.size(); ++i)
		{
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapchainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = format;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create image views");
			}
		}

		return imageViews;
	}


	// The attachments referenced by the pipeline stages and their usage
	[[nodiscard]] static VkRenderPass CreateRenderPass(VkFormat format, VkDevice device)
	{
		// Define the colour/depth buffer attachments
		VkAttachmentDescription colorAttachmentDesc = {};
		{
			colorAttachmentDesc.format = format;
			colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // what to do with color/depth data before rendering
			colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // what to do with color/depth data after rendering
			colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // not using stencil
			colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // mem layout before render pass begins
			colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // outgoing memory layout after render pass
		}
		VkAttachmentReference colorAttachmentRef = {};
		{
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		VkSubpassDescription subpassDesc = {};
		{
			subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDesc.colorAttachmentCount = 1;
			subpassDesc.pColorAttachments = &colorAttachmentRef;
		}
		// Set dependency for the implicit external subpass to wait for the swapchain to finish reading from it
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
		VkRenderPassCreateInfo renderPassCI = {};
		{
			renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassCI.attachmentCount = 1;
			renderPassCI.pAttachments = &colorAttachmentDesc;
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


	// The uniform and push values referenced by the shader that can be updated at draw time
	[[nodiscard]] static std::tuple<VkPipeline, VkPipelineLayout>
		CreateGraphicsPipeline(const std::string& shaderDir, VkRenderPass renderPass, VkDevice device,
			const VkExtent2D& swapchainExtent)
	{

		//// SHADER MODULES ////


		// Load shader stages
		VkShaderModule vertShaderModule;
		VkShaderModule fragShaderModule;
		const auto numShaders = 2;
		std::array<VkPipelineShaderStageCreateInfo, numShaders> shaderStageCIs{};
		{
			const auto vertShaderCode = ReadFile(shaderDir + "shader.vert.spv");
			const auto fragShaderCode = ReadFile(shaderDir + "shader.frag.spv");

			vertShaderModule = CreateShaderModule(vertShaderCode, device);
			fragShaderModule = CreateShaderModule(fragShaderCode, device);

			VkPipelineShaderStageCreateInfo vertCI = {};
			vertCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertCI.module = vertShaderModule;
			vertCI.pName = "main";

			VkPipelineShaderStageCreateInfo fragCI = {};
			fragCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragCI.module = fragShaderModule;
			fragCI.pName = "main";

			shaderStageCIs[0] = vertCI;
			shaderStageCIs[1] = fragCI;
		}



		//// FIXED FUNCTIONS ////

		// all of the structures that define the fixed - function stages of the pipeline, like input assembly,
		// rasterizer, viewport and color blending


		// Vertex Input  -  Define the format of the vertex data passed to the vert shader
		VkPipelineVertexInputStateCreateInfo vertexInputCI = {};
		{
			vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputCI.vertexBindingDescriptionCount = 0;
			vertexInputCI.pVertexBindingDescriptions = nullptr;
			vertexInputCI.vertexAttributeDescriptionCount = 0;
			vertexInputCI.pVertexAttributeDescriptions = nullptr;
		}


		// Input Assembly  -  What kind of geo will be drawn from the verts and whether primitive restart is enabled
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI = {};
		{
			inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
		}


		// Viewports and scissor  -  The region of the frambuffer we render output to
		VkViewport viewport = {}; // the output is stretch-fitted into these viewport bounds
		{
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float)swapchainExtent.width;
			viewport.height = (float)swapchainExtent.height;
			viewport.minDepth = 0; // depth buffer value range within [0,1]. Min can be > Max.
			viewport.maxDepth = 1;
		}
		VkRect2D scissor = {}; // scissor filters out pixels beyond these bounds
		{
			scissor.offset = { 0,0 };
			scissor.extent = swapchainExtent;
		}
		VkPipelineViewportStateCreateInfo viewportCI = {};
		{
			viewportCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportCI.viewportCount = 1;
			viewportCI.pViewports = &viewport;
			viewportCI.scissorCount = 1;
			viewportCI.pScissors = &scissor;
		}


		// Rasterizer  -  Config how geometry turns into fragments
		VkPipelineRasterizationStateCreateInfo rasterizationCI = {};
		{
			rasterizationCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationCI.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationCI.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizationCI.frontFace = VK_FRONT_FACE_CLOCKWISE;
			rasterizationCI.lineWidth = 1; // > 1 requires wideLines GPU feature
			rasterizationCI.depthBiasEnable = VK_FALSE;
			rasterizationCI.depthBiasConstantFactor = 0.0f; // optional
			rasterizationCI.depthBiasClamp = 0.0f; // optional
			rasterizationCI.depthBiasSlopeFactor = 0.0f; // optional
			rasterizationCI.depthClampEnable = VK_FALSE; // clamp depth frags beyond the near/far clip planes?
			rasterizationCI.rasterizerDiscardEnable = VK_FALSE; // stop geo from passing through the raster stage?
		}


		// Multisampling
		VkPipelineMultisampleStateCreateInfo multisampleCI = {};
		{
			multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleCI.sampleShadingEnable = VK_FALSE;
			multisampleCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampleCI.minSampleShading = 1; // optional
			multisampleCI.pSampleMask = nullptr; // Optional
			multisampleCI.alphaToCoverageEnable = VK_FALSE; // Optional
			multisampleCI.alphaToOneEnable = VK_FALSE; // Optional
		}


		// TODO Depth and Stencil testing
		//VkPipelineDepthStencilStateCreateInfo depthStencilCI = {};
		//{
		//	depthStencilCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		//	// ...
		//	// ...
		//	// ...
		//}


		// Color Blending  -  How colors output from frag shader are combined with existing colors
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {}; // Mix old with new to create a final color
		{
			colorBlendAttachment.blendEnable = VK_FALSE;
			colorBlendAttachment.colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		}
		VkPipelineColorBlendStateCreateInfo colorBlendCI = {}; // Combine old and new with a bitwise operation
		{
			colorBlendCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendCI.logicOpEnable = VK_FALSE;
			colorBlendCI.logicOp = VK_LOGIC_OP_COPY; // Optional
			colorBlendCI.attachmentCount = 1;
			colorBlendCI.pAttachments = &colorBlendAttachment;
			colorBlendCI.blendConstants[0] = 0.0f; // Optional
			colorBlendCI.blendConstants[1] = 0.0f; // Optional
			colorBlendCI.blendConstants[2] = 0.0f; // Optional
			colorBlendCI.blendConstants[3] = 0.0f; // Optional
		}


		// Dynamic State  -  Set which states can be changed without recreating the pipeline. Must be set at draw time
		//std::array<VkDynamicState,1> dynamicStates =
		//{
		//	VK_DYNAMIC_STATE_VIEWPORT,
		//	//VK_DYNAMIC_STATE_LINE_WIDTH,
		//};
		//VkPipelineDynamicStateCreateInfo dynamicStateCI = {};
		//{
		//	dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		//	dynamicStateCI.dynamicStateCount = (uint32_t)dynamicStates.size();
		//	dynamicStateCI.pDynamicStates = dynamicStates.data();
		//}


		// Create Pipeline Layout  -  Used to pass uniforms to shaders at runtime
		VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
		{
			pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCI.setLayoutCount = 0;
			pipelineLayoutCI.pSetLayouts = nullptr;
			pipelineLayoutCI.pushConstantRangeCount = 0;
			pipelineLayoutCI.pPushConstantRanges = nullptr;
		}

		VkPipelineLayout pipelineLayout = nullptr;
		if (vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to Create Pipeline Layout!");
		}



		// Create the Pipeline  -  Finally!...
		VkGraphicsPipelineCreateInfo graphicsPipelineCI = {};
		{
			graphicsPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

			// Programmable
			graphicsPipelineCI.stageCount = (uint32_t)shaderStageCIs.size();
			graphicsPipelineCI.pStages = shaderStageCIs.data();

			// Fixed function
			graphicsPipelineCI.pVertexInputState = &vertexInputCI;
			graphicsPipelineCI.pInputAssemblyState = &inputAssemblyCI;
			graphicsPipelineCI.pViewportState = &viewportCI;
			graphicsPipelineCI.pRasterizationState = &rasterizationCI;
			graphicsPipelineCI.pMultisampleState = &multisampleCI;
			graphicsPipelineCI.pDepthStencilState = nullptr;
			graphicsPipelineCI.pColorBlendState = &colorBlendCI;
			graphicsPipelineCI.pDynamicState = nullptr;

			graphicsPipelineCI.layout = pipelineLayout;

			graphicsPipelineCI.renderPass = renderPass;
			graphicsPipelineCI.subpass = 0;

			graphicsPipelineCI.basePipelineHandle = VK_NULL_HANDLE; // is our pipeline derived from another?
			graphicsPipelineCI.basePipelineIndex = -1;
		}
		VkPipeline pipeline;
		if (vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCI, nullptr, &pipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Pipeline");
		}


		// Cleanup
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
		vkDestroyShaderModule(device, fragShaderModule, nullptr);

		return { pipeline, pipelineLayout };
	}
	static VkShaderModule CreateShaderModule(const std::vector<char>& code, VkDevice device)
	{
		VkShaderModuleCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = code.size();
		ci.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &ci, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module");
		}

		return shaderModule;
	}


	[[nodiscard]] static std::vector<VkFramebuffer> CreateFramebuffer(
		VkDevice device,
		VkRenderPass renderPass,
		const VkExtent2D& swapchainExtent,
		const std::vector<VkImageView>& swapchainImageViews)
	{
		std::vector<VkFramebuffer> swapchainFramebuffers{ swapchainImageViews.size() };

		for (size_t i = 0; i < swapchainImageViews.size(); ++i)
		{
			VkImageView attachments[] = { swapchainImageViews[i] };

			VkFramebufferCreateInfo framebufferCI = {};
			{
				framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferCI.renderPass = renderPass;
				framebufferCI.attachmentCount = 1;
				framebufferCI.pAttachments = attachments;
				framebufferCI.width = swapchainExtent.width;
				framebufferCI.height = swapchainExtent.height;
				framebufferCI.layers = 1;

				if (vkCreateFramebuffer(device, &framebufferCI, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS)
				{
					throw std::runtime_error("Failed to reate Framebuffer");
				}
			}
		}

		return swapchainFramebuffers;
	}


	[[nodiscard]] static VkCommandPool CreateCommandPool(QueueFamilyIndices queueFamilyIndices, VkDevice device)
	{
		VkCommandPoolCreateInfo commandPoolCI = {};
		{
			commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			commandPoolCI.queueFamilyIndex = queueFamilyIndices.GraphicsFamily.value();
			commandPoolCI.flags = 0;
		}
		VkCommandPool commandPool;
		if (vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Command Pool");
		}

		return commandPool;
	}


	[[nodiscard]] static std::vector<VkCommandBuffer> CreateCommandBuffers(
		uint32_t numBuffers,
		VkCommandPool commandPool,
		VkDevice device,
		VkRenderPass renderPass,
		VkExtent2D swapchainExtent,
		VkPipeline pipeline,
		const std::vector<VkFramebuffer>& swapchainFramebuffers)
	{
		std::vector<VkCommandBuffer> commandBuffers{ numBuffers };


		// Allocate command buffer
		VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
		{
			commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocInfo.commandPool = commandPool;
			commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
		}

		if (vkAllocateCommandBuffers(device, &commandBufferAllocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate Command Buffers");
		}


		// Record command buffer
		for (size_t i = 0; i < commandBuffers.size(); ++i)
		{
			VkCommandBufferBeginInfo beginInfo = {};
			{
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = 0;
				beginInfo.pInheritanceInfo = nullptr;
			}
			VkRenderPassBeginInfo renderPassBeginInfo = {};
			{
				renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassBeginInfo.renderPass = renderPass;
				renderPassBeginInfo.framebuffer = swapchainFramebuffers[i];
				renderPassBeginInfo.renderArea.offset = { 0,0 };
				renderPassBeginInfo.renderArea.extent = swapchainExtent;
				VkClearValue clearColor = { 0.f,0.f,0.f,1.f };
				renderPassBeginInfo.clearValueCount = 1;
				renderPassBeginInfo.pClearValues = &clearColor;
			}


			// Begin recording renderpass
			if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to begin recording command buffer");
			}

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); // TODO Ewww....
			vkCmdEndRenderPass(commandBuffers[i]);

			// Finish up!
			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to end recording command buffer");
			}
		}


		return commandBuffers;
	}


	// Returns render finished semaphore and image available semaphore, in that order
	[[nodiscard]]
	static std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>, std::vector<VkFence>>
		CreateSyncObjects(size_t numFramesInFlight, size_t numSwapchainImages, VkDevice device)
	{
		VkSemaphoreCreateInfo semaphoreCI = {};
		{
			semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		}
		VkFenceCreateInfo fenceCI = {};
		{
			fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT; // set signaled so we aren't locked while waiting on first frame
		}

		std::vector<VkSemaphore> renderFinishedSemaphores{ numFramesInFlight };
		std::vector<VkSemaphore> imageAvailableSemaphores{ numFramesInFlight };
		std::vector<VkFence> inFlightFences{ numFramesInFlight };
		std::vector<VkFence> framesInFlight{ numSwapchainImages, nullptr };

		for (size_t i = 0; i < numFramesInFlight; i++)
		{
			if (vkCreateSemaphore(device, &semaphoreCI, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceCI, nullptr, &inFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create semaphores for a frame");
			}
		}


		return { renderFinishedSemaphores, imageAvailableSemaphores, inFlightFences, framesInFlight };
	}

	#pragma endregion


	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = (VulkanTutorial*)glfwGetWindowUserPointer(window);
		app->FramebufferResized = true;
	}
};

