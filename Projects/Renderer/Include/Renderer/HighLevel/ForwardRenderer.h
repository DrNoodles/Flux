#pragma once

#include "Renderer/HighLevel/CommonRendererHighLevel.h"
#include "Renderer/HighLevel/ResourceRegistry.h"
#include "Renderer/HighLevel/RenderStages/BloomRenderStage.h"
#include "Renderer/HighLevel/RenderStages/PbrRenderStage.h"
#include "Renderer/HighLevel/RenderStages/PostEffectsRenderStage.h"
#include "Renderer/HighLevel/RenderStages/ShadowMapRenderStage.h"
#include "Renderer/HighLevel/RenderStages/SkyboxRenderStage.h"
//#include "Renderer/HighLevel/RenderStages/ToneMappingRenderStage.h"
#include "Renderer/LowLevel/UniformBufferObjects.h"

#include "Renderer/LowLevel/Framebuffer.h"
#include "Renderer/LowLevel/VulkanService.h"

#include <Framework/CommonTypes.h>
#include <Framework/IModelLoaderService.h> // Used for mesh/model/texture definitions TODO remove dependency?

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
#define FEATURE_BLOOM 0

class ForwardRenderer final : public IPbrRenderStageDelegate
{
public: // Data
private:// Data

	// Dependencies
	VulkanService& _vk;
	std::string _shaderDir;
	std::string _assetsDir;
	IModelLoaderService& _modelLoaderService;

	std::unique_ptr<ResourceRegistry> _resourceRegistry = nullptr;
	
	// Framebuffers
	std::unique_ptr<FramebufferResources> _shadowmapFramebuffer = nullptr;
	std::unique_ptr<FramebufferResources> _sceneFramebuffer = nullptr;
	std::unique_ptr<FramebufferResources> _postFramebuffer = nullptr;

	// RenderStages
	std::unique_ptr<PbrRenderStage>         _pbrRenderStage = nullptr;
	std::unique_ptr<SkyboxRenderStage>      _skyboxRenderStage   = nullptr;
	std::unique_ptr<ShadowMapRenderStage>   _shadowMapRenderStage = nullptr;
	//std::unique_ptr<ToneMappingRenderStage> _toneMappingRenderStage = nullptr;
	std::unique_ptr<PostEffectsRenderStage> _postEffectsRenderStage = nullptr;
#if FEATURE_BLOOM
	std::unique_ptr<BloomRenderStage>       _bloomRenderStage = nullptr;
#endif

public: // Lifetime
	
	ForwardRenderer(VulkanService& vulkanService, std::string shaderDir, std::string assetsDir, IModelLoaderService& modelLoaderService, Extent2D resolution) :
		_vk(vulkanService),
		_shaderDir(std::move(shaderDir)),
		_assetsDir(std::move(assetsDir)),
		_modelLoaderService(modelLoaderService)
	{
		_resourceRegistry = std::make_unique<ResourceRegistry>(&_vk, &modelLoaderService, _shaderDir, _assetsDir);
		
		// Shadowmap
		_shadowMapRenderStage = std::make_unique<ShadowMapRenderStage>( _shaderDir, _vk );
		_shadowmapFramebuffer = CreateShadowmapFramebuffer(4096, 4096, _shadowMapRenderStage->GetRenderPass());

		// Scene
		_skyboxRenderStage = std::make_unique<SkyboxRenderStage>(_vk, _resourceRegistry.get(), _shaderDir, _assetsDir, _modelLoaderService);
		_pbrRenderStage = std::make_unique<PbrRenderStage>(_vk, _resourceRegistry.get(), *this, _shaderDir, _assetsDir);
		_sceneFramebuffer = CreateSceneFramebuffer(resolution.Width, resolution.Height, _pbrRenderStage->GetRenderPass());
#if FEATURE_BLOOM
		// Bloom
		_bloomRenderStage = std::make_unique<BloomRenderStage>(_shaderDir, &_vk);
		_bloomRenderStage->CreateDescriptorResources(InputFramebuffers{ _sceneFramebuffer->OutputDescriptor }, { resolution.Width, resolution.Height });
#endif
		// Post
		_postEffectsRenderStage = std::make_unique<PostEffectsRenderStage>(_shaderDir, &_vk);
#if FEATURE_BLOOM
		_postEffectsRenderStage->CreateDescriptorResources(TextureData{_bloomRenderStage->GetOutput().ColorInfo});
#else
		_postEffectsRenderStage->CreateDescriptorResources(TextureData{_sceneFramebuffer->OutputDescriptor});
#endif
		_postFramebuffer = CreatePostFramebuffer(resolution.Width, resolution.Height, _postEffectsRenderStage->GetRenderPass());
	}
	
