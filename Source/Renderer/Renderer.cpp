
#include "Renderer.h"
#include "GpuTypes.h"
#include "VulkanHelpers.h"
#include "VulkanInitializers.h"
#include "UniformBufferObjects.h"
#include "RenderableMesh.h"
#include "CubemapTextureLoader.h"
#include "IblLoader.h"

#include <Shared/FileService.h>

// TODO Remove all notion of imgui in Renderer
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // to comply with vulkan

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <vector>
#include <string>
#include <chrono>


using vkh = VulkanHelpers;


bool flip = true;

Renderer::Renderer(bool enableValidationLayers, std::string shaderDir, const std::string& assetsDir,
	IRendererDelegate& delegate, IModelLoaderService& modelLoaderService) : _delegate(delegate), _shaderDir(std::move(shaderDir))
{
	_enableValidationLayers = enableValidationLayers;
	InitVulkan();
	
	InitImgui();

	_placeholderTexture = CreateTextureResource(assetsDir + "placeholder.png");

	// Load a cube
	auto model = modelLoaderService.LoadModel(assetsDir + "skybox.obj");
	auto& meshDefinition = model.value().Meshes[0];
	_skyboxMesh = CreateMeshResource(meshDefinition);
}

std::optional<u32> Renderer::PreFrame()
{
	// Sync CPU-GPU
	vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], true, UINT64_MAX);

	// Aquire an image from the swap chain
	u32 imageIndex;
	VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame],
		nullptr, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain();
		return std::nullopt;
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

	return imageIndex;
}

void Renderer::PostFrame(u32 imageIndex)
{

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
		submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];
		// cmdbuf that binds the swapchain image we acquired as color attachment
		submitInfo.waitSemaphoreCount = waitCount;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.signalSemaphoreCount = signalCount;
		submitInfo.pSignalSemaphores = signalSemaphores;
	}

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

	VkResult result = vkQueuePresentKHR(_presentQueue, &presentInfo);
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


void Renderer::DrawFrame(float dt, const RenderOptions& options,
                         const std::vector<RenderableResourceId>& renderableIds,
                         const std::vector<glm::mat4>& transforms,
                         const std::vector<Light>& lights,
                         glm::mat4 view, glm::vec3 camPos, glm::ivec2 regionPos, glm::ivec2 regionSize)
{
	const auto imageIndex = PreFrame();
	if (!imageIndex.has_value())
	{
		return;
	}
	
	const auto startBench = std::chrono::steady_clock::now();

	
	// Diff render options and force state updates where needed
	{
		// Process whether refreshing is required
		_refreshSkyboxDescriptorSets |= _lastOptions.ShowIrradiance != options.ShowIrradiance;


		_lastOptions = options;

		// Rebuild descriptor sets as needed
		if (_refreshSkyboxDescriptorSets)
		{
			_refreshSkyboxDescriptorSets = false;
			UpdateSkyboxesDescriptorSets();
		}

		if (_refreshRenderableDescriptorSets)
		{
			_refreshRenderableDescriptorSets = false;
			UpdateRenderableDescriptorSets();
		}
	}
	

	assert(renderableIds.size() == transforms.size());

	DrawEverything(options, renderableIds, transforms, lights, view, camPos, *imageIndex, regionPos, regionSize);


	
	const std::chrono::duration<double, std::chrono::milliseconds::period> duration
		= std::chrono::steady_clock::now() - startBench;
	//std::cout << "# Update loop took:  " << std::setprecision(3) << duration.count() << "ms.\n";

	
	PostFrame(*imageIndex);
}


