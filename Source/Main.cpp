
#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>

#define OUT // syntax helper

/////// GLOBALS ///////////////////////////////////////////////////////////////////////////////////////////////////////
const int g_width = 800;
const int g_height = 600;
const std::vector<const char*> g_validationLayers = {
	"VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> g_physicalDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef DEBUG
	const bool g_enableValidationLayers = true;
#else
	const bool g_enableValidationLayers = false;
#endif


struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR Capabilities{};
	std::vector<VkSurfaceFormatKHR> Formats{};
	std::vector<VkPresentModeKHR> PresentModes{};
};

struct QueueFamilyIndices
{
	std::optional<uint32_t> GraphicsFamily = std::nullopt;
	std::optional<uint32_t> PresentFamily = std::nullopt;

	bool IsComplete() const
	{
		return GraphicsFamily.has_value() && PresentFamily.has_value();
	}
};
QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies{ queueFamilyCount };
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	// This code is stupid (vulkan-tutorial.com... wtf).
	// It takes the last found queue that has a graphics bit. Not sure why it doesn't break after the first find...
	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.GraphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
		if (presentSupport)
		{
			indices.PresentFamily = i;
		}

		if (indices.IsComplete())
		{
			break;
		}
	}
	
	return indices;
}
SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	SwapChainSupportDetails deets;

	// Query Capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &deets.Capabilities);

	
	// Query formats
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	if (formatCount != 0)
	{
		deets.Formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, deets.Formats.data());
	}


	// Query present modes
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0)
	{
		deets.PresentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, deets.PresentModes.data());
	}

	return deets;
}


VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, 
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	const auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	return func ? func(instance, pCreateInfo, pAllocator, pDebugMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, 
	VkDebugUtilsMessengerEXT messenger, 
	const VkAllocationCallbacks* pAllocator)
{
	const auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func) func(instance, messenger, pAllocator);
}



/////// CLASS HELLOTRIANGLEAPPLICATION ////////////////////////////////////////////////////////////////////////////////
class HelloTriangleApplication
{
public:
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
	
	std::vector<VkImage> _swapchainImages{};
	VkFormat _swapchainImageFormat;
	VkExtent2D _swapchainExtent;

	
	void InitWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't use opengl
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // we'll handle resizing
		_window = glfwCreateWindow(g_width, g_height, "Vulkan", nullptr, nullptr);
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

		_swapchain = CreateSwapchain(_physicalDevice, _surface, _device, 
			OUT _swapchainImages, OUT _swapchainImageFormat, OUT _swapchainExtent);
	}
	
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
			throw std::runtime_error{"Failed to create Vulkan Instance"};
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
		std::vector<VkPhysicalDevice> devices{deviceCount};
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
		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device))
		{
			throw std::runtime_error("Failed to create logical device.");
		}
		
		VkQueue graphicsQueue;
		vkGetDeviceQueue(device, indices.GraphicsFamily.value(), 0, &graphicsQueue);

		VkQueue presentQueue;
		vkGetDeviceQueue(device, indices.PresentFamily.value(), 0, &presentQueue);
		
		return { device, graphicsQueue, presentQueue };
	}

	[[nodiscard]] static VkSwapchainKHR CreateSwapchain(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, 
		VkDevice device, 
		std::vector<VkImage>& OUTswapchainImages, VkFormat& OUTswapchainImageFormat, VkExtent2D& OUTswapchainExtent)
	// TODO Remove OUT params, use tuple return
	{
		const SwapChainSupportDetails deets = QuerySwapChainSupport(physicalDevice, surface);

		const auto surfaceFormat = ChooseSwapSurfaceFormat(deets.Formats);
		const auto presentMode = ChooseSwapPresentMode(deets.PresentModes);
		const auto extent = ChooseSwapExtent(deets.Capabilities);

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
		if (vkCreateSwapchainKHR(device, &info, nullptr, &swapchain))
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
	static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		const auto& c = capabilities;

		// Use currentExtent if it has not been set to the "override" value of max uint32
		const auto useExistingCurrentExtent = c.currentExtent.width != UINT32_MAX;
		if (useExistingCurrentExtent)
		{
			return c.currentExtent;
		}

		// Find the biggest extent possible
		VkExtent2D e = { g_width, g_height };
		e.width = std::max(c.minImageExtent.width, std::min(c.maxImageExtent.width, e.width));
		e.height = std::max(c.minImageExtent.height, std::min(c.maxImageExtent.height, e.height));

		return e;
	}

	
	void MainLoop()
	{
		while (!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();
		}
	}

	
	void CleanUp(bool enableValidationLayers)
	{
		if (enableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
		}

		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
		vkDestroyDevice(_device, nullptr);
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyInstance(_instance, nullptr);
		glfwDestroyWindow(_window);
		glfwTerminate();
	}
};








//// MAIN /////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main()
{
	HelloTriangleApplication app;

	try
	{
		app.Run();
	}
	catch (const std::exception & e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
