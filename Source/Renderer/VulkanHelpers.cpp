#include "VulkanHelpers.h"
#include "UniformBufferObjects.h"
#include "App/IModelLoaderService.h"
#include "Renderable.h"

#include <Shared/FileService.h>

#include <stbi/stb_image.h>
#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // to comply with vulkan

#include <iostream>
#include <set>


VkInstance VulkanHelpers::CreateInstance(bool enableValidationLayers, const std::vector<const char*>& validationLayers)
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
		if (!CheckValidationLayerSupport(validationLayers))
		{
			throw std::runtime_error("Validation layers requested but not available");
		}

		createInfo.enabledLayerCount = uint32_t(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

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

bool VulkanHelpers::CheckValidationLayerSupport(const std::vector<const char*>& validationLayers)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers{ layerCount };
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* requestedLayer : validationLayers)
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

std::vector<const char*> VulkanHelpers::GetRequiredExtensions(bool enableValidationLayers)
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

VkSurfaceKHR VulkanHelpers::CreateSurface(VkInstance instance, GLFWwindow* window)
{
	VkSurfaceKHR surface;
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface");
	}
	return surface;
}

VkDebugUtilsMessengerEXT VulkanHelpers::SetupDebugMessenger(VkInstance instance)
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

void VulkanHelpers::PopulateDebugUtilsMessengerCreateInfoEXT(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
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

VkBool32 VulkanHelpers::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
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

std::tuple<VkPhysicalDevice, VkSampleCountFlagBits> VulkanHelpers::PickPhysicalDevice(
	const std::vector<const char*>& physicalDeviceExtensions, VkInstance instance, VkSurfaceKHR surface)
{
	VkPhysicalDevice physicalDevice = nullptr;
	VkSampleCountFlagBits msaaSamples;

	// Query available devices
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		throw std::runtime_error("Failed to find GPUs with Vulkan support.");
	}
	std::vector<VkPhysicalDevice> physicalDevices{ deviceCount };
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());


	// Use the first suitable physical device
	for (const auto& pd : physicalDevices)
	{
		if (IsDeviceSuitable(physicalDeviceExtensions, pd, surface))
		{
			physicalDevice = pd;
			msaaSamples = GetMaxUsableSampleCount(pd);
			std::cout << "MSAAx" << std::to_string(msaaSamples) << std::endl;
			break;
		}
	}

	if (physicalDevice == nullptr)
	{
		throw std::runtime_error("Failed to find a suitable GPU");
	}

	return { physicalDevice, msaaSamples };
}

bool VulkanHelpers::IsDeviceSuitable(const std::vector<const char*>& physicalDeviceExtensions,
	VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	// TODO Run this on surface book and write something that selects the more powerful gpu - when attached!


	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	std::cout << "PhysicalDevice Name:" << properties.deviceName << " Type:" << properties.deviceType << std::endl;


	// Check it has all necessary queues
	const QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);


	// Check physical device features
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(physicalDevice, &features);


	// Check physical device extension support
	const bool extensionsAreAdequate = CheckPhysicalDeviceExtensionSupport(physicalDeviceExtensions, physicalDevice);
	bool swapchainIsAdequate = false;
	if (extensionsAreAdequate) // CodeSmell: logical coupling of swap chain presence and extensionsSupported
	{
		auto swapChainSupport = QuerySwapChainSupport(physicalDevice, surface);
		swapchainIsAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
	}


	// Does it cut the mustard?!
	return indices.IsComplete() && extensionsAreAdequate && swapchainIsAdequate && features.samplerAnisotropy;
}

bool VulkanHelpers::CheckPhysicalDeviceExtensionSupport(const std::vector<const char*>& physicalDeviceExtensions,
	VkPhysicalDevice physicalDevice)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions{ physicalDeviceExtensions.begin(), physicalDeviceExtensions.end() };

	for (const auto& availableExt : availableExtensions)
	{
		requiredExtensions.erase(availableExt.extensionName);
	}

	return requiredExtensions.empty();
}

VkSampleCountFlagBits VulkanHelpers::GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(physicalDevice, &props);

	const VkSampleCountFlags counts =
		props.limits.sampledImageColorSampleCounts &
		props.limits.sampledImageDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	return VK_SAMPLE_COUNT_1_BIT;
}

std::tuple<VkDevice, VkQueue, VkQueue> VulkanHelpers::CreateLogicalDevice(VkPhysicalDevice physicalDevice,
	VkSurfaceKHR surface,
	const std::vector<const char*>&
	validationLayers,
	const std::vector<const char*>&
	physicalDeviceExtensions)
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
	{
		deviceFeatures.samplerAnisotropy = true;
	}


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

VkSwapchainKHR VulkanHelpers::CreateSwapchain(const VkExtent2D& windowSize, VkPhysicalDevice physicalDevice,
	VkSurfaceKHR surface, VkDevice device,
	std::vector<VkImage>& OUTswapchainImages,
	VkFormat& OUTswapchainImageFormat,
	VkExtent2D& OUTswapchainExtent)
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
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //VK_IMAGE_USAGE_TRANSFER_DST_BIT for post processing buffer
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

VkSurfaceFormatKHR VulkanHelpers::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
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

VkPresentModeKHR VulkanHelpers::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
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

VkExtent2D VulkanHelpers::ChooseSwapExtent(const VkExtent2D& windowSize, const VkSurfaceCapabilitiesKHR& capabilities)
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

