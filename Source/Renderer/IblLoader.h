#pragma once 

#include "VulkanHelpers.h"
#include "VulkanInitializers.h"
#include "TextureResource.h"
#include "CubemapTextureLoader.h"
#include "EquirectangularCubemapLoader.h"

#include <Shared/CommonTypes.h>
#include <Shared/FileService.h>

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <chrono>

using vkh = VulkanHelpers;

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
		const MeshResource& skyboxMesh, const std::string& shaderDir, VkCommandPool transferPool, VkQueue transferQueue, 
		VkPhysicalDevice physicalDevice, VkDevice device)
	{
		auto env = CubemapTextureLoader::LoadFromFacePaths(paths, CubemapFormat::RGBA_F32, transferPool, transferQueue, physicalDevice, device);
		auto irradiance = CreateIrradianceFromEnvCubemap(env, skyboxMesh, shaderDir, transferPool, transferQueue, physicalDevice, device);
		auto prefilter = CreatePrefilterFromEnvCubemap(env, skyboxMesh, shaderDir, transferPool, transferQueue, physicalDevice, device);
		auto brdf = CreateBrdfLutFromEnvCubemap(env, shaderDir, transferPool, transferQueue, physicalDevice, device);

		IblTextureResources iblRes
		{
			std::move(env),
			std::move(irradiance),
			std::move(prefilter),
			std::move(brdf),
		};
		return iblRes;
	}

	static IblTextureResources LoadIblFromEquirectangularPath(const std::string& equirectangularHdrPath, 
		const MeshResource& skyboxMesh, const std::string& shaderDir, VkCommandPool transferPool, VkQueue transferQueue,
		VkPhysicalDevice physicalDevice, VkDevice device)
	{
		auto env = EquirectangularCubemapLoader::LoadFromPath(equirectangularHdrPath, skyboxMesh, shaderDir, transferPool, transferQueue, physicalDevice, device);
		auto irradiance = CreateIrradianceFromEnvCubemap(env, skyboxMesh, shaderDir, transferPool, transferQueue, physicalDevice, device);
		auto prefilter = CreatePrefilterFromEnvCubemap(env, skyboxMesh, shaderDir, transferPool, transferQueue,
			physicalDevice, device);

		auto brdf = CreateBrdfLutFromEnvCubemap(env, shaderDir, transferPool, transferQueue, physicalDevice, device);

		IblTextureResources iblRes
		{
			std::move(env),
			std::move(irradiance),
			std::move(prefilter),
			std::move(brdf),
		};
		return iblRes;
	}


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
			vkDestroyFramebuffer(device, Framebuffer, nullptr);
			vkFreeMemory(device, Memory, nullptr);
			vkDestroyImageView(device, View, nullptr);
			vkDestroyImage(device, Image, nullptr);
			
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
		//f32 DeltaPhi = (2.0f * (f32)PI) / 180.0f;
		//f32 DeltaTheta = (0.5f * (f32)PI) / 64.0f;
	};

	struct PrefilteredPushConstants
	{
		// Vert
		glm::mat4 Mvp{};

		// Frag
		f32 EnvMapResPerFace = 0;
		f32 Roughness = 0;
	};
	

