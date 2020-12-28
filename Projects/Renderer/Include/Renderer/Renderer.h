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

class IRendererDelegate
{
public:
	virtual ~IRendererDelegate() = default;
	virtual VkDescriptorImageInfo GetShadowmapDescriptor() = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Renderer
{
public:
	const std::vector<std::unique_ptr<RenderableMesh>>& Hack_GetRenderables() const { return _renderables; }
	const std::vector<std::unique_ptr<MeshResource>>& Hack_GetMeshes() const { return _meshes; }
	
	explicit Renderer(VulkanService& vulkanService, IRendererDelegate& delegate, std::string shaderDir, const std::string& assetsDir, IModelLoaderService& modelLoaderService);

	void UpdateDescriptors(const RenderOptions& options);
	
	void Draw(VkCommandBuffer commandBuffer, u32 frameIndex, 
		const RenderOptions& options,
		const std::vector<RenderableResourceId>& renderableIds,
		const std::vector<glm::mat4>& transforms,
		const std::vector<Light>& lights,
		const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, const glm::mat4& lightSpaceMatrix);
	
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

	VkRenderPass GetRenderPass() const { return _renderPass; }

	VkPipelineLayout Hack_GetPbrPipelineLayout() const { return _pbrPipelineLayout; }
	
	static VkRenderPass CreateRenderPass(VkFormat format, VulkanService& vk);


private: // Dependencies
	VulkanService& _vk;
	IRendererDelegate& _delegate;
	std::string _shaderDir{};

	VkRenderPass _renderPass = nullptr;
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


	//TODO
	/*
	Split up concepts clearly.

	Scene:
		- Deals with user level resources.
		- Eg. loads Caustic.fbx which contains a mesh and textures for diff layers.
		- Uses SceneAssets to define unique user assets in the scene.
		- Scene itself joins these unique AssetDescs to build the scene itself.

		Eg, ive loaded .../mesh.fbx and .../diffuse.png and applied one to the other. It has no knowledge of renderer constructs.

		SceneAssets:
			Owns individual meshes/textures/ibls/etc descriptions. Doesn't care about how/if they're used.

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
	std::vector<std::unique_ptr<MeshResource>> _meshes{};      // TODO Move these to a resource registry
	std::vector<std::unique_ptr<TextureResource>> _textures{}; // TODO Move these to a resource registry

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
	//static VkRenderPass CreateRenderPass(VkSampleCountFlagBits msaaSamples, VkDevice device, VkPhysicalDevice physicalDevice);

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
		const TextureResource& emissiveMap,
		const TextureResource& transparencyMap,
		const TextureResource& irradianceMap,
		const TextureResource& prefilterMap,
		const TextureResource& brdfMap,
		VkDescriptorImageInfo shadowmapDescriptor,
		VkDevice device);

	// The uniform and push values referenced by the shader that can be updated at draw time
	static VkPipeline CreatePbrGraphicsPipeline(const std::string& shaderDir, VkPipelineLayout pipelineLayout,
			VkSampleCountFlagBits msaaSamples, VkRenderPass renderPass, VkDevice device);

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

	VkDescriptorImageInfo GetShadowmapTextureResource() const
	{
		//TODO Write a hack to get the resource from elsewhere
		return _delegate.GetShadowmapDescriptor();
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