VkRenderPass VulkanHelpers::CreateRenderPass(VkSampleCountFlagBits msaaSamples, VkFormat swapchainFormat,
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
		depthAttachmentDesc.format = FindDepthFormat(physicalDevice);
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
	std::array<VkAttachmentDescription, 3> attachments = {
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

std::tuple<VkPipeline, VkPipelineLayout> VulkanHelpers::CreateGraphicsPipeline(const std::string& shaderDir,
	VkDescriptorSetLayout&
	descriptorSetLayout,
	VkSampleCountFlagBits msaaSamples,
	VkRenderPass renderPass,
	VkDevice device,
	const VkExtent2D& swapchainExtent)
{
	//// SHADER MODULES ////


	// Load shader stages
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	const auto numShaders = 2;
	std::array<VkPipelineShaderStageCreateInfo, numShaders> shaderStageCIs{};
	{
		const auto vertShaderCode = FileService::ReadFile(shaderDir + "shader.vert.spv");
		const auto fragShaderCode = FileService::ReadFile(shaderDir + "shader.frag.spv");

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
	auto vertBindingDesc = Vertex::BindingDescription();
	auto vertAttrDesc = Vertex::AttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputCI = {};
	{
		vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCI.vertexBindingDescriptionCount = 1;
		vertexInputCI.pVertexBindingDescriptions = &vertBindingDesc;
		vertexInputCI.vertexAttributeDescriptionCount = (uint32_t)vertAttrDesc.size();
		vertexInputCI.pVertexAttributeDescriptions = vertAttrDesc.data();
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
		scissor.offset = { 0, 0 };
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
		rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
		multisampleCI.rasterizationSamples = msaaSamples;
		multisampleCI.sampleShadingEnable = VK_FALSE;
		multisampleCI.minSampleShading = 1; // optional
		multisampleCI.pSampleMask = nullptr; // optional
		multisampleCI.alphaToCoverageEnable = VK_FALSE; // optional
		multisampleCI.alphaToOneEnable = VK_FALSE; // optional
	}


	// Depth and Stencil testing
	VkPipelineDepthStencilStateCreateInfo depthStencilCI = {};
	{
		depthStencilCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilCI.depthTestEnable = true; // should compare new frags against depth to determine if discarding?
		depthStencilCI.depthWriteEnable = true; // can new depth tests wrhite to buffer?
		depthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS;

		depthStencilCI.depthBoundsTestEnable = false; // optional test to keep only frags within a set bounds
		depthStencilCI.minDepthBounds = 0; // optional
		depthStencilCI.maxDepthBounds = 0; // optional

		depthStencilCI.stencilTestEnable = false;
		depthStencilCI.front = {}; // optional
		depthStencilCI.back = {}; // optional
	}


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
		pipelineLayoutCI.setLayoutCount = 1;
		pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;
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
		graphicsPipelineCI.pDepthStencilState = &depthStencilCI;
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

VkShaderModule VulkanHelpers::CreateShaderModule(const std::vector<char>& code, VkDevice device)
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

std::vector<VkFramebuffer> VulkanHelpers::CreateFramebuffer(VkImageView colorImageView, VkImageView depthImageView,
	VkDevice device, VkRenderPass renderPass,
	const VkExtent2D& swapchainExtent,
	const std::vector<VkImageView>& swapchainImageViews)
{
	std::vector<VkFramebuffer> swapchainFramebuffers{ swapchainImageViews.size() };

	for (size_t i = 0; i < swapchainImageViews.size(); ++i)
	{
		std::array<VkImageView, 3> attachments = { colorImageView, depthImageView, swapchainImageViews[i] };

		VkFramebufferCreateInfo framebufferCI = {};
		{
			framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCI.renderPass = renderPass;
			framebufferCI.attachmentCount = (uint32_t)attachments.size();
			framebufferCI.pAttachments = attachments.data();
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

VkCommandPool VulkanHelpers::CreateCommandPool(QueueFamilyIndices queueFamilyIndices, VkDevice device)
{
	VkCommandPoolCreateInfo commandPoolCI = {};
	{
		commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCI.queueFamilyIndex = queueFamilyIndices.GraphicsFamily.value();
		commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	}
	VkCommandPool commandPool;
	if (vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Command Pool");
	}

	return commandPool;
}

std::tuple<VkBuffer, VkDeviceMemory> VulkanHelpers::CreateVertexBuffer(const std::vector<Vertex>& vertices,
	VkQueue transferQueue,
	VkCommandPool transferCommandPool,
	VkPhysicalDevice physicalDevice, VkDevice device)
{
	const VkDeviceSize bufSize = sizeof(vertices[0]) * vertices.size();

	// Create temp staging buffer - to copy vertices from system mem to gpu mem
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	std::tie(stagingBuffer, stagingBufferMemory) = CreateBuffer(bufSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage flags
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, // property flags
		device, physicalDevice);


	// Load vertex data into staging buffer
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufSize, 0, &data);
	memcpy(data, vertices.data(), bufSize);
	vkUnmapMemory(device, stagingBufferMemory);


	// Create vertex buffer - with optimal memory speeds
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	std::tie(vertexBuffer, vertexBufferMemory) = CreateBuffer(bufSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, // usage flags
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // property flags
		device, physicalDevice);


	// Copy from staging buffer to vertex buffer
	CopyBuffer(stagingBuffer, vertexBuffer, bufSize, transferCommandPool, transferQueue, device);


	// Cleanup temp buffer
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);


	return { vertexBuffer, vertexBufferMemory };
}

std::tuple<VkBuffer, VkDeviceMemory> VulkanHelpers::CreateIndexBuffer(const std::vector<uint32_t>& indices,
	VkQueue transferQueue,
	VkCommandPool transferCommandPool,
	VkPhysicalDevice physicalDevice, VkDevice device)
{
	const VkDeviceSize bufSize = sizeof(indices[0]) * indices.size();

	// Create temp staging buffer - to copy indices from system mem to gpu mem
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	std::tie(stagingBuffer, stagingBufferMemory) = CreateBuffer(bufSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage flags
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, // property flags
		device, physicalDevice);

	// Load vertex data into staging buffer
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufSize, 0, &data);
	memcpy(data, indices.data(), bufSize);
	vkUnmapMemory(device, stagingBufferMemory);

	// Create index buffer - with optimal memory speeds
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	std::tie(indexBuffer, indexBufferMemory) = CreateBuffer(bufSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, // usage flags
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // property flags
		device, physicalDevice);

	// Copy from staging buffer to vertex buffer
	CopyBuffer(stagingBuffer, indexBuffer, bufSize, transferCommandPool, transferQueue, device);

	// Cleanup temp buffer
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);

	return { indexBuffer, indexBufferMemory };
}

std::tuple<VkBuffer, VkDeviceMemory> VulkanHelpers::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags,
	VkMemoryPropertyFlags propertyFlags, VkDevice device,
	VkPhysicalDevice physicalDevice)
{
	VkBuffer outBuffer;
	VkDeviceMemory outBufferMemory;

	VkBufferCreateInfo bufferCI = {};
	{
		bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCI.size = size;
		bufferCI.usage = usageFlags;
		bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	if (vkCreateBuffer(device, &bufferCI, nullptr, &outBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vertex buffer");
	}


	// Memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, outBuffer, &memRequirements);


	// Allocate buffer memory
	VkMemoryAllocateInfo memoryAllocInfo = {};
	{
		memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocInfo.allocationSize = memRequirements.size;
		memoryAllocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, propertyFlags, physicalDevice);
	}

	if (vkAllocateMemory(device, &memoryAllocInfo, nullptr, &outBufferMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory");
	}


	// Associate buffer memory with the buffer
	vkBindBufferMemory(device, outBuffer, outBufferMemory, 0);

	return { outBuffer, outBufferMemory };
}

uint32_t VulkanHelpers::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags,
	VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceMemoryProperties physicalMemProps;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalMemProps);

	// Find a mem type that's suitable
	for (uint32_t i = 0; i < physicalMemProps.memoryTypeCount; i++)
	{
		const bool flagExists = typeFilter & (1 << i);
		const bool propertiesMatch = (physicalMemProps.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags;

		if (flagExists && propertiesMatch)
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find a suitable memory type");
}

void VulkanHelpers::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize,
	VkCommandPool transferCommandPool, VkQueue transferQueue, VkDevice device)
{
	const auto commandBuffer = BeginSingleTimeCommands(transferCommandPool, device);

	BeginSingleTimeCommands(transferCommandPool, device);

	VkBufferCopy copyRegion = {};
	{
		copyRegion.size = bufferSize;
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
	}
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	EndSingeTimeCommands(commandBuffer, transferCommandPool, transferQueue, device);
}

void VulkanHelpers::CopyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height,
	VkCommandPool transferCommandPool, VkQueue transferQueue, VkDevice device)
{
	const auto commandBuffer = BeginSingleTimeCommands(transferCommandPool, device);

	BeginSingleTimeCommands(transferCommandPool, device);

	VkBufferImageCopy region = {};
	{
		// buffer params define any padding around the image. 0 is tightly packed.
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		// subresource, offset and extent indicate which part of the image we want to copy from
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };
	}
	vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // assuming pixels are already in optimal layout
		1, &region);

	EndSingeTimeCommands(commandBuffer, transferCommandPool, transferQueue, device);
}

