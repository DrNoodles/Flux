#pragma once

#include "VulkanService.h"
#include "GpuTypes.h"
#include "RenderableMesh.h"
#include "TextureResource.h"
#include "CubemapTextureLoader.h"

#include <Framework/IModelLoaderService.h> // Used for mesh/model/texture definitions TODO remove dependency?
#include <Framework/CommonTypes.h>

#include <vector>

class VulkanService;
struct UniversalUbo;
struct RenderableMeshCreateInfo;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SkyboxRenderPass
{
public: // Data
private:// Data
	
	// Dependencies
	VulkanService& _vk;

	std::string _shaderDir{};

	VkRenderPass _renderPass = nullptr;
	VkDescriptorPool _rendererDescriptorPool = nullptr;

	// Skybox
	VkPipeline _skyboxPipeline = nullptr;
	VkPipelineLayout _skyboxPipelineLayout = nullptr;
	VkDescriptorSetLayout _skyboxDescriptorSetLayout = nullptr;

	// Resources

	SkyboxResourceId _activeSkybox = {};
	std::vector<std::unique_ptr<Skybox>> _skyboxes{};

	std::vector<std::unique_ptr<MeshResource>> _meshes{};      // TODO Move these to a resource registry
	std::vector<std::unique_ptr<TextureResource>> _textures{}; // TODO Move these to a resource registry

	bool _refreshSkyboxDescriptorSets = false;

	// Required resources
	TextureResourceId _placeholderTexture;
	MeshResourceId _skyboxMesh;

	RenderOptions _lastOptions;


public: // Members

	explicit SkyboxRenderPass(VulkanService& vulkanService, const std::string& shaderDir, const std::string& assetsDir, IModelLoaderService& modelLoaderService);

	void Destroy();
	
	bool UpdateDescriptors(const RenderOptions& options);
	
	void Draw(VkCommandBuffer commandBuffer, u32 frameIndex,
		const RenderOptions& options,
		const std::vector<RenderableResourceId>& renderableIds,
		const std::vector<glm::mat4>& transforms,
		const std::vector<Light>& lights,
		const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, const glm::mat4& lightSpaceMatrix);

	TextureResourceId CreateTextureResource(const std::string& path);
	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition);

	// Generate Image Based Lighting resources from 6 textures representing the sides of a cubemap. 32b/channel. Ordered +X -X +Y -Y +Z -Z
	[[deprecated]] // the cubemaps will appear mirrored (text is backwards)
	IblTextureResourceIds CreateIblTextureResources(const std::array<std::string, 6>& sidePaths);

	// Generate Image Based Lighting resources from an Equirectangular HDRI map. 32b/channel
	IblTextureResourceIds CreateIblTextureResources(const std::string& path);
	TextureResourceId CreateCubemapTextureResource(const std::array<std::string, 6>& sidePaths, CubemapFormat format);
	
	SkyboxResourceId CreateSkybox(const SkyboxCreateInfo& createInfo);

	VkRenderPass GetRenderPass() const { return _renderPass; }

	void SetSkybox(const SkyboxResourceId& resourceId);

	void HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages);

private:
	void InitRenderer();
	void DestroyRenderer();
	void InitRendererResourcesDependentOnSwapchain(u32 numImagesInFlight);
	void DestroyRenderResourcesDependentOnSwapchain();
	static VkRenderPass CreateRenderPass(VkFormat format, VulkanService& vk);
	static VkDescriptorPool CreateDescriptorPool(u32 numImagesInFlight, VkDevice device);


public:
	
	const TextureResource& GetIrradianceTextureResource() const
	{
		const auto* skybox = GetCurrentSkyboxOrNull();
		return *_textures[skybox ? skybox->IblTextureIds.IrradianceCubemapId.Id : _placeholderTexture.Id];
	}

	const TextureResource& GetPrefilterTextureResource() const
	{
		const auto* skybox = GetCurrentSkyboxOrNull();
		return *_textures[skybox ? skybox->IblTextureIds.PrefilterCubemapId.Id : _placeholderTexture.Id];
	}

	const TextureResource& GetBrdfTextureResource() const
	{
		const auto* skybox = GetCurrentSkyboxOrNull();
		return *_textures[skybox ? skybox->IblTextureIds.BrdfLutId.Id : _placeholderTexture.Id];
	}

private:
	const Skybox* GetCurrentSkyboxOrNull() const
	{
		return _skyboxes.empty() ? nullptr : _skyboxes[_activeSkybox.Id].get();
	}

	/*const TextureResource& GetSkyboxTextureResource() const
	{
		const auto* skybox = GetCurrentSkyboxOrNull();
		return *_textures[skybox ? skybox->IblTextureIds.IrradianceCubemapId.Id : _placeholderTexture.Id];
	}*/

	std::vector<SkyboxResourceFrame> CreateSkyboxModelFrameResources(u32 numImagesInFlight, const Skybox& skybox) const;

	// Defines the layout of the data bound to the shaders
	static VkDescriptorSetLayout CreateSkyboxDescriptorSetLayout(VkDevice device);

	// Associates the UBO and texture to sets for use in shaders
	static void WriteSkyboxDescriptorSets(
		u32 count,
		const std::vector<VkDescriptorSet>& descriptorSets,
		const std::vector<VkBuffer>& skyboxVertUbo,
		const std::vector<VkBuffer>& skyboxFragUbo,
		const TextureResource& skyboxMap,
		VkDevice device);

	// The uniform and push values referenced by the shader that can be updated at draw time
	static VkPipeline CreateSkyboxGraphicsPipeline(const std::string& shaderDir, VkPipelineLayout pipelineLayout,
		VkSampleCountFlagBits msaaSamples, VkRenderPass renderPass, VkDevice device,
		const VkExtent2D& swapchainExtent);

	void UpdateSkyboxesDescriptorSets();
};