	ForwardRenderer(const ForwardRenderer&) = delete;
	ForwardRenderer& operator=(const ForwardRenderer&) = delete;

	ForwardRenderer(ForwardRenderer&& other) = delete;
	ForwardRenderer& operator=(ForwardRenderer&& other) = delete;

	~ForwardRenderer() override
	{
		_sceneFramebuffer->Destroy();
		_sceneFramebuffer = nullptr;

		_shadowmapFramebuffer->Destroy();
		_shadowmapFramebuffer = nullptr;

		_postFramebuffer->Destroy();
		_postFramebuffer = nullptr;

		_shadowMapRenderStage->Destroy(_vk.LogicalDevice(), _vk.Allocator());
		_shadowMapRenderStage = nullptr;

		_pbrRenderStage->Destroy();
		_pbrRenderStage = nullptr;

		_skyboxRenderStage->Destroy();
		_skyboxRenderStage = nullptr;

		_postEffectsRenderStage->Destroy();
		_postEffectsRenderStage = nullptr;
#if FEATURE_BLOOM
		_bloomRenderStage->Destroy();
		_bloomRenderStage = nullptr;
#endif
	}


public: // Methods
	void HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
	{
		_postEffectsRenderStage->DestroyDescriptorResources();
#if FEATURE_BLOOM
		_bloomRenderStage->DestroyDescriptorResources();
#endif
		_postFramebuffer->Destroy();
		_sceneFramebuffer->Destroy();

		_skyboxRenderStage->HandleSwapchainRecreated(width, height, numSwapchainImages);

		_pbrRenderStage->HandleSwapchainRecreated(width, height, numSwapchainImages);
	
		_sceneFramebuffer = CreateSceneFramebuffer(width, height, _pbrRenderStage->GetRenderPass());

#if FEATURE_BLOOM
		_bloomRenderStage->CreateDescriptorResources(InputFramebuffers{ _sceneFramebuffer->OutputDescriptor }, { width, height });
		_postEffectsRenderStage->CreateDescriptorResources(TextureData{_bloomRenderStage->GetOutput().ColorInfo});
#else
		_postEffectsRenderStage->CreateDescriptorResources(TextureData{_sceneFramebuffer->OutputDescriptor});
#endif
		_postFramebuffer = CreatePostFramebuffer(width, height, _postEffectsRenderStage->GetRenderPass());
	}
	
	void Draw(u32 imageIndex, VkCommandBuffer commandBuffer, const SceneRendererPrimitives& scene, const RenderOptions& options) const
	{
		// Update all descriptors
		const auto skyboxDescUpdated = _skyboxRenderStage->UpdateDescriptors(options);
		_pbrRenderStage->UpdateDescriptors(imageIndex, options, skyboxDescUpdated, scene); // also update other passes?

		// TODO Just update the descriptor for this imageIndex????
		//_postEffectsRenderStage.CreateDescriptorResources(TextureData{_sceneFramebuffer.OutputDescriptor});

		// Draw Shadow Pass to Shadowmap Framebuffer
		auto lightSpaceMatrix = glm::identity<glm::mat4>();
		if (FindShadowCasterMatrix(scene.Lights, lightSpaceMatrix))
		{
			const auto shadowRenderArea = vki::Rect2D({}, _shadowmapFramebuffer->Desc.Extent);

			auto beginInfo = vki::RenderPassBeginInfo(
				_shadowMapRenderStage->GetRenderPass(),
				_shadowmapFramebuffer->Framebuffer,
				shadowRenderArea,
				_shadowmapFramebuffer->ClearValues);

			vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				_shadowMapRenderStage->Draw(commandBuffer, shadowRenderArea,
					scene, lightSpaceMatrix,
					_pbrRenderStage->Hack_GetRenderables(),// TODO extract resources from PbrRenderStage into ForwardRenderer
					_resourceRegistry->Hack_GetMeshes()); // TODO pass resRegistry into shadow pass so it can get meshes it needs
			}
			vkCmdEndRenderPass(commandBuffer);
		}


