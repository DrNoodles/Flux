
#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>
#include <vector>
#include <optional>
#include <set>

/////// GLOBALS ///////////////////////////////////////////////////////////////////////////////////////////////////////
const int g_width = 800;
const int g_height = 600;
const std::vector<const char*> g_validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef DEBUG
	const bool g_enableValidationLayers = true;
#else
	const bool g_enableValidationLayers = false;
#endif


bool CheckValidationLayerSupport()
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

std::vector<const char*> GetRequiredExtensions(bool enableValidationLayers)
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


struct QueueFamilyIndices
{
	std::optional<uint32_t> GraphicsFamily = std::nullopt;
	std::optional<uint32_t> PresentFamily = std::nullopt;

	bool IsComplete() const
	{
		return GraphicsFamily.has_value() && PresentFamily.has_value();
	}
};
QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies{ queueFamilyCount };
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	// This code is stupid (vulkan-tutorial.com... wtf).
	// It takes the last found queue that has a graphics bit. Not sure why it doesn't break after the first find...
	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.GraphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
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
		if (enableValidationLayers) _debugMessenger = SetupDebugMessenger(_instance);
		_surface = CreateSurface(_instance, _window);
		_physicalDevice = PickPhysicalDevice(_instance, _surface);
		std::tie(_device, _graphicsQueue, _presentQueue) = CreateLogicalDevice(_physicalDevice, g_validationLayers, _surface);
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


		// Helper function to determine device suitability
		auto IsDeviceSuitable = [](VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			VkPhysicalDeviceProperties properties;
			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceProperties(device, &properties);
			vkGetPhysicalDeviceFeatures(device, &features);

			const QueueFamilyIndices indices = FindQueueFamilies(device, surface);
			
			
			// TODO Run this on surface book and write something that selects the more powerful gpu - when attached!
			std::cout << "PhysicalDevice Name:" << properties.deviceName << " Type:" << properties.deviceType << std::endl;
			
			return indices.IsComplete();
		};


		
		// Use the first suitable device
		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device, surface))
			{
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("Failed to find a suitable GPU");
		}

		return physicalDevice;
	}
	
	[[nodiscard]] static std::tuple<VkDevice, VkQueue, VkQueue> CreateLogicalDevice(VkPhysicalDevice physicalDevice,
		const std::vector<const char*>& validationLayers, VkSurfaceKHR surface)
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


		// Create the logical device
		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();

		
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
