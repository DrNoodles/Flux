#pragma once

#include "GpuTypes.h"
#include <App/IModelLoaderService.h> // Used for mesh/model/texture definitions TODO remove dependency on App layer
#include <vector>

struct CreateModelResourceInfo;

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

	explicit Renderer(bool enableValidationLayers, std::string shaderDir, IRendererDelegate& delegate);
	void DrawFrame(float dt,
	               const std::vector<ModelResourceId>& modelResourceIds,
	               const std::vector<glm::mat4>& transforms,
	               const glm::mat4& view);
	void CleanUp(); // TODO convert to RAII?
	TextureResourceId CreateTextureResource(const std::string& path);
	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition);
	ModelResourceId CreateModelResource(const CreateModelResourceInfo& createModelResourceInfo);


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

	VkDescriptorSetLayout _descriptorSetLayout = nullptr;
	VkDescriptorPool _descriptorPool = nullptr;

	// Resources
	std::vector<std::unique_ptr<ModelResource>> _models{};
	std::vector<std::unique_ptr<MeshResource>> _meshes{};
	std::vector<std::unique_ptr<TextureResource>> _textures{};


	void InitVulkan();
	static void UpdateUniformBuffer(VkDeviceMemory uniformBufferMemory, const glm::mat4& model,
	                                const glm::mat4& view, const glm::mat4& projection, VkDevice device);
	void CleanupSwapchainAndDependents();
	void CreateSwapchainAndDependents(int width, int height);
	void RecreateSwapchain();
	std::vector<ModelResourceFrame> CreateModelFrameResources(const ModelResource& model) const;
};