		const auto sceneRenderArea = vki::Rect2D({}, _sceneFramebuffer->Desc.Extent);
		const auto sceneViewport = vki::Viewport(sceneRenderArea);

#if FEATURE_BLOOM
		vkh::TransitionImageLayout(commandBuffer, _sceneFramebuffer->OutputImage,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1, 0, 1); // TODO Optimise stages
#endif

		// Draw Scene (skybox and pbr) to Scene Framebuffer
		{
			// Calc Projection
			const auto vfov = 45.f;
			const auto aspect = _sceneFramebuffer->Desc.Extent.width / (f32)_sceneFramebuffer->Desc.Extent.height;
			auto projection = glm::perspective(glm::radians(vfov), aspect, 0.05f, 1000.f);
			projection = glm::scale(projection, glm::vec3{ 1.f,-1.f,1.f });// flip Y to convert glm from OpenGL coord system to Vulkan

			auto renderPassBeginInfo = vki::RenderPassBeginInfo(
				_pbrRenderStage->GetRenderPass(),
				_sceneFramebuffer->Framebuffer,
				sceneRenderArea,
				_sceneFramebuffer->ClearValues);

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				vkCmdSetViewport(commandBuffer, 0, 1, &sceneViewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &sceneRenderArea);

				_skyboxRenderStage->Draw(commandBuffer, imageIndex, options, scene.ViewMatrix, projection);

				_pbrRenderStage->Draw(commandBuffer, imageIndex, options, scene.Objects, scene.Lights, scene.ViewMatrix, projection, scene.ViewPosition, lightSpaceMatrix);
			}
			vkCmdEndRenderPass(commandBuffer);
		}

#if FEATURE_BLOOM
		// Wait for scene to be drawn so we can sample it
		vkh::TransitionImageLayout(commandBuffer, _sceneFramebuffer->OutputImage,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1, 0, 1); // TODO Optimise stages


		// Draw Bloom
		_bloomRenderStage->Draw(commandBuffer, imageIndex, options);

		// Wait for bloom Compute to finish for use in Post
		vkh::TransitionImageLayout(commandBuffer, _bloomRenderStage->GetOutput().ColorImage,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1, 0, 1); // TODO Optimise stages
#else
		// Wait for scene to be drawn so we can sample it
		vkh::TransitionImageLayout(commandBuffer, _sceneFramebuffer->OutputImage,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1, 0, 1); // TODO Optimise stages
#endif

		// Draw PostEffects to PostFramebuffer
		{
			auto beginInfo = vki::RenderPassBeginInfo(
				_postEffectsRenderStage->GetRenderPass(),
				_postFramebuffer->Framebuffer,
				sceneRenderArea,
				_postFramebuffer->ClearValues);

			vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				vkCmdSetViewport(commandBuffer, 0, 1, &sceneViewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &sceneRenderArea);
				_postEffectsRenderStage->Draw(commandBuffer, imageIndex, options);
			}
			vkCmdEndRenderPass(commandBuffer);
		}
	}


