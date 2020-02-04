#pragma once 


//#include "CubemapTextureLoader.h" // TexelsRgbaF16
#include "VulkanHelpers.h"
#include "TextureResource.h"

#include <Shared/CommonTypes.h>
#include <Shared/FileService.h>

#include <vulkan/vulkan.h>
#include <stbi/stb_image.h>

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <chrono>

using vkh = VulkanHelpers;
using vki = VulkanInitHelpers;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct IblTextureResources
{
	TextureResource EnvironmentCubemap;
	TextureResource IrradianceCubemap;
	TextureResource PrefilterCubemap;
	TextureResource BrdfLut;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IblLoader
{
public:
	static IblTextureResources LoadIblFromCubemapPath(const std::array<std::string, 6>& paths,
		const std::string& shaderDir, VkCommandPool commandPool, VkQueue graphicsQueue, VkPhysicalDevice physicalDevice,
		VkDevice device)
	{
		auto env = CubemapTextureLoader::LoadFromPath(paths, CubemapFormat::RGBA_F32, shaderDir, commandPool,
			graphicsQueue, physicalDevice, device);
		auto irradiance = CreateIrradianceFromEnvCubemap(env, shaderDir, commandPool, graphicsQueue, physicalDevice, 
			device);
		auto prefilter = CreatePrefilterFromEnvCubemap();
		auto brdf = CreateBrdfLutFromEnvCubemap();

		IblTextureResources iblRes
		{
			std::move(env),
			std::move(irradiance),
			std::move(prefilter),
			std::move(brdf),
		};
		return iblRes;
	}

	//static IblTextureResources LoadIblFromEquirectangularPath(const std::string& equirectangularHdrPath, const std::string& shaderDir, 
	//	VkCommandPool commandPool, VkQueue graphicsQueue, VkPhysicalDevice physicalDevice, VkDevice device)
	//{
	//	auto environmentCubemap = LoadCubemapFromEquirectangularPath(equirectangularHdrPath, shaderDir, commandPool, graphicsQueue, physicalDevice, device);
	//	auto irradianceCubemap = LoadCubemapFromPath(equirectangularHdrPath, shaderDir, commandPool, graphicsQueue, physicalDevice, device); //CreateIrradianceFromEnvCubemap();
	//	auto prefilterCubemap = LoadCubemapFromPath(equirectangularHdrPath, shaderDir, commandPool, graphicsQueue, physicalDevice, device); //CreatePrefilterFromEnvCubemap();
	//	auto brdfLut = LoadCubemapFromPath(equirectangularHdrPath, shaderDir, commandPool, graphicsQueue, physicalDevice, device); //CreateBrdfLutFromEnvCubemap();

	//	IblTextureResources iblRes
	//	{
	//		std::move(environmentCubemap),
	//		std::move(irradianceCubemap),
	//		std::move(prefilterCubemap),
	//		std::move(brdfLut),
	//	};
	//	return iblRes;
	//}


private:
	static constexpr f64 PI = 3.1415926535897932384626433;
	
	struct RenderTarget
	{
		VkImage Image;
		VkImageView View;
		VkDeviceMemory Memory;
		VkFramebuffer Framebuffer;

		void Destroy(VkDevice device)
		{
			vkDestroyImage(device, Image, nullptr);
			vkFreeMemory(device, Memory, nullptr);
			vkDestroyImageView(device, View, nullptr);
			vkDestroyFramebuffer(device, Framebuffer, nullptr);
			
			Image = nullptr;
			View = nullptr;
			Memory = nullptr;
			Framebuffer = nullptr;
		}
	};

	struct IrradiancePushConstants
	{
		glm::mat4 Mvp{};

		// Sampling deltas
		float DeltaPhi = (2.0f * (f32)PI) / 180.0f;
		float DeltaTheta = (0.5f * (f32)PI) / 64.0f;
	} ;
	
#pragma region LoadCubemapFromEquirectangularPath

	static TextureResource LoadCubemapFromEquirectangularPath(const std::string& path, const std::string& shaderDir,
		VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		// Load source image to convert
		TexelsRgbaF32 srcTexels = {};
		srcTexels.Load(path);

		const VkFormat srcFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
		const VkFormat dstFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

		VkDescriptorImageInfo srcImageInfo = {};
		VkImage srcImage;
		VkDeviceMemory srcMemory;
		std::tie(srcImage, srcMemory) = CreateSrcImage(srcTexels, device, physicalDevice, transferPool, transferQueue);

		srcImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		srcImageInfo.imageView = CreateSrcImageView(srcImage, device);
		srcImageInfo.sampler = CreateSrcSampler(device);

		auto mipLevels = Texels::CalcMipLevels(srcTexels.Width(), srcTexels.Height());
		TextureResource src = { device, srcTexels.Width(), srcTexels.Height(), mipLevels, 1,
			srcImage, srcMemory, srcImageInfo.imageView, srcImageInfo.sampler, srcFormat };
		return src;
		CreateColorAttachment(device);
		CreateSubpassDependencies(device);
		//VkRenderPass renderPass = CreateRenderPass(device, physicalDevice, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32B32A32_SFLOAT);
		CreateOffscreenFramebuffer(device);

		auto descriptorPool = CreateDescriptorPool(device);
		auto descriptorSetLayout = CreateDescriptorSetLayout(device);
		auto descriptorSet = CreateDescriptorSets(srcImageInfo, device, descriptorPool, descriptorSetLayout);

		auto pipelineLayout = vkh::CreatePipelineLayout(device, { descriptorSetLayout });
		//auto shaders = LoadShaders(shaderDir, device);
		//VkPipeline CreatePipeline(device, pipelineLayout, shaders);

		//Render(device);

		OptimiseImageForRead(device);

		// Cleanup
		/*vkDestroySampler(device, srcImageInfo.sampler, nullptr);
		vkDestroyImageView(device, srcImageInfo.imageView, nullptr);
		vkDestroyImage(device, srcImage, nullptr);
		vkFreeMemory(device, srcMemory, nullptr);*/

		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);



		VkImage dstImage{};
		VkDeviceMemory dstMemory{};
		VkImageView dstView{};
		u32 dstWidth{}, dstHeight{};
		u32 dstMipLevels{};
		u32 dstLayerCount{};

		VkSampler dstSampler{};
		VkImageLayout dstLayout{};


		return TextureResource(device, dstWidth, dstHeight, dstMipLevels, dstLayerCount, dstImage, dstMemory, dstView, dstSampler, dstFormat, dstLayout);
	}

	static std::tuple<VkImage, VkDeviceMemory> CreateSrcImage(const TexelsRgbaF32& texels,
		VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool transferPool, VkQueue transferQueue)
	{
		// Create staging buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		auto size = texels.Data().size() * sizeof(float);
		auto size2 = texels.Width() * texels.Height() * 8; // Bpp
		auto size3 = texels.DataSize(); // Bpp
		std::tie(stagingBuffer, stagingBufferMemory) = vkh::CreateBuffer(
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage flags
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // property flags
			device, physicalDevice);


		// Copy texels from system mem to GPU staging buffer
		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, size, 0, &data);
		memcpy(data, texels.Data().data(), size);
		vkUnmapMemory(device, stagingBufferMemory);


		// Create image buffer
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
		std::tie(textureImage, textureImageMemory) = vkh::CreateImage2D(
			texels.Width(), texels.Height(),
			1, // miplevels
			VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R16G16B16A16_SFLOAT, // format
			VK_IMAGE_TILING_OPTIMAL, // tiling
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, //usageflags
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //propertyflags
			physicalDevice, device);


		const auto cmdBuffer = vkh::BeginSingleTimeCommands(transferPool, device);

		// Transition image layout to optimal for copying to it from the staging buffer
		VkImageSubresourceRange subresourceRange = {};
		{
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = 1;
		}
		vkh::TransitionImageLayout(cmdBuffer, textureImage,
			VK_IMAGE_LAYOUT_UNDEFINED, // from
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // to
			subresourceRange);

		vkh::CopyBufferToImage(cmdBuffer, stagingBuffer, textureImage, texels.Width(), texels.Height());

		vkh::EndSingeTimeCommands(cmdBuffer, transferPool, transferQueue, device);


		return { textureImage, textureImageMemory };
	}


	static VkImageView CreateSrcImageView(VkImage image, VkDevice device)
	{
		return vkh::CreateImage2DView(image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, 
			VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, device);
	}
	static VkSampler CreateSrcSampler(VkDevice device)
	{
		VkSamplerCreateInfo samplerCI = {};
		{
			samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCI.magFilter = VK_FILTER_LINEAR;
			samplerCI.minFilter = VK_FILTER_LINEAR;
			samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCI.anisotropyEnable = VK_TRUE; // TODO Test to see if anisotrophy is supported and it's max
			samplerCI.maxAnisotropy = 16;         // TODO Test to see if anisotrophy is supported and it's max
			samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // applied with addressMode is clamp
			samplerCI.unnormalizedCoordinates = VK_FALSE; // false addresses tex coord via [0,1), true = [0,dimensionSize]
			samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCI.mipLodBias = 0;
			samplerCI.minLod = 0;
			samplerCI.maxLod = 1;
		}

		VkSampler textureSampler;
		if (VK_SUCCESS != vkCreateSampler(device, &samplerCI, nullptr, &textureSampler))
		{
			throw std::runtime_error("Failed to create texture sampler");
		}

		return textureSampler;
	}

	static void CreateColorAttachment(VkDevice device) {}
	static void CreateSubpassDependencies(VkDevice device) {}
	//static VkRenderPass CreateRenderPass(VkDevice device, VkPhysicalDevice physicalDevice, VkSampleCountFlagBits msaaSamples, VkFormat format)
	//{
	//	// Color attachment
	//	VkAttachmentDescription colorAttachmentDesc = {};
	//	{
	//		colorAttachmentDesc.format = swapchainFormat;
	//		colorAttachmentDesc.samples = msaaSamples;
	//		colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // what to do with color/depth data before rendering
	//		colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // what to do with color/depth data after rendering
	//		colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // not using stencil
	//		colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//		colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // mem layout before renderpass
	//		colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // memory layout after renderpass
	//	}
	//	VkAttachmentReference colorAttachmentRef = {};
	//	{
	//		colorAttachmentRef.attachment = 0;
	//		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//	}


	//	// Depth attachment  -  multisample depth doesn't need to be resolved as it won't be displayed
	//	VkAttachmentDescription depthAttachmentDesc = {};
	//	{
	//		depthAttachmentDesc.format = FindDepthFormat(physicalDevice);
	//		depthAttachmentDesc.samples = msaaSamples;
	//		depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	//		depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // not used after drawing
	//		depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 
	//		depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//		depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//		depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	//	}
	//	VkAttachmentReference depthAttachmentRef = {};
	//	{
	//		depthAttachmentRef.attachment = 1;
	//		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	//	}


	//	// Color resolve attachment  -  used to resolve multisampled image into one that can be displayed
	//	VkAttachmentDescription colorAttachmentResolveDesc = {};
	//	{
	//		colorAttachmentResolveDesc.format = swapchainFormat;
	//		colorAttachmentResolveDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	//		colorAttachmentResolveDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	//		colorAttachmentResolveDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	//		colorAttachmentResolveDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	//		colorAttachmentResolveDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//		colorAttachmentResolveDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//		colorAttachmentResolveDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//	}
	//	VkAttachmentReference colorAttachmentResolveRef = {};
	//	{
	//		colorAttachmentResolveRef.attachment = 2;
	//		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//	}


	//	// Associate color and depth attachements with a subpass
	//	VkSubpassDescription subpassDesc = {};
	//	{
	//		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	//		subpassDesc.colorAttachmentCount = 1;
	//		subpassDesc.pColorAttachments = &colorAttachmentRef;
	//		subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;
	//		subpassDesc.pResolveAttachments = &colorAttachmentResolveRef;
	//	}


	//	// Set subpass dependency for the implicit external subpass to wait for the swapchain to finish reading from it
	//	VkSubpassDependency subpassDependency = {};
	//	{
	//		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL; // implicit subpass before render
	//		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	//		subpassDependency.srcAccessMask = 0;
	//		subpassDependency.dstSubpass = 0; // this pass
	//		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	//		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	//	}


	//	// Create render pass
	//	std::array<VkAttachmentDescription, 3> attachments = {
	//		colorAttachmentDesc,
	//		depthAttachmentDesc,
	//		colorAttachmentResolveDesc
	//	};

	//	VkRenderPassCreateInfo renderPassCI = {};
	//	{
	//		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	//		renderPassCI.attachmentCount = (uint32_t)attachments.size();
	//		renderPassCI.pAttachments = attachments.data();
	//		renderPassCI.subpassCount = 1;
	//		renderPassCI.pSubpasses = &subpassDesc;
	//		renderPassCI.dependencyCount = 1;
	//		renderPassCI.pDependencies = &subpassDependency;
	//	}

	//	VkRenderPass renderPass;
	//	if (vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass) != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed to create render pass");
	//	}

	//	return renderPass;
	//}
	static void CreateOffscreenFramebuffer(VkDevice device) {}


	static VkDescriptorPool CreateDescriptorPool(VkDevice device)
	{
		const std::vector<VkDescriptorPoolSize> poolSizes = { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} };
		return vkh::CreateDescriptorPool(poolSizes, 2, device);
	}
	static VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device)
	{
		return vkh::CreateDescriptorSetLayout(device, {
			vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		});
	}
	static VkDescriptorSet CreateDescriptorSets(const VkDescriptorImageInfo& imageInfo, VkDevice device,
		VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout)
	{
		const auto set = vkh::AllocateDescriptorSets(1, descriptorSetLayout, descriptorPool, device)[0]; // Note [0]

		std::array<VkWriteDescriptorSet, 1> descriptorWrites{};
		{
			vki::WriteDescriptorSet(set, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &imageInfo);
		}

		vkUpdateDescriptorSets(device, (u32)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);

		return set;
	}

	static void LoadShaders(const std::string& shaderDir, VkDevice device)
	{
		const auto vertShaderCode = FileService::ReadFile(shaderDir + "Cubemap.vert.vert.spv");
		const auto fragShaderCode = FileService::ReadFile(shaderDir + "CubemapFromEquirectangular.frag.frag.spv");

		VkShaderModule vertShaderModule = vkh::CreateShaderModule(vertShaderCode, device);
		VkShaderModule fragShaderModule = vkh::CreateShaderModule(fragShaderCode, device);



	}
	//static VkPipeline CreatePipeline(VkDevice device)
	//{
	//	// Create the Pipeline  -  Finally!...
	//	VkGraphicsPipelineCreateInfo graphicsPipelineCI = {};
	//	{
	//		graphicsPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	//		// Programmable
	//		graphicsPipelineCI.stageCount = (uint32_t)shaderStageCIs.size();
	//		graphicsPipelineCI.pStages = shaderStageCIs.data();

	//		// Fixed function
	//		graphicsPipelineCI.pVertexInputState = &vertexInputCI;
	//		graphicsPipelineCI.pInputAssemblyState = &inputAssemblyCI;
	//		graphicsPipelineCI.pViewportState = &viewportCI;
	//		graphicsPipelineCI.pRasterizationState = &rasterizationCI;
	//		graphicsPipelineCI.pMultisampleState = &multisampleCI;
	//		graphicsPipelineCI.pDepthStencilState = &depthStencilCI;
	//		graphicsPipelineCI.pColorBlendState = &colorBlendCI;
	//		graphicsPipelineCI.pDynamicState = nullptr;

	//		graphicsPipelineCI.layout = pipelineLayout;

	//		graphicsPipelineCI.renderPass = renderPass;
	//		graphicsPipelineCI.subpass = 0;

	//		graphicsPipelineCI.basePipelineHandle = VK_NULL_HANDLE; // is our pipeline derived from another?
	//		graphicsPipelineCI.basePipelineIndex = -1;
	//	}
	//	VkPipeline pipeline;
	//	if (vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCI, nullptr, &pipeline) != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed to create Pipeline");
	//	}
	//}

	static void OptimiseImageForRead(VkDevice device) {}