#pragma region LoadIrradianceFromEnvCubemap

	static TextureResource CreateIrradianceFromEnvCubemap(const TextureResource& envMap, const MeshResource& skyboxMesh,
		const std::string& shaderDir, VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice,
		VkDevice device)
	{
		std::cout << "Generating irradianc cubemap\n";
		const auto benchStart = std::chrono::high_resolution_clock::now();

		const VkFormat irrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
		const i32 irrDim = 64;
		const u32 irrMips = 1;


		TextureResource irrCubemap = Shared_CreateCubeTextureResource(physicalDevice, device, irrFormat, irrDim, irrMips);


		auto renderPass = Shared_CreateRenderPass(device, irrFormat);


		auto renderTarget = Shared_CreateRenderTarget(device, physicalDevice, transferPool, transferQueue, renderPass, irrFormat, 
			irrDim);


		auto descPool = vkh::CreateDescriptorPool({ VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} }, 1, device);


		auto descSetLayout = vkh::CreateDescriptorSetLayout(device, {
			vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		});


		// Create pipeline layout
		VkPushConstantRange pushConstantRange = {};
		pushConstantRange.size = sizeof(IrradiancePushConstants);
		pushConstantRange.offset = 0;
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		const VkPipelineLayout pipelineLayout = vkh::CreatePipelineLayout(device, { descSetLayout }, { pushConstantRange });


		const auto vertPath = shaderDir + "Cubemap.vert.spv";
		const auto fragPath = shaderDir + "CubemapFromIrradianceConvolution.frag.spv";
		std::vector<VkVertexInputAttributeDescription> vertAttrDesc(1);
		{
			// Pos
			vertAttrDesc[0].binding = 0;
			vertAttrDesc[0].location = 0;
			vertAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertAttrDesc[0].offset = offsetof(Vertex, Pos);
		}
		const VkPipeline pipeline = Shared_CreatePipeline(device, pipelineLayout, renderPass, vertPath, fragPath, vertAttrDesc);


		// Allocate and Update Descriptor Sets
		auto descSet = vkh::AllocateDescriptorSets(1, descSetLayout, descPool, device)[0]; // Note [0]
		vkh::UpdateDescriptorSets(device, {
			vki::WriteDescriptorSet(descSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &envMap.DescriptorImageInfo())
			});


		RenderIrradianceMap(device, transferPool, transferQueue, renderPass, pipeline, pipelineLayout, descSet, 
			irrCubemap, skyboxMesh, renderTarget);


		// Cleanup
		vkDestroyRenderPass(device, renderPass, nullptr);
		renderTarget.Destroy(device);
		vkDestroyDescriptorPool(device, descPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		
		// Benchmark
		const auto benchEnd = std::chrono::high_resolution_clock::now();
		const auto benchDiff = std::chrono::duration<double, std::milli>(benchEnd - benchStart).count();
		std::cout << "Generating irradiance cube with " << irrMips << " mip levels took " << benchDiff << " ms\n";

		return irrCubemap;
	}

	static void RenderIrradianceMap(VkDevice device, VkCommandPool transferPool, VkQueue transferQueue, 
		VkRenderPass renderPass, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descSet, 
		TextureResource& irrTex, const MeshResource& skyboxMesh, RenderTarget& renderTarget)
	{
		std::vector<VkClearValue> clearValues(1);
		clearValues[0].color = { 0.0f, 0.0f, 0.2f, 0.0f };

		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(renderPass, renderTarget.Framebuffer,
			vki::Rect2D(0, 0, irrTex.Width(), irrTex.Height()),
			clearValues);


		std::array<glm::mat4, 6> matrices = 
		{
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		
		auto cmdBuf = vkh::BeginSingleTimeCommands(transferPool, device);

		VkViewport viewport = vki::Viewport(0, 0, (float)irrTex.Width(), (float)irrTex.Height(), 0.0f, 1.0f);
		VkRect2D scissor = vki::Rect2D(0, 0, irrTex.Width(), irrTex.Height());

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		const auto irrCubeSubresRange = vki::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, irrTex.MipLevels(), 0, 6);
		const auto renderTargetSubresRange = vki::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
		
		// Change image layout for all cubemap faces to transfer destination
		vkh::TransitionImageLayout(cmdBuf,
			irrTex.Image(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			irrCubeSubresRange);

		IrradiancePushConstants pushBlock{};
		
		for (u32 mip = 0; mip < irrTex.MipLevels(); mip++) 
		{
			for (u32 face = 0; face < 6; face++) 
			{
				viewport.width = f32(irrTex.Width() * std::pow(0.5f, mip));
				viewport.height = f32(irrTex.Height() * std::pow(0.5f, mip));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				
				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				{
					// Update shader push constant block
					pushBlock.Mvp = glm::perspective(f32(PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[face];

					vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
						sizeof(IrradiancePushConstants), &pushBlock);

					vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
					vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSet, 0, nullptr);

					VkDeviceSize offsets[1] = { 0 };
					vkCmdBindVertexBuffers(cmdBuf, 0, 1, &skyboxMesh.VertexBuffer, offsets);
					vkCmdBindIndexBuffer(cmdBuf, skyboxMesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(cmdBuf, (u32)skyboxMesh.IndexCount, 1, 0, 0, 0);
				}
				vkCmdEndRenderPass(cmdBuf);

				
				vkh::TransitionImageLayout(cmdBuf,
					renderTarget.Image,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, renderTargetSubresRange);

				
				// Copy image from the framebuffers to cube face
				{
					VkImageCopy copyRegion = {};
					copyRegion.srcSubresource = vki::ImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1);
					copyRegion.srcOffset = vki::Offset3D(0, 0, 0);
					copyRegion.dstSubresource = vki::ImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, mip, face, 1);
					copyRegion.dstOffset = vki::Offset3D(0, 0, 0);
					copyRegion.extent = vki::Extent3D(u32(viewport.width), u32(viewport.height), 1);

					vkCmdCopyImage(cmdBuf,
						renderTarget.Image,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						irrTex.Image(),
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&copyRegion);
				}

				
				// Transform framebuffer color attachment back 
				vkh::TransitionImageLayout(
					cmdBuf,
					renderTarget.Image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, renderTargetSubresRange);
			}
		}

		vkh::TransitionImageLayout(
			cmdBuf,
			irrTex.Image(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			irrCubeSubresRange);

		vkh::EndSingeTimeCommands(cmdBuf, transferPool, transferQueue, device);
	}
	
#pragma endregion 


#pragma region CreatePrefilterFromEnvCubemap

	static TextureResource CreatePrefilterFromEnvCubemap(const TextureResource& envMap, const MeshResource& skyboxMesh,
		const std::string& shaderDir, VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice,
		VkDevice device)
	{
		std::cout << "Generating prefilter convolution cubemap\n";
		const auto benchStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT; 
		const i32 dim = 512;
		const u32 numMips = 5;


		// TODO Sampler: GL_LINEAR_MIPMAP_LINEAR min filter!
		TextureResource prefilterCubemap = Shared_CreateCubeTextureResource(physicalDevice, device, format, dim, numMips);


		auto renderPass = Shared_CreateRenderPass(device, format);


		auto renderTarget = Shared_CreateRenderTarget(device, physicalDevice, transferPool, transferQueue, renderPass, format,
			dim);


		auto descPool = vkh::CreateDescriptorPool({ VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} }, 1, device);


		auto descSetLayout = vkh::CreateDescriptorSetLayout(device, {
			vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			});


		// Create Pipeline Layout
		VkPushConstantRange pushConstantRange = {};
		pushConstantRange.size = sizeof(PrefilteredPushConstants);
		pushConstantRange.offset = 0;
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		const VkPipelineLayout pipelineLayout = vkh::CreatePipelineLayout(device, { descSetLayout }, { pushConstantRange });

		
		// Create Pipeline
		const auto vertPath = shaderDir + "Cubemap.vert.spv";
		const auto fragPath = shaderDir + "CubemapFromPreFilterConvolution.frag.spv";
		std::vector<VkVertexInputAttributeDescription> vertAttrDesc(1);
		{
			// Pos
			vertAttrDesc[0].binding = 0;
			vertAttrDesc[0].location = 0;
			vertAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertAttrDesc[0].offset = offsetof(Vertex, Pos);
		}
		const VkPipeline pipeline = Shared_CreatePipeline(device, pipelineLayout, renderPass, vertPath, fragPath, vertAttrDesc);


		// Allocate and Update Descriptor Sets
		auto descSet = vkh::AllocateDescriptorSets(1, descSetLayout, descPool, device)[0]; // Note [0]
		vkh::UpdateDescriptorSets(device, {
			vki::WriteDescriptorSet(descSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &envMap.DescriptorImageInfo())
			});


		RenderPrefilterMap(device, transferPool, transferQueue, renderPass, pipeline, pipelineLayout, descSet,
			prefilterCubemap, skyboxMesh, renderTarget);


		// Cleanup
		vkDestroyRenderPass(device, renderPass, nullptr);
		renderTarget.Destroy(device);
		vkDestroyDescriptorPool(device, descPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);


		// Benchmark
		const auto benchEnd = std::chrono::high_resolution_clock::now();
		const auto benchDiff = std::chrono::duration<double, std::milli>(benchEnd - benchStart).count();
		std::cout << "Generating prefilter convolution cubemap with " << numMips << " mip levels took " << benchDiff << " ms\n";

		return prefilterCubemap;
	}

	static void RenderPrefilterMap(VkDevice device, VkCommandPool transferPool, VkQueue transferQueue,
		VkRenderPass renderPass, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descSet,
		TextureResource& targetTex, const MeshResource& skyboxMesh, RenderTarget& renderTarget)
	{
		std::vector<VkClearValue> clearValues(1);
		clearValues[0].color = { 0.0f, 0.2f, 0.0f, 0.0f };

		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(renderPass, renderTarget.Framebuffer,
			vki::Rect2D(0, 0, targetTex.Width(), targetTex.Height()),
			clearValues);


		std::array<glm::mat4, 6> viewMats =
		{
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};


		auto cmdBuf = vkh::BeginSingleTimeCommands(transferPool, device);

		VkViewport viewport = vki::Viewport(0, 0, (float)targetTex.Width(), (float)targetTex.Height(), 0.0f, 1.0f);
		VkRect2D scissor = vki::Rect2D(0, 0, targetTex.Width(), targetTex.Height());

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		const auto targetCubeSubresRange = vki::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, targetTex.MipLevels(), 0, 6);
		const auto renderTargetSubresRange = vki::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		// Change image layout for all cubemap faces to transfer destination
		vkh::TransitionImageLayout(cmdBuf,
			targetTex.Image(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			targetCubeSubresRange);

		PrefilteredPushConstants pushBlock{};

		for (u32 mip = 0; mip < targetTex.MipLevels(); mip++)
		{
			const auto mipDim = f32(targetTex.Width() * std::pow(0.5f, mip));

			viewport.width = mipDim;
			viewport.height = mipDim;
			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

			pushBlock.EnvMapResPerFace = mipDim;
			pushBlock.Roughness = mip / f32(targetTex.MipLevels() - 1); // mip 0 = 0, max mip = 1

			for (u32 face = 0; face < 6; face++)
			{
				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				{
					// Update shader push constant block
					pushBlock.Mvp = glm::perspective(f32(PI / 2.0), 1.0f, 0.1f, 512.0f) * viewMats[face];

					vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
						sizeof(PrefilteredPushConstants), &pushBlock);

					vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
					vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSet, 0, nullptr);

					VkDeviceSize offsets[1] = { 0 };
					vkCmdBindVertexBuffers(cmdBuf, 0, 1, &skyboxMesh.VertexBuffer, offsets);
					vkCmdBindIndexBuffer(cmdBuf, skyboxMesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(cmdBuf, (u32)skyboxMesh.IndexCount, 1, 0, 0, 0);
				}
				vkCmdEndRenderPass(cmdBuf);


				vkh::TransitionImageLayout(cmdBuf,
					renderTarget.Image,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, renderTargetSubresRange);


				// Copy image from the framebuffers to cube face
				{
					VkImageCopy copyRegion = {};
					copyRegion.srcSubresource = vki::ImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1);
					copyRegion.srcOffset = vki::Offset3D(0, 0, 0);
					copyRegion.dstSubresource = vki::ImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, mip, face, 1);
					copyRegion.dstOffset = vki::Offset3D(0, 0, 0);
					copyRegion.extent = vki::Extent3D(u32(viewport.width), u32(viewport.height), 1);

					vkCmdCopyImage(cmdBuf,
						renderTarget.Image,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						targetTex.Image(),
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&copyRegion);
				}


				// Transform framebuffer color attachment back 
				vkh::TransitionImageLayout(
					cmdBuf,
					renderTarget.Image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, renderTargetSubresRange);
			}
		}

		vkh::TransitionImageLayout(
			cmdBuf,
			targetTex.Image(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			targetCubeSubresRange);

		vkh::EndSingeTimeCommands(cmdBuf, transferPool, transferQueue, device);
	}

