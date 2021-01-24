#pragma once

#include "VulkanService.h"
#include "GpuTypes.h"
#include "RenderableMesh.h"
#include "TextureResource.h"
#include "UniformBufferObjects.h"
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
	void Destroy()
	{
		if (_vk)
		{
			vkDestroyBuffer(_vk->LogicalDevice(), _uniformBuffer, nullptr);
			vkFreeMemory(_vk->LogicalDevice(), _uniformBufferMemory, nullptr);
		}
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
	VulkanService* _vk = nullptr;
	VkDescriptorSet _descSet = nullptr;
	VkBuffer _uniformBuffer = nullptr;
	VkDeviceMemory _uniformBufferMemory = nullptr;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MaterialResourceManager
{
public:
	MaterialResourceManager() = delete;
	explicit MaterialResourceManager(VulkanService& vk, VkDescriptorPool pool, VkDescriptorSetLayout descSetLayout, const std::vector<std::unique_ptr<TextureResource>>& textures, TextureResourceId placeholder)
		: _vk(&vk), _pool(pool), _descSetLayout(descSetLayout), _textures(textures), _placeholder(placeholder)
	{}
	~MaterialResourceManager() = default;
	// Copy
	MaterialResourceManager(const MaterialResourceManager&) = delete;
	MaterialResourceManager& operator=(const MaterialResourceManager&) = delete;
	// Move
	MaterialResourceManager(MaterialResourceManager&&) = default;
	MaterialResourceManager& operator=(MaterialResourceManager&&) = default;

	const PbrMaterialResource& GetOrCreate(const Material& material, u32 swapImageIndex)
	{
		const auto key = CreateKey(material.Id.Value(), swapImageIndex);
		
		if (const auto it = _materialFrameResources.find(key); 
			it == _materialFrameResources.end())
		{
			// No match, create an store a new one
			auto res = CreateMaterialFrameResources(material);
			auto res2 = std::move(res);
			auto [it2, success] = _materialFrameResources.emplace(key, std::move(res2));
			assert(success);
			return it2->second;
		}
		else
		{
			return it->second;
		}
	}

	PbrMaterialResource CreateMaterialFrameResources(const Material& material) const
	{
		const auto numImagesInFlight = 1;
		
		// Create uniform buffers
		auto [materialBuffers, materialBuffersMemory] = vkh::CreateUniformBuffers(numImagesInFlight, sizeof(PbrMaterialUbo),
			_vk->LogicalDevice(), _vk->PhysicalDevice());


		// Create descriptor sets
		const auto materialDescSets = vkh::AllocateDescriptorSets(numImagesInFlight, _descSetLayout, _pool, _vk->LogicalDevice());


		// Get the id of an existing texture, fallback to placeholder if necessary.
		const auto pid = _placeholder.Value();
		auto GetId = [pid](const std::optional<Material::Map>& map) { return map.has_value() ? map->Id.Value() : pid; };


		for (u32 i = 0; i < numImagesInFlight; i++)
		{
			// Write updated descriptor sets
			WriteMaterialDescriptorSet(
				materialDescSets[i],
				materialBuffers[i],
				*_textures[GetId(material.BasecolorMap)],
				*_textures[GetId(material.NormalMap)],
				*_textures[GetId(material.RoughnessMap)],
				*_textures[GetId(material.MetalnessMap)],
				*_textures[GetId(material.AoMap)],
				*_textures[GetId(material.EmissiveMap)],
				*_textures[GetId(material.TransparencyMap)],
				_vk->LogicalDevice());
		}

		return PbrMaterialResource{_vk, materialDescSets[0], materialBuffers[0], materialBuffersMemory[0] };
	}


	static void WriteMaterialDescriptorSet(
		VkDescriptorSet descriptorSet,
		VkBuffer materialUbo,
		const TextureResource& basecolorMap, const TextureResource& normalMap, const TextureResource& roughnessMap,
		const TextureResource& metalnessMap, const TextureResource& aoMap, const TextureResource& emissiveMap,
		const TextureResource& transparencyMap, VkDevice device)
	{
		// Configure our new descriptor sets to point to our buffer/image data
		VkDescriptorBufferInfo materialUboInfo = {};
		{
			materialUboInfo.buffer = materialUbo;
			materialUboInfo.offset = 0;
			materialUboInfo.range = sizeof(PbrMaterialUbo);
		}

		const auto& s = descriptorSet;

		vkh::UpdateDescriptorSet(device, {
			vki::WriteDescriptorSet(s, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &materialUboInfo),
			vki::WriteDescriptorSet(s, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &basecolorMap.ImageInfo()),
			vki::WriteDescriptorSet(s, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &normalMap.ImageInfo()),
			vki::WriteDescriptorSet(s, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &roughnessMap.ImageInfo()),
			vki::WriteDescriptorSet(s, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &metalnessMap.ImageInfo()),
			vki::WriteDescriptorSet(s, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &aoMap.ImageInfo()),
			vki::WriteDescriptorSet(s, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &emissiveMap.ImageInfo()),
			vki::WriteDescriptorSet(s, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &transparencyMap.ImageInfo()),
			});
	}

private:
	static u32 CreateKey(u32 id, u32 frame)
	{
		assert(frame <= 3);          // only reserving 2 bits for frame
		assert(id <= UINT_MAX << 2); // id cant be larger than UINT_MAX<<2 as we remove 2 bits to store frame

		u32 hash = id << 2; // shift the id along 2 bits to make space for the frame
		hash |= frame;      // store the frame in the first 2 bits

		return hash;
	}

private:
	std::unordered_map<u32, PbrMaterialResource> _materialFrameResources{};
	VulkanService* _vk = nullptr;
	VkDescriptorPool _pool = nullptr;
	VkDescriptorSetLayout _descSetLayout = nullptr;
	const std::vector<std::unique_ptr<TextureResource>>& _textures;
	TextureResourceId _placeholder{};

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
	
	bool UpdateDescriptors(u32 imageIndex, const RenderOptions& options, bool skyboxUpdated, const SceneRendererPrimitives& scene);

	void Draw(VkCommandBuffer commandBuffer, u32 frameIndex,
		const RenderOptions& options,
		const std::vector<SceneRendererPrimitives::RenderableObject>& objects,
		const std::vector<Light>& lights,
		const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, const glm::mat4& lightSpaceMatrix);

	TextureResourceId CreateTextureResource(const std::string& path);
	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition);
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
