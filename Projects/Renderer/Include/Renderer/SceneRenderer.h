#pragma once

#include "Renderer.h"
#include "RenderPasses/ShadowMap.h"

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

	void Draw()
	{

	}

	VkDescriptorImageInfo GetOutputDescritpor() const { return _sceneFramebuffer->OutputDescriptor; }

	const FramebufferResources& TEMP_GetFramebufferRef() const { return *_sceneFramebuffer; }
	const ShadowmapDrawResources& TEMP_GetShadowmapResourcesRef() const { return _shadowDrawResources; }

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
};