void Renderer::DrawEverything(const RenderOptions& options, const std::vector<RenderableResourceId>& renderableIds,
	const std::vector<glm::mat4>& transforms, const std::vector<Light>& lights, glm::mat4 view, glm::vec3 camPos,
	u32 imageIndex, glm::ivec2 regionPos, glm::ivec2 regionSize)
{
	// Calc Projection
	const auto vfov = 45.f;
	const float aspect = _swapchainExtent.width / (float)_swapchainExtent.height;
	auto projection = glm::perspective(glm::radians(vfov), aspect, 0.1f, 1000.f);
	if (flip)
	{
		// flip Y to convert glm from OpenGL coord system to Vulkan
		projection = glm::scale(projection, glm::vec3{ 1.f,-1.f,1.f });
	}

	// Update light buffers
	{
		auto lightsUbo = LightUbo::Create(lights);

		void* data;
		auto size = sizeof(lightsUbo);
		vkMapMemory(_device, _lightBuffersMemory[imageIndex], 0, size, 0, &data);
		memcpy(data, &lightsUbo, size);
		vkUnmapMemory(_device, _lightBuffersMemory[imageIndex]);
	}



	// Update skybox buffer
	const Skybox* skybox = GetCurrentSkyboxOrNull();
	if (skybox)
	{
		// Vert ubo
		{
			// Populate ubo
			auto skyboxVertUbo = SkyboxVertUbo{};
			skyboxVertUbo.Projection = projection; // same as camera
			skyboxVertUbo.Rotation = rotate(glm::radians(options.SkyboxRotation), glm::vec3{ 0,1,0 });
			skyboxVertUbo.View = glm::mat4{ glm::mat3{view} }; // only keep view rotation

			// Copy to gpu
			void* data;
			auto size = sizeof(skyboxVertUbo);
			vkMapMemory(_device, skybox->FrameResources[imageIndex].VertUniformBufferMemory, 0, size, 0, &data);
			memcpy(data, &skyboxVertUbo, size);
			vkUnmapMemory(_device, skybox->FrameResources[imageIndex].VertUniformBufferMemory);
		}

		// Frab ubo
		{
			// Populate ubo
			auto skyboxFragUbo = SkyboxFragUbo{};
			skyboxFragUbo.ExposureBias_ShowClipping[0] = options.ExposureBias;
			skyboxFragUbo.ExposureBias_ShowClipping[1] = options.ShowClipping;

			// Copy to gpu
			void* data;
			auto size = sizeof(skyboxFragUbo);
			vkMapMemory(_device, skybox->FrameResources[imageIndex].FragUniformBufferMemory, 0, size, 0, &data);
			memcpy(data, &skyboxFragUbo, size);
			vkUnmapMemory(_device, skybox->FrameResources[imageIndex].FragUniformBufferMemory);
		}
	}


	// Update Model
	for (size_t i = 0; i < renderableIds.size(); i++)
	{
		UniversalUboCreateInfo info = {};
		info.Model = transforms[i];
		info.View = view;
		info.Projection = projection;
		info.CamPos = camPos;
		info.ExposureBias = options.ExposureBias;
		info.ShowClipping = options.ShowClipping;
		info.ShowNormalMap = false;
		info.CubemapRotation = options.SkyboxRotation;

		const auto& renderable = _renderables[renderableIds[i].Id].get();

		auto& modelBufferMemory = renderable->FrameResources[imageIndex].UniformBufferMemory;
		auto modelUbo = UniversalUbo::Create(info, renderable->Mat);

		// Update model ubo
		void* data;
		auto size = sizeof(modelUbo);
		vkMapMemory(_device, modelBufferMemory, 0, size, 0, &data);
		memcpy(data, &modelUbo, size);
		vkUnmapMemory(_device, modelBufferMemory);
	}


	// Record Command Buffer

	auto frameIndex = imageIndex;
	auto renderPass = _renderPass;
	auto swapchainExtent = _swapchainExtent;
	auto commandBuffer = _commandBuffers[imageIndex];
	auto swapchainFramebuffer = _swapchainFramebuffers[imageIndex];
	auto pbrPipeline = _pbrPipeline;
	auto pbrPipelineLayout = _pbrPipelineLayout;
	auto skyboxPipeline = _skyboxPipeline;
	auto skyboxPipelineLayout = _skyboxPipelineLayout;
	auto& meshes = _meshes;

	// Start command buffer
	const auto beginInfo = vki::CommandBufferBeginInfo(0, nullptr);
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) == VK_SUCCESS)
	{
		// Record renderpass
		std::vector<VkClearValue> clearColors(2);
		clearColors[0].color = { 0.f, 0.f, 0.f, 1.f };
		clearColors[1].depthStencil = { 1.f, 0ui32 };

		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(renderPass, swapchainFramebuffer,
			vki::Rect2D(vki::Offset2D(0, 0), swapchainExtent), clearColors);


		// Render region
		auto viewport = vki::Viewport(0, 0, (f32)swapchainExtent.width, (f32)swapchainExtent.height, 0, 1);
		auto scissor = vki::Rect2D({ regionPos.x,regionPos.y }, { (u32)regionSize.x, (u32)regionSize.y });

		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);


		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			// Skybox
			if (skybox)
			{
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);

				const auto& mesh = *meshes[skybox->MeshId.Id];

				// Draw mesh
				VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindDescriptorSets(commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout,
					0, 1, &skybox->FrameResources[frameIndex].DescriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, (uint32_t)mesh.IndexCount, 1, 0, 0, 0);
			}


			// Objects
			{
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipeline);

				for (const auto& renderableId : renderableIds)
				{
					const auto& renderable = _renderables[renderableId.Id].get();
					const auto& mesh = *meshes[renderable->MeshId.Id];

					// Draw mesh
					VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdBindDescriptorSets(commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipelineLayout,
						0, 1, &renderable->FrameResources[frameIndex].DescriptorSet, 0, nullptr);

					/*const void* pValues;
					vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 1, pValues);*/

					vkCmdDrawIndexed(commandBuffer, (uint32_t)mesh.IndexCount, 1, 0, 0, 0);
				}
			}


			// ImGui
			{
				_delegate.BuildGui();
				DrawImgui(commandBuffer);
			}
		}
		vkCmdEndRenderPass(commandBuffer);
	}
	else
	{
		throw std::runtime_error("Failed to begin recording command buffer");
	}

	// End command buffer
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to end recording command buffer");
	}
}


void Renderer::CleanUp()
{
	vkDeviceWaitIdle(_device);

	
	CleanupImgui();


	// Cleanup Renderer & Vulkan
	{
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

		_textures.clear(); // RAII will cleanup

		vkDestroyPipelineLayout(_device, _pbrPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _pbrDescriptorSetLayout, nullptr);

		vkDestroyPipelineLayout(_device, _skyboxPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _skyboxDescriptorSetLayout, nullptr);

		vkDestroyCommandPool(_device, _commandPool, nullptr);
		vkDestroyDevice(_device, nullptr);
		if (_enableValidationLayers) { vkh::DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr); }
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyInstance(_instance, nullptr);
	}
}

IblTextureResourceIds
Renderer::CreateIblTextureResources(const std::array<std::string, 6>& sidePaths)
{
	IblTextureResources iblRes = IblLoader::LoadIblFromCubemapPath(sidePaths, *_meshes[_skyboxMesh.Id], _shaderDir, 
		_commandPool, _graphicsQueue, _physicalDevice, _device);

	IblTextureResourceIds ids = {};

	ids.EnvironmentCubemapId = (u32)_textures.size();
	_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.EnvironmentCubemap)));
		
	ids.IrradianceCubemapId = (u32)_textures.size();
	_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.IrradianceCubemap)));

	ids.PrefilterCubemapId = (u32)_textures.size();
	_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.PrefilterCubemap)));

	ids.BrdfLutId = (u32)_textures.size();
	_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.BrdfLut)));

	return ids;
}

IblTextureResourceIds
Renderer::CreateIblTextureResources(const std::string& path)
{
	IblTextureResources iblRes = IblLoader::LoadIblFromEquirectangularPath(path, *_meshes[_skyboxMesh.Id], _shaderDir,
		_commandPool, _graphicsQueue, _physicalDevice, _device);

	IblTextureResourceIds ids = {};

	ids.EnvironmentCubemapId = (u32)_textures.size();
	_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.EnvironmentCubemap)));

	ids.IrradianceCubemapId = (u32)_textures.size();
	_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.IrradianceCubemap)));

	ids.PrefilterCubemapId = (u32)_textures.size();
	_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.PrefilterCubemap)));

	ids.BrdfLutId = (u32)_textures.size();
	_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.BrdfLut)));

	return ids;
}

TextureResourceId Renderer::CreateCubemapTextureResource(const std::array<std::string, 6>& sidePaths, 
	CubemapFormat format)
{
	const TextureResourceId id = (u32)_textures.size();

	_textures.emplace_back(std::make_unique<TextureResource>(
		CubemapTextureLoader::LoadFromFacePaths(
			sidePaths, format, _commandPool, _graphicsQueue, _physicalDevice, _device)));
	
	return id;
}