void VulkanHelpers::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
	VkImageLayout newLayout, uint32_t mipLevels,
	VkCommandPool transferCommandPool, VkQueue transferQueue,
	VkDevice device)
{
	// Setup barrier before transitioning image layout
	VkImageMemoryBarrier barrier = {};
	{
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // these are used when transferring queue families
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = // Defined below - subresource range defines which parts of..  
			barrier.subresourceRange.baseMipLevel = 0; // ..the image are affected.
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0; // Defined below - which operations to wait on before the barrier
		barrier.dstAccessMask = 0; // Defined below - which operations will wait this the barrier

		// Set aspectMask
		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (HasStencilComponent(format))
			{
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}


	// Define barriers and pipeline stages for supported transitions - https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPipelineStageFlagBits.html
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = // read & write depth
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		throw std::invalid_argument(
			"Unsupported layout transition: " + std::to_string(oldLayout) + " > " + std::to_string(newLayout) + "\n");
	}


	// Execute transition
	const auto commandBuffer = BeginSingleTimeCommands(transferCommandPool, device);

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr, // mem barriers
		0, nullptr, // buffer barriers
		1, &barrier); // image barriers

	EndSingeTimeCommands(commandBuffer, transferCommandPool, transferQueue, device);
}

VkCommandBuffer VulkanHelpers::BeginSingleTimeCommands(VkCommandPool transferCommandPool, VkDevice device)
{
	// Allocate temp command buffer
	VkCommandBufferAllocateInfo allocInfo = {};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = transferCommandPool;
		allocInfo.commandBufferCount = 1;
	}

	VkCommandBuffer commandBuffer;
	if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate temp command buffer for buffer copying");
	}


	// Record commands to copy memory from src to dst
	VkCommandBufferBeginInfo beginInfo = {};
	{
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to begin recording command buffer");
	}

	return commandBuffer;
}

