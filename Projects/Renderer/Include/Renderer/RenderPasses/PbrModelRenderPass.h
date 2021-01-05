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

class IPbrModelRenderPassDelegate
{
public:
	virtual ~IPbrModelRenderPassDelegate() = default;
	virtual VkDescriptorImageInfo GetShadowmapDescriptor() = 0;
	virtual const TextureResource& GetIrradianceTextureResource() = 0;
	virtual const TextureResource& GetPrefilterTextureResource() = 0;
	virtual const TextureResource& GetBrdfTextureResource() = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class PbrModelRenderPass
{
public: // Data
private:// Data

	// Dependencies
	VulkanService& _vk;
	IPbrModelRenderPassDelegate& _delegate;
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

	explicit PbrModelRenderPass(VulkanService& vulkanService, IPbrModelRenderPassDelegate& delegate, std::string shaderDir, const std::string& assetsDir);

	void Destroy();
	
	bool UpdateDescriptors(const RenderOptions& options, bool skyboxUpdated);

	void Draw(VkCommandBuffer commandBuffer, u32 frameIndex,
		const RenderOptions& options,
		const std::vector<SceneRendererPrimitives::RenderableObject>& objects,
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
		const std::vector<VkBuffer>& meshUbos,
		const std::vector<VkBuffer>& materialUbos,
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
