#pragma once

#include "Renderer.h"
#include "RenderPasses/ShadowMap.h"
#include "Renderer/UniformBufferObjects.h"

#include "Framebuffer.h"
#include "VulkanService.h" // TODO Investigate why compilation breaks if above TextureResource.h
#include "CubemapTextureLoader.h"

#include <Framework/IModelLoaderService.h> // Used for mesh/model/texture definitions TODO remove dependency?
#include <Framework/CommonTypes.h>

#include <vector>


struct SceneRendererPrimitives
{
	std::vector<RenderableResourceId> RenderableIds;
	std::vector<glm::mat4> RenderableTransforms;
	std::vector<Light> Lights;
	glm::vec3 ViewPosition;
	glm::mat4 ViewMatrix;
	glm::mat4 ProjectionMatrix;
	glm::mat4 LightSpaceMatrix;
};


class SceneRenderer
{
public: // Data
private:// Data
	// Dependencies
	VulkanService& _vk;
	Renderer& _renderer;

	// Resources
	std::unique_ptr<FramebufferResources> _sceneFramebuffer = nullptr;
	ShadowmapDrawResources _shadowDrawResources;
	std::string _shaderDir;

public: // Methods
	SceneRenderer(VulkanService& vulkanService, Renderer& renderer, std::string shaderDir, const std::string& assetsDir, IModelLoaderService& modelLoaderService) : _vk(vulkanService), _renderer(renderer), _shaderDir(std::move(shaderDir))
	{
	}

	void Init(u32 width, u32 height)
	{
		_shadowDrawResources = ShadowmapDrawResources{ { 4096,4096 }, _shaderDir, _vk, _renderer.Hack_GetPbrPipelineLayout() };
		_sceneFramebuffer = CreateSceneFramebuffer(width, height);
	}

	void Destroy()
	{
		_sceneFramebuffer->Destroy();
		_shadowDrawResources.Destroy(_vk.LogicalDevice(), _vk.Allocator());
	}

	void Resize(u32 width, u32 height)
	{
		_sceneFramebuffer->Destroy();
		_sceneFramebuffer = CreateSceneFramebuffer(width, height);
	}

	void Draw(u32 imageIndex, VkCommandBuffer commandBuffer, const SceneRendererPrimitives& scene, const RenderOptions& options)
	{
		// Update all descriptors
		_renderer.UpdateDescriptors(options); // also update other passes?

		
		const auto& renderableIds = scene.RenderableIds;
		const auto& transforms = scene.RenderableTransforms;
		const auto& lights = scene.Lights;
		

		// Draw shadow pass
		auto lightSpaceMatrix = glm::identity<glm::mat4>();
		if (FindShadowCasterMatrix(lights, lightSpaceMatrix))
		{
			const auto& renderables = _renderer.Hack_GetRenderables(); // TODO extract resources from Renderer
			const auto& meshes = _renderer.Hack_GetMeshes();           // TODO extract resources from Renderer

			
			// Update UBOs - TODO introduce a new MVP only vert shader only ubo for use with Pbr and Shadow shaders. 
			for (size_t i = 0; i < renderableIds.size(); i++)
			{
				const auto& renderable = *renderables[renderableIds[i].Id];
				// TODO need to create UBOs 
				const auto& modelBufferMemory = renderable.FrameResources[imageIndex].UniformBufferMemory; // NOTE This memory is used for UniversalUbo PBR rendering!

				UniversalUbo ubo = {};
				ubo.LightSpaceMatrix = lightSpaceMatrix;
				ubo.Model = transforms[i];

				// Copy to gpu - TODO PERF Keep mem mapped 
				void* data;
				auto size = sizeof(ubo);
				vkMapMemory(_vk.LogicalDevice(), modelBufferMemory, 0, size, 0, &data);
				memcpy(data, &ubo, size);
				vkUnmapMemory(_vk.LogicalDevice(), modelBufferMemory);
			}


			// Draw objects

			std::vector<VkClearValue> clearColors(1);
			clearColors[0].depthStencil = { 1.f, 0 };

			float depthBiasConstant = 1.0f;
			float depthBiasSlope = 1.f;
			auto& shadow = _shadowDrawResources;
			auto shadowRect = vki::Rect2D(0, 0, shadow.Resolution.width, shadow.Resolution.height);
			auto shadowViewport = vki::Viewport(shadowRect);
			const auto renderPassBeginInfo = vki::RenderPassBeginInfo(shadow.RenderPass, shadow.Framebuffer->Framebuffer,
				shadowRect,
				clearColors);

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &shadowRect);
				vkCmdSetDepthBias(commandBuffer, depthBiasConstant, 0, depthBiasSlope);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow.Pipeline);