#pragma endregion 

	
#pragma region CreateBdrfLutFromEnvCubemap

	static TextureResource CreateBrdfLutFromEnvCubemap(const TextureResource & envMap, const std::string & shaderDir,
		VkCommandPool transferPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		std::cout << "Generating brdf\n";
		const auto benchStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;
		const i32 dim = 512;
		const u32 numMips = 1;
		const u32 arrayLayers = 1;


		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
		VkSampler sampler;

		// Create Image & Memory
		std::tie(image, memory) = vkh::CreateImage2D(
			dim, dim,
			numMips,
			VK_SAMPLE_COUNT_1_BIT,
			format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			physicalDevice, device, arrayLayers);


		// Create View
		view = vkh::CreateImage2DView(image, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, numMips, arrayLayers, device);


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

		
		
		auto renderPass = Shared_CreateRenderPass(device, format);

		
		// Create Render Target
		RenderTarget renderTarget = {};
		{
			renderTarget.Image = image;
			renderTarget.Memory = memory;
			renderTarget.View = view;
			renderTarget.Framebuffer = vkh::CreateFramebuffer(device, dim, dim, { view }, renderPass);
		}


		auto descPool = vkh::CreateDescriptorPool({ VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} }, 1, device);


		auto descSetLayout = vkh::CreateDescriptorSetLayout(device, {
			vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			});


		const VkPipelineLayout pipelineLayout = vkh::CreatePipelineLayout(device, { descSetLayout });


		// Create Pipeline
		const auto vertPath = shaderDir + "BrdfIntegration.vert.spv";
		const auto fragPath = shaderDir + "BrdfIntegration.frag.spv";
		std::vector<VkVertexInputAttributeDescription> vertAttrDesc(2);
		{
			// Pos
			vertAttrDesc[0].binding = 0;
			vertAttrDesc[0].location = 0;
			vertAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertAttrDesc[0].offset = offsetof(Vertex, Pos);
			// UVW
			vertAttrDesc[1].binding = 0;
			vertAttrDesc[1].location = 1;
			vertAttrDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
			vertAttrDesc[1].offset = offsetof(Vertex, TexCoord);
		}
		const VkPipeline pipeline = Shared_CreatePipeline(device, pipelineLayout, renderPass, vertPath, fragPath, vertAttrDesc);


		// Allocate and Update Descriptor Sets
		auto descSet = vkh::AllocateDescriptorSets(1, descSetLayout, descPool, device)[0]; // Note [0]
		vkh::UpdateDescriptorSets(device, {
			vki::WriteDescriptorSet(descSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &envMap.DescriptorImageInfo())
			});


		RenderBrdLut(device, physicalDevice, transferPool, transferQueue, renderPass, pipeline, pipelineLayout, descSet,
			renderTarget, dim);


		// Cleanup
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyFramebuffer(device, renderTarget.Framebuffer, nullptr);
		//renderTarget.Destroy(device);
		vkDestroyDescriptorPool(device, descPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);


		// Benchmark
		const auto benchEnd = std::chrono::high_resolution_clock::now();
		const auto benchDiff = std::chrono::duration<double, std::milli>(benchEnd - benchStart).count();
		std::cout << "Generating brdf lut took " << benchDiff << " ms\n";


		return TextureResource(device, dim, dim, numMips, arrayLayers, image, memory, view, sampler, format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	static void RenderBrdLut(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool transferPool, VkQueue transferQueue,
		VkRenderPass renderPass, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descSet,
		RenderTarget & renderTarget, u32 dim)
	{
		std::vector<VkClearValue> clearValues(1);
		clearValues[0].color = { 0.2f, 0.0f, 0.0f, 0.0f };

		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(
			renderPass, 
			renderTarget.Framebuffer, 
			vki::Rect2D(0, 0, dim, dim), 
			clearValues);


		// Create a temporary quad to render to
		MeshDefinition meshDefinition = {};
		{
			Vertex v0 = {};
			Vertex v1 = {};
			Vertex v2 = {};
			Vertex v3 = {};
			v0.Pos = { -1,-1, 0 };
			v1.Pos = { -1, 1, 0 };
			v2.Pos = {  1,-1, 0 };
			v3.Pos = {  1, 1, 0 };
			v0.TexCoord = { 0,1 };
			v1.TexCoord = { 0,0 };
			v2.TexCoord = { 1,1 };
			v3.TexCoord = { 1,0 };
			meshDefinition.Vertices.push_back(v0);
			meshDefinition.Vertices.push_back(v1);
			meshDefinition.Vertices.push_back(v2);
			meshDefinition.Vertices.push_back(v3);

			meshDefinition.Indices.push_back(0);
			meshDefinition.Indices.push_back(1);
			meshDefinition.Indices.push_back(2);
			meshDefinition.Indices.push_back(2);
			meshDefinition.Indices.push_back(1);
			meshDefinition.Indices.push_back(3);
		}
		
		MeshResource mesh{};
		std::tie(mesh.VertexBuffer, mesh.VertexBufferMemory)
			= vkh::CreateVertexBuffer(meshDefinition.Vertices, transferQueue, transferPool, physicalDevice, device);
		std::tie(mesh.IndexBuffer, mesh.IndexBufferMemory)
			= vkh::CreateIndexBuffer(meshDefinition.Indices, transferQueue, transferPool, physicalDevice, device);
		mesh.IndexCount = 6;
		mesh.VertexCount = 4;
		
		auto cmdBuf = vkh::BeginSingleTimeCommands(transferPool, device);

		VkViewport viewport = vki::Viewport(0, 0, (f32)dim, (f32)dim, 0.0f, 1.0f);
		VkRect2D scissor = vki::Rect2D(0, 0, dim, dim);

		
		// Render scene from cube face's point of view
		vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
			vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
			
			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSet, 0, nullptr);
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &mesh.VertexBuffer, offsets);
			vkCmdBindIndexBuffer(cmdBuf, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdBuf, (u32)mesh.IndexCount, 1, 0, 0, 0);
		}
		vkCmdEndRenderPass(cmdBuf);

		vkh::TransitionImageLayout(cmdBuf,
			renderTarget.Image,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
			vki::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
		
		vkh::EndSingeTimeCommands(cmdBuf, transferPool, transferQueue, device);

		// Cleanup
		vkDestroyBuffer(device, mesh.VertexBuffer, nullptr);
		vkDestroyBuffer(device, mesh.IndexBuffer, nullptr);
		vkFreeMemory(device, mesh.VertexBufferMemory, nullptr);
		vkFreeMemory(device, mesh.IndexBufferMemory, nullptr);
	}

