#include "VulkanHelpers.h"
#include "VulkanInitializers.h"
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

// TODO Move to Renderer - it isn't a helper.
VkRenderPass VulkanHelpers::CreateSwapchainRenderPass(VkSampleCountFlagBits msaaSamples, VkFormat swapchainFormat,
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


VkFramebuffer VulkanHelpers::CreateFramebuffer(VkDevice device, u32 width, 
	u32 height, const std::vector<VkImageView>& attachments, VkRenderPass renderPass, u32 layers)
{
	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.renderPass = renderPass;
	info.attachmentCount = (u32)attachments.size();
	info.pAttachments = attachments.data();
	info.width = width;
	info.height = height;
	info.layers = layers;
	info.flags = 0;

	VkFramebuffer framebuffer;
	if (vkCreateFramebuffer(device, &info, nullptr, &framebuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to reate Framebuffer");
	}

	return framebuffer;
}

std::vector<VkFramebuffer> VulkanHelpers::CreateSwapchainFramebuffer(VkDevice device, VkImageView colorImageView,
	VkImageView depthImageView, const std::vector<VkImageView>& swapchainImageViews, const VkExtent2D& swapchainExtent,
	VkRenderPass renderPass)
{
	std::vector<VkFramebuffer> swapchainFramebuffers{ swapchainImageViews.size() };

	for (size_t i = 0; i < swapchainImageViews.size(); ++i)
	{
		std::vector<VkImageView> attachments = { colorImageView, depthImageView, swapchainImageViews[i] };

		swapchainFramebuffers[i]
			= CreateFramebuffer(device, swapchainExtent.width, swapchainExtent.height, attachments, renderPass);
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

void VulkanHelpers::CopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkImage dstImage,
	u32 width, u32 height)
{
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
	vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, dstImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // assuming pixels are already in optimal layout
		1, &region);
}

void VulkanHelpers::TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout,
	VkImageLayout newLayout, VkImageAspectFlagBits aspect, u32 baseMipLevel, u32 levelCount, u32 baseArrayLayer, 
	u32 layerCount, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	TransitionImageLayout(cmdBuffer, image, oldLayout, newLayout,
		vki::ImageSubresourceRange(aspect, baseMipLevel, levelCount, baseArrayLayer, layerCount), 
		srcStageMask, dstStageMask);
}

void VulkanHelpers::TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout,
	VkImageLayout newLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask,
	VkPipelineStageFlags dstStageMask)
{
	// Method modified from https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanTools.h
	
	// Create an image barrier object
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.oldLayout = oldLayout;
	imageMemoryBarrier.newLayout = newLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	// Source layouts (old)
	// Source access mask controls actions that have to be finished on the old layout
	// before it will be transitioned to the new layout
	switch (oldLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		// Image layout is undefined (or does not matter)
		// Only valid as initial layout
		// No flags required, listed only for completeness
		imageMemoryBarrier.srcAccessMask = 0;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// Image is preinitialized
		// Only valid as initial layout for linear images, preserves memory contents
		// Make sure host writes have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image is a color attachment
		// Make sure any writes to the color buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image is a depth/stencil attachment
		// Make sure any writes to the depth/stencil buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image is a transfer source 
		// Make sure any reads from the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image is a transfer destination
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image is read by a shader
		// Make sure any shader reads from the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	default:
		// Other source layouts aren't handled (yet)
		break;
	}

	// Target layouts (new)
	// Destination access mask controls the dependency for the new image layout
	switch (newLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image will be used as a transfer destination
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image will be used as a transfer source
		// Make sure any reads from the image have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image will be used as a color attachment
		// Make sure any writes to the color buffer have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image layout will be used as a depth/stencil attachment
		// Make sure any writes to depth/stencil buffer have been finished
		imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image will be read in a shader (sampler, input attachment)
		// Make sure any writes to the image have been finished
		if (imageMemoryBarrier.srcAccessMask == 0)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	default:
		// Other source layouts aren't handled (yet)
		break;
	}

	// Put barrier inside setup command buffer
	vkCmdPipelineBarrier(
		cmdBuffer,
		srcStageMask,
		dstStageMask,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
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
		throw std::runtime_error("Failed to allocate command buffer");
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

void VulkanHelpers::EndSingeTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool transferPool,
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
	
	// Create fence to ensure that the command buffer has finished executing
	/*VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = 0;
	VkFence fence;
	if (VK_SUCCESS != vkCreateFence(device, &fenceInfo, nullptr, &fence))
	{
		throw std::runtime_error("Failed to create fence");
	}*/
	
	vkQueueSubmit(transferQueue, 1, &submitInfo, nullptr);

	//vkWaitForFences(device, 1, &fence, true, u64_max);
	//vkDestroyFence(device, fence, nullptr);
	vkQueueWaitIdle(transferQueue);


	// Cleanup
	vkFreeCommandBuffers(device, transferPool, 1, &commandBuffer);
}

std::vector<VkCommandBuffer> VulkanHelpers::AllocateAndRecordCommandBuffers(uint32_t numBuffersToCreate,
	const Skybox* skybox,
	const std::vector<std::unique_ptr<Renderable>>& renderables,
	const std::vector<std::unique_ptr<MeshResource>>& meshes,
	VkExtent2D swapchainExtent,
	const std::vector<VkFramebuffer>&
	swapchainFramebuffers, VkCommandPool commandPool,
	VkDevice device,
	VkRenderPass renderPass, 
	VkPipeline pbrPipeline, VkPipelineLayout pbrPipelineLayout,
	VkPipeline skyboxPipeline, VkPipelineLayout skyboxPipelineLayout)
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
	for (u32 i = 0; i < commandBuffers.size(); ++i)
	{
		RecordCommandBuffer(
			commandBuffers[i],
			skybox,
			renderables,
			meshes,
			i,
			swapchainExtent,
			swapchainFramebuffers[i],
			renderPass,
			pbrPipeline, pbrPipelineLayout,
			skyboxPipeline, skyboxPipelineLayout);
	}

	return commandBuffers;
}