TextureResourceId Renderer::CreateTextureResource(const std::string& path)
{
	const TextureResourceId id = (u32)_textures.size();
	auto texRes = TextureResourceHelpers::LoadTexture(path, _commandPool, _graphicsQueue, _physicalDevice, _device);
	_textures.emplace_back(std::make_unique<TextureResource>(std::move(texRes)));
	return id;
}

MeshResourceId Renderer::CreateMeshResource(const MeshDefinition& meshDefinition)
{
	// Load mesh resource
	auto mesh = std::make_unique<MeshResource>();
	
	mesh->IndexCount = meshDefinition.Indices.size();
	mesh->VertexCount = meshDefinition.Vertices.size();
	//mesh->Bounds = meshDefinition.Bounds;

	std::tie(mesh->VertexBuffer, mesh->VertexBufferMemory)
		= vkh::CreateVertexBuffer(meshDefinition.Vertices, _graphicsQueue, _commandPool, _physicalDevice, _device);

	std::tie(mesh->IndexBuffer, mesh->IndexBufferMemory)
		= vkh::CreateIndexBuffer(meshDefinition.Indices, _graphicsQueue, _commandPool, _physicalDevice, _device);


	const MeshResourceId id = (u32)_meshes.size();
	_meshes.emplace_back(std::move(mesh));

	return id;
}

SkyboxResourceId Renderer::CreateSkybox(const SkyboxCreateInfo& createInfo)
{
	auto skybox = std::make_unique<Skybox>();
	skybox->MeshId = _skyboxMesh;
	skybox->IblTextureIds = createInfo.IblTextureIds;
	skybox->FrameResources = CreateSkyboxModelFrameResources((u32)_swapchainImages.size(), *skybox);

	const SkyboxResourceId id = (u32)_skyboxes.size();
	_skyboxes.emplace_back(std::move(skybox));

	return id;
}

RenderableResourceId Renderer::CreateRenderable(const MeshResourceId& meshId, const Material& material)
{
	auto model = std::make_unique<RenderableMesh>();
	model->MeshId = meshId;
	model->Mat = material;
	model->FrameResources = CreatePbrModelFrameResources((u32)_swapchainImages.size(), *model);

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

	if (!descriptorSetsMatch)
	{
		_refreshRenderableDescriptorSets = true;
	}
}

void Renderer::SetSkybox(const SkyboxResourceId& resourceId)
{
	// Set skybox
	_activeSkybox = resourceId;
	_refreshRenderableDescriptorSets = true;	
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


	// PBR pipe
	_pbrDescriptorSetLayout = CreatePbrDescriptorSetLayout(_device);
	_pbrPipelineLayout = vkh::CreatePipelineLayout(_device, { _pbrDescriptorSetLayout });

	// Skybox pipe
	_skyboxDescriptorSetLayout = CreateSkyboxDescriptorSetLayout(_device);
	_skyboxPipelineLayout = vkh::CreatePipelineLayout(_device, { _skyboxDescriptorSetLayout });

	
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

	for (auto& skybox : _skyboxes)
	{
		for (auto& info : skybox->FrameResources)
		{
			vkDestroyBuffer(_device, info.VertUniformBuffer, nullptr);
			vkFreeMemory(_device, info.VertUniformBufferMemory, nullptr);
			vkDestroyBuffer(_device, info.FragUniformBuffer, nullptr);
			vkFreeMemory(_device, info.FragUniformBufferMemory, nullptr);
			//vkFreeDescriptorSets(_device, _descriptorPool, (uint32_t)mesh.DescriptorSets.size(), mesh.DescriptorSets.data());
		}
	}
	
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
	
	vkDestroyPipeline(_device, _pbrPipeline, nullptr);
	vkDestroyPipeline(_device, _skyboxPipeline, nullptr);
	
	vkDestroyRenderPass(_device, _renderPass, nullptr);
	for (auto& x : _swapchainImageViews) { vkDestroyImageView(_device, x, nullptr); }
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

void Renderer::CreateSwapchainAndDependents(int width, int height)
{
	_swapchain = vkh::CreateSwapchain({(uint32_t)width, (uint32_t)height}, _physicalDevice, _surface, _device,
	                                  _swapchainImages, _swapchainImageFormat, _swapchainExtent);

	_swapchainImageViews= vkh::CreateImageViews(_swapchainImages, _swapchainImageFormat, VK_IMAGE_VIEW_TYPE_2D, 
		VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, _device);

	std::tie(_colorImage, _colorImageMemory, _colorImageView)
		= vkh::CreateColorResources(_swapchainImageFormat, _swapchainExtent, _msaaSamples,
		                            _commandPool, _graphicsQueue, _device, _physicalDevice);

	std::tie(_depthImage, _depthImageMemory, _depthImageView)
		= vkh::CreateDepthResources(_swapchainExtent, _msaaSamples, _commandPool, _graphicsQueue, _device,
		                            _physicalDevice);

	_renderPass = vkh::CreateSwapchainRenderPass(_msaaSamples, _swapchainImageFormat, _device, _physicalDevice);

	_pbrPipeline = CreatePbrGraphicsPipeline(_shaderDir, _pbrPipelineLayout, _msaaSamples, _renderPass, _device,
		_swapchainExtent);

	_skyboxPipeline = CreateSkyboxGraphicsPipeline(_shaderDir, _skyboxPipelineLayout, _msaaSamples, _renderPass, _device,
		_swapchainExtent);

	_swapchainFramebuffers
		= vkh::CreateSwapchainFramebuffer(_device, _colorImageView, _depthImageView, _swapchainImageViews, 
			_swapchainExtent, _renderPass);


	const u32 numImagesInFlight = (u32)_swapchainImages.size();

	_descriptorPool = CreateDescriptorPool(numImagesInFlight, _device);


	// Create light uniform buffers per swapchain image
	std::tie(_lightBuffers, _lightBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(LightUbo), _device, _physicalDevice);

	// Create model uniform buffers and descriptor sets per swapchain image
	for (auto& renderable : _renderables)
	{
		renderable->FrameResources = CreatePbrModelFrameResources(numImagesInFlight , *renderable);
	}

	// Create frame resources for skybox
	for (auto& skybox : _skyboxes)
	{
		skybox->FrameResources = CreateSkyboxModelFrameResources(numImagesInFlight, *skybox);
	}

	_commandBuffers = vkh::AllocateCommandBuffers(numImagesInFlight, _commandPool, _device);

	// TODO Break CreateSyncObjects() method so we can recreate the parts that are dependend on num swapchainImages
}

void Renderer::RecreateSwapchain()
{
	auto size = _delegate.WaitTillFramebufferHasSize();

	vkDeviceWaitIdle(_device);
	CleanupSwapchainAndDependents();
	CreateSwapchainAndDependents(size.width, size.height);
}


#pragma region Shared

VkDescriptorPool Renderer::CreateDescriptorPool(u32 numImagesInFlight, VkDevice device)
{
	const u32 maxPbrObjects = 1000; // Max scene objects! This is gross, but it'll do for now.
	const u32 maxSkyboxObjects = 1;

	// Match these to CreatePbrDescriptorSetLayout
	const auto numPbrUniformBuffers = 2;
	const auto numPbrCombinedImageSamplers = 8;

	// Match these to CreateSkyboxDescriptorSetLayout
	const auto numSkyboxUniformBuffers = 2;
	const auto numSkyboxCombinedImageSamplers = 1;
	
	// Define which descriptor types our descriptor sets contain
	const std::vector<VkDescriptorPoolSize> poolSizes
	{
		// PBR Objects
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numPbrUniformBuffers * maxPbrObjects * numImagesInFlight},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numPbrCombinedImageSamplers * maxPbrObjects * numImagesInFlight},

		// Skybox Object
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numSkyboxUniformBuffers * maxSkyboxObjects * numImagesInFlight},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numSkyboxCombinedImageSamplers * maxSkyboxObjects * numImagesInFlight},
	};

	const auto totalDescSets = (maxPbrObjects + maxSkyboxObjects) * numImagesInFlight;

	return vkh::CreateDescriptorPool(poolSizes, totalDescSets, device);
}