void VulkanHelpers::EndSingeTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool transferCommandPool,
	VkQueue transferQueue, VkDevice device)
{
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to end recording command buffer");
	}


	// Execute and wait for copy command
	VkSubmitInfo submitInfo = {};
	{
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
	}
	vkQueueSubmit(transferQueue, 1, &submitInfo, nullptr);
	vkQueueWaitIdle(transferQueue);


	// Cleanup
	vkFreeCommandBuffers(device, transferCommandPool, 1, &commandBuffer);
}

std::vector<VkCommandBuffer> VulkanHelpers::CreateCommandBuffers(uint32_t numBuffersToCreate,
	const std::vector<std::unique_ptr<Renderable>>& renderables,
	const std::vector<std::unique_ptr<MeshResource>>& meshes,
	VkExtent2D swapchainExtent,
	const std::vector<VkFramebuffer>&
	swapchainFramebuffers, VkCommandPool commandPool,
	VkDevice device,
	VkRenderPass renderPass, VkPipeline pipeline,
	VkPipelineLayout pipelineLayout)
{
	assert(numBuffersToCreate == swapchainFramebuffers.size());
	//assert(numBuffersToCreate == descriptorSets.size());

	// Allocate command buffer
	VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
	{
		commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocInfo.commandPool = commandPool;
		commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocInfo.commandBufferCount = numBuffersToCreate;
	}

	std::vector<VkCommandBuffer> commandBuffers{ numBuffersToCreate };
	if (vkAllocateCommandBuffers(device, &commandBufferAllocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Command Buffers");
	}

	// TODO Is recording necessary as part of the create? We're rebuilding this per frame anyways
	for (uint32_t i = 0; i < commandBuffers.size(); ++i)
	{
		RecordCommandBuffer(
			commandBuffers[i],
			renderables,
			meshes,
			i,
			swapchainExtent,
			swapchainFramebuffers[i],
			renderPass,
			pipeline, pipelineLayout);
	}

	return commandBuffers;
}

void VulkanHelpers::RecordCommandBuffer(VkCommandBuffer commandBuffer,
	const std::vector<std::unique_ptr<Renderable>>& renderables,
	const std::vector<std::unique_ptr<MeshResource>>& meshes,
	int frameIndex,
	VkExtent2D swapchainExtent,
	VkFramebuffer swapchainFramebuffer, VkRenderPass renderPass,
	VkPipeline pipeline, VkPipelineLayout pipelineLayout)
{
	// Start recording command buffer
	VkCommandBufferBeginInfo beginInfo = {};
	{
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;
	}

	// Start command buffer
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) == VK_SUCCESS)
	{
		// Record renderpass
		std::array<VkClearValue, 2> clearColors = {};
		clearColors[0].color = { 0.f, 0.f, 0.f, 1.f };
		clearColors[1].depthStencil = { 1.f, 0ui32 }; //depth, stencil

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		{
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = swapchainFramebuffer;
			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent = swapchainExtent;

			renderPassBeginInfo.clearValueCount = (uint32_t)clearColors.size();
			renderPassBeginInfo.pClearValues = clearColors.data();
		}

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			for (const auto& renderable : renderables)
			{
				const auto& mesh = *meshes[renderable->MeshId.Id];

				// Draw mesh
				VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
					&renderable->FrameResources[frameIndex].DescriptorSet, 0, nullptr);
				/*const void* pValues;
				vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 1, pValues);*/
				vkCmdDrawIndexed(commandBuffer, (uint32_t)mesh.IndexCount, 1, 0, 0, 0);
			}
		}
		vkCmdEndRenderPass(commandBuffer);
	}
	else
	{
		throw std::runtime_error("Failed to begin recording command buffer");
	}


	// End command buffer
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to end recording command buffer");
	}
}

std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>, std::vector<VkFence>> VulkanHelpers
::CreateSyncObjects(size_t numFramesInFlight, size_t numSwapchainImages, VkDevice device)
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
	std::vector<VkFence> imagesInFlight{ numSwapchainImages, nullptr };

	for (size_t i = 0; i < numFramesInFlight; i++)
	{
		if (vkCreateSemaphore(device, &semaphoreCI, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fenceCI, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create semaphores for a frame");
		}
	}


	return { renderFinishedSemaphores, imageAvailableSemaphores, inFlightFences, imagesInFlight };
}

