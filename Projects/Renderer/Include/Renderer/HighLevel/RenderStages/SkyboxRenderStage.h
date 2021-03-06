#pragma once

#include "Renderer/LowLevel/VulkanService.h"

class ResourceRegistry;
class IModelLoaderService;
class VulkanService;
class TextureResource;
struct UniversalUbo;
struct RenderableMeshCreateInfo;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SkyboxRenderStage
{
public: // Data
private:// Data
	
	// Dependencies
	VulkanService& _vk;
	ResourceRegistry* _resources = nullptr;

	std::string _shaderDir{};

	VkRenderPass _renderPass = nullptr;
	VkPipeline _pipeline = nullptr;
	VkPipelineLayout _pipelineLayout = nullptr;
	
	VkDescriptorPool _descPool = nullptr;
	VkDescriptorSetLayout _descSetLayout = nullptr;

	// Resources
	SkyboxResourceId _activeSkybox = {};
	std::vector<std::unique_ptr<Skybox>> _skyboxes{};

	bool _refreshDescSets = false;

	// Required resources
	TextureResourceId _placeholderTextureId;
	MeshResourceId _skyboxMeshId;

	RenderOptions _lastOptions;


public: // Members

	SkyboxRenderStage() = delete;
	SkyboxRenderStage(VulkanService& vulkanService, ResourceRegistry* registry, std::string shaderDir, const std::string& assetsDir, IModelLoaderService& modelLoaderService);

	void Destroy();
	
	bool UpdateDescriptors(const RenderOptions& options);
	
	void Draw(VkCommandBuffer commandBuffer, u32 frameIndex, const RenderOptions& options, const glm::mat4& view, const glm::mat4& projection) const;
	
	// Generate Image Based Lighting resources from 6 textures representing the sides of a cubemap. 32b/channel. Ordered +X -X +Y -Y +Z -Z
	[[deprecated]] // the cubemaps will appear mirrored (text is backwards)
	IblTextureResourceIds CreateIblTextureResources(const std::array<std::string, 6>& sidePaths) const;

	// Generate Image Based Lighting resources from an Equirectangular HDRI map. 32b/channel
	IblTextureResourceIds CreateIblTextureResources(const std::string& path) const;
	
	SkyboxResourceId CreateSkybox(const SkyboxCreateInfo& createInfo);

	VkRenderPass GetRenderPass() const { return _renderPass; }

	void SetSkybox(const SkyboxResourceId& resourceId);

	void HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages);

private:
	void InitResources();
	void DestroyResources();
	void InitResourcesDependentOnSwapchain(u32 numImagesInFlight);
	void DestroyResourcesDependentOnSwapchain();
	static VkRenderPass CreateRenderPass(VkFormat format, VulkanService& vk);
	static VkDescriptorPool CreateDescPool(u32 numImagesInFlight, VkDevice device);


public:
	const TextureResource& GetIrradianceTextureResource() const;
	const TextureResource& GetPrefilterTextureResource() const;
	const TextureResource& GetBrdfTextureResource() const;

private:
	const Skybox* GetCurrentSkyboxOrNull() const
	{
		return _skyboxes.empty() ? nullptr : _skyboxes[_activeSkybox.Value()].get();
	}

	/*const TextureResource& GetSkyboxTextureResource() const
	{
		const auto* skybox = GetCurrentSkyboxOrNull();
		return *_textures[skybox ? skybox->IblTextureIds.IrradianceCubemapId.Id : _placeholderTextureId.Id];
	}*/

	std::vector<SkyboxResourceFrame> CreateModelFrameResources(u32 numImagesInFlight, const Skybox& skybox) const;

	// Defines the layout of the data bound to the shaders
	static VkDescriptorSetLayout CreateDescSetLayout(VkDevice device);

	// Associates the UBO and texture to sets for use in shaders
	static void WriteDescSets(
		u32 count,
		const std::vector<VkDescriptorSet>& descriptorSets,
		const std::vector<VkBuffer>& skyboxVertUbo,
		const std::vector<VkBuffer>& skyboxFragUbo,
		const TextureResource& skyboxMap,
		VkDevice device);

	// The uniform and push values referenced by the shader that can be updated at draw time
	static VkPipeline CreateGraphicsPipeline(const std::string& shaderDir, VkPipelineLayout pipelineLayout,
		VkSampleCountFlagBits msaaSamples, VkRenderPass renderPass, VkDevice device);

	void UpdateDescSets();
};
