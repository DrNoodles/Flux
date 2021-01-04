#pragma once

#include "RenderPasses/PbrModelRenderPass.h"
#include "UniformBufferObjects.h"
#include "RenderPasses/DirectionalShadowRenderPass.h"
#include "RenderPasses/SkyboxRenderPass.h"

#include "Framebuffer.h"
#include "VulkanService.h"
#include "CubemapTextureLoader.h"

#include <Framework/IModelLoaderService.h> // Used for mesh/model/texture definitions TODO remove dependency?
#include <Framework/CommonTypes.h>

#include <vector>


/* TODO TODO TODO TODO TODO
Split up concepts clearly.

Scene:
	- Deals with user level resources.
	- Eg. load character.fbx which contain meshes, materials and textures.
	- Each unique Mesh/Texture GPU resource is has a exactly 1 AssetDesc entry.
	- A scene is a DAG of Models (comprised of Mesh Resources) and Materials (comprised of Textures Resources and properties).
	- Model and Material are scene constructs.
	- Mesh and Texture are GPU Resources.
	
	Eg, ive seperately loaded .../mesh.fbx and .../diffuse.png and applied one to the other. It has no knowledge of renderer constructs.

	SceneAssets:
		- Owns individual meshes/textures/ibls/etc descriptions. Doesn't care about how/if they're used.

		Eg, the editor could have an asset viewer with a filter on type. Type=Texture would show all textures in the scene regardless of how/if they're used.

		struct AssetDesc
			GUID Id
			string Path
			AssetType Type // Texture/Mesh/Ibl
			// Maybe this needs subclassing for control

Renderer:
	- Consumes a scene description: probably a scene graph with AssetDescs?

	AssetToResourceMap
		Maps an AssetDesc.Id to 1 to n resources.
		Necessary abstraction for things like Ibl which is one concept, but has many texture resources.

		- Queries RendererResourceManager for resources based on AssetDesc.Id (Lazy?) loads any resources

	RendererResourceManager:
		- Used exclusively by Renderer to manage resources
		ResourceId GetResourceId(AssetDesc asset) // lazy load? could be tricky with composite resources. 1 input = n output (ibl for eg)

*/

class SceneRenderer final : public IPbrModelRenderPassDelegate
{
public: // Data
private:// Data

	// Dependencies
	VulkanService& _vk;
	std::string _shaderDir;
	std::string _assetsDir;
	IModelLoaderService& _modelLoaderService;

	// Framebuffers
	std::unique_ptr<FramebufferResources> _sceneFramebuffer = nullptr;
	std::unique_ptr<FramebufferResources> _shadowmapFramebuffer = nullptr;

	// Renderpasses
	std::unique_ptr<PbrModelRenderPass>          _pbrRenderPass = nullptr;
	std::unique_ptr<SkyboxRenderPass>            _skyboxRenderPass   = nullptr;
	std::unique_ptr<DirectionalShadowRenderPass> _dirShadowRenderPass = nullptr;


public: // Lifetime
	SceneRenderer(VulkanService& vulkanService, std::string shaderDir, std::string assetsDir, IModelLoaderService& modelLoaderService, Extent2D resolution) :
		_vk(vulkanService),
		_shaderDir(std::move(shaderDir)),
		_assetsDir(std::move(assetsDir)),
		_modelLoaderService(modelLoaderService)
	{
		_skyboxRenderPass = std::make_unique<SkyboxRenderPass>(_vk, _shaderDir, _assetsDir, _modelLoaderService);
		_pbrRenderPass = std::make_unique<PbrModelRenderPass>(_vk, *this, _shaderDir, _assetsDir);
		_dirShadowRenderPass = std::make_unique<DirectionalShadowRenderPass>( _shaderDir, _vk );
		
		_shadowmapFramebuffer = CreateShadowmapFramebuffer(4096, 4096, _dirShadowRenderPass->GetRenderPass());
		_sceneFramebuffer = CreateSceneFramebuffer(resolution.Width, resolution.Height, _pbrRenderPass->GetRenderPass());
	}

	SceneRenderer(const SceneRenderer&) = delete;
	SceneRenderer& operator=(const SceneRenderer&) = delete;

	SceneRenderer(SceneRenderer&& other) = default;
	SceneRenderer& operator=(SceneRenderer&& other) = default;

	~SceneRenderer() override
	{
		_sceneFramebuffer->Destroy();
		_sceneFramebuffer = nullptr;

		_shadowmapFramebuffer->Destroy();
		_shadowmapFramebuffer = nullptr;
		
		_dirShadowRenderPass->Destroy(_vk.LogicalDevice(), _vk.Allocator());
		_dirShadowRenderPass = nullptr;
		
		_pbrRenderPass->Destroy();
		_pbrRenderPass = nullptr;
		
		_skyboxRenderPass->Destroy();
		_skyboxRenderPass = nullptr;
	}

public: // Methods
	void HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
	{
		_skyboxRenderPass->HandleSwapchainRecreated(width, height, numSwapchainImages);
		_pbrRenderPass->HandleSwapchainRecreated(width, height, numSwapchainImages);
		
		_sceneFramebuffer->Destroy();
		_sceneFramebuffer = CreateSceneFramebuffer(width, height, _pbrRenderPass->GetRenderPass());
	}
	