VkDescriptorSetLayout VulkanHelpers::CreateDescriptorSetLayout(VkDevice device)
{
	// Prepare layout bindings
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	{
		uboLayoutBinding.binding = 0; // correlates to shader
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		// TODO Is it more effecient to create a buffer per stage?
		uboLayoutBinding.pImmutableSamplers = nullptr; // not used, only useful for image descriptors
	}
	VkDescriptorSetLayoutBinding basecolorMapLayoutBinding = {};
	{
		basecolorMapLayoutBinding.binding = 1; // correlates to shader
		basecolorMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		basecolorMapLayoutBinding.descriptorCount = 1;
		basecolorMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		basecolorMapLayoutBinding.pImmutableSamplers = nullptr;
	}
	VkDescriptorSetLayoutBinding normalMapLayoutBinding = {};
	{
		normalMapLayoutBinding.binding = 2; // correlates to shader
		normalMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalMapLayoutBinding.descriptorCount = 1;
		normalMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		normalMapLayoutBinding.pImmutableSamplers = nullptr;
	}
	VkDescriptorSetLayoutBinding roughnessMapLayoutBinding = {};
	{
		roughnessMapLayoutBinding.binding = 3; // correlates to shader
		roughnessMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		roughnessMapLayoutBinding.descriptorCount = 1;
		roughnessMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		roughnessMapLayoutBinding.pImmutableSamplers = nullptr;
	}
	VkDescriptorSetLayoutBinding metalnessMapLayoutBinding = {};
	{
		metalnessMapLayoutBinding.binding = 4; // correlates to shader
		metalnessMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		metalnessMapLayoutBinding.descriptorCount = 1;
		metalnessMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		metalnessMapLayoutBinding.pImmutableSamplers = nullptr;
	}
	VkDescriptorSetLayoutBinding aoMapLayoutBinding = {};
	{
		aoMapLayoutBinding.binding = 5; // correlates to shader
		aoMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		aoMapLayoutBinding.descriptorCount = 1;
		aoMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		aoMapLayoutBinding.pImmutableSamplers = nullptr;
	}

	std::array<VkDescriptorSetLayoutBinding, 6> bindings = {
		uboLayoutBinding,
		basecolorMapLayoutBinding,
		normalMapLayoutBinding,
		roughnessMapLayoutBinding,
		metalnessMapLayoutBinding,
		aoMapLayoutBinding
	};


	// Create descriptor set layout
	VkDescriptorSetLayoutCreateInfo layoutCI = {};
	{
		layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCI.bindingCount = (uint32_t)bindings.size();
		layoutCI.pBindings = bindings.data();
	}

	VkDescriptorSetLayout descriptorSetLayout;
	if (vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor set layout");
	}

	return descriptorSetLayout;
}

VkDescriptorPool VulkanHelpers::CreateDescriptorPool(uint32_t count, VkDevice device)
{
	// count: max num of descriptor sets that may be allocated

	// Define which descriptor types our descriptor sets contain
	std::array<VkDescriptorPoolSize, 6> poolSizes = {};
	{
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = count;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = count;
		poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[2].descriptorCount = count;
		poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[3].descriptorCount = count;
		poolSizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[4].descriptorCount = count;
		poolSizes[5].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[5].descriptorCount = count;
	}


	// Create descriptor pool
	VkDescriptorPoolCreateInfo poolCI = {};
	{
		poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCI.poolSizeCount = (uint32_t)poolSizes.size();
		poolCI.pPoolSizes = poolSizes.data();
		poolCI.maxSets = count;
	}

	VkDescriptorPool pool;
	if (vkCreateDescriptorPool(device, &poolCI, nullptr, &pool) != VK_SUCCESS)
	{
		// TODO Handle allocation failures as it's determined normal behaviour in spec
		throw std::runtime_error("Failed to create descriptor pool!");
	}

	return pool;
}

