#pragma once

#include "GpuTypes.h"
#include <App/IModelLoaderService.h> // Used for mesh/model/texture definitions TODO remove dependency on App layer
#include "Renderable.h"
#include "Material.h"
#include "TextureResource.h"
#include "CubemapTextureLoader.h"

#include <vector>

struct UniversalUbo;
struct RenderableCreateInfo;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//struct IblTextureResourceIds
//{
//	TextureResourceId EnvironmentCubemapId;
//	TextureResourceId IrradianceCubemapId;
//	TextureResourceId PrefilterCubemapId;
//	TextureResourceId BrdfLutId;
//};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IRendererDelegate
{
public:
	virtual ~IRendererDelegate() = default;
	virtual VkSurfaceKHR CreateSurface(VkInstance instance) const = 0;

	// Renderer stuff
	virtual VkExtent2D GetFramebufferSize() = 0;
	virtual VkExtent2D WaitTillFramebufferHasSize() = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Renderer
{
public:
	bool FramebufferResized = false;

	explicit Renderer(bool enableValidationLayers, const std::string& shaderDir, const std::string& assetsDir, 
	                  IRendererDelegate& delegate, IModelLoaderService& modelLoaderService);
	void DrawFrame(float dt,
		const std::vector<RenderableResourceId>& renderableIds,
		const std::vector<glm::mat4>& transforms,
		const std::vector<Light>& lights,
		glm::mat4 view, glm::vec3 camPos);
	void CleanUp(); // TODO convert to RAII?

	TextureResourceId CreateTextureResource(const std::string& path);

	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition);

	RenderableResourceId CreateRenderable(const RenderableCreateInfo& createInfo);



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

	const Renderable& GetRenderable(const RenderableResourceId& id) const { return *_renderables[id.Id]; }
	void SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat);
	void SetSkybox(const SkyboxResourceId& resourceId);


private:
	// Dependencies
	IRendererDelegate& _delegate;

	const size_t _maxFramesInFlight = 2;
	std::string _shaderDir{};
	bool _enableValidationLayers = false;
	const std::vector<const char*> _validationLayers = { "VK_LAYER_KHRONOS_validation", };
	const std::vector<const char*> _physicalDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkInstance _instance = nullptr;
	VkSurfaceKHR _surface = nullptr;
	VkDebugUtilsMessengerEXT _debugMessenger = nullptr;
	VkPhysicalDevice _physicalDevice = nullptr;
	VkDevice _device = nullptr;
	VkQueue _graphicsQueue = nullptr;
	VkQueue _presentQueue = nullptr;

	VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkSwapchainKHR _swapchain = nullptr;
	VkFormat _swapchainImageFormat{};
	VkExtent2D _swapchainExtent{};
	std::vector<VkFramebuffer> _swapchainFramebuffers{};
	std::vector<VkImage> _swapchainImages{};
	std::vector<VkImageView> _swapchainImageViews{};

	// Color image
	VkImage _colorImage = nullptr;
	VkDeviceMemory _colorImageMemory = nullptr;
	VkImageView _colorImageView = nullptr;
	
	// Depth image - one instance paired with each swapchain instance for use in the framebuffer
	VkImage _depthImage = nullptr;
	VkDeviceMemory _depthImageMemory = nullptr;
	VkImageView _depthImageView = nullptr;
	
	VkRenderPass _renderPass = nullptr;
	
	VkDescriptorPool _descriptorPool = nullptr;
	VkCommandPool _commandPool = nullptr;
	std::vector<VkCommandBuffer> _commandBuffers{};
	
	// Synchronization
	std::vector<VkSemaphore> _renderFinishedSemaphores{};
	std::vector<VkSemaphore> _imageAvailableSemaphores{};
	std::vector<VkFence> _inFlightFences{};
	std::vector<VkFence> _imagesInFlight{};
	size_t _currentFrame = 0;

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
	std::vector<std::unique_ptr<Renderable>> _renderables{};
	
	std::vector<std::unique_ptr<MeshResource>> _meshes{};
	std::vector<std::unique_ptr<TextureResource>> _textures{};

	// Required resources
	TextureResourceId _placeholderTexture;
	MeshResourceId _skyboxMesh;


	void InitVulkan();
	void CleanupSwapchainAndDependents();

	void CreateSwapchainAndDependents(int width, int height);
	void RecreateSwapchain();
	std::vector<PbrModelResourceFrame> CreatePbrModelFrameResources(u32 numImagesInFlight, const Renderable& renderable) const;



	#pragma region Shared

	static VkDescriptorPool CreateDescriptorPool(u32 numImagesInFlight, VkDevice device);

	#pragma endregion Shared


	
	#pragma region Pbr

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

	std::vector<SkyboxResourceFrame>
	CreateSkyboxModelFrameResources(u32 numImagesInFlight, const Skybox& skybox) const;

	// Defines the layout of the data bound to the shaders
	static VkDescriptorSetLayout CreateSkyboxDescriptorSetLayout(VkDevice device);

	// Associates the UBO and texture to sets for use in shaders
	static void WriteSkyboxDescriptorSets(
		u32 count,
		const std::vector<VkDescriptorSet>& descriptorSets,
		const std::vector<VkBuffer>& skyboxVertUbo,
		const TextureResource& skyboxMap,
		VkDevice device);

	// The uniform and push values referenced by the shader that can be updated at draw time
	static VkPipeline CreateSkyboxGraphicsPipeline(const std::string& shaderDir, VkPipelineLayout pipelineLayout,
		VkSampleCountFlagBits msaaSamples, VkRenderPass renderPass, VkDevice device,
		const VkExtent2D& swapchainExtent);

	#pragma endregion Skybox
};
