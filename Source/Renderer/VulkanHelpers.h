#pragma once

#include <Shared/CommonTypes.h>

#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <memory>

struct QueueFamilyIndices;
struct GLFWwindow;
struct Vertex;
struct Renderable;
class TextureResource;
struct MeshResource;
struct SwapChainSupportDetails;

class VulkanHelpers
{
public:

#pragma region InitVulkan

	[[nodiscard]] static VkInstance
		CreateInstance(bool enableValidationLayers, const std::vector<const char*>& validationLayers);
	static bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayers);
	static std::vector<const char*> GetRequiredExtensions(bool enableValidationLayers);


	[[nodiscard]] static VkSurfaceKHR CreateSurface(VkInstance instance, GLFWwindow* window);


	[[nodiscard]] static VkDebugUtilsMessengerEXT SetupDebugMessenger(VkInstance instance);
	static void PopulateDebugUtilsMessengerCreateInfoEXT(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);


	[[nodiscard]] static std::tuple<VkPhysicalDevice, VkSampleCountFlagBits>
		PickPhysicalDevice(const std::vector<const char*>& physicalDeviceExtensions, VkInstance instance,
			VkSurfaceKHR surface);

	static bool IsDeviceSuitable(const std::vector<const char*>& physicalDeviceExtensions,
		VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
	static bool CheckPhysicalDeviceExtensionSupport(const std::vector<const char*>& physicalDeviceExtensions,
		VkPhysicalDevice physicalDevice);
	static VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice);


	[[nodiscard]] static std::tuple<VkDevice, VkQueue, VkQueue> CreateLogicalDevice(
		VkPhysicalDevice physicalDevice,
		VkSurfaceKHR surface,
		const std::vector<const char*>& validationLayers,
		const std::vector<const char*>& physicalDeviceExtensions);


	// TODO Remove OUT params, use tuple return
	[[nodiscard]] static VkSwapchainKHR
		CreateSwapchain(const VkExtent2D& windowSize, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
			VkDevice device, std::vector<VkImage>& OUTswapchainImages, VkFormat& OUTswapchainImageFormat,
			VkExtent2D& OUTswapchainExtent);
	static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
	static VkExtent2D ChooseSwapExtent(const VkExtent2D& windowSize, const VkSurfaceCapabilitiesKHR& capabilities);


	// The attachments referenced by the pipeline stages and their usage
	[[nodiscard]] static VkRenderPass
		CreateRenderPass(VkSampleCountFlagBits msaaSamples, VkFormat swapchainFormat, VkDevice device,
			VkPhysicalDevice physicalDevice);

	static VkShaderModule CreateShaderModule(const std::vector<char>& code, VkDevice device);


	[[nodiscard]] static std::vector<VkFramebuffer> CreateFramebuffer(
		VkImageView colorImageView,
		VkImageView depthImageView,
		VkDevice device,
		VkRenderPass renderPass,
		const VkExtent2D& swapchainExtent,
		const std::vector<VkImageView>& swapchainImageViews);


	[[nodiscard]] static VkCommandPool CreateCommandPool(QueueFamilyIndices queueFamilyIndices, VkDevice device);


	[[nodiscard]] static std::tuple<VkBuffer, VkDeviceMemory>
		CreateVertexBuffer(const std::vector<Vertex>& vertices, VkQueue transferQueue, VkCommandPool transferCommandPool,
			VkPhysicalDevice physicalDevice, VkDevice device);

	[[nodiscard]] static std::tuple<VkBuffer, VkDeviceMemory>
		CreateIndexBuffer(const std::vector<uint32_t>& indices, VkQueue transferQueue, VkCommandPool transferCommandPool,
			VkPhysicalDevice physicalDevice, VkDevice device);


	[[nodiscard]] static std::tuple<VkBuffer, VkDeviceMemory>
		CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags,
			VkDevice device, VkPhysicalDevice physicalDevice);


	// Helper method to find suitable memory type on GPU
	static uint32_t
		FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags, VkPhysicalDevice physicalDevice);;


	static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize,
		VkCommandPool transferCommandPool, VkQueue transferQueue, VkDevice device);


	static void CopyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height,
		VkCommandPool transferCommandPool, VkQueue transferQueue, VkDevice device);


	static void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout,
		VkImageLayout newLayout, VkImageSubresourceRange subresourceRange, 
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	
	[[deprecated]] static void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t mipLevels, VkCommandPool transferCommandPool, VkQueue transferQueue,
		VkDevice device);

	static VkCommandBuffer BeginSingleTimeCommands(VkCommandPool transferCommandPool, VkDevice device);
	static void EndSingeTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool transferCommandPool,
		VkQueue transferQueue, VkDevice device);


	[[nodiscard]] static std::vector<VkCommandBuffer> CreateCommandBuffers(
		uint32_t numBuffersToCreate,

		const std::vector<std::unique_ptr<Renderable>>& renderables,
		const std::vector<std::unique_ptr<MeshResource>>& meshes,
		VkExtent2D swapchainExtent,
		const std::vector<VkFramebuffer>& swapchainFramebuffers,

		VkCommandPool commandPool,
		VkDevice device,
		VkRenderPass renderPass,
		VkPipeline pipeline,
		VkPipelineLayout pipelineLayout);


	static void RecordCommandBuffer(
		VkCommandBuffer commandBuffer,

		const std::vector<std::unique_ptr<Renderable>>& renderables,
		const std::vector<std::unique_ptr<MeshResource>>& meshes,
		int frameIndex,

		VkExtent2D swapchainExtent,
		VkFramebuffer swapchainFramebuffer,

		VkRenderPass renderPass,
		VkPipeline pipeline,
		VkPipelineLayout pipelineLayout);


	// Returns render finished semaphore and image available semaphore, in that order
	[[nodiscard]]
	static std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>, std::vector<VkFence>>
		CreateSyncObjects(size_t numFramesInFlight, size_t numSwapchainImages, VkDevice device);



	// count: max num of descriptor sets that may be allocated
	static VkDescriptorPool CreateDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes, u32 maxSets,
		VkDevice device);

	static VkDescriptorSetLayout CreateDescriptorSetLayout(
		const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDevice device);
	
	
	static std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>>
		CreateUniformBuffers(u32 count, VkDeviceSize typeSize, VkDevice device, VkPhysicalDevice physicalDevice);

