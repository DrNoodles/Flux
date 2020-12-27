#pragma once

#include "Renderer.h"
#include "GpuTypes.h"
#include "RenderableMesh.h"
#include "Framebuffer.h"
#include "TextureResource.h"
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

	// Framebuffers
	std::unique_ptr<FramebufferResources> _sceneFramebuffer = nullptr;


	// Renderpasses

public: // Methods
	SceneRenderer(VulkanService& vulkanService, Renderer& renderer, std::string shaderDir, const std::string& assetsDir, IModelLoaderService& modelLoaderService) : _vk(vulkanService), _renderer(renderer)
	{

	}
	void Destroy()
	{
		_sceneFramebuffer->Destroy();
	}

	void Init(u32 width, u32 height)
	{
		BuildFramebuffer(width, height);
	}

	void HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
	{
		_sceneFramebuffer->Destroy();
		BuildFramebuffer(width, height); // resolution
	}

	void Draw()
	{

	}

	FramebufferResources& TEMP_GetFramebufferRef() const { return *_sceneFramebuffer; }
	VkDescriptorImageInfo GetOutputDescritpor() const { return _sceneFramebuffer->OutputDescriptor; }
private:// Methods


	void BuildFramebuffer(u32 width, u32 height)
	{
		// Scene framebuffer is only the size of the scene render region on screen
		_sceneFramebuffer = std::make_unique<FramebufferResources>(FramebufferResources::CreateSceneFramebuffer(
			VkExtent2D{ width, height },
			VK_FORMAT_R16G16B16A16_SFLOAT,
			_renderer.GetRenderPass(),
			_vk.MsaaSamples(),
			_vk.LogicalDevice(), _vk.PhysicalDevice(), _vk.Allocator()));
	}
};