void VulkanHelpers::RecordCommandBuffer(VkCommandBuffer commandBuffer,
	const Skybox* skybox,
	const std::vector<std::unique_ptr<Renderable>>& renderables,
	const std::vector<std::unique_ptr<MeshResource>>& meshes,
	int frameIndex,
	VkExtent2D swapchainExtent,
	VkFramebuffer swapchainFramebuffer, VkRenderPass renderPass,
	VkPipeline pbrPipeline, VkPipelineLayout pbrPipelineLayout,
	VkPipeline skyboxPipeline, VkPipelineLayout skyboxPipelineLayout)
{
	// Start command buffer
	const auto beginInfo = vki::CommandBufferBeginInfo(0, nullptr);
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) == VK_SUCCESS)
	{
		// Record renderpass
		std::vector<VkClearValue> clearColors(2);
		clearColors[0].color = { 0.f, 0.f, 0.f, 1.f };
		clearColors[1].depthStencil = { 1.f, 0ui32 };

		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(renderPass, swapchainFramebuffer,
			vki::Rect2D(vki::Offset2D(0, 0), swapchainExtent), clearColors);

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			// Skybox
			if (skybox)
			{
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);

				const auto& mesh = *meshes[skybox->MeshId.Id];

				// Draw mesh
				VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindDescriptorSets(commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout,
					0, 1, &skybox->FrameResources[frameIndex].DescriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, (uint32_t)mesh.IndexCount, 1, 0, 0, 0);
			}



			// Objects
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipeline);

			for (const auto& renderable : renderables)
			{
				const auto& mesh = *meshes[renderable->MeshId.Id];

				// Draw mesh
				VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindDescriptorSets(commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipelineLayout,
					0, 1, &renderable->FrameResources[frameIndex].DescriptorSet, 0, nullptr);

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


VkDescriptorPool VulkanHelpers::CreateDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes, u32 maxSets, 
	VkDevice device)
{
	// Create descriptor pool
	VkDescriptorPoolCreateInfo poolCI = {};
	{
		poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCI.poolSizeCount = (uint32_t)poolSizes.size();
		poolCI.pPoolSizes = poolSizes.data();
		poolCI.maxSets = maxSets;
		//poolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	}

	VkDescriptorPool pool;
	if (vkCreateDescriptorPool(device, &poolCI, nullptr, &pool) != VK_SUCCESS)
	{
		// TODO Handle allocation failures as it's determined normal behaviour in spec
		throw std::runtime_error("Failed to create descriptor pool!");
	}

	return pool;
}

std::vector<VkDescriptorSet>
VulkanHelpers::AllocateDescriptorSets(u32 count, VkDescriptorSetLayout layout, VkDescriptorPool pool, VkDevice device)
{
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

	return descriptorSets;
}

VkDescriptorSetLayout VulkanHelpers::CreateDescriptorSetLayout(VkDevice device, 
	const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
	// Create descriptor set layout
	VkDescriptorSetLayoutCreateInfo layoutCI = {};
	{
		layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCI.bindingCount = (u32)bindings.size();
		layoutCI.pBindings = bindings.data();
	}

	VkDescriptorSetLayout descriptorSetLayout;
	if (vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor set layout");
	}

	return descriptorSetLayout;
}


#pragma region Image Helpers

std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>> VulkanHelpers::CreateUniformBuffers(u32 count, 
	VkDeviceSize typeSize, VkDevice device, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(physicalDevice, &props);
	const size_t minUboAlignment = props.limits.minUniformBufferOffsetAlignment;
	const size_t maxUboRange = props.limits.maxUniformBufferRange;
	
	const auto doAlignMemory = false;
	if (doAlignMemory)
	{
		auto dynamicAlignment = typeSize;
		if (minUboAlignment > 0)
		{
			dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
		}
		typeSize = dynamicAlignment;
		//uboDataDynamic.model = (glm::mat4*)alignedAlloc(bufferSize, dynamicAlignment);
	}


	
	std::vector<VkBuffer> buffers{ count };
	std::vector<VkDeviceMemory> buffersMemory{ count };

	for (size_t i = 0; i < count; ++i)
	{
		const auto usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		const auto propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		std::tie(buffers[i], buffersMemory[i])
			= CreateBuffer(typeSize, usageFlags, propertyFlags, device, physicalDevice);
	}

	return { buffers, buffersMemory };
}


std::tuple<VkImage, VkDeviceMemory> VulkanHelpers::CreateImage2D(u32 width, u32 height, u32 mipLevels,
	VkSampleCountFlagBits numSamples, VkFormat format,
	VkImageTiling tiling, VkImageUsageFlags usageFlags,
	VkMemoryPropertyFlags propertyFlags,
	VkPhysicalDevice physicalDevice, VkDevice device, u32 arrayLayers, VkImageCreateFlags flags)
{
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;

	// Create image buffer
	VkImageCreateInfo imageCI = {};
	{
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //not usable by gpu and first transition discards the texels
		imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCI.extent.depth = 1; // one-dimensional
		imageCI.extent.width = width;
		imageCI.extent.height = height;
		imageCI.mipLevels = mipLevels;
		imageCI.arrayLayers = arrayLayers;
		imageCI.format = format;
		imageCI.tiling = tiling;
		imageCI.usage = usageFlags;
		imageCI.samples = numSamples; // the multisampling mode
		imageCI.flags = flags;
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
		throw std::runtime_error("Failed to allocate image memory");
	}

	if (VK_SUCCESS != vkBindImageMemory(device, textureImage, textureImageMemory, 0))
	{
		throw std::runtime_error("Failed to bind image memory");
	}

	return { textureImage, textureImageMemory };
}

VkImageView VulkanHelpers::CreateImage2DView(VkImage image, VkFormat format, VkImageViewType viewType,
	VkImageAspectFlagBits aspectFlags, u32 mipLevels, u32 layerCount, VkDevice device)
{
	VkImageView imageView;

	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = viewType;
	createInfo.format = format;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = aspectFlags;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = mipLevels;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = layerCount;

	if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create image views");
	}

	return imageView;
}