				for (const auto& id : renderableIds)
				{
					const auto& renderable = renderables[id.Id].get();
					const auto& mesh = *meshes[renderable->MeshId.Id];

					// Draw mesh
					VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdBindDescriptorSets(commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS, _renderer.Hack_GetPbrPipelineLayout(),
						0, 1, &renderable->FrameResources[imageIndex].DescriptorSet, 0, nullptr);
					vkCmdDrawIndexed(commandBuffer, (u32)mesh.IndexCount, 1, 0, 0, 0);
				}
			}
			vkCmdEndRenderPass(commandBuffer);
		}


		// Draw scene to gbuf
		{
			// Scene Viewport - Only the part of the screen showing the scene.
			auto renderRect = vki::Rect2D({ 0, 0 }, { _sceneFramebuffer->Desc.Extent });
			auto renderViewport = vki::Viewport(renderRect);

			// Clear colour
			std::vector<VkClearValue> clearColors(2);
			clearColors[0].color = { 1.f, 1.f, 0.f, 1.f };
			clearColors[1].depthStencil = { 1.f, 0ui32 };

			const auto renderPassBeginInfo = vki::RenderPassBeginInfo(_renderer.GetRenderPass(),
				_sceneFramebuffer->Framebuffer,
				renderRect,
				clearColors);

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				vkCmdSetViewport(commandBuffer, 0, 1, &renderViewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &renderRect);

				// Calc Projection
				const auto vfov = 45.f;
				const auto aspect = _sceneFramebuffer->Desc.Extent.width / (f32)_sceneFramebuffer->Desc.Extent.height;
				auto projection = glm::perspective(glm::radians(vfov), aspect, 0.05f, 1000.f);
				projection = glm::scale(projection, glm::vec3{ 1.f,-1.f,1.f });// flip Y to convert glm from OpenGL coord system to Vulkan
				
				_renderer.Draw(commandBuffer, imageIndex, options, renderableIds, transforms, lights, scene.ViewMatrix, projection, scene.ViewPosition, lightSpaceMatrix);
			}
			vkCmdEndRenderPass(commandBuffer);
		}
	}

	VkDescriptorImageInfo GetOutputDescritpor() const { return _sceneFramebuffer->OutputDescriptor; }

	VkDescriptorImageInfo TEMP_GetShadowmapDescriptor() const { return _shadowDrawResources.Framebuffer->OutputDescriptor; }

private:// Methods

	std::unique_ptr<FramebufferResources> CreateSceneFramebuffer(u32 width, u32 height) const
	{
		// Scene framebuffer is only the size of the scene render region on screen
		return std::make_unique<FramebufferResources>(FramebufferResources::CreateSceneFramebuffer(
			VkExtent2D{ width, height },
			VK_FORMAT_R16G16B16A16_SFLOAT,
			_renderer.GetRenderPass(),
			_vk.MsaaSamples(),
			_vk.LogicalDevice(), _vk.PhysicalDevice(), _vk.Allocator()));
	}

	// Returns light transform matrix if found, otherwise identity
	static bool FindShadowCasterMatrix(const std::vector<Light>& lights, glm::mat4& outMatrix)
	{
		// Find teh first direction light as use as a shadow caster
		for (auto&& light : lights)
		{
			if (light.Type == Light::LightType::Directional)
			{
				const auto view = glm::lookAt(light.Pos, { 0,0,0 }, { 0,1,0 });
				const auto projection = glm::ortho(-50.f, 50.f, -50.f, 50.f, -50.f, 50.f); // TODO Set the bounds dynamically. Near/Far clip planes seems weird. -50 near solves odd clipping issues. Check my understanding of this.

				outMatrix = projection * view;
				return true;
			}
		}

		outMatrix = glm::identity<glm::mat4>();
		return false;
	}
};
