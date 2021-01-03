#pragma once

#include "VulkanService.h"
#include "GpuTypes.h"
#include "RenderableMesh.h"
#include "TextureResource.h"
#include "CubemapTextureLoader.h"

#include <Framework/IModelLoaderService.h> // Used for mesh/model/texture definitions TODO remove dependency?
#include <Framework/CommonTypes.h>

#include <vector>



/* TODO TODO TODO TODO TODO
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

class VulkanService;
struct UniversalUbo;
struct RenderableMeshCreateInfo;

class IRendererDelegate
{
public:
	virtual ~IRendererDelegate() = default;
	virtual VkDescriptorImageInfo GetShadowmapDescriptor() = 0;
	virtual const TextureResource& GetIrradianceTextureResource() = 0;
	virtual const TextureResource& GetPrefilterTextureResource() = 0;
	virtual const TextureResource& GetBrdfTextureResource() = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Renderer
{
public: // Data
private:// Data

	// Dependencies
	VulkanService& _vk;
	IRendererDelegate& _delegate;
	std::string _shaderDir{};
	
	VkRenderPass _renderPass = nullptr;
	VkDescriptorPool _rendererDescriptorPool = nullptr;

	// PBR
	VkPipeline _pbrPipeline = nullptr;
	VkPipelineLayout _pbrPipelineLayout = nullptr;
	VkDescriptorSetLayout _pbrDescriptorSetLayout = nullptr;

	// Resources
	std::vector<VkBuffer> _lightBuffers{}; // 1 per frame in flight
	std::vector<VkDeviceMemory> _lightBuffersMemory{};

	std::vector<std::unique_ptr<RenderableMesh>> _renderables{};

	std::vector<std::unique_ptr<MeshResource>> _meshes{};      // TODO Move these to a resource registry
	std::vector<std::unique_ptr<TextureResource>> _textures{}; // TODO Move these to a resource registry

	bool _refreshRenderableDescriptorSets = false;

	// Required resources
	TextureResourceId _placeholderTexture;

	RenderOptions _lastOptions;


public: // Members
	const std::vector<std::unique_ptr<RenderableMesh>>& Hack_GetRenderables() const { return _renderables; }
	const std::vector<std::unique_ptr<MeshResource>>& Hack_GetMeshes() const { return _meshes; }

	explicit Renderer(VulkanService& vulkanService, IRendererDelegate& delegate, const std::string& shaderDir, const std::string& assetsDir, IModelLoaderService& modelLoaderService);

	void Destroy();
	
	bool UpdateDescriptors(const RenderOptions& options, bool skyboxUpdated);

	void Draw(VkCommandBuffer commandBuffer, u32 frameIndex,
		const RenderOptions& options,
		const std::vector<RenderableResourceId>& renderableIds,
		const std::vector<glm::mat4>& transforms,
		const std::vector<Light>& lights,
		const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, const glm::mat4& lightSpaceMatrix);

	TextureResourceId CreateTextureResource(const std::string& path);
	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition);
	RenderableResourceId CreateRenderable(const MeshResourceId& meshId, const Material& material);

	VkRenderPass GetRenderPass() const { return _renderPass; }
	const Material& GetMaterial(const RenderableResourceId& id) const { return _renderables[id.Id]->Mat; }

	void SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat);
	void SetSkyboxDirty() { _refreshRenderableDescriptorSets = true; }

	
	void HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages);

private:
	void InitRenderer();
	void DestroyRenderer();
	void InitRendererResourcesDependentOnSwapchain(u32 numImagesInFlight);
	void DestroyRenderResourcesDependentOnSwapchain();
	static VkRenderPass CreateRenderPass(VkFormat format, VulkanService& vk);

	
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


#pragma endregion Pbr
};