std::vector<VkImageView> VulkanHelpers::CreateImageViews(const std::vector<VkImage>& images, VkFormat format,
	VkImageViewType viewType, VkImageAspectFlagBits aspectFlags, u32 mipLevels, u32 layerCount, VkDevice device)
{
	std::vector<VkImageView> imageViews{ images.size() };

	for (size_t i = 0; i < images.size(); ++i)
	{
		imageViews[i] = CreateImage2DView(images[i], format, viewType, aspectFlags, mipLevels, layerCount, device);
	}

	return imageViews;
}

std::tuple<VkImage, VkDeviceMemory, VkImageView> VulkanHelpers::CreateColorResources(VkFormat format, VkExtent2D extent,
	VkSampleCountFlagBits msaaSamples,
	VkCommandPool transferCommandPool,
	VkQueue transferQueue,
	VkDevice device,
	VkPhysicalDevice physicalDevice)
{
	const u32 mipLevels = 1;
	const u32 layerCount = 1;

	// Create color image and memory
	VkImage colorImage;
	VkDeviceMemory colorImageMemory;

	std::tie(colorImage, colorImageMemory) = CreateImage2D(
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
		= CreateImage2DView(colorImage, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, layerCount, device);

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
	const u32 mipLevels = 1;
	const u32 arrayLayers = 1;

	// Create depth image and memory
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	std::tie(depthImage, depthImageMemory) = CreateImage2D(
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
		= CreateImage2DView(depthImage, depthFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, mipLevels, arrayLayers, device);

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

#pragma endregion Image Helpers


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