#pragma endregion


#pragma region Image, ImageView, ImageMemory, MipLevels, DepthImage, TextureImage, etc

	[[nodiscard]] static std::tuple<VkImage, VkDeviceMemory>
		CreateImage2D(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
			VkFormat format,
			VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags,
			VkPhysicalDevice physicalDevice, VkDevice device, u32 arrayLayers = 1, VkImageCreateFlags flags = 0);


	[[nodiscard]] static VkImageView CreateImageView(VkImage image, VkFormat format,
		VkImageAspectFlagBits aspectFlags,
		uint32_t mipLevels, VkDevice device);


	[[nodiscard]] static std::vector<VkImageView> CreateImageViews(const std::vector<VkImage>& images,
		VkFormat format, VkImageAspectFlagBits aspectFlags,
		uint32_t mipLevels, VkDevice device);


	[[nodiscard]] static std::tuple<VkImage, VkDeviceMemory, VkImageView>
		CreateColorResources(VkFormat format, VkExtent2D extent, VkSampleCountFlagBits msaaSamples,
			VkCommandPool transferCommandPool, VkQueue transferQueue, VkDevice device,
			VkPhysicalDevice physicalDevice);


	[[nodiscard]] static std::tuple<VkImage, VkDeviceMemory, VkImageView>
		CreateDepthResources(VkExtent2D extent, VkSampleCountFlagBits msaaSamples, VkCommandPool transferCommandPool,
			VkQueue transferQueue, VkDevice device, VkPhysicalDevice physicalDevice);


	static bool HasStencilComponent(VkFormat format);


	static VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);


	static VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
		VkFormatFeatureFlags features, VkPhysicalDevice physicalDevice);

#pragma endregion


	static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

	static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);

	static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
		VkDebugUtilsMessengerEXT messenger,
		const VkAllocationCallbacks* pAllocator);
};
