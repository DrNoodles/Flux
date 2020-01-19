#pragma once

#include "GpuTypes.h"
#include "VulkanHelpers.h"

#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // to comply with vulkan
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <set>
#include <array>
#include <chrono>
#include <string>

using vkh = VulkanHelpers;

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
	
	explicit Renderer(bool enableValidationLayers, std::string shaderDir, IRendererDelegate& delegate)
		: _delegate(delegate), _shaderDir(std::move(shaderDir))
	{
		_enableValidationLayers = enableValidationLayers;
		InitVulkan();
	}

	
	void DrawFrame(float dt, 
		const std::vector<ModelResourceId>& modelResourceIds,
		const std::vector<glm::mat4>& transforms,
		const glm::mat4& view)
	{
		assert(modelResourceIds.size() == transforms.size());
		
		// Sync CPU-GPU
		vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], true, UINT64_MAX);

		// Aquire an image from the swap chain
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame],
			nullptr, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapchain();
			return;
		}
		const auto isUsable = result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
		if (!isUsable)
		{
			throw std::runtime_error("Failed to acquire swapchain image!");
		}


		// If the image is still used by a previous frame, wait for it to finish!
		if (_imagesInFlight[imageIndex] != nullptr)
		{
			vkWaitForFences(_device, 1, &_imagesInFlight[imageIndex], true, UINT64_MAX);
		}
		// Mark the image as now being in use by this frame
		_imagesInFlight[imageIndex] = _inFlightFences[_currentFrame];


		auto startBench = std::chrono::steady_clock::now();

		//auto camera = _surfaceBuilder.UpdateState(dt);


		// Calc Projection
		const auto vfov = 45.f;
		const float aspect = _swapchainExtent.width / (float)_swapchainExtent.height;
		auto projection = glm::perspective(glm::radians(vfov), aspect, 0.1f, 1000.f);
		projection[1][1] *= -1; // flip Y to convert glm from OpenGL coord system to Vulkan

		// Update Model
		for (size_t i = 0; i < modelResourceIds.size(); i++)
		{

			UpdateUniformBuffer(
				_models[modelResourceIds[i].Id]->FrameResources[imageIndex].UniformBufferMemory,
				transforms[i],
				view, 
				projection, 
				_device);
		}

		vkh::RecordCommandBuffer(
			_commandBuffers[imageIndex],
			_models,
			imageIndex,
			_swapchainExtent,
			_swapchainFramebuffers[imageIndex],
			_renderPass,
			_pipeline, _pipelineLayout);


		// Execute command buffer with the image as an attachment in the framebuffer
		const uint32_t waitCount = 1; // waitSemaphores and waitStages arrays sizes must match as they're matched by index
		VkSemaphore waitSemaphores[waitCount] = { _imageAvailableSemaphores[_currentFrame] };
		VkPipelineStageFlags waitStages[waitCount] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		const uint32_t signalCount = 1;
		VkSemaphore signalSemaphores[signalCount] = { _renderFinishedSemaphores[_currentFrame] };

		VkSubmitInfo submitInfo = {};
		{
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &_commandBuffers[imageIndex]; // cmdbuf that binds the swapchain image we acquired as color attachment
			submitInfo.waitSemaphoreCount = waitCount;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.signalSemaphoreCount = signalCount;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}

		const std::chrono::duration<double, std::chrono::milliseconds::period> duration
			= std::chrono::steady_clock::now() - startBench;
		//std::cout << "# Update loop took:  " << std::setprecision(3) << duration.count() << "ms.\n";

		vkResetFences(_device, 1, &_inFlightFences[_currentFrame]);

		if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to submit Draw Command Buffer");
		}


		// Return the image to the swap chain for presentation
		std::array<VkSwapchainKHR, 1> swapchains = { _swapchain };
		VkPresentInfoKHR presentInfo = {};
		{
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;
			presentInfo.swapchainCount = (uint32_t)swapchains.size();
			presentInfo.pSwapchains = swapchains.data();
			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr;
		}

		result = vkQueuePresentKHR(_presentQueue, &presentInfo);
		if (FramebufferResized || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			FramebufferResized = false;
			RecreateSwapchain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed ot present swapchain image!");
		}

		_currentFrame = (_currentFrame + 1) % _maxFramesInFlight;
	}


	// TODO convert to RAII?
	void CleanUp()
	{
		vkDeviceWaitIdle(_device);

		CleanupSwapchainAndDependents();

		for (auto& x : _inFlightFences) { vkDestroyFence(_device, x, nullptr); }
		for (auto& x : _renderFinishedSemaphores) { vkDestroySemaphore(_device, x, nullptr); }
		for (auto& x : _imageAvailableSemaphores) { vkDestroySemaphore(_device, x, nullptr); }

		for (auto& mesh : _meshes)
		{
			//mesh.Vertices.clear();
			//mesh.Indices.clear();
			vkDestroyBuffer(_device, mesh->IndexBuffer, nullptr);
			vkFreeMemory(_device, mesh->IndexBufferMemory, nullptr);
			vkDestroyBuffer(_device, mesh->VertexBuffer, nullptr);
			vkFreeMemory(_device, mesh->VertexBufferMemory, nullptr);
		}

		for (auto& texture : _textures)
		{
			vkDestroySampler(_device, texture->Sampler, nullptr);
			vkDestroyImageView(_device, texture->View, nullptr);
			vkDestroyImage(_device, texture->Image, nullptr);
			vkFreeMemory(_device, texture->Memory, nullptr);
		}

		vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
		vkDestroyCommandPool(_device, _commandPool, nullptr);
		vkDestroyDevice(_device, nullptr);
		if (_enableValidationLayers) { vkh::DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr); }
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyInstance(_instance, nullptr);
	}


	TextureResourceId CreateTextureResource(const std::string& path)
	{
		auto tex = std::make_unique<TextureResource>();

		// TODO Pull the teture library out of the CreateTextureImage, just work on an TextureDefinition struct that
			// has an array of pixels and width, height, channels, etc

		std::tie(tex->Image, tex->Memory, tex->MipLevels, tex->Width, tex->Height)
			= vkh::CreateTextureImage(path, _commandPool, _graphicsQueue, _physicalDevice, _device);

		tex->View = vkh::CreateTextureImageView(tex->Image, tex->MipLevels, _device);

		tex->Sampler = vkh::CreateTextureSampler(tex->MipLevels, _device);

		const TextureResourceId id = (u32)_textures.size();
		_textures.emplace_back(std::move(tex));

		return id;
	}
	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition)
	{
		auto mesh = std::make_unique<MeshResource>();

		// Compute AABB
		std::vector<glm::vec3> positions{ meshDefinition.Vertices.size() };
		for (size_t i = 0; i < meshDefinition.Vertices.size(); i++)
		{
			positions[i] = meshDefinition.Vertices[i].Pos;
		}
		const AABB bounds{ positions };


		// Load mesh resource
		mesh->IndexCount = meshDefinition.Indices.size();
		mesh->VertexCount = meshDefinition.Vertices.size();
		mesh->Bounds = bounds;

		std::tie(mesh->VertexBuffer, mesh->VertexBufferMemory)
			= vkh::CreateVertexBuffer(meshDefinition.Vertices, _graphicsQueue, _commandPool, _physicalDevice, _device);

		std::tie(mesh->IndexBuffer, mesh->IndexBufferMemory)
			= vkh::CreateIndexBuffer(meshDefinition.Indices, _graphicsQueue, _commandPool, _physicalDevice, _device);

		
		const MeshResourceId id = (u32)_meshes.size();
		_meshes.emplace_back(std::move(mesh));
		
		return id;
	}
	ModelResourceId CreateModelResource(const CreateModelResourceInfo& createModelResourceInfo)
	{
		auto model = std::make_unique<ModelResource>();

		model->Mesh = _meshes[createModelResourceInfo.Mesh.Id].get();
		model->BasecolorMap = _textures[createModelResourceInfo.BasecolorMap.Id].get();
		model->NormalMap = _textures[createModelResourceInfo.NormalMap.Id].get();
		model->FrameResources = CreateModelFrameResources(*model);

		const ModelResourceId id = (u32)_models.size();
		_models.emplace_back(std::move(model));
		
		return id;
	}


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


	void InitVulkan()
	{
		_instance = vkh::CreateInstance(_enableValidationLayers, _validationLayers);

		if (_enableValidationLayers)
		{
			_debugMessenger = vkh::SetupDebugMessenger(_instance);
		}

		_surface = _delegate.CreateSurface(_instance);

		std::tie(_physicalDevice, _msaaSamples) = vkh::PickPhysicalDevice(_physicalDeviceExtensions, _instance, _surface);

		std::tie(_device, _graphicsQueue, _presentQueue)
			= vkh::CreateLogicalDevice(_physicalDevice, _surface, _validationLayers, _physicalDeviceExtensions);

		_commandPool = vkh::CreateCommandPool(vkh::FindQueueFamilies(_physicalDevice, _surface), _device);

		_descriptorSetLayout = vkh::CreateDescriptorSetLayout(_device);

		const auto size = _delegate.GetFramebufferSize();
		CreateSwapchainAndDependents(size.width, size.height);
		
		std::tie(_renderFinishedSemaphores, _imageAvailableSemaphores, _inFlightFences, _imagesInFlight)
			= vkh::CreateSyncObjects(_maxFramesInFlight, _swapchainImages.size(), _device);
	}


	static void UpdateUniformBuffer(VkDeviceMemory uniformBufferMemory, const glm::mat4& model, 
		const glm::mat4& view, const glm::mat4& projection, VkDevice device)
	{
		// Create new ubo
		UniversalUbo ubo = {};
		{
			ubo.Model = model;
			ubo.View = view;
			ubo.Projection = projection;
			ubo.DrawNormalMap = false;
			ubo.ExposureBias = 1.0f;
		}
		// Push ubo
		void* data;
		vkMapMemory(device, uniformBufferMemory, 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(device, uniformBufferMemory);
	}


	void CleanupSwapchainAndDependents()
	{
		vkDestroyImageView(_device, _colorImageView, nullptr);
		vkDestroyImage(_device, _colorImage, nullptr);
		vkFreeMemory(_device, _colorImageMemory, nullptr);
		
		vkDestroyImageView(_device, _depthImageView, nullptr);
		vkDestroyImage(_device, _depthImage, nullptr);
		vkFreeMemory(_device, _depthImageMemory, nullptr);

		for (auto& model : _models)
		{
			for (auto& info : model->FrameResources)
			{
				vkDestroyBuffer(_device, info.UniformBuffer, nullptr);
				vkFreeMemory(_device, info.UniformBufferMemory, nullptr); 
				//vkFreeDescriptorSets(_device, _descriptorPool, (uint32_t)mesh.DescriptorSets.size(), mesh.DescriptorSets.data());
			}
		}

		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
		for (auto& x : _swapchainFramebuffers) { vkDestroyFramebuffer(_device, x, nullptr); }
		vkFreeCommandBuffers(_device, _commandPool, (uint32_t)_commandBuffers.size(), _commandBuffers.data());
		vkDestroyPipeline(_device, _pipeline, nullptr);
		vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
		vkDestroyRenderPass(_device, _renderPass, nullptr);
		for (auto& x : _swapchainImageViews) { vkDestroyImageView(_device, x, nullptr); }
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	}


	void CreateSwapchainAndDependents(int width, int height)
	{
		_swapchain = vkh::CreateSwapchain({ (uint32_t)width, (uint32_t)height }, _physicalDevice, _surface, _device,
			_swapchainImages, _swapchainImageFormat, _swapchainExtent);

		_swapchainImageViews
			= vkh::CreateImageViews(_swapchainImages, _swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, _device);

		std::tie(_colorImage, _colorImageMemory, _colorImageView)
			= vkh::CreateColorResources(_swapchainImageFormat, _swapchainExtent, _msaaSamples,
				_commandPool, _graphicsQueue, _device, _physicalDevice);

		std::tie(_depthImage, _depthImageMemory, _depthImageView)
			= vkh::CreateDepthResources(_swapchainExtent, _msaaSamples, _commandPool, _graphicsQueue, _device,
				_physicalDevice);

		_renderPass = vkh::CreateRenderPass(_msaaSamples, _swapchainImageFormat, _device, _physicalDevice);

		std::tie(_pipeline, _pipelineLayout)
			= vkh::CreateGraphicsPipeline(_shaderDir, _descriptorSetLayout, _msaaSamples, _renderPass, _device,
				_swapchainExtent);

		_swapchainFramebuffers
			= vkh::CreateFramebuffer(_colorImageView, _depthImageView, _device, _renderPass, _swapchainExtent,
				_swapchainImageViews);


		const size_t numImagesInFlight = _swapchainImages.size();

		
		const uint32_t numDescSetGroups = 1000; // Max renderable objects! This is gross, but it'll do for now.
		_descriptorPool = vkh::CreateDescriptorPool((uint32_t)numImagesInFlight * numDescSetGroups, _device);


		// Create uniform buffers and descriptor sets per image in swapchain
		for (auto& model : _models)
		{
			std::vector<VkBuffer> uniformBuffers;
			std::vector<VkDeviceMemory> uniformBuffersMemory;
			
			std::tie(uniformBuffers, uniformBuffersMemory)
				= vkh::CreateUniformBuffers(numImagesInFlight, _device, _physicalDevice);

			std::vector<VkDescriptorSet> descriptorSets = vkh::CreateDescriptorSets((uint32_t)numImagesInFlight,
				_descriptorSetLayout, _descriptorPool, uniformBuffers, *model->BasecolorMap, *model->NormalMap, 
				_device);

			
			auto& modelInfos = model->FrameResources;
			modelInfos.resize(numImagesInFlight);

			for (auto i = 0; i < numImagesInFlight; i++)
			{
				modelInfos[i].UniformBuffer = uniformBuffers[i];
				modelInfos[i].UniformBufferMemory = uniformBuffersMemory[i];
				modelInfos[i].DescriptorSet = descriptorSets[i];
			}
		}

		
		_commandBuffers = vkh::CreateCommandBuffers(
			(uint32_t)numImagesInFlight,
			_models,
			_swapchainExtent,
			_swapchainFramebuffers,
			_commandPool, _device, _renderPass,
			_pipeline, _pipelineLayout);
		// TODO Break CreateSyncObjects() method so we can recreate the parts that are dependend on num swapchainImages
	}

	
	void RecreateSwapchain()
	{
		auto size = _delegate.WaitTillFramebufferHasSize();

		vkDeviceWaitIdle(_device);
		CleanupSwapchainAndDependents();
		CreateSwapchainAndDependents(size.width, size.height);
	}


	std::vector<ModelResourceFrame> CreateModelFrameResources(const ModelResource& model) const
	{
		std::vector<ModelResourceFrame> modelInfos{};

		std::vector<VkBuffer> uniformBuffers;
		std::vector<VkDeviceMemory> uniformBuffersMemory;
		std::vector<VkDescriptorSet> descriptorSets;
		const auto numThingsToMake = _swapchainImages.size();


		std::tie(uniformBuffers, uniformBuffersMemory) = vkh::CreateUniformBuffers(numThingsToMake, _device, _physicalDevice);


		descriptorSets = vkh::CreateDescriptorSets((uint32_t)numThingsToMake, _descriptorSetLayout, _descriptorPool,
			uniformBuffers, *model.BasecolorMap, *model.NormalMap, _device);


		modelInfos.resize(numThingsToMake);
		for (auto i = 0; i < numThingsToMake; i++)
		{
			modelInfos[i].UniformBuffer = uniformBuffers[i];
			modelInfos[i].UniformBufferMemory = uniformBuffersMemory[i];
			modelInfos[i].DescriptorSet = descriptorSets[i];
		}

		return modelInfos;
	}
};
