
#include "Renderer.h"
#include "GpuTypes.h"
#include "VulkanHelpers.h"
#include "UniformBufferObjects.h"
#include "Renderable.h"

#define GLFW_INCLUDE_VULKAN // glfw includes vulkan.h
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // to comply with vulkan
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <chrono>

using vkh = VulkanHelpers;

Renderer::Renderer(bool enableValidationLayers, std::string shaderDir, std::string assetsDir,
	IRendererDelegate& delegate): _delegate(delegate), _shaderDir(std::move(shaderDir))
{
	_enableValidationLayers = enableValidationLayers;
	InitVulkan();
	_placeholderTexture = CreateTextureResource(assetsDir + "placeholder.png");
}

void Renderer::DrawFrame(float dt,
	const std::vector<RenderableResourceId>& renderableIds,
	const std::vector<glm::mat4>& transforms,
	const std::vector<Light>& lights,
	const glm::mat4& view, const glm::vec3& camPos)
{
	assert(renderableIds.size() == transforms.size());

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


	
	// Update light buffers

	// Single light support
	auto lightUbo = LightUbo::Create(lights);
	void* data;
	auto size = sizeof(lightUbo);
	vkMapMemory(_device, _lightBuffersMemory[imageIndex], 0, size, 0, &data);
	memcpy(data, &lightUbo, size);
	vkUnmapMemory(_device, _lightBuffersMemory[imageIndex]);
	
	
	// Update Model
	for (size_t i = 0; i < renderableIds.size(); i++)
	{
		UniversalUboCreateInfo info = {};
		info.Model = transforms[i];
		info.View = view;
		info.Projection = projection;
		info.CamPos = camPos;
		info.ExposureBias = 1.0f;
		info.ShowNormalMap = false;

		const auto& renderable = _renderables[renderableIds[i].Id].get();
		
		auto& modelBufferMemory = renderable->FrameResources[imageIndex].UniformBufferMemory;
		auto modelUbo = UniversalUbo::Create(info, renderable->Mat);

		// Update model ubo
		void* data;
		vkMapMemory(_device, modelBufferMemory, 0, sizeof(modelUbo), 0, &data);
		memcpy(data, &modelUbo, sizeof(modelUbo));
		vkUnmapMemory(_device, modelBufferMemory);
	}

	vkh::RecordCommandBuffer(
		_commandBuffers[imageIndex],
		_renderables,
		_meshes,
		imageIndex,
		_swapchainExtent,
		_swapchainFramebuffers[imageIndex],
		_renderPass,
		_pipeline, _pipelineLayout);


	// Execute command buffer with the image as an attachment in the framebuffer
	const uint32_t waitCount = 1; // waitSemaphores and waitStages arrays sizes must match as they're matched by index
	VkSemaphore waitSemaphores[waitCount] = {_imageAvailableSemaphores[_currentFrame]};
	VkPipelineStageFlags waitStages[waitCount] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	const uint32_t signalCount = 1;
	VkSemaphore signalSemaphores[signalCount] = {_renderFinishedSemaphores[_currentFrame]};

	VkSubmitInfo submitInfo = {};
	{
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];
		// cmdbuf that binds the swapchain image we acquired as color attachment
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
	std::array<VkSwapchainKHR, 1> swapchains = {_swapchain};
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

void Renderer::CleanUp()
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

TextureResourceId Renderer::CreateTextureResource(const std::string& path)
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

MeshResourceId Renderer::CreateMeshResource(const MeshDefinition& meshDefinition)
{
	auto mesh = std::make_unique<MeshResource>();

	// Compute AABB
	std::vector<glm::vec3> positions{meshDefinition.Vertices.size()};
	for (size_t i = 0; i < meshDefinition.Vertices.size(); i++)
	{
		positions[i] = meshDefinition.Vertices[i].Pos;
	}
	const AABB bounds{positions};


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

RenderableResourceId Renderer::CreateRenderable(const RenderableCreateInfo& createModelResourceInfo)
{
	auto model = std::make_unique<Renderable>();
	model->MeshId = createModelResourceInfo.Mesh;
	model->Mat = createModelResourceInfo.Mat;
	model->FrameResources = CreateModelFrameResources(_swapchainImages.size(), *model);

	const RenderableResourceId id = (u32)_renderables.size();
	_renderables.emplace_back(std::move(model));

	return id;
}



void Renderer::SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat)
{
	auto renderable = _renderables[renderableResId.Id].get();
	auto& oldMat = renderable->Mat;

	// Existing ids
	const auto currentBasecolorMapId = oldMat.BasecolorMap.value_or(_placeholderTexture).Id;
	const auto currentNormalMapId = oldMat.NormalMap.value_or(_placeholderTexture).Id;
	const auto currentRoughnessMapId = oldMat.RoughnessMap.value_or(_placeholderTexture).Id;
	const auto currentMetalnessMapId = oldMat.MetalnessMap.value_or(_placeholderTexture).Id;
	const auto currentAoMapId = oldMat.AoMap.value_or(_placeholderTexture).Id;

	// New ids
	const auto basecolorMapId = newMat.BasecolorMap.value_or(_placeholderTexture).Id;
	const auto normalMapId = newMat.NormalMap.value_or(_placeholderTexture).Id;
	const auto roughnessMapId = newMat.RoughnessMap.value_or(_placeholderTexture).Id;
	const auto metalnessMapId = newMat.MetalnessMap.value_or(_placeholderTexture).Id;
	const auto aoMapId = newMat.AoMap.value_or(_placeholderTexture).Id;


	// Store new mat
	renderable->Mat = newMat;


	// Bail early if the new descriptor set is identical (eg, if not changing a Map id!)
	const bool descriptorSetsMatch =
		currentBasecolorMapId == basecolorMapId &&
		currentNormalMapId == normalMapId &&
		currentRoughnessMapId == roughnessMapId &&
		currentMetalnessMapId == metalnessMapId &&
		currentAoMapId == aoMapId;

	if (descriptorSetsMatch)
	{
		return;
	}


	// Gather descriptor sets and uniform buffers
	const auto count = renderable->FrameResources.size();
	std::vector<VkDescriptorSet> descriptorSets{};
	std::vector<VkBuffer> modelBuffers{};
	descriptorSets.resize(count);
	modelBuffers.resize(count);
	for (size_t i = 0; i < count; i++)
	{
		descriptorSets[i] = renderable->FrameResources[i].DescriptorSet;
		modelBuffers[i] = renderable->FrameResources[i].UniformBuffer;
	}

	
	// Write updated descriptor sets
	vkh::WriteDescriptorSets((u32)count, descriptorSets, 
		modelBuffers,
		_lightBuffers,
		*_textures[basecolorMapId],
		*_textures[normalMapId],
		*_textures[roughnessMapId],
		*_textures[metalnessMapId],
		*_textures[aoMapId],
		_device);
}

void Renderer::InitVulkan()
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

void Renderer::CleanupSwapchainAndDependents()
{
	vkDestroyImageView(_device, _colorImageView, nullptr);
	vkDestroyImage(_device, _colorImage, nullptr);
	vkFreeMemory(_device, _colorImageMemory, nullptr);

	vkDestroyImageView(_device, _depthImageView, nullptr);
	vkDestroyImage(_device, _depthImage, nullptr);
	vkFreeMemory(_device, _depthImageMemory, nullptr);
	
	for (auto& renderable : _renderables)
	{
		for (auto& info : renderable->FrameResources)
		{
			vkDestroyBuffer(_device, info.UniformBuffer, nullptr);
			vkFreeMemory(_device, info.UniformBufferMemory, nullptr);
			//vkFreeDescriptorSets(_device, _descriptorPool, (uint32_t)mesh.DescriptorSets.size(), mesh.DescriptorSets.data());
		}
	}

	for (auto& x : _lightBuffers) { vkDestroyBuffer(_device, x, nullptr); }
	for (auto& x : _lightBuffersMemory) { vkFreeMemory(_device, x, nullptr); }

	vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
	for (auto& x : _swapchainFramebuffers) { vkDestroyFramebuffer(_device, x, nullptr); }
	vkFreeCommandBuffers(_device, _commandPool, (uint32_t)_commandBuffers.size(), _commandBuffers.data());
	vkDestroyPipeline(_device, _pipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
	vkDestroyRenderPass(_device, _renderPass, nullptr);
	for (auto& x : _swapchainImageViews) { vkDestroyImageView(_device, x, nullptr); }
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

void Renderer::CreateSwapchainAndDependents(int width, int height)
{
	_swapchain = vkh::CreateSwapchain({(uint32_t)width, (uint32_t)height}, _physicalDevice, _surface, _device,
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


	// Create light uniform buffersper swapchain image
	std::tie(_lightBuffers, _lightBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(LightUbo), _device, _physicalDevice);

	// Create model uniform buffers and descriptor sets per swapchain image
	for (auto& renderable : _renderables)
	{
		renderable->FrameResources = CreateModelFrameResources(numImagesInFlight , *renderable);
	}


	_commandBuffers = vkh::CreateCommandBuffers(
		(uint32_t)numImagesInFlight,
		_renderables,
		_meshes,
		_swapchainExtent,
		_swapchainFramebuffers,
		_commandPool, _device, _renderPass,
		_pipeline, _pipelineLayout);
	// TODO Break CreateSyncObjects() method so we can recreate the parts that are dependend on num swapchainImages
}

void Renderer::RecreateSwapchain()
{
	auto size = _delegate.WaitTillFramebufferHasSize();

	vkDeviceWaitIdle(_device);
	CleanupSwapchainAndDependents();
	CreateSwapchainAndDependents(size.width, size.height);
}


std::vector<ModelResourceFrame> Renderer::CreateModelFrameResources(size_t numImagesInFlight, const Renderable& renderable) const
{
	std::vector<VkBuffer> modelBuffers;
	std::vector<VkDeviceMemory> modelBuffersMemory;
	std::tie(modelBuffers, modelBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(UniversalUbo), _device, _physicalDevice);


	// Get the id of an existing texture, fallback to placeholder if necessary.
	const auto basecolorMapId = renderable.Mat.BasecolorMap.value_or(_placeholderTexture).Id;
	const auto normalMapId = renderable.Mat.NormalMap.value_or(_placeholderTexture).Id;
	const auto roughnessMapId = renderable.Mat.RoughnessMap.value_or(_placeholderTexture).Id;
	const auto metalnessMapId = renderable.Mat.MetalnessMap.value_or(_placeholderTexture).Id;
	const auto aoMapId = renderable.Mat.AoMap.value_or(_placeholderTexture).Id;

	
	std::vector<VkDescriptorSet> descriptorSets = vkh::CreateDescriptorSets(
		(uint32_t)numImagesInFlight,
		_descriptorSetLayout, _descriptorPool,
		modelBuffers,
		_lightBuffers,
		*_textures[basecolorMapId],
		*_textures[normalMapId],
		*_textures[roughnessMapId],
		*_textures[metalnessMapId],
		*_textures[aoMapId],
		_device);
	

	std::vector<ModelResourceFrame> modelInfos;
	modelInfos.resize(numImagesInFlight);
	
	for (size_t i = 0; i < numImagesInFlight; i++)
	{
		modelInfos[i].UniformBuffer = modelBuffers[i];
		modelInfos[i].UniformBufferMemory = modelBuffersMemory[i];
		modelInfos[i].DescriptorSet = descriptorSets[i];
	}

	return modelInfos;
}
