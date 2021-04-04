#pragma once

#include <Framework/CommonTypes.h>

#include <vulkan/vulkan.h>

#include <vector>
#include <cassert>
#include <stdexcept>

struct QueueFamilyIndices;
struct Vertex;
struct Skybox;
struct RenderableMesh;
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

	
	
	static VkPipelineLayout CreatePipelineLayout(VkDevice device, 
		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
		const std::vector<VkPushConstantRange>& pushConstantRanges = {})
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.pNext = nullptr;
		pipelineLayoutCI.setLayoutCount = (u32)descriptorSetLayouts.size();
		pipelineLayoutCI.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutCI.pushConstantRangeCount = (u32)pushConstantRanges.size();;
		pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
		pipelineLayoutCI.flags = 0;

		VkPipelineLayout pipelineLayout = nullptr;
		if (vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to Create Pipeline Layout!");
		}

		return pipelineLayout;
	}

	
	static VkShaderModule CreateShaderModule(const std::vector<char>& code, VkDevice device);

	static VkFramebuffer CreateFramebuffer(VkDevice device, u32 width, u32 height, 
		const std::vector<VkImageView>& attachments, VkRenderPass renderPass, u32 layers = 1);


	[[nodiscard]] static VkCommandPool CreateCommandPool(QueueFamilyIndices queueFamilyIndices, VkDevice device);


	[[nodiscard]] static std::tuple<VkBuffer, VkDeviceMemory>
		CreateVertexBuffer(const std::vector<Vertex>& vertices, VkQueue transferQueue, VkCommandPool transferCommandPool,
			VkPhysicalDevice physicalDevice, VkDevice device);

	[[nodiscard]] static std::tuple<VkBuffer, VkDeviceMemory>
		CreateIndexBuffer(const std::vector<uint32_t>& indices, VkQueue transferQueue, VkCommandPool transferCommandPool,
			VkPhysicalDevice physicalDevice, VkDevice device);


	[[nodiscard]] static std::tuple<VkBuffer, VkDeviceMemory>
		CreateBuffer(VkDeviceSize sizeBytes, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags,
			VkDevice device, VkPhysicalDevice physicalDevice);


	// Helper method to find suitable memory type on GPU
	static uint32_t
		FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags, VkPhysicalDevice physicalDevice);;


	static void CopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize);

	static void CopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkImage dstImage, 
		u32 width, u32 height);

	static void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout,
		VkImageLayout newLayout, VkImageAspectFlagBits aspect, 
		u32 baseMipLevel = 0, u32 levelCount = 1, u32 baseArrayLayer = 0, u32 layerCount = 1,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	
	static void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout,
		VkImageLayout newLayout, VkImageSubresourceRange subresourceRange, 
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	static VkCommandBuffer BeginSingleTimeCommands(VkCommandPool transferCommandPool, VkDevice device);
	static void EndSingeTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool transferPool,
		VkQueue transferQueue, VkDevice device);


	[[nodiscard]] static std::vector<VkCommandBuffer> AllocateCommandBuffers(u32 numBuffersToCreate,
		VkCommandPool commandPool, VkDevice device);


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
		VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings);
	
	
	static std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>>
		CreateUniformBuffers(u32 count, VkDeviceSize typeSize, VkDevice device, VkPhysicalDevice physicalDevice);

#pragma endregion