public: // PBR RenderPass routing methods
	const FramebufferResources& GetOutputFramebuffer() const { return *_postFramebuffer; }

	RenderableResourceId CreateRenderable(const MeshResourceId& meshId) const
	{
		return _pbrRenderStage->CreateRenderable(meshId);
	}

	[[deprecated("MeshResourceId is becoming internal to Renderer")]]
	MeshResourceId Hack_CreateMeshResource(const MeshDefinition& meshDefinition) const
	{
		return _resourceRegistry->CreateMeshResource(meshDefinition);
	}
	
	[[deprecated("TextureResourceId is becoming internal to Renderer")]]
	TextureResourceId Hack_CreateTextureResource(const std::string& path) const
	{
		return _resourceRegistry->CreateTextureResource(path);
	}

public: // Skybox RenderPass routing methods
	VkDescriptorImageInfo GetShadowmapDescriptor() override { return _shadowmapFramebuffer->OutputDescriptor; }
	const TextureResource& GetIrradianceTextureResource()  override { return _skyboxRenderStage->GetIrradianceTextureResource(); }
	const TextureResource& GetPrefilterTextureResource() override { return _skyboxRenderStage->GetPrefilterTextureResource(); }
	const TextureResource& GetBrdfTextureResource() override { return _skyboxRenderStage->GetBrdfTextureResource(); }
	IblTextureResourceIds CreateIblTextureResources(const std::string& path) const
	{
		return _skyboxRenderStage->CreateIblTextureResources(path);
	}

	SkyboxResourceId CreateSkybox(const SkyboxCreateInfo& createInfo) const
	{
		return _skyboxRenderStage->CreateSkybox(createInfo);
	}

	void SetSkybox(const SkyboxResourceId& resourceId) const
	{
		_skyboxRenderStage->SetSkybox(resourceId);
		_pbrRenderStage->SetSkyboxDirty();
	}


private:// Methods
	
	std::unique_ptr<FramebufferResources> CreateSceneFramebuffer(u32 width, u32 height, VkRenderPass renderPass) const
	{
		const auto samples = _vk.GetMsaaSamples();
		
		FramebufferDesc desc = {
			.Extent = { width, height },
			.Attachments = {
				FramebufferAttachmentDesc::CreateColor(VK_FORMAT_R16G16B16A16_SFLOAT, samples),
				FramebufferAttachmentDesc::CreateDepth(vkh::FindDepthFormat(_vk.PhysicalDevice()), samples),
			},
			.OutputAttachmentIndex = 0, // Color
		};

		// Resolving MSAA in Scene pass 
		const auto usingMultisampling = samples > VK_SAMPLE_COUNT_1_BIT;
		if (usingMultisampling)
		{
			desc.Attachments.emplace_back(FramebufferAttachmentDesc::CreateResolve(VK_FORMAT_R16G16B16A16_SFLOAT));
#if FEATURE_BLOOM
			desc.Attachments[2].AdditionalUsageFlags = VK_IMAGE_USAGE_STORAGE_BIT; // Resolve will be used in a compute for post processing
#endif
			desc.OutputAttachmentIndex = 2; // Resolve
		}

		return std::make_unique<FramebufferResources>(desc, renderPass, _vk);
	}

	std::unique_ptr<FramebufferResources> CreateShadowmapFramebuffer(u32 width, u32 height, VkRenderPass renderPass) const
	{
		const auto depthFormat = vkh::FindDepthFormat(_vk.PhysicalDevice());

		FramebufferDesc desc = {
			.Extent = { width, height },
		 .Attachments = {
			FramebufferAttachmentDesc::CreateDepth(depthFormat),
		 },
			.OutputAttachmentIndex = 0,
	  };

		return std::make_unique<FramebufferResources>(desc, renderPass, _vk);
	}

	std::unique_ptr<FramebufferResources> CreatePostFramebuffer(u32 width, u32 height, VkRenderPass renderPass) const
	{
		FramebufferDesc desc = {
			.Extent = { width, height },
			.Attachments = {
				FramebufferAttachmentDesc::CreateColor(VK_FORMAT_R16G16B16A16_SFLOAT),
			},
			.OutputAttachmentIndex = 0,
		};

		return std::make_unique<FramebufferResources>(desc, renderPass, _vk);
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