#pragma endregion 


#pragma region Shared

	static TextureResource Shared_CreateCubeTextureResource(VkPhysicalDevice physicalDevice, VkDevice device,
		const VkFormat format, const i32 dim, const u32 numMips)
	{
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
		VkSampler sampler;

		const u32 arrayLayers = 6; // cube faces

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

		return TextureResource(device, dim, dim, numMips, arrayLayers, image, memory, view, sampler, format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	static VkRenderPass Shared_CreateRenderPass(VkDevice device, VkFormat format)
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

	static RenderTarget Shared_CreateRenderTarget(VkDevice device, VkPhysicalDevice physicalDevice, 
		VkCommandPool transferPool, VkQueue transferQueue, VkRenderPass renderPass, VkFormat format, i32 dim)
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

	static VkPipeline Shared_CreatePipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, const std::string& vertPath, const std::string& fragPath, 
		const std::vector<VkVertexInputAttributeDescription>& vertAttrDesc)
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
		VkVertexInputBindingDescription vertBindingDesc = Vertex::BindingDescription();

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
		vertShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(vertPath), device);
		vertShaderStage.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStage = {};
		fragShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStage.module = vkh::CreateShaderModule(FileService::ReadFile(fragPath), device);
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
	
#pragma endregion 
};