std::vector<VkDescriptorSet> VulkanHelpers::CreateDescriptorSets(uint32_t count, VkDescriptorSetLayout layout,
	VkDescriptorPool pool,
	const std::vector<VkBuffer>& uniformBuffers,
	const TextureResource& basecolorMap,
	const TextureResource& normalMap,
	const TextureResource& roughnessMap,
	const TextureResource& metalnessMap,
	const TextureResource& aoMap,
	VkDevice device)
{
	assert(count == uniformBuffers.size());

	// Need a copy of the layout per set as they'll be index matched arrays
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ count, layout };


	// Create descriptor sets
	VkDescriptorSetAllocateInfo allocInfo = {};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = count;
		allocInfo.pSetLayouts = descriptorSetLayouts.data();
	}

	std::vector<VkDescriptorSet> descriptorSets{ count };
	if (VK_SUCCESS != vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()))
	{
		throw std::runtime_error("Failed to create descriptor sets");
	}

	std::array<VkWriteDescriptorSet, 6> descriptorWrites{};

	// Configure our new descriptor sets to point to our buffer and configured for what's in the buffer
	for (size_t i = 0; i < count; ++i)
	{
		
		// Uniform descriptor set
		VkDescriptorBufferInfo bufferInfo = {};
		{
			bufferInfo.buffer = uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniversalUbo);//sadf use same size calc for other bit of code?
		}
		{
			const auto binding = 0;
			descriptorWrites[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[binding].dstSet = descriptorSets[i];
			descriptorWrites[binding].dstBinding = binding; // correlates to shader binding
			descriptorWrites[binding].dstArrayElement = 0;
			descriptorWrites[binding].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[binding].descriptorCount = 1;
			descriptorWrites[binding].pBufferInfo = &bufferInfo; // descriptor is one of buffer, image or texelbufferview
			descriptorWrites[binding].pImageInfo = nullptr;
			descriptorWrites[binding].pTexelBufferView = nullptr;
		}


		// Basecolor Map  -  Texture image descriptor set
		VkDescriptorImageInfo baseColorInfo = {};
		{
			baseColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			baseColorInfo.imageView = basecolorMap.View;
			baseColorInfo.sampler = basecolorMap.Sampler;
		}
		{
			const auto binding = 1;
			descriptorWrites[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[binding].dstSet = descriptorSets[i];
			descriptorWrites[binding].dstBinding = binding; // correlates to shader binding
			descriptorWrites[binding].dstArrayElement = 0;
			descriptorWrites[binding].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[binding].descriptorCount = 1;
			descriptorWrites[binding].pBufferInfo = nullptr; // descriptor is one of buffer, image or texelbufferview
			descriptorWrites[binding].pImageInfo = &baseColorInfo;
			descriptorWrites[binding].pTexelBufferView = nullptr;
		}


		// Normal Map  -  Texture image descriptor set
		VkDescriptorImageInfo normalMapInfo = {};
		{
			normalMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			normalMapInfo.imageView = normalMap.View;
			normalMapInfo.sampler = normalMap.Sampler;
		}
		{
			const auto binding = 2;
			descriptorWrites[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[binding].dstSet = descriptorSets[i];
			descriptorWrites[binding].dstBinding = binding; // correlates to shader binding
			descriptorWrites[binding].dstArrayElement = 0;
			descriptorWrites[binding].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[binding].descriptorCount = 1;
			descriptorWrites[binding].pBufferInfo = nullptr; // descriptor is one of buffer, image or texelbufferview
			descriptorWrites[binding].pImageInfo = &normalMapInfo;
			descriptorWrites[binding].pTexelBufferView = nullptr;
		}


		// Roughness Map  -  Texture image descriptor set
		VkDescriptorImageInfo roughnessMapInfo = {};
		{
			roughnessMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			roughnessMapInfo.imageView = roughnessMap.View;
			roughnessMapInfo.sampler = roughnessMap.Sampler;
		}
		{
			const auto binding = 3;
			descriptorWrites[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[binding].dstSet = descriptorSets[i];
			descriptorWrites[binding].dstBinding = binding; // correlates to shader binding
			descriptorWrites[binding].dstArrayElement = 0;
			descriptorWrites[binding].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[binding].descriptorCount = 1;
			descriptorWrites[binding].pBufferInfo = nullptr; // descriptor is one of buffer, image or texelbufferview
			descriptorWrites[binding].pImageInfo = &roughnessMapInfo;
			descriptorWrites[binding].pTexelBufferView = nullptr;
		}


		// Metalness Map  -  Texture image descriptor set
		VkDescriptorImageInfo metalnessMapInfo = {};
		{
			metalnessMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			metalnessMapInfo.imageView = metalnessMap.View;
			metalnessMapInfo.sampler = metalnessMap.Sampler;
		}
		{
			const auto binding = 4;
			descriptorWrites[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[binding].dstSet = descriptorSets[i];
			descriptorWrites[binding].dstBinding = binding; // correlates to shader binding
			descriptorWrites[binding].dstArrayElement = 0;
			descriptorWrites[binding].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[binding].descriptorCount = 1;
			descriptorWrites[binding].pBufferInfo = nullptr; // descriptor is one of buffer, image or texelbufferview
			descriptorWrites[binding].pImageInfo = &metalnessMapInfo;
			descriptorWrites[binding].pTexelBufferView = nullptr;
		}


		// AO Map  -  Texture image descriptor set
		VkDescriptorImageInfo aoMapInfo = {};
		{
			aoMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			aoMapInfo.imageView = aoMap.View;
			aoMapInfo.sampler = aoMap.Sampler;
		}
		{
			const auto binding = 5;
			descriptorWrites[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[binding].dstSet = descriptorSets[i];
			descriptorWrites[binding].dstBinding = binding; // correlates to shader binding
			descriptorWrites[binding].dstArrayElement = 0;
			descriptorWrites[binding].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[binding].descriptorCount = 1;
			descriptorWrites[binding].pBufferInfo = nullptr; // descriptor is one of buffer, image or texelbufferview
			descriptorWrites[binding].pImageInfo = &aoMapInfo;
			descriptorWrites[binding].pTexelBufferView = nullptr;
		}
		
		vkUpdateDescriptorSets(device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}

	return descriptorSets;
}

std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>> VulkanHelpers::CreateUniformBuffers(size_t count,
	VkDevice device, VkPhysicalDevice physicalDevice)
{
	// Input - TODO Pass this in
	VkDeviceSize uboSize;

	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(physicalDevice, &props);
	const size_t minUboAlignment = props.limits.minUniformBufferOffsetAlignment;
	const size_t maxUboRange = props.limits.maxUniformBufferRange;

	
	if (true)
	{
		uboSize = sizeof(UniversalUbo);
	}
	else

	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(physicalDevice, &props);
		const size_t minUboAlignment = props.limits.minUniformBufferOffsetAlignment;
		auto dynamicAlignment = sizeof(UniversalUbo);
		if (minUboAlignment > 0)
		{
			dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
		}
		uboSize = dynamicAlignment;
		//uboDataDynamic.model = (glm::mat4*)alignedAlloc(bufferSize, dynamicAlignment);
	}


	
	std::vector<VkBuffer> buffers{ count };
	std::vector<VkDeviceMemory> buffersMemory{ count };

	for (size_t i = 0; i < count; ++i)
	{
		const auto usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		const auto propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		std::tie(buffers[i], buffersMemory[i])
			= CreateBuffer(uboSize, usageFlags, propertyFlags, device, physicalDevice);
	}

	return { buffers, buffersMemory };
}

std::tuple<VkImage, VkDeviceMemory> VulkanHelpers::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
	VkSampleCountFlagBits numSamples, VkFormat format,
	VkImageTiling tiling, VkImageUsageFlags usageFlags,
	VkMemoryPropertyFlags propertyFlags,
	VkPhysicalDevice physicalDevice, VkDevice device)
{
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;

	// Create image buffer
	VkImageCreateInfo imageCI = {};
	{
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.extent.width = width;
		imageCI.extent.height = height;
		imageCI.extent.depth = 1; // one-dimensional
		imageCI.mipLevels = mipLevels;
		imageCI.arrayLayers = 1; // not an array
		imageCI.format = format;
		imageCI.tiling = tiling;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //not usable by gpu and first transition discards the texels
		imageCI.usage = usageFlags;
		imageCI.samples = numSamples; // the multisampling mode
		imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	if (VK_SUCCESS != vkCreateImage(device, &imageCI, nullptr, &textureImage))
	{
		throw std::runtime_error("Failed to create image");
	}


	// Allocate image buffer memory
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, textureImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(
			memRequirements.memoryTypeBits,
			propertyFlags, // optimal reads
			physicalDevice);
	}

	if (VK_SUCCESS != vkAllocateMemory(device, &allocInfo, nullptr, &textureImageMemory))
	{
		throw std::runtime_error("Failed to allocate texture image memory");
	}

	vkBindImageMemory(device, textureImage, textureImageMemory, 0);

	return { textureImage, textureImageMemory };
}

VkImageView VulkanHelpers::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlagBits aspectFlags,
	uint32_t mipLevels, VkDevice device)
{
	VkImageView imageView;

	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = aspectFlags;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = mipLevels;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create image views");
	}

	return imageView;
}

std::vector<VkImageView> VulkanHelpers::CreateImageViews(const std::vector<VkImage>& images, VkFormat format,
	VkImageAspectFlagBits aspectFlags, uint32_t mipLevels,
	VkDevice device)
{
	std::vector<VkImageView> imageViews{ images.size() };

	for (size_t i = 0; i < images.size(); ++i)
	{
		imageViews[i] = CreateImageView(images[i], format, aspectFlags, mipLevels, device);
	}

	return imageViews;
}

void VulkanHelpers::GenerateMipmaps(VkImage image, VkFormat format, uint32_t texWidth, uint32_t texHeight,
	uint32_t mipLevels, VkCommandPool transferCommandPool, VkQueue transferQueue,
	VkDevice device,
	VkPhysicalDevice physicalDevice)
{
	// Check if device supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("Texture image format does not support linear blitting!");
	}


	auto commandBuffer = BeginSingleTimeCommands(transferCommandPool, device);

	VkImageMemoryBarrier barrier = {};
	{
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0; // Defined later
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
	}

	auto srcMipWidth = (int32_t)texWidth;
	auto srcMipHeight = (int32_t)texHeight;

	for (uint32_t i = 1; i < mipLevels; i++)
	{
		const uint32_t srcMipLevel = i - 1;
		const uint32_t dstMipLevel = i;
		const int32_t dstMipWidth = srcMipWidth > 1 ? srcMipWidth / 2 : 1;
		const int32_t dstMipHeight = srcMipHeight > 1 ? srcMipHeight / 2 : 1;


		// Transition layout of src mip to TRANSFER_SRC_OPTIMAL (Note: dst mip is already TRANSFER_DST_OPTIMAL)
		barrier.subresourceRange.baseMipLevel = srcMipLevel;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr, // mem barriers
			0, nullptr, // buffer barriers
			1, &barrier); // image barriers


// Blit the smaller image to the dst 
		VkImageBlit blit = {};
		{
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { srcMipWidth, srcMipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = srcMipLevel;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;

			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { dstMipWidth, dstMipHeight, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = dstMipLevel;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;
		}
		vkCmdBlitImage(commandBuffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);


		// Transition layout of the src mip to optimal shader readible (we don't need to read it again)
		barrier.subresourceRange.baseMipLevel = srcMipLevel;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr, // mem barriers
			0, nullptr, // buffer barriers
			1, &barrier); // image barriers


// Halve mip dimensions in prep for next loop iteration 
		if (srcMipWidth > 1) srcMipWidth /= 2;
		if (srcMipHeight > 1) srcMipHeight /= 2;
	}


	// Transition the final mip to be optimal for reading by shader (wasn't processed in the loop)
	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // still dst from precondition
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr, // mem barriers
		0, nullptr, // buffer barriers
		1, &barrier); // image barriers

	EndSingeTimeCommands(commandBuffer, transferCommandPool, transferQueue, device);
}