#pragma endregion Shared


#pragma region Pbr

std::vector<PbrModelResourceFrame> Renderer::CreatePbrModelFrameResources(u32 numImagesInFlight,
	const RenderableMesh& renderable) const
{
	// Create uniform buffers
	std::vector<VkBuffer> modelBuffers;
	std::vector<VkDeviceMemory> modelBuffersMemory;
	std::tie(modelBuffers, modelBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(UniversalUbo), _device, _physicalDevice);


	// Create descriptor sets
	auto descriptorSets = vkh::AllocateDescriptorSets(numImagesInFlight, _pbrDescriptorSetLayout, _descriptorPool, _device);

	// Get the id of an existing texture, fallback to placeholder if necessary.
	const auto basecolorMapId = renderable.Mat.BasecolorMap.value_or(_placeholderTexture).Id;
	const auto normalMapId = renderable.Mat.NormalMap.value_or(_placeholderTexture).Id;
	const auto roughnessMapId = renderable.Mat.RoughnessMap.value_or(_placeholderTexture).Id;
	const auto metalnessMapId = renderable.Mat.MetalnessMap.value_or(_placeholderTexture).Id;
	const auto aoMapId = renderable.Mat.AoMap.value_or(_placeholderTexture).Id;

	WritePbrDescriptorSets(
		numImagesInFlight,
		descriptorSets,
		modelBuffers,
		_lightBuffers,
		*_textures[basecolorMapId],
		*_textures[normalMapId],
		*_textures[roughnessMapId],
		*_textures[metalnessMapId],
		*_textures[aoMapId],
		GetIrradianceTextureResource(),
		GetPrefilterTextureResource(),
		GetBrdfTextureResource(),
		_device
	);


	// Group data for return
	std::vector<PbrModelResourceFrame> modelInfos;
	modelInfos.resize(numImagesInFlight);

	for (size_t i = 0; i < numImagesInFlight; i++)
	{
		modelInfos[i].UniformBuffer = modelBuffers[i];
		modelInfos[i].UniformBufferMemory = modelBuffersMemory[i];
		modelInfos[i].DescriptorSet = descriptorSets[i];
	}

	return modelInfos;
}

