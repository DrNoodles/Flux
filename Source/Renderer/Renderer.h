#pragma once

#include "GpuTypes.h"
#include <App/IModelLoaderService.h> // Used for mesh/model/texture definitions TODO remove dependency on App layer
#include "Renderable.h"
#include "Material.h"

#include <vector>
#include "TextureResource.h"

struct UniversalUbo;
struct RenderableCreateInfo;

class IRendererDelegate
{
public:
	virtual ~IRendererDelegate() = default;
	virtual VkSurfaceKHR CreateSurface(VkInstance instance) const = 0;

	// Renderer stuff
	virtual VkExtent2D GetFramebufferSize() = 0;
	virtual VkExtent2D WaitTillFramebufferHasSize() = 0;
};

class Renderer
{
public:
	bool FramebufferResized = false;

	explicit Renderer(bool enableValidationLayers, const std::string& shaderDir, const std::string& assetsDir, 
		IRendererDelegate& delegate);
	void DrawFrame(float dt,
		const std::vector<RenderableResourceId>& renderableIds,
		const std::vector<glm::mat4>& transforms,
		const std::vector<Light>& lights,
		const glm::mat4& view, const glm::vec3& camPos);
	void CleanUp(); // TODO convert to RAII?
	TextureResourceId CreateTextureResource(const std::string& path);
	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition);
	RenderableResourceId CreateRenderable(const RenderableCreateInfo& createModelResourceInfo);
	//TextureResourceId CreateCubemapTextureResource(const std::string& equirectangularHdrPath);
	TextureResourceId CreateCubemapTextureResource(const std::array<std::string, 6>& sidePaths);



	const Renderable& GetRenderable(const RenderableResourceId& id) const { return *_renderables[id.Id]; }
	void SetMaterial(const RenderableResourceId& renderableResId, const Material& material);





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
	VkPipeline _pipeline = nullptr;
	VkPipelineLayout _pipelineLayout = nullptr;

	VkCommandPool _commandPool = nullptr;
	std::vector<VkCommandBuffer> _commandBuffers{};
	
	// Synchronization
	std::vector<VkSemaphore> _renderFinishedSemaphores{};
	std::vector<VkSemaphore> _imageAvailableSemaphores{};
	std::vector<VkFence> _inFlightFences{};
	std::vector<VkFence> _imagesInFlight{};
	size_t _currentFrame = 0;

	VkDescriptorSetLayout _pbrDescriptorSetLayout = nullptr;
	VkDescriptorPool _descriptorPool = nullptr;

	// Resources
	std::vector<VkBuffer> _lightBuffers{}; // 1 per frame in flight
	std::vector<VkDeviceMemory> _lightBuffersMemory{};
	
	std::vector<std::unique_ptr<Renderable>> _renderables{};
	std::vector<std::unique_ptr<MeshResource>> _meshes{};
	std::vector<std::unique_ptr<TextureResource>> _textures{};
	TextureResourceId _placeholderTexture;


	void InitVulkan();
	void CleanupSwapchainAndDependents();

	void CreateSwapchainAndDependents(int width, int height);
	void RecreateSwapchain();
	std::vector<ModelResourceFrame> CreateModelFrameResources(u32 numImagesInFlight, const Renderable& renderable) const;





	#pragma region Shared

	static VkDescriptorPool CreateDescriptorPool(u32 numImagesInFlight, VkDevice device);

	// Defines the layout of the data bound to the shaders
	static VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device);

	// Associates the UBO and texture to sets for use in shaders
	static std::vector<VkDescriptorSet> CreateDescriptorSets(
		uint32_t count,
		VkDescriptorSetLayout layout,
		VkDescriptorPool pool,
		const std::vector<VkBuffer>& modelUbos,
		const std::vector<VkBuffer>& lightUbos,
		const TextureResource& basecolorMap,
		const TextureResource& normalMap,
		const TextureResource& roughnessMap,
		const TextureResource& metalnessMap,
		const TextureResource& aoMap,
		VkDevice device);

	static void WriteDescriptorSets(
		uint32_t count,
		const std::vector<VkDescriptorSet>& descriptorSets,
		const std::vector<VkBuffer>& modelUbos,
		const std::vector<VkBuffer>& lightUbos,
		const TextureResource& basecolorMap,
		const TextureResource& normalMap,
		const TextureResource& roughnessMap,
		const TextureResource& metalnessMap,
		const TextureResource& aoMap,
		VkDevice device);

	#pragma endregion Shared


	
	#pragma region Pbr

	// The uniform and push values referenced by the shader that can be updated at draw time
	static VkPipeline CreatePbrGraphicsPipeline(const std::string& shaderDir, VkPipelineLayout pipelineLayout,
			VkSampleCountFlagBits msaaSamples, VkRenderPass renderPass, VkDevice device,
			const VkExtent2D& swapchainExtent);

	#pragma endregion Pbr


	#pragma region Cubemap // Everything cubemap: resources, pipelines, etc and rendering



	#pragma endregion Cubemap
};