std::tuple<VkImage, VkDeviceMemory, uint32_t, uint32_t, uint32_t> VulkanHelpers::CreateTextureImage(
	const std::string& path, VkCommandPool transferCommandPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice,
	VkDevice device)
{
	// Load texture from file into system mem
	int texWidth, texHeight, texChannels;
	unsigned char* texels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!texels)
	{
		stbi_image_free(texels);
		throw std::runtime_error("Failed to load texture image: " + path);
	}

	const VkDeviceSize imageSize = (uint64_t)texWidth * (uint64_t)texHeight * 4; // RGBA = 4bytes
	const uint32_t mipLevels = (uint32_t)std::floor(std::log2(std::max(texWidth, texHeight))) + 1;

	// Create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	std::tie(stagingBuffer, stagingBufferMemory) = CreateBuffer(
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage flags
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // property flags
		device, physicalDevice);


	// Copy texels from system mem to GPU staging buffer
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, texels, imageSize);
	vkUnmapMemory(device, stagingBufferMemory);


	// Free loaded image from system mem
	stbi_image_free(texels);


	// Create image buffer
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	std::tie(textureImage, textureImageMemory) = CreateImage(texWidth, texHeight,
		mipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_UNORM, // format
		VK_IMAGE_TILING_OPTIMAL, // tiling
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT, //usageflags
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //propertyflags
		physicalDevice, device);


	// Transition image layout to optimal for copying to it from the staging buffer
	TransitionImageLayout(textureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_UNDEFINED, // from
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
		mipLevels,
		transferCommandPool, transferQueue, device);


	// Copy texels from staging buffer to image buffer
	CopyBufferToImage(stagingBuffer, textureImage, texWidth, texHeight, transferCommandPool, transferQueue, device);


	GenerateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_UNORM, texWidth, texHeight, mipLevels,
		transferCommandPool, transferQueue, device, physicalDevice);


	// Destroy the staging buffer
	vkFreeMemory(device, stagingBufferMemory, nullptr);
	vkDestroyBuffer(device, stagingBuffer, nullptr);

	return { textureImage, textureImageMemory, mipLevels, texWidth, texHeight };
}