#pragma endregion


#pragma region LoadIrradianceFromEnvCubemap

	static TextureResource CreateIrradianceFromEnvCubemap(const TextureResource& envMap, const std::string& shaderDir,
	                                                      VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		const auto benchStart = std::chrono::high_resolution_clock::now();

		const VkFormat irrFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
		const i32 irrDim = 64;
		const u32 irrMips = (u32)(floor(log2(irrDim))) + 1;


		TextureResource irrCubemap = CreateCubeTextureResource(physicalDevice, device, irrFormat, irrDim, irrMips);


		auto renderPass = CreateRenderPass(device, irrFormat);


		auto renderTarget = CreateRenderTarget(device, physicalDevice, transferPool, transferQueue, renderPass, irrFormat, 
			irrDim);


		auto descSetLayout = vkh::CreateDescriptorSetLayout(device, {
			vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		});

		
		auto descPool = vkh::CreateDescriptorPool({ VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} }, 1, device);


		// Allocate and Update Descriptor Sets
		auto descSet = vkh::AllocateDescriptorSets(1, descSetLayout, descPool, device)[0]; // Note [0]
		vkh::UpdateDescriptorSets(device, { 
			vki::WriteDescriptorSet(descSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &envMap.DescriptorImageInfo())
		});

		const VkPipelineLayout pipelineLayout = vkh::CreatePipelineLayout(device, { descSetLayout });


		const VkPipeline pipeline = CreatePipeline(device, pipelineLayout, renderPass, shaderDir);


		RenderIrradianceMap(device, renderPass, pipeline, pipelineLayout, descSet, irrMips, irrCubemap, renderTarget);


		// Cleanup
		renderTarget.Destroy(device);
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorPool(device, descPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);

		
		// Benchmark
		const auto benchEnd = std::chrono::high_resolution_clock::now();
		const auto benchDiff = std::chrono::duration<double, std::milli>(benchEnd - benchStart).count();
		std::cout << "Generating irradiance cube with " << irrMips << " mip levels took " << benchDiff << " ms\n";

		return irrCubemap;
	}

	static TextureResource CreateCubeTextureResource(VkPhysicalDevice physicalDevice, VkDevice device, 
		const VkFormat format, const i32 dim, const u32 numMips)
	{
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
		VkSampler sampler;

		const u32 arrayLayers = 6;
		
		// Create Image & Memory
		std::tie(image, memory) = vkh::CreateImage2D(
			dim, dim, 
			numMips, 
			VK_SAMPLE_COUNT_1_BIT, 
			format, 
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			physicalDevice, device, arrayLayers, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

		
		// Create View
		view = vkh::CreateImage2DView(image, format, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_ASPECT_COLOR_BIT, numMips, arrayLayers, device);


		// Create Sampler
		{
			VkSamplerCreateInfo samplerCI = {};
			samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCI.magFilter = VK_FILTER_LINEAR;
			samplerCI.minFilter = VK_FILTER_LINEAR;
			samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // applied with addressMode is clamp
			samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCI.mipLodBias = 0;
			samplerCI.minLod = 0;
			samplerCI.maxLod = (float)numMips;
			samplerCI.anisotropyEnable = VK_FALSE;
			samplerCI.maxAnisotropy = 1;

			if (VK_SUCCESS != vkCreateSampler(device, &samplerCI, nullptr, &sampler))
			{
				throw std::runtime_error("Failed to create cubemap sampler");
			}
		}

		return TextureResource(device, dim, dim, numMips, 6, image, memory, view, sampler, format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	static VkRenderPass CreateRenderPass(VkDevice device, VkFormat format)
	{
		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies{};
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkAttachmentDescription colorAttachmentDesc = {};
		{
			colorAttachmentDesc.format = format;
			colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		{
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorReference;
		}
		

		// Renderpass
		VkRenderPassCreateInfo renderPassCI = {};
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &colorAttachmentDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = (u32)dependencies.size();
		renderPassCI.pDependencies = dependencies.data();
		
		VkRenderPass renderPass;
		if (VK_SUCCESS != vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass))
		{
			throw std::runtime_error("Failed to create RenderPass");
		}

		return renderPass;
	}

	static RenderTarget CreateRenderTarget(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool transferPool,
		VkQueue transferQueue, VkRenderPass renderPass, VkFormat format, i32 dim)
	{
		RenderTarget rt{};
		
		std::tie(rt.Image, rt.Memory) = vkh::CreateImage2D(dim, dim, 1, 
			VK_SAMPLE_COUNT_1_BIT, 
			format, 
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			physicalDevice, device);


		// Transition image layout
		{
			const auto cmdBuf = vkh::BeginSingleTimeCommands(transferPool, device);
			
			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = 1;
			
			vkh::TransitionImageLayout(cmdBuf, rt.Image,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, subresourceRange);

			vkh::EndSingeTimeCommands(cmdBuf, transferPool, transferQueue, device);
		}

		
		rt.View = vkh::CreateImage2DView(rt.Image, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, device);

		rt.Framebuffer = vkh::CreateFramebuffer(device, dim, dim, { rt.View }, renderPass);

		return rt;
	}

	static VkPipeline CreatePipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, const std::string& shaderDir)
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
		inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyState.flags = 0;
		inputAssemblyState.primitiveRestartEnable = VK_FALSE;

		VkPipelineRasterizationStateCreateInfo rasterizationState = {};
		rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationState.flags = 0;
		rasterizationState.depthClampEnable = VK_FALSE;
		rasterizationState.lineWidth = 1.0f;

		VkPipelineColorBlendAttachmentState blendAttachmentState = {};
		blendAttachmentState.colorWriteMask = 0xf;
		blendAttachmentState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlendState = {};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &blendAttachmentState;

		VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_FALSE;
		depthStencilState.depthWriteEnable = VK_FALSE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		viewportState.flags = 0;

		VkPipelineMultisampleStateCreateInfo multisampleState = {};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleState.flags = 0;

		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables.data();
		dynamicState.dynamicStateCount = (u32)dynamicStateEnables.size();
		dynamicState.flags = 0;

		
		// Vertex Input  -  Define the format of the vertex data passed to the vert shader
		auto vertBindingDesc = Vertex::BindingDescription();
		//std::array<VkVertexInputAttributeDescription, 1> vertAttrDesc = {};
		//{
		//	// Pos
		//	vertAttrDesc[0].binding = 0;
		//	vertAttrDesc[0].location = 0;
		//	vertAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		//	vertAttrDesc[0].offset = offsetof(Vertex, Pos);
		//}
		auto vertAttrDesc = Vertex::AttributeDescriptions();
		
		VkPipelineVertexInputStateCreateInfo vertexInputState = {};
		vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputState.vertexBindingDescriptionCount = 1;
		vertexInputState.pVertexBindingDescriptions = &vertBindingDesc;
		vertexInputState.vertexAttributeDescriptionCount = (u32)vertAttrDesc.size();
		vertexInputState.pVertexAttributeDescriptions = vertAttrDesc.data();

		// Shaders
		VkPipelineShaderStageCreateInfo vertShaderStage = {};
		vertShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(shaderDir + "Cubemap.vert.spv"), device);
		vertShaderStage.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStage = {};
		fragShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(shaderDir + "CubemapFromIrradianceConvolution.frag.spv"), device);
		fragShaderStage.pName = "main";
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{ vertShaderStage, fragShaderStage };
		

		// Create the pipeline
		VkGraphicsPipelineCreateInfo pipelineCI = {};
		pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.pVertexInputState = &vertexInputState;
		pipelineCI.stageCount = (u32)shaderStages.size();
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.renderPass = renderPass;
		pipelineCI.layout = pipelineLayout;

		VkPipeline pipeline;
		if (VK_SUCCESS != vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCI, nullptr, &pipeline))
		{
			throw std::runtime_error("Failed to create pipeline");
		}

		
		// Cleanup
		vkDestroyShaderModule(device, vertShaderStage.module, nullptr);
		vkDestroyShaderModule(device, fragShaderStage.module, nullptr);

		
		return pipeline;
	}

	static void RenderIrradianceMap(VkDevice device, VkRenderPass renderPass, VkPipeline pipeline, 
		VkPipelineLayout pipelineLayout, VkDescriptorSet descSet, u32 irrMips, TextureResource& irrCubemap, 
		RenderTarget& renderTarget)
	{
	
	}
	
#pragma endregion 

	
	static TextureResource CreatePrefilterFromEnvCubemap()
	{
		TextureResource tr{ nullptr, 1,1,1,1,nullptr,nullptr,nullptr,nullptr,VkFormat{} };
		return tr;
	}

	static TextureResource CreateBrdfLutFromEnvCubemap()
	{
		TextureResource tr{ nullptr, 1,1,1,1,nullptr,nullptr,nullptr,nullptr,VkFormat{} };
		return tr;
	}

};