#pragma region Image, ImageView, ImageMemory, MipLevels, DepthImage, TextureImage, etc

	[[nodiscard]] static std::tuple<VkImage, VkDeviceMemory>
		CreateImage2D(u32 width, u32 height, u32 mipLevels, VkSampleCountFlagBits multisampleSamples,
			VkFormat format,
			VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags,
			VkPhysicalDevice physicalDevice, VkDevice device, u32 arrayLayers = 1, VkImageCreateFlags flags = 0);


	[[nodiscard]] static VkImageView CreateImage2DView(VkImage image, VkFormat format, VkImageViewType viewType,
		VkImageAspectFlagBits aspectFlags, u32 mipLevels, u32 layerCount, VkDevice device);


	[[nodiscard]] static std::vector<VkImageView> CreateImageViews(const std::vector<VkImage>& images,
		VkFormat format, VkImageViewType viewType, VkImageAspectFlagBits aspectFlags, u32 mipLevels, u32 layerCount, VkDevice device);


	/**
	Creates a configured VkSampler
	@param device vulkan device
	@param minFilter is a VkFilter value specifying the minification filter to apply to lookups.
	@param magFilter is a VkFilter value specifying the magnification filter to apply to lookups.
	@param addressModeU is a VkSamplerAddressMode value specifying the addressing mode for outside [0..1] range for U coordinate.
	@param addressModeV is a VkSamplerAddressMode value specifying the addressing mode for outside [0..1] range for V coordinate.
	@param addressModeW is a VkSamplerAddressMode value specifying the addressing mode for outside [0..1] range for W coordinate.
	@param anisotropyEnable is VK_TRUE to enable anisotropic filtering, as described in the Texel Anisotropic Filtering section, or VK_FALSE otherwise.
	@param maxAnisotropy is the anisotropy value clamp used by the sampler when anisotropyEnable is VK_TRUE. If anisotropyEnable is VK_FALSE, maxAnisotropy is ignored.
	@param minLod and maxLod are the values used to clamp the computed LOD value, as described in the Level-of-Detail Operation section.
	@param maxLod and minLod are the values used to clamp the computed LOD value, as described in the Level-of-Detail Operation section.
	@param mipmapMode is a VkSamplerMipmapMode value specifying the mipmap filter to apply to lookups.
	@param mipLodBias is the bias to be added to mipmap LOD (level-of-detail) calculation and bias provided by image sampling functions in SPIR-V, as described in the Level-of-Detail Operation section.
	@param borderColor is a VkBorderColor value specifying the predefined border color to use.
	@param unnormalizedCoordinates controls whether to use unnormalized or normalized texel coordinates to address texels of the image. When set to VK_TRUE, the range of the image coordinates used to lookup the texel is in the range of zero to the image dimensions for x, y and z. When set to VK_FALSE the range of image coordinates is zero to one.
	 When unnormalizedCoordinates is VK_TRUE, images the sampler is used with in the shader have the following requirements:
	     The viewType must be either VK_IMAGE_VIEW_TYPE_1D or VK_IMAGE_VIEW_TYPE_2D.
	     The image view must have a single layer and a single mip level.
	 When unnormalizedCoordinates is VK_TRUE, image built-in functions in the shader that use the sampler have the following requirements:
	     The functions must not use projection.
	     The functions must not use offsets.
	@param compareEnable is VK_TRUE to enable comparison against a reference value during lookups, or VK_FALSE otherwise.
	     Note: Some implementations will default to shader state if this member does not match.
	@param compareOp is a VkCompareOp value specifying the comparison function to apply to fetched data before filtering as described in the Depth Compare Operation section.
	@param flags is a bitmask of VkSamplerCreateFlagBits describing additional parameters of the sampler.
	 */
	
	[[nodiscard]] inline static VkSampler CreateSampler(VkDevice device,
		VkFilter minFilter = VK_FILTER_LINEAR,
		VkFilter magFilter = VK_FILTER_LINEAR,
		VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		bool anisotropyEnable = false,
		f32 maxAnisotropy = 0,
		f32 minLod = 0.f,
		f32 maxLod = 1.f,
		VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		f32 mipLodBias = 0.f,
		VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		bool unnormalizedCoordinates = false,
		bool compareEnable = false,
		VkCompareOp compareOp = VK_COMPARE_OP_NEVER,
		VkSamplerCreateFlags flags = 0
	)
	{
		// TODO Query device to find max if anisotrophy is supported
		/*if (device.features.samplerAnisotropy)
		{
		  sampler.maxAnisotropy = device.properties.limits.maxSamplerAnisotropy;
		  sampler.anisotropyEnable = VK_TRUE;
		}*/


		VkSamplerCreateInfo s = {};
		s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		s.pNext = nullptr;

		s.minFilter = minFilter;
		s.magFilter = magFilter;
		s.addressModeU = addressModeU;
		s.addressModeV = addressModeV;
		s.addressModeW = addressModeW;
		s.anisotropyEnable = anisotropyEnable;
		s.maxAnisotropy = maxAnisotropy;
		s.minLod = minLod;
		s.maxLod = maxLod;
		s.mipmapMode = mipmapMode;
		s.mipLodBias = mipLodBias;
		s.borderColor = borderColor; // applied with addressMode is clamp
		s.unnormalizedCoordinates = unnormalizedCoordinates; // false addresses tex coord via [0,1), true = [0,dimensionSize]
		s.compareEnable = compareEnable;
		s.compareOp = compareOp;
		s.flags = flags;

		VkSampler sampler;
		if (VK_SUCCESS != vkCreateSampler(device, &s, nullptr, &sampler))
		{
			throw std::runtime_error("Failed to create cubemap sampler");
		}

		return sampler;
	}

	
	[[nodiscard]] static std::tuple<VkImage, VkDeviceMemory, VkImageView>
		CreateColorResources(VkFormat format, VkExtent2D extent, VkSampleCountFlagBits msaaSamples,
			VkDevice device, VkPhysicalDevice physicalDevice);


	[[nodiscard]] static std::tuple<VkImage, VkDeviceMemory, VkImageView>
		CreateDepthResources(VkExtent2D extent, VkSampleCountFlagBits msaaSamples, VkDevice device, VkPhysicalDevice physicalDevice);


	static bool HasStencilComponent(VkFormat format);


	static VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);


	static VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
		VkFormatFeatureFlags features, VkPhysicalDevice physicalDevice);


	// Preconditions: srcImage/dstImage layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL/DST_OPTIMAL, respectively.
	static void BlitSrcToDstImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImage dstImage, u32 width, u32 height, VkImageSubresourceRange subresourceRange)
	{
		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcSubresource.aspectMask = subresourceRange.aspectMask;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = subresourceRange.layerCount;
		
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstSubresource.aspectMask = subresourceRange.aspectMask;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = subresourceRange.layerCount;

		auto mipWidth = (i32)width;
		auto mipHeight = (i32)height;
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


	// TODO Add support for n layers
	// Preconditions: image layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	// Postconditions: image layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 
	static void GenerateMipmaps(VkCommandBuffer commandBuffer, VkPhysicalDevice physicalDevice,
		VkImage image, VkFormat format, u32 width, u32 height, u32 mipLevels, u32 arrayLayers = 1)
	{
		assert(arrayLayers == 1); // TODO Support for arrayLayers != 1 has NOT been tested

		// Check if device supports linear blitting
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
		{
			throw std::runtime_error("Texture image format does not support linear blitting!");
		}

		VkImageMemoryBarrier barrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = VkImageSubresourceRange {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0, // Defined later
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

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
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
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

	static inline VkRenderPass CreateRenderPass(VkDevice device, 
		const std::vector<VkAttachmentDescription>& attachments,
		const std::vector<VkSubpassDescription>& subpasses,
		const std::vector<VkSubpassDependency>& dependencies)
	{
		// Renderpass
		VkRenderPassCreateInfo renderPassCI = {};
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCI.pNext = nullptr;
		renderPassCI.attachmentCount = (u32)attachments.size();
		renderPassCI.pAttachments = attachments.data();
		renderPassCI.subpassCount = (u32)subpasses.size();
		renderPassCI.pSubpasses = subpasses.data();
		renderPassCI.dependencyCount = (u32)dependencies.size();
		renderPassCI.pDependencies = dependencies.data();
		renderPassCI.flags = 0;

		VkRenderPass renderPass;
		if (VK_SUCCESS != vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass))
		{
			throw std::runtime_error("Failed to create RenderPass");
		}

		return renderPass;
	}

	static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
	static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
	static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes, bool vsync);
	static VkExtent2D ChooseSwapExtent(const VkExtent2D& desiredExtent, const VkSurfaceCapabilitiesKHR& capabilities);


	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);

	static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
		VkDebugUtilsMessengerEXT messenger,
		const VkAllocationCallbacks* pAllocator);

	inline static void UpdateDescriptorSet(VkDevice device, 
		const std::vector<VkWriteDescriptorSet>& descriptorWrites, 
		i32 descriptorCopyCount = 0, 
		VkCopyDescriptorSet* pCopyDescriptorSet = nullptr)
	{
		vkUpdateDescriptorSets(device, (u32)descriptorWrites.size(), descriptorWrites.data(), 
			descriptorCopyCount, 
			pCopyDescriptorSet);
	}
};