VkDescriptorSetLayout Renderer::CreatePbrDescriptorSetLayout(VkDevice device)
{
	return vkh::CreateDescriptorSetLayout(device, {
		// pbr ubo
		vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
		// basecolor
		vki::DescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// normalMap
		vki::DescriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// roughnessMap
		vki::DescriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// metalnessMap
		vki::DescriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// aoMap
		vki::DescriptorSetLayoutBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// light ubo
		vki::DescriptorSetLayoutBinding(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
		// irradiance map
		vki::DescriptorSetLayoutBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// prefilter map
		vki::DescriptorSetLayoutBinding(8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// brdf map
		vki::DescriptorSetLayoutBinding(9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
	});
}

void Renderer::WritePbrDescriptorSets(
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
	VkDevice device)
{
	assert(count == modelUbos.size());// 1 per image in swapchain
	assert(count == lightUbos.size());

	// Configure our new descriptor sets to point to our buffer/image data
	for (size_t i = 0; i < count; ++i)
	{
		VkDescriptorBufferInfo bufferUboInfo = {};
		{
			bufferUboInfo.buffer = modelUbos[i];
			bufferUboInfo.offset = 0;
			bufferUboInfo.range = sizeof(UniversalUbo);
		}
		
		VkDescriptorBufferInfo lightUboInfo = {};
		{
			lightUboInfo.buffer = lightUbos[i];
			lightUboInfo.offset = 0;
			lightUboInfo.range = sizeof(LightUbo);
		}

		auto& set = descriptorSets[i];
		
		std::vector<VkWriteDescriptorSet> descriptorWrites
		{
			vki::WriteDescriptorSet(set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &bufferUboInfo),
			vki::WriteDescriptorSet(set, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &basecolorMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &normalMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &roughnessMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &metalnessMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &aoMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &lightUboInfo),
			vki::WriteDescriptorSet(set, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &irradianceMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &prefilterMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &brdfMap.DescriptorImageInfo()),
		};

		vkUpdateDescriptorSets(device, (u32)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}
}

VkPipeline Renderer::CreatePbrGraphicsPipeline(const std::string& shaderDir,
	VkPipelineLayout pipelineLayout,
	VkSampleCountFlagBits msaaSamples,
	VkRenderPass renderPass,
	VkDevice device,
	const VkExtent2D& swapchainExtent)
{
	//// SHADER MODULES ////


	// Load shader stages
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	const auto numShaders = 2;
	std::array<VkPipelineShaderStageCreateInfo, numShaders> shaderStageCIs{};
	{
		const auto vertShaderCode = FileService::ReadFile(shaderDir + "PbrModel.vert.spv");
		const auto fragShaderCode = FileService::ReadFile(shaderDir + "PbrModel.frag.spv");

		vertShaderModule = vkh::CreateShaderModule(vertShaderCode, device);
		fragShaderModule = vkh::CreateShaderModule(fragShaderCode, device);

		VkPipelineShaderStageCreateInfo vertCI = {};
		vertCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertCI.module = vertShaderModule;
		vertCI.pName = "main";

		VkPipelineShaderStageCreateInfo fragCI = {};
		fragCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragCI.module = fragShaderModule;
		fragCI.pName = "main";

		shaderStageCIs[0] = vertCI;
		shaderStageCIs[1] = fragCI;
	}


	//// FIXED FUNCTIONS ////

	// all of the structures that define the fixed - function stages of the pipeline, like input assembly,
	// rasterizer, viewport and color blending


	// Vertex Input  -  Define the format of the vertex data passed to the vert shader
	auto vertBindingDesc = Vertex::BindingDescription();
	auto vertAttrDesc = Vertex::AttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputCI = {};
	{
		vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCI.vertexBindingDescriptionCount = 1;
		vertexInputCI.pVertexBindingDescriptions = &vertBindingDesc;
		vertexInputCI.vertexAttributeDescriptionCount = (uint32_t)vertAttrDesc.size();
		vertexInputCI.pVertexAttributeDescriptions = vertAttrDesc.data();
	}


	// Input Assembly  -  What kind of geo will be drawn from the verts and whether primitive restart is enabled
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI = {};
	{
		inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
	}


	// Viewports and scissor  -  The region of the framebuffer we render output to
	//VkViewport viewport = {}; // the output is stretch-fitted into these viewport bounds
	//{
	//	viewport.x = 0;
	//	viewport.y = 0;
	//	viewport.width = 100;// (f32)swapchainExtent.width;
	//	viewport.height = 100;// (f32)swapchainExtent.height;
	//	
	///*	viewport.x = 0;
	//	viewport.y = (float)swapchainExtent.height;
	//	viewport.width = (float)swapchainExtent.width;
	//	viewport.height = -(float)swapchainExtent.height;*/
	//	
	//	viewport.minDepth = 0; // depth buffer value range within [0,1]. Min can be > Max.
	//	viewport.maxDepth = 1;
	//}
	//VkRect2D scissor = {}; // scissor filters out pixels beyond these bounds
	//{
	//	scissor.offset = { 0, 0 };
	//	scissor.extent = { 100,100 }; //{ swapchainExtent.width, swapchainExtent.height };
	//}
	VkPipelineViewportStateCreateInfo viewportCI = {};
	{
		viewportCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportCI.viewportCount = 1;
		//viewportCI.pViewports = &viewport;
		viewportCI.scissorCount = 1;
	//	viewportCI.pScissors = &scissor;
	}


	// Rasterizer  -  Config how geometry turns into fragments
	VkPipelineRasterizationStateCreateInfo rasterizationCI = {};
	{
		rasterizationCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationCI.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationCI.cullMode = flip ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT;
		rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationCI.lineWidth = 1; // > 1 requires wideLines GPU feature
		rasterizationCI.depthBiasEnable = VK_FALSE;
		rasterizationCI.depthBiasConstantFactor = 0.0f; // optional
		rasterizationCI.depthBiasClamp = 0.0f; // optional
		rasterizationCI.depthBiasSlopeFactor = 0.0f; // optional
		rasterizationCI.depthClampEnable = VK_FALSE; // clamp depth frags beyond the near/far clip planes?
		rasterizationCI.rasterizerDiscardEnable = VK_FALSE; // stop geo from passing through the raster stage?
	}


	// Multisampling
	VkPipelineMultisampleStateCreateInfo multisampleCI = {};
	{
		multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleCI.rasterizationSamples = msaaSamples;
		multisampleCI.sampleShadingEnable = VK_FALSE;
		multisampleCI.minSampleShading = 1; // optional
		multisampleCI.pSampleMask = nullptr; // optional
		multisampleCI.alphaToCoverageEnable = VK_FALSE; // optional
		multisampleCI.alphaToOneEnable = VK_FALSE; // optional
	}


	// Depth and Stencil testing
	VkPipelineDepthStencilStateCreateInfo depthStencilCI = {};
	{
		depthStencilCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilCI.depthTestEnable = true; // should compare new frags against depth to determine if discarding?
		depthStencilCI.depthWriteEnable = true; // can new depth tests wrhite to buffer?
		depthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS;

		depthStencilCI.depthBoundsTestEnable = false; // optional test to keep only frags within a set bounds
		depthStencilCI.minDepthBounds = 0; // optional
		depthStencilCI.maxDepthBounds = 0; // optional

		depthStencilCI.stencilTestEnable = false;
		depthStencilCI.front = {}; // optional
		depthStencilCI.back = {}; // optional
	}


	// Color Blending  -  How colors output from frag shader are combined with existing colors
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {}; // Mix old with new to create a final color
	{
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	}
	VkPipelineColorBlendStateCreateInfo colorBlendCI = {}; // Combine old and new with a bitwise operation
	{
		colorBlendCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendCI.logicOpEnable = VK_FALSE;
		colorBlendCI.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlendCI.attachmentCount = 1;
		colorBlendCI.pAttachments = &colorBlendAttachment;
		colorBlendCI.blendConstants[0] = 0.0f; // Optional
		colorBlendCI.blendConstants[1] = 0.0f; // Optional
		colorBlendCI.blendConstants[2] = 0.0f; // Optional
		colorBlendCI.blendConstants[3] = 0.0f; // Optional
	}


	// Dynamic State  -  Set which states can be changed without recreating the pipeline. Must be set at draw time
	std::array<VkDynamicState,2> dynamicStates =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		//VK_DYNAMIC_STATE_LINE_WIDTH,
	};
	VkPipelineDynamicStateCreateInfo dynamicStateCI = {};
	{
		dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCI.dynamicStateCount = (u32)dynamicStates.size();
		dynamicStateCI.pDynamicStates = dynamicStates.data();
	}

	
	// Create the Pipeline  -  Finally!...
	VkGraphicsPipelineCreateInfo graphicsPipelineCI = {};
	{
		graphicsPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

		// Programmable
		graphicsPipelineCI.stageCount = (uint32_t)shaderStageCIs.size();
		graphicsPipelineCI.pStages = shaderStageCIs.data();

		// Fixed function
		graphicsPipelineCI.pVertexInputState = &vertexInputCI;
		graphicsPipelineCI.pInputAssemblyState = &inputAssemblyCI;
		graphicsPipelineCI.pViewportState = &viewportCI;
		graphicsPipelineCI.pRasterizationState = &rasterizationCI;
		graphicsPipelineCI.pMultisampleState = &multisampleCI;
		graphicsPipelineCI.pDepthStencilState = &depthStencilCI;
		graphicsPipelineCI.pColorBlendState = &colorBlendCI;
		graphicsPipelineCI.pDynamicState = &dynamicStateCI;

		graphicsPipelineCI.layout = pipelineLayout;

		graphicsPipelineCI.renderPass = renderPass;
		graphicsPipelineCI.subpass = 0;

		graphicsPipelineCI.basePipelineHandle = VK_NULL_HANDLE; // is our pipeline derived from another?
		graphicsPipelineCI.basePipelineIndex = -1;
	}
	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCI, nullptr, &pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Pipeline");
	}


	// Cleanup
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
	vkDestroyShaderModule(device, fragShaderModule, nullptr);

	return pipeline;
}

void Renderer::UpdateRenderableDescriptorSets()
{
	// Rebuild all objects dependent on skybox
	for (auto& renderable : _renderables)
	{
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

		const auto basecolorMapId = renderable->Mat.BasecolorMap.value_or(_placeholderTexture).Id;
		const auto normalMapId = renderable->Mat.NormalMap.value_or(_placeholderTexture).Id;
		const auto roughnessMapId = renderable->Mat.RoughnessMap.value_or(_placeholderTexture).Id;
		const auto metalnessMapId = renderable->Mat.MetalnessMap.value_or(_placeholderTexture).Id;
		const auto aoMapId = renderable->Mat.AoMap.value_or(_placeholderTexture).Id;


		// Write updated descriptor sets
		WritePbrDescriptorSets((u32)count, descriptorSets,
			modelBuffers,
			_lightBuffers,
			*_textures[basecolorMapId],
			*_textures[normalMapId],
			*_textures[roughnessMapId],
			*_textures[metalnessMapId],
			*_textures[aoMapId],
			GetIrradianceTextureResource(),
			GetPrefilterTextureResource(),
			GetBrdfTextureResource(),
			_device);
	}
}

#pragma endregion Pbr


#pragma region Skybox

std::vector<SkyboxResourceFrame>
Renderer::CreateSkyboxModelFrameResources(u32 numImagesInFlight, const Skybox& skybox) const
{
	// Allocate descriptor sets
	std::vector<VkDescriptorSet> descriptorSets
		= vkh::AllocateDescriptorSets(numImagesInFlight, _skyboxDescriptorSetLayout, _descriptorPool, _device);


	// Vert Uniform buffers
	std::vector<VkBuffer> skyboxVertBuffers;
	std::vector<VkDeviceMemory> skyboxVertBuffersMemory;
	
	std::tie(skyboxVertBuffers, skyboxVertBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(SkyboxVertUbo), _device, _physicalDevice);

	
	// Frag Uniform buffers
	std::vector<VkBuffer> skyboxFragBuffers;
	std::vector<VkDeviceMemory> skyboxFragBuffersMemory;
	
	std::tie(skyboxFragBuffers, skyboxFragBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(SkyboxFragUbo), _device, _physicalDevice);


	const auto textureId = _lastOptions.ShowIrradiance
		? skybox.IblTextureIds.IrradianceCubemapId.Id
		: skybox.IblTextureIds.EnvironmentCubemapId.Id;
	
	WriteSkyboxDescriptorSets(
		numImagesInFlight, descriptorSets, skyboxVertBuffers, skyboxFragBuffers, *_textures[textureId], _device);


	// Group data for return
	std::vector<SkyboxResourceFrame> modelInfos;
	modelInfos.resize(numImagesInFlight);
	
	for (size_t i = 0; i < numImagesInFlight; i++)
	{
		modelInfos[i].VertUniformBuffer = skyboxVertBuffers[i];
		modelInfos[i].VertUniformBufferMemory = skyboxVertBuffersMemory[i];
		modelInfos[i].FragUniformBuffer = skyboxFragBuffers[i];
		modelInfos[i].FragUniformBufferMemory = skyboxFragBuffersMemory[i];
		modelInfos[i].DescriptorSet = descriptorSets[i];
	}

	return modelInfos;
}

VkDescriptorSetLayout Renderer::CreateSkyboxDescriptorSetLayout(VkDevice device)
{
	return vkh::CreateDescriptorSetLayout(device, {
		// vert ubo
		vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
		// frag ubo
		vki::DescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// skybox map
		vki::DescriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
	});
}

void Renderer::WriteSkyboxDescriptorSets(
	u32 count,
	const std::vector<VkDescriptorSet>& descriptorSets,
	const std::vector<VkBuffer>& skyboxVertUbo,
	const std::vector<VkBuffer>& skyboxFragUbo,
	const TextureResource& skyboxMap,
	VkDevice device)
{
	assert(count == skyboxVertUbo.size());// 1 per image in swapchain


	// Configure our new descriptor sets to point to our buffer and configured for what's in the buffer
	for (size_t i = 0; i < count; ++i)
	{
		VkDescriptorBufferInfo vertBufferUboInfo = {};
		vertBufferUboInfo.buffer = skyboxVertUbo[i];
		vertBufferUboInfo.offset = 0;
		vertBufferUboInfo.range = sizeof(SkyboxVertUbo);

		VkDescriptorBufferInfo fragBufferUboInfo = {};
		fragBufferUboInfo.buffer = skyboxFragUbo[i];
		fragBufferUboInfo.offset = 0;
		fragBufferUboInfo.range = sizeof(SkyboxFragUbo);
		
		auto& set = descriptorSets[i];
		std::array<VkWriteDescriptorSet, 3> descriptorWrites
		{
			vki::WriteDescriptorSet(set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &vertBufferUboInfo),
			vki::WriteDescriptorSet(set, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &fragBufferUboInfo),
			vki::WriteDescriptorSet(set, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &skyboxMap.DescriptorImageInfo()),
		};

		vkUpdateDescriptorSets(device, (u32)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}
}

VkPipeline Renderer::CreateSkyboxGraphicsPipeline(const std::string& shaderDir,
	VkPipelineLayout pipelineLayout,
	VkSampleCountFlagBits msaaSamples,
	VkRenderPass renderPass,
	VkDevice device,
	const VkExtent2D& swapchainExtent)
{
	//// SHADER MODULES ////


	// Load shader stages
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	const auto numShaders = 2;
	std::array<VkPipelineShaderStageCreateInfo, numShaders> shaderStageCIs{};
	{
		const auto vertShaderCode = FileService::ReadFile(shaderDir + "Skybox.vert.spv");
		const auto fragShaderCode = FileService::ReadFile(shaderDir + "Skybox.frag.spv");

		vertShaderModule = vkh::CreateShaderModule(vertShaderCode, device);
		fragShaderModule = vkh::CreateShaderModule(fragShaderCode, device);

		VkPipelineShaderStageCreateInfo vertCI = {};
		vertCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertCI.module = vertShaderModule;
		vertCI.pName = "main";

		VkPipelineShaderStageCreateInfo fragCI = {};
		fragCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragCI.module = fragShaderModule;
		fragCI.pName = "main";

		shaderStageCIs[0] = vertCI;
		shaderStageCIs[1] = fragCI;
	}


	//// FIXED FUNCTIONS ////

	// all of the structures that define the fixed - function stages of the pipeline, like input assembly,
	// rasterizer, viewport and color blending


	// Vertex Input  -  Define the format of the vertex data passed to the vert shader
	auto vertBindingDesc = Vertex::BindingDescription();
	auto vertAttrDesc = Vertex::AttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputCI = {};
	{
		vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCI.vertexBindingDescriptionCount = 1;
		vertexInputCI.pVertexBindingDescriptions = &vertBindingDesc;
		vertexInputCI.vertexAttributeDescriptionCount = (u32)vertAttrDesc.size();
		vertexInputCI.pVertexAttributeDescriptions = vertAttrDesc.data();
	}


	// Input Assembly  -  What kind of geo will be drawn from the verts and whether primitive restart is enabled
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI = {};
	{
		inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
	}


	// Viewports and scissor  -  The region of the frambuffer we render output to
	//VkViewport viewport = {}; // the output is stretch-fitted into these viewport bounds
	//{
	//	viewport.x = 0;
	//	viewport.y = 0;
	//	viewport.width = 100;// (float)swapchainExtent.width;
	//	viewport.height = 100;// (float)swapchainExtent.height;
	//	
	//	/*viewport.x = 0;
	//	viewport.y = (float)swapchainExtent.height;
	//	viewport.width = (float)swapchainExtent.width;
	//	viewport.height = -(float)swapchainExtent.height;*/
	//	
	//	viewport.minDepth = 0; // depth buffer value range within [0,1]. Min can be > Max.
	//	viewport.maxDepth = 1;
	//}
	//VkRect2D scissor = {}; // scissor filters out pixels beyond these bounds
	//{
	//	scissor.offset = { 0, 0 };
	//	scissor.extent = { 100,100 };// swapchainExtent;
	//}
	VkPipelineViewportStateCreateInfo viewportCI = {};
	{
		viewportCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportCI.viewportCount = 1;
		//viewportCI.pViewports = &viewport;
		viewportCI.scissorCount = 1;
	//	viewportCI.pScissors = &scissor;
	}


	// Rasterizer  -  Config how geometry turns into fragments
	VkPipelineRasterizationStateCreateInfo rasterizationCI = {};
	{
		rasterizationCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationCI.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationCI.cullMode = flip ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT; // SKYBOX: flipped from norm
		rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationCI.lineWidth = 1; // > 1 requires wideLines GPU feature
		rasterizationCI.depthBiasEnable = VK_FALSE;
		rasterizationCI.depthBiasConstantFactor = 0.0f; // optional
		rasterizationCI.depthBiasClamp = 0.0f; // optional
		rasterizationCI.depthBiasSlopeFactor = 0.0f; // optional
		rasterizationCI.depthClampEnable = VK_FALSE; // clamp depth frags beyond the near/far clip planes?
		rasterizationCI.rasterizerDiscardEnable = VK_FALSE; // stop geo from passing through the raster stage?
	}


	// Multisampling
	VkPipelineMultisampleStateCreateInfo multisampleCI = {};
	{
		multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleCI.rasterizationSamples = msaaSamples;
		multisampleCI.sampleShadingEnable = VK_FALSE;
		multisampleCI.minSampleShading = 1; // optional
		multisampleCI.pSampleMask = nullptr; // optional
		multisampleCI.alphaToCoverageEnable = VK_FALSE; // optional
		multisampleCI.alphaToOneEnable = VK_FALSE; // optional
	}


	// Depth and Stencil testing
	VkPipelineDepthStencilStateCreateInfo depthStencilCI = {};
	{
		depthStencilCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilCI.depthTestEnable = false; // SKYBOX: normally true
		depthStencilCI.depthWriteEnable = false; // SKYBOX: normally true
		depthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // SKYBOX: normally VK_COMPARE_OP_LESS

		depthStencilCI.depthBoundsTestEnable = false; // optional test to keep only frags within a set bounds
		depthStencilCI.minDepthBounds = 0; // optional
		depthStencilCI.maxDepthBounds = 0; // optional

		depthStencilCI.stencilTestEnable = false;
		depthStencilCI.front = {}; // optional
		depthStencilCI.back = {}; // optional
	}


	// Color Blending  -  How colors output from frag shader are combined with existing colors
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {}; // Mix old with new to create a final color
	{
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	}
	VkPipelineColorBlendStateCreateInfo colorBlendCI = {}; // Combine old and new with a bitwise operation
	{
		colorBlendCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendCI.logicOpEnable = VK_FALSE;
		colorBlendCI.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlendCI.attachmentCount = 1;
		colorBlendCI.pAttachments = &colorBlendAttachment;
		colorBlendCI.blendConstants[0] = 0.0f; // Optional
		colorBlendCI.blendConstants[1] = 0.0f; // Optional
		colorBlendCI.blendConstants[2] = 0.0f; // Optional
		colorBlendCI.blendConstants[3] = 0.0f; // Optional
	}


	// Dynamic State  -  Set which states can be changed without recreating the pipeline. Must be set at draw time
	std::array<VkDynamicState, 2> dynamicStates =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		//VK_DYNAMIC_STATE_LINE_WIDTH,
	};
	VkPipelineDynamicStateCreateInfo dynamicStateCI = {};
	{
		dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCI.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicStateCI.pDynamicStates = dynamicStates.data();
	}

	
	// Create the Pipeline  -  Finally!...
	VkGraphicsPipelineCreateInfo graphicsPipelineCI = {};
	{
		graphicsPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

		// Programmable
		graphicsPipelineCI.stageCount = (uint32_t)shaderStageCIs.size();
		graphicsPipelineCI.pStages = shaderStageCIs.data();

		// Fixed function
		graphicsPipelineCI.pVertexInputState = &vertexInputCI;
		graphicsPipelineCI.pInputAssemblyState = &inputAssemblyCI;
		graphicsPipelineCI.pViewportState = &viewportCI;
		graphicsPipelineCI.pRasterizationState = &rasterizationCI;
		graphicsPipelineCI.pMultisampleState = &multisampleCI;
		graphicsPipelineCI.pDepthStencilState = &depthStencilCI;
		graphicsPipelineCI.pColorBlendState = &colorBlendCI;
		graphicsPipelineCI.pDynamicState = &dynamicStateCI;

		graphicsPipelineCI.layout = pipelineLayout;

		graphicsPipelineCI.renderPass = renderPass;
		graphicsPipelineCI.subpass = 0;

		graphicsPipelineCI.basePipelineHandle = VK_NULL_HANDLE; // is our pipeline derived from another?
		graphicsPipelineCI.basePipelineIndex = -1;
	}
	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCI, nullptr, &pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Pipeline");
	}


	// Cleanup
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
	vkDestroyShaderModule(device, fragShaderModule, nullptr);

	return pipeline;
}

void Renderer::UpdateSkyboxesDescriptorSets()
{
	for (auto& skybox : _skyboxes)
	{
		const auto count = skybox->FrameResources.size();

		std::vector<VkDescriptorSet> descriptorSets = {};
		std::vector<VkBuffer> vertUbos = {};
		std::vector<VkBuffer> fragUbos = {};
		
		descriptorSets.resize(count);
		vertUbos.resize(count);
		fragUbos.resize(count);
		
		for (size_t i = 0; i < count; i++)
		{
			descriptorSets[i] = skybox->FrameResources[i].DescriptorSet;
			vertUbos[i] = skybox->FrameResources[i].VertUniformBuffer;
			fragUbos[i] = skybox->FrameResources[i].FragUniformBuffer;
		}

		const auto textureId = _lastOptions.ShowIrradiance
			? skybox->IblTextureIds.IrradianceCubemapId.Id
			: skybox->IblTextureIds.EnvironmentCubemapId.Id;

		WriteSkyboxDescriptorSets((u32)count, descriptorSets, vertUbos, fragUbos, *_textures[textureId], _device);
	}
}

#pragma endregion Skybox


#pragma region ImGui

void Renderer::InitImgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	
	// UI Style
	const float rounding = 3;

	ImGui::StyleColorsLight();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0;
	style.WindowBorderSize = 0;
	style.WindowRounding = 0;
	style.FrameRounding = rounding;
	style.ChildRounding = rounding;

	
	// This is required as this layer doesn't have access to GLFW. // TODO Refactor so Renderer has no logical coupling to GLFW at all
	_delegate.InitImguiWithGlfwVulkan();


	const auto imageCount = (u32)_swapchainImages.size();
	
	// Create descriptor pool - from main_vulkan.cpp imgui example code
	{
		_imguiDescriptorPool = vkh::CreateDescriptorPool({
			 { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			 { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			 { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			 { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			 { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			 { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			 { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			 { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			 { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			 { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			 { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			}, imageCount, _device); 
	}


	// Get the min image count
	u32 minImageCount;
	{
		// Copied from VulkanHelpers::CreateSwapchain - TODO either store minImageCount or make it a separate func
		const SwapChainSupportDetails deets = vkh::QuerySwapChainSupport(_physicalDevice, _surface);
		
		minImageCount = deets.Capabilities.minImageCount + 1; // 1 extra image to avoid waiting on driver
		const auto maxImageCount = deets.Capabilities.maxImageCount;
		const auto maxImageCountExists = maxImageCount != 0;
		if (maxImageCountExists && minImageCount > maxImageCount)
		{
			minImageCount = maxImageCount;
		}
	}

	
	// Init device info
	ImGui_ImplVulkan_InitInfo initInfo = {};
	{
		initInfo.Instance = _instance;
		initInfo.PhysicalDevice = _physicalDevice;
		initInfo.Device = _device;
		initInfo.QueueFamily = vkh::FindQueueFamilies(_physicalDevice, _surface).GraphicsFamily.value(); // vomit
		initInfo.Queue = _graphicsQueue;
		initInfo.PipelineCache = nullptr;
		initInfo.DescriptorPool = _imguiDescriptorPool;
		initInfo.MinImageCount = minImageCount;
		initInfo.ImageCount = imageCount;
		initInfo.MSAASamples = _msaaSamples;
		initInfo.Allocator = nullptr;
		initInfo.CheckVkResultFn = [](VkResult err)
		{
			if (err == VK_SUCCESS) return;
			printf("VkResult %d\n", err);
			if (err < 0)
				abort();
		};
	}

	ImGui_ImplVulkan_Init(&initInfo, _renderPass);


	// Upload Fonts
	{
		const auto cmdBuf = vkh::BeginSingleTimeCommands(_commandPool, _device);
		ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);
		vkh::EndSingeTimeCommands(cmdBuf, _commandPool, _graphicsQueue, _device);
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}
}
void Renderer::DrawImgui(VkCommandBuffer commandBuffer)
{
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}
void Renderer::CleanupImgui()
{
	vkDestroyDescriptorPool(_device, _imguiDescriptorPool, nullptr);
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

#pragma endregion