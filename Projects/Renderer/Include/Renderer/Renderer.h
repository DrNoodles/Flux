#pragma once

#include "GpuTypes.h"
#include "RenderableMesh.h"
#include "TextureResource.h"
#include "VulkanService.h" // TODO Investigate why compilation breaks if above TextureResource.h
#include "CubemapTextureLoader.h"

#include <Framework/IModelLoaderService.h> // Used for mesh/model/texture definitions TODO remove dependency?
#include <Framework/CommonTypes.h>

#include <vector>



class VulkanService;
struct UniversalUbo;
struct RenderableMeshCreateInfo;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IRendererDelegate
{
public:
	virtual ~IRendererDelegate() = default;
	virtual VkExtent2D GetFramebufferSize() = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Renderer
{
public:
	bool FramebufferResized = false;

	explicit Renderer(VulkanService* vulkanService, std::string shaderDir, const std::string& assetsDir, 
	                  IRendererDelegate& delegate, IModelLoaderService& modelLoaderService);

	void DrawFrame(VkCommandBuffer commandBuffer, u32 frameIndex, 
		const RenderOptions& options,
		const std::vector<RenderableResourceId>& renderableIds,
		const std::vector<glm::mat4>& transforms,
		const std::vector<Light>& lights,
		glm::mat4 view, glm::vec3 camPos, glm::ivec2 regionPos, glm::ivec2 regionSize);
	
	void CleanUp(); // TODO convert to RAII?

	TextureResourceId CreateTextureResource(const std::string& path);

	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition);

	RenderableResourceId CreateRenderable(const MeshResourceId& meshId, const Material& material);



	/**
	 * Generate Image Based Lighting resources from 6 textures representing the sides of a cubemap. 32b/channel.
	 * Ordered +X -X +Y -Y +Z -Z
	 */
	[[deprecated]] // the cubemaps will appear mirrored (text is backwards)
	IblTextureResourceIds CreateIblTextureResources(const std::array<std::string, 6>& sidePaths);

	/**
	 * Generate Image Based Lighting resources from an Equirectangular HDRI map. 32b/channel
	 */
	IblTextureResourceIds CreateIblTextureResources(const std::string& path);
	
	TextureResourceId CreateCubemapTextureResource(const std::array<std::string, 6>& sidePaths, CubemapFormat format);

	SkyboxResourceId CreateSkybox(const SkyboxCreateInfo& createInfo);

	//const RenderableMesh& GetRenderableMesh(const RenderableResourceId& id) const { return *_renderables[id.Id]; }
	
	const Material& GetMaterial(const RenderableResourceId& id) const { return _renderables[id.Id]->Mat; }
	void SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat);
	void SetSkybox(const SkyboxResourceId& resourceId);

	void HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages);


private: // Dependencies
	VulkanService* _vk = nullptr;
	IRendererDelegate& _delegate;
	std::string _shaderDir{};

	VkDescriptorPool _rendererDescriptorPool = nullptr;

	// PBR
	VkPipeline _pbrPipeline = nullptr;
	VkPipelineLayout _pbrPipelineLayout = nullptr;
	VkDescriptorSetLayout _pbrDescriptorSetLayout = nullptr;

	// Skybox
	VkPipeline _skyboxPipeline = nullptr;
	VkPipelineLayout _skyboxPipelineLayout = nullptr;
	VkDescriptorSetLayout _skyboxDescriptorSetLayout = nullptr;
	
	// Resources
	std::vector<VkBuffer> _lightBuffers{}; // 1 per frame in flight
	std::vector<VkDeviceMemory> _lightBuffersMemory{};

	SkyboxResourceId _activeSkybox = {};
	std::vector<std::unique_ptr<Skybox>> _skyboxes{};
	std::vector<std::unique_ptr<RenderableMesh>> _renderables{};
	
	std::vector<std::unique_ptr<MeshResource>> _meshes{};
	std::vector<std::unique_ptr<TextureResource>> _textures{};

	bool _refreshRenderableDescriptorSets = false;
	bool _refreshSkyboxDescriptorSets = false;

	// Required resources
	TextureResourceId _placeholderTexture;
	MeshResourceId _skyboxMesh;

	RenderOptions _lastOptions;
	
	void InitRenderer();
	void DestroyRenderer();

	void InitRendererResourcesDependentOnSwapchain(u32 numImagesInFlight);
	void DestroyRenderResourcesDependentOnSwapchain();



	
	#pragma region Shared

	static VkDescriptorPool CreateDescriptorPool(u32 numImagesInFlight, VkDevice device);

	#pragma endregion Shared


	
	#pragma region Pbr

	std::vector<PbrModelResourceFrame> CreatePbrModelFrameResources(u32 numImagesInFlight, 
		const RenderableMesh& renderable) const;
	
	// Defines the layout of the data bound to the shaders
	static VkDescriptorSetLayout CreatePbrDescriptorSetLayout(VkDevice device);

	static void WritePbrDescriptorSets(
		uint32_t count,
		const std::vector<VkDescriptorSet>& descriptorSets,
		const std::vector<VkBuffer>& modelUbos,
		const std::vector<VkBuffer>& lightUbos,
		const TextureResource& basecolorMap,
		const TextureResource& normalMap,
		const TextureResource& roughnessMap,
		const TextureResource& metalnessMap,
		const TextureResource& aoMap,
		const TextureResource& irradianceMap,
		const TextureResource& prefilterMap,
		const TextureResource& brdfMap,
		VkDevice device);

	// The uniform and push values referenced by the shader that can be updated at draw time
	static VkPipeline CreatePbrGraphicsPipeline(const std::string& shaderDir, VkPipelineLayout pipelineLayout,
			VkSampleCountFlagBits msaaSamples, VkRenderPass renderPass, VkDevice device,
			const VkExtent2D& swapchainExtent);

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

	void UpdateRenderableDescriptorSets();

	#pragma endregion Pbr


	
	#pragma region Skybox - Everything cubemap: resources, pipelines, etc and rendering

	const Skybox* GetCurrentSkyboxOrNull() const
	{
		return _skyboxes.empty() ? nullptr : _skyboxes[_activeSkybox.Id].get();
	}

	const TextureResource& GetSkyboxTextureResource() const
	{
		const auto* skybox = GetCurrentSkyboxOrNull();
		return *_textures[skybox ? skybox->IblTextureIds.IrradianceCubemapId.Id : _placeholderTexture.Id];
	}

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

	#pragma endregion Skybox
	
};