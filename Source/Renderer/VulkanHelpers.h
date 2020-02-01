#pragma once

#include <Shared/CommonTypes.h>

#include <vulkan/vulkan.h>

#include <array>
#include <vector>
#include <string>
#include <memory>
#include <cassert>

struct QueueFamilyIndices;
struct GLFWwindow;
struct Vertex;
struct Skybox;
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


	[[deprecated]] static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize,
		VkCommandPool transferCommandPool, VkQueue transferQueue, VkDevice device);


	[[deprecated]] static void CopyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height,
		VkCommandPool transferCommandPool, VkQueue transferQueue, VkDevice device);

	static void CopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkImage dstImage, 
		u32 width, u32 height);

	static void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout,
		VkImageLayout newLayout, VkImageSubresourceRange subresourceRange, 
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	
	[[deprecated]] static void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t mipLevels, VkCommandPool transferCommandPool, VkQueue transferQueue,
		VkDevice device);

	static VkCommandBuffer BeginSingleTimeCommands(VkCommandPool transferCommandPool, VkDevice device);
	static void EndSingeTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool transferPool,
		VkQueue transferQueue, VkDevice device);


	[[nodiscard]] static std::vector<VkCommandBuffer> AllocateAndRecordCommandBuffers(
		uint32_t numBuffersToCreate,

		const Skybox* skybox,
		const std::vector<std::unique_ptr<Renderable>>& renderables,
		const std::vector<std::unique_ptr<MeshResource>>& meshes,

		VkExtent2D swapchainExtent, const std::vector<VkFramebuffer>& swapchainFramebuffers,

		VkCommandPool commandPool,
		VkDevice device,
		VkRenderPass renderPass,
		VkPipeline pbrPipeline, VkPipelineLayout pbrPipelineLayout, 
		VkPipeline skyboxPipeline, VkPipelineLayout skyboxPipelineLayout);


	static void RecordCommandBuffer(
		VkCommandBuffer commandBuffer,

		const Skybox* skybox,
		const std::vector<std::unique_ptr<Renderable>>& renderables,
		const std::vector<std::unique_ptr<MeshResource>>& meshes,
		int frameIndex,

		VkExtent2D swapchainExtent,
		VkFramebuffer swapchainFramebuffer,

		VkRenderPass renderPass,
		VkPipeline pbrPipeline, VkPipelineLayout pbrPipelineLayout,
		VkPipeline skyboxPipeline, VkPipelineLayout skyboxPipelineLayout);


	// Returns render finished semaphore and image available semaphore, in that order
	[[nodiscard]]
	static std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>, std::vector<VkFence>>
		CreateSyncObjects(size_t numFramesInFlight, size_t numSwapchainImages, VkDevice device);



	// count: max num of descriptor sets that may be allocated
	static VkDescriptorPool CreateDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes, u32 maxSets,
		VkDevice device);

	static std::vector<VkDescriptorSet> AllocateDescriptorSets(
		u32 count,
		VkDescriptorSetLayout layout,
		VkDescriptorPool pool,
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


	[[nodiscard]] static VkImageView CreateImage2DView(VkImage image, VkFormat format,
		VkImageAspectFlagBits aspectFlags,
		u32 mipLevels, u32 layerCount, VkDevice device);


	[[nodiscard]] static std::vector<VkImageView> CreateImageViews(const std::vector<VkImage>& images,
		VkFormat format, VkImageAspectFlagBits aspectFlags, u32 mipLevels, u32 layerCount, VkDevice device);


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


	// TODO Move this to VulkanHelpers and add support for n layers, and pass in a command buffer for external management
	// Preconditions: srcImage/dstImage layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL/DST_OPTIMAL, respectively.
	static void ChangeFormat(VkCommandBuffer commandBuffer, VkImage srcImage, VkImage dstImage, u32 width, u32 height, 
		VkImageSubresourceRange subresourceRange)
	{
		auto mipWidth = (i32)width;
		auto mipHeight = (i32)height;

		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcSubresource.aspectMask = subresourceRange.aspectMask;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = subresourceRange.layerCount;
		
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstSubresource.aspectMask = subresourceRange.aspectMask;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = subresourceRange.layerCount;
		
		for (u32 mip = 0; mip < subresourceRange.levelCount; mip++)
		{
			// Blit the smaller image to the dst 
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.mipLevel = mip;

			blit.dstOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.dstSubresource.mipLevel = mip;
				
			vkCmdBlitImage(commandBuffer,
				srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			// Halve mip dimensions in prep for next loop iteration 
			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}
	}


	// TODO Move this to VulkanHelpers and add support for n layers, and pass in a command buffer for external management
	// Preconditions: image layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	// Postconditions: image layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 
	static void GenerateMipmaps(VkCommandBuffer commandBuffer, VkPhysicalDevice physicalDevice,
		VkImage image, VkFormat format, u32 width, u32 height, u32 mipLevels = 1, u32 arrayLayers = 1)
	{
		assert(arrayLayers == 1); // TODO Support for arrayLayers != 1 has NOT been tested

		// Check if device supports linear blitting
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
		{
			throw std::runtime_error("Texture image format does not support linear blitting!");
		}

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

		auto srcMipWidth = (i32)width;
		auto srcMipHeight = (i32)height;

		for (u32 layer = 0; layer < arrayLayers; layer++)
		{
			for (u32 mip = 1; mip < mipLevels; mip++)
			{
				const u32 srcMipLevel = mip - 1;
				const u32 dstMipLevel = mip;
				const i32 dstMipWidth = srcMipWidth > 1 ? srcMipWidth / 2 : 1;
				const i32 dstMipHeight = srcMipHeight > 1 ? srcMipHeight / 2 : 1;


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
					blit.srcSubresource.baseArrayLayer = layer;
					blit.srcSubresource.layerCount = 1;

					blit.dstOffsets[0] = { 0, 0, 0 };
					blit.dstOffsets[1] = { dstMipWidth, dstMipHeight, 1 };
					blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit.dstSubresource.mipLevel = dstMipLevel;
					blit.dstSubresource.baseArrayLayer = layer;
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
	}



	
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
