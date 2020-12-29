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
		_shadowDrawResources = ShadowmapDrawResources{ { 4096,4096 }, _shaderDir, _vk };
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


		// Draw shadow pass
		auto lightSpaceMatrix = glm::identity<glm::mat4>();
		if (FindShadowCasterMatrix(scene.Lights, lightSpaceMatrix))
		{
			const auto shadowRenderArea = vki::Rect2D({}, _shadowDrawResources.Framebuffer->Desc.Extent);
			
			auto beginInfo = vki::RenderPassBeginInfo(*_shadowDrawResources.Framebuffer, shadowRenderArea);
			vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				_shadowDrawResources.Draw(commandBuffer, shadowRenderArea, 
					scene, lightSpaceMatrix, 
					_renderer.Hack_GetRenderables(),// TODO extract resources from Renderer into a common resource manager
					_renderer.Hack_GetMeshes());    // TODO extract resources from Renderer into a common resource manager
			}
			vkCmdEndRenderPass(commandBuffer);
		}


		// Draw scene to gbuf
		{
			// Scene Viewport - Only the part of the screen showing the scene.
			auto sceneRenderArea = vki::Rect2D({}, _sceneFramebuffer->Desc.Extent);
			auto sceneViewport = vki::Viewport(sceneRenderArea);

			const auto renderPassBeginInfo = vki::RenderPassBeginInfo(_renderer.GetRenderPass(),
				_sceneFramebuffer->Framebuffer,
				sceneRenderArea,
				_sceneFramebuffer->Desc.ClearValues);

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				vkCmdSetViewport(commandBuffer, 0, 1, &sceneViewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &sceneRenderArea);

				// Calc Projection
				const auto vfov = 45.f;
				const auto aspect = _sceneFramebuffer->Desc.Extent.width / (f32)_sceneFramebuffer->Desc.Extent.height;
				auto projection = glm::perspective(glm::radians(vfov), aspect, 0.05f, 1000.f);
				projection = glm::scale(projection, glm::vec3{ 1.f,-1.f,1.f });// flip Y to convert glm from OpenGL coord system to Vulkan
				
				_renderer.Draw(commandBuffer, imageIndex, options, scene.RenderableIds, scene.RenderableTransforms, scene.Lights, scene.ViewMatrix, projection, scene.ViewPosition, lightSpaceMatrix);
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
