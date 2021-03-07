#pragma once

#include "Renderer/LowLevel/VulkanService.h"

class VulkanService;
class ResourceRegistry;
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
struct PbrMaterialResource
{
public:
	PbrMaterialResource() = delete;
	PbrMaterialResource(VulkanService* vk, VkDescriptorSet descSet, VkBuffer buffer, VkDeviceMemory memory)
		: _vk(vk), _descSet(descSet), _uniformBuffer(buffer), _uniformBufferMemory(memory)
	{
		// TODO Create the resource here so it's symmetrical with Destroy()
	}
	~PbrMaterialResource()
	{
		Destroy();
	}
	
	// Delete Copy
	PbrMaterialResource(const PbrMaterialResource&) = delete;
	PbrMaterialResource& operator=(const PbrMaterialResource&) = delete;
	// Move
	PbrMaterialResource(PbrMaterialResource&& other) noexcept { *this = std::move(other); }
	PbrMaterialResource& operator=(PbrMaterialResource&& other) noexcept
	{
		if (this != &other)
		{
			Destroy();
			_vk = other._vk;
			_descSet = other._descSet;
			_uniformBuffer = other._uniformBuffer;
			_uniformBufferMemory = other._uniformBufferMemory;
			other._vk = nullptr;
			other._descSet = nullptr;
			other._uniformBuffer = nullptr;
			other._uniformBufferMemory = nullptr;
		}
		
		return *this;
	}
	

	VkDescriptorSet GetMaterialDescriptorSet() const { return _descSet; }
	VkBuffer GetMaterialUniformBuffer() const { return _uniformBuffer; }
	VkDeviceMemory GetMaterialUniformBufferMemory() const { return _uniformBufferMemory; }
	
private:
	void Destroy();

	VulkanService* _vk = nullptr;
	VkDescriptorSet _descSet = nullptr;
	VkBuffer _uniformBuffer = nullptr;
	VkDeviceMemory _uniformBufferMemory = nullptr;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MaterialResourceManager
{
private:
	// Dependencies
	VulkanService* _vk = nullptr;
	ResourceRegistry* _resourceRegistry = nullptr;
	VkDescriptorPool _pool = nullptr;
	VkDescriptorSetLayout _descSetLayout = nullptr;
	TextureResourceId _placeholderTexture{};

	
	std::unordered_map<u32, PbrMaterialResource> _materialFrameResources{};

public:
	MaterialResourceManager() = delete;
	explicit MaterialResourceManager(VulkanService& vk, VkDescriptorPool pool, VkDescriptorSetLayout descSetLayout, ResourceRegistry* registry, TextureResourceId placeholder)
		: _vk(&vk), _resourceRegistry(registry), _pool(pool), _descSetLayout(descSetLayout), _placeholderTexture(placeholder)
	{}
	~MaterialResourceManager() = default;
	// Copy
	MaterialResourceManager(const MaterialResourceManager&) = delete;
	MaterialResourceManager& operator=(const MaterialResourceManager&) = delete;
	// Move
	MaterialResourceManager(MaterialResourceManager&&) = default;
	MaterialResourceManager& operator=(MaterialResourceManager&&) = delete;

	const PbrMaterialResource& GetOrCreate(const Material& material, u32 swapImageIndex);

	PbrMaterialResource CreateMaterialFrameResources(const Material& material) const;


	static void WriteMaterialDescriptorSet(
		VkDescriptorSet descriptorSet,
		VkBuffer materialUbo,
		const TextureResource& basecolorMap, const TextureResource& normalMap, const TextureResource& roughnessMap,
		const TextureResource& metalnessMap, const TextureResource& aoMap, const TextureResource& emissiveMap,
		const TextureResource& transparencyMap, VkDevice device);

private:
	static u32 CreateKey(u32 id, u32 frame)
	{
		assert(frame <= 3);          // only reserving 2 bits for frame
		assert(id <= UINT_MAX << 2); // id cant be larger than UINT_MAX<<2 as we remove 2 bits to store frame

		u32 hash = id << 2; // shift the id along 2 bits to make space for the frame
		hash |= frame;      // store the frame in the first 2 bits

		return hash;
	}


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
	VkDescriptorSetLayout _materialDescriptorSetLayout = nullptr;
	VkDescriptorSetLayout _pbrDescriptorSetLayout = nullptr;

	// Resources
	std::vector<VkBuffer> _lightBuffers{}; // 1 per frame in flight
	std::vector<VkDeviceMemory> _lightBuffersMemory{};

	std::unique_ptr<MaterialResourceManager> _materialFrameResources = nullptr;
	std::vector<std::unique_ptr<RenderableMesh>> _renderables{};

	bool _refreshRenderableDescriptorSets = false;

	// Required resources
	TextureResourceId _placeholderTexture;

	RenderOptions _lastOptions;
	ResourceRegistry* _resourceRegistry = nullptr;


public: // Members
	const std::vector<std::unique_ptr<RenderableMesh>>& Hack_GetRenderables() const { return _renderables; }

	PbrModelRenderPass(VulkanService& vulkanService, ResourceRegistry* registry, IPbrModelRenderPassDelegate& delegate, std::string shaderDir, const std::string& assetsDir);

	void Destroy();
	
	bool UpdateDescriptors(u32 imageIndex, const RenderOptions& options, bool skyboxUpdated, const SceneRendererPrimitives& scene);

	void Draw(VkCommandBuffer commandBuffer, u32 frameIndex,
		const RenderOptions& options,
		const std::vector<SceneRendererPrimitives::RenderableObject>& objects,
		const std::vector<Light>& lights,
		const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, const glm::mat4& lightSpaceMatrix);

	RenderableResourceId CreateRenderable(const MeshResourceId& meshId);

	VkRenderPass GetRenderPass() const { return _renderPass; }
	
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

	std::vector<PbrCommonResourceFrame> CreateCommonFrameResources(u32 numImagesInFlight) const;

	// Defines the layout of the data bound to the shaders
	static VkDescriptorSetLayout CreateMaterialDescriptorSetLayout(VkDevice device);
	static VkDescriptorSetLayout CreatePbrDescriptorSetLayout(VkDevice device);

	static void WriteCommonDescriptorSet(
		VkDescriptorSet descriptorSet,
		VkBuffer meshUbo,
		VkBuffer lightUbo,
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