	void Draw(u32 imageIndex, VkCommandBuffer commandBuffer, const SceneRendererPrimitives& scene, const RenderOptions& options) const
	{
		// Update all descriptors
		const auto skyboxDescUpdated = _skyboxRenderPass->UpdateDescriptors(options);
		_pbrRenderPass->UpdateDescriptors(options, skyboxDescUpdated); // also update other passes?


		// Draw shadow pass
		auto lightSpaceMatrix = glm::identity<glm::mat4>();
		if (FindShadowCasterMatrix(scene.Lights, lightSpaceMatrix))
		{
			const auto shadowRenderArea = vki::Rect2D({}, _shadowmapFramebuffer->Desc.Extent);
			
			auto beginInfo = vki::RenderPassBeginInfo(*_shadowmapFramebuffer, shadowRenderArea);
			vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				_dirShadowRenderPass->Draw(commandBuffer, shadowRenderArea, 
					scene, lightSpaceMatrix, 
					_pbrRenderPass->Hack_GetRenderables(),// TODO extract resources from PbrModelRenderPass into SceneRenderer
					_pbrRenderPass->Hack_GetMeshes());    // TODO extract resources from PbrModelRenderPass into SceneRenderer
			}
			vkCmdEndRenderPass(commandBuffer);
		}


		// Draw scene (skybox and pbr) to gbuffer
		{
			// Scene Viewport - Only the part of the screen showing the scene.
			auto sceneRenderArea = vki::Rect2D({}, _sceneFramebuffer->Desc.Extent);
			auto sceneViewport = vki::Viewport(sceneRenderArea);

			const auto renderPassBeginInfo = vki::RenderPassBeginInfo(_pbrRenderPass->GetRenderPass(),
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

				_skyboxRenderPass->Draw(commandBuffer, imageIndex, options, scene.RenderableIds, scene.RenderableTransforms, scene.ViewMatrix, projection);
				
				_pbrRenderPass->Draw(commandBuffer, imageIndex, options, scene.RenderableIds, scene.RenderableTransforms, scene.Lights, scene.ViewMatrix, projection, scene.ViewPosition, lightSpaceMatrix);
			}
			vkCmdEndRenderPass(commandBuffer);
		}
	}


public: // PBR RenderPass routing methods
	VkDescriptorImageInfo GetOutputDescritpor() const { return _sceneFramebuffer->OutputDescriptor; }

	RenderableResourceId CreateRenderable(const MeshResourceId& meshId, const Material& material) const
	{
		return _pbrRenderPass->CreateRenderable(meshId, material);
	}

	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition) const
	{
		return _pbrRenderPass->CreateMeshResource(meshDefinition);
	}

	const Material& GetMaterial(const RenderableResourceId& id) const { return _pbrRenderPass->GetMaterial(id); }
	void SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat) const
	{
		_pbrRenderPass->SetMaterial(renderableResId, newMat);
	}

	TextureResourceId CreateTextureResource(const std::string& path) const
	{
		return _pbrRenderPass->CreateTextureResource(path);
	}

public: // Skybox RenderPass routing methods
	VkDescriptorImageInfo GetShadowmapDescriptor() override { return _shadowmapFramebuffer->OutputDescriptor; }
	const TextureResource& GetIrradianceTextureResource()  override { return _skyboxRenderPass->GetIrradianceTextureResource(); }
	const TextureResource& GetPrefilterTextureResource() override { return _skyboxRenderPass->GetPrefilterTextureResource(); }
	const TextureResource& GetBrdfTextureResource() override { return _skyboxRenderPass->GetBrdfTextureResource(); }
	IblTextureResourceIds CreateIblTextureResources(const std::string& path) const
	{
		return _skyboxRenderPass->CreateIblTextureResources(path);
	}

	SkyboxResourceId CreateSkybox(const SkyboxCreateInfo& createInfo) const
	{
		return _skyboxRenderPass->CreateSkybox(createInfo);
	}

	void SetSkybox(const SkyboxResourceId& resourceId) const
	{
		_skyboxRenderPass->SetSkybox(resourceId);
		_pbrRenderPass->SetSkyboxDirty();
	}


private:// Methods

	std::unique_ptr<FramebufferResources> CreateSceneFramebuffer(u32 width, u32 height, VkRenderPass renderPass) const
	{
		// Scene framebuffer is only the size of the scene render region on screen
		return std::make_unique<FramebufferResources>(FramebufferResources::CreateSceneFramebuffer(
			{ width, height },
			VK_FORMAT_R16G16B16A16_SFLOAT,
			renderPass,
			_vk.MsaaSamples(),
			_vk.LogicalDevice(), _vk.PhysicalDevice(), _vk.Allocator()));
	}

	std::unique_ptr<FramebufferResources> CreateShadowmapFramebuffer(u32 width, u32 height, VkRenderPass renderPass) const
	{
		return std::make_unique<FramebufferResources>(FramebufferResources::CreateShadowFramebuffer(
			{ width, height }, 
			renderPass, 
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