VkImageView VulkanHelpers::CreateTextureImageView(VkImage textureImage, uint32_t mipLevels, VkDevice device)
{
	return CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, device);
}

VkSampler VulkanHelpers::CreateTextureSampler(uint32_t mipLevels, VkDevice device)
{
	VkSamplerCreateInfo samplerCI = {};
	{
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.anisotropyEnable = VK_TRUE;
		samplerCI.maxAnisotropy = 16;
		samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // applied with addressMode is clamp
		samplerCI.unnormalizedCoordinates = VK_FALSE; // false addresses tex coord via [0,1), true = [0,dimensionSize]
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.mipLodBias = 0;
		samplerCI.minLod = 0;
		samplerCI.maxLod = (float)mipLevels;
	}

	VkSampler textureSampler;
	if (VK_SUCCESS != vkCreateSampler(device, &samplerCI, nullptr, &textureSampler))
	{
		throw std::runtime_error("Failed to create texture sampler");
	}

	return textureSampler;
}

std::tuple<VkImage, VkDeviceMemory, VkImageView> VulkanHelpers::CreateColorResources(VkFormat format, VkExtent2D extent,
	VkSampleCountFlagBits msaaSamples,
	VkCommandPool transferCommandPool,
	VkQueue transferQueue,
	VkDevice device,
	VkPhysicalDevice physicalDevice)
{
	const uint32_t mipLevels = 1;

	// Create color image and memory
	VkImage colorImage;
	VkDeviceMemory colorImageMemory;

	std::tie(colorImage, colorImageMemory) = CreateImage(
		extent.width, extent.height,
		mipLevels,
		msaaSamples,
		format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		physicalDevice, device);


	// Create image view
	VkImageView colorImageView
		= CreateImageView(colorImage, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, device);

	return { colorImage, colorImageMemory, colorImageView };
}

std::tuple<VkImage, VkDeviceMemory, VkImageView> VulkanHelpers::CreateDepthResources(VkExtent2D extent,
	VkSampleCountFlagBits msaaSamples,
	VkCommandPool transferCommandPool,
	VkQueue transferQueue,
	VkDevice device,
	VkPhysicalDevice physicalDevice)
{
	const VkFormat depthFormat = FindDepthFormat(physicalDevice);
	const uint32_t mipLevels = 1;

	// Create depth image and memory
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	std::tie(depthImage, depthImageMemory) = CreateImage(
		extent.width, extent.height,
		mipLevels,
		msaaSamples,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		physicalDevice, device);


	// Create image view
	VkImageView depthImageView
		= CreateImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, mipLevels, device);

	return { depthImage, depthImageMemory, depthImageView };
}

bool VulkanHelpers::HasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat VulkanHelpers::FindDepthFormat(VkPhysicalDevice physicalDevice)
{
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
		physicalDevice);
}

VkFormat VulkanHelpers::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
	VkFormatFeatureFlags features, VkPhysicalDevice physicalDevice)
{
	for (auto format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR &&
			(props.linearTilingFeatures & features) == features)
		{
			return format;
		}

		if (tiling == VK_IMAGE_TILING_OPTIMAL &&
			(props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find supported format");
}

QueueFamilyIndices VulkanHelpers::FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies{ queueFamilyCount };
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		// TODO Get a VK_QUEUE_TRANSFER_BIT for a new transferQueue for all the host to device local transfers
		// 
		// Transfer queue will be async and not block other work. It has some latency though.
		// If needing an immediate copy for work done right now, then graphics or compute queues are faster, but clog system

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

SwapChainSupportDetails VulkanHelpers::QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
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

VkResult VulkanHelpers::CreateDebugUtilsMessengerEXT(VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	const auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkCreateDebugUtilsMessengerEXT");
	return func ? func(instance, pCreateInfo, pAllocator, pDebugMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanHelpers::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
	const VkAllocationCallbacks* pAllocator)
{
	const auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func) func(instance, messenger, pAllocator);
}
