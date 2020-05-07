
#include "Renderer.h"
#include "GpuTypes.h"
#include "VulkanHelpers.h"
#include "VulkanInitializers.h"
#include "UniformBufferObjects.h"
#include "RenderableMesh.h"
#include "CubemapTextureLoader.h"
#include "IblLoader.h"
#include "VulkanService.h"

#include <Framework/FileService.h>


#define GLM_FORCE_DEPTH_ZERO_TO_ONE // to comply with vulkan
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <vector>
#include <string>
#include <chrono>
#include <map>

using vkh = VulkanHelpers;

Renderer::Renderer(VulkanService* vulkanService, std::string shaderDir, const std::string& assetsDir,
	IRendererDelegate& delegate, IModelLoaderService& modelLoaderService) : _vk(vulkanService), _delegate(delegate), _shaderDir(std::move(shaderDir))
{
	InitRenderer();
	InitRendererResourcesDependentOnSwapchain(_vk->SwapchainImageCount());
	
	_placeholderTexture = CreateTextureResource(assetsDir + "placeholder.png");

	// Load a cube
	auto model = modelLoaderService.LoadModel(assetsDir + "skybox.obj");
	auto& meshDefinition = model.value().Meshes[0];
	_skyboxMesh = CreateMeshResource(meshDefinition);
}

void Renderer::Draw(VkCommandBuffer commandBuffer, u32 frameIndex,
	const RenderOptions& options,
	const std::vector<RenderableResourceId>& renderableIds,
	const std::vector<glm::mat4>& transforms,
	const std::vector<Light>& lights,
	glm::mat4 view, glm::vec3 camPos, glm::ivec2 regionPos, glm::ivec2 regionSize)
{
	assert(renderableIds.size() == transforms.size());
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
	


	// Update UBOs
	{
		// Calc Projection
		const auto vfov = 45.f;
		const auto aspect = regionSize.x / (f32)regionSize.y;
		auto projection = glm::perspective(glm::radians(vfov), aspect, 0.05f, 1000.f);
		projection = glm::scale(projection, glm::vec3{ 1.f,-1.f,1.f });// flip Y to convert glm from OpenGL coord system to Vulkan


		// Light ubo - TODO PERF Keep mem mapped
		{
			auto lightsUbo = LightUbo::Create(lights);

			void* data;
			auto size = sizeof(lightsUbo);
			vkMapMemory(_vk->LogicalDevice(), _lightBuffersMemory[frameIndex], 0, size, 0, &data);
			memcpy(data, &lightsUbo, size);
			vkUnmapMemory(_vk->LogicalDevice(), _lightBuffersMemory[frameIndex]);
		}


		// Update skybox ubos
		const Skybox* skybox = GetCurrentSkyboxOrNull();
		if (skybox)
		{
			// Vert ubo
			{
				// Populate
				auto skyboxVertUbo = SkyboxVertUbo{};
				skyboxVertUbo.Projection = projection; // same as camera
				skyboxVertUbo.Rotation = rotate(glm::radians(options.SkyboxRotation), glm::vec3{ 0,1,0 });
				skyboxVertUbo.View = glm::mat4{ glm::mat3{view} }; // only keep view rotation

				// Copy to gpu - TODO PERF Keep mem mapped 
				void* data;
				auto size = sizeof(skyboxVertUbo);
				vkMapMemory(_vk->LogicalDevice(), skybox->FrameResources[frameIndex].VertUniformBufferMemory, 0, size, 0, &data);
				memcpy(data, &skyboxVertUbo, size);
				vkUnmapMemory(_vk->LogicalDevice(), skybox->FrameResources[frameIndex].VertUniformBufferMemory);
			}

			// Frag ubo
			{
				// Populate
				auto skyboxFragUbo = SkyboxFragUbo{};
				skyboxFragUbo.ExposureBias_ShowClipping_IblStrength_DisplayBrightness[0] = options.ExposureBias;
				skyboxFragUbo.ExposureBias_ShowClipping_IblStrength_DisplayBrightness[1] = options.ShowClipping;
				skyboxFragUbo.ExposureBias_ShowClipping_IblStrength_DisplayBrightness[2] = options.IblStrength;
				skyboxFragUbo.ExposureBias_ShowClipping_IblStrength_DisplayBrightness[3] = options.BackdropBrightness;

				// Copy to gpu - TODO PERF Keep mem mapped 
				void* data;
				auto size = sizeof(skyboxFragUbo);
				vkMapMemory(_vk->LogicalDevice(), skybox->FrameResources[frameIndex].FragUniformBufferMemory, 0, size, 0, &data);
				memcpy(data, &skyboxFragUbo, size);
				vkUnmapMemory(_vk->LogicalDevice(), skybox->FrameResources[frameIndex].FragUniformBufferMemory);
			}
		}


		// Update Pbr Model ubos
		for (size_t i = 0; i < renderableIds.size(); i++)
		{
			// Populate
			const auto& renderable = _renderables[renderableIds[i].Id].get();
			auto& modelBufferMemory = renderable->FrameResources[frameIndex].UniformBufferMemory;
			
			UniversalUboCreateInfo info = {};
			info.Model = transforms[i];
			info.View = view;
			info.Projection = projection;
			info.CamPos = camPos;
			info.ExposureBias = options.ExposureBias;
			info.IblStrength = options. IblStrength;
			info.ShowClipping = options.ShowClipping;
			info.ShowNormalMap = false;
			info.CubemapRotation = options.SkyboxRotation;
			const auto& renderable = *_renderables[renderableIds[i].Id];
			
			const auto& modelBufferMemory = renderable.FrameResources[frameIndex].UniformBufferMemory;
			const auto modelUbo = UniversalUbo::Create(info, renderable.Mat);

			
			// Copy to gpu - TODO PERF Keep mem mapped 
			void* data;
			auto size = sizeof(modelUbo);
			vkMapMemory(_vk->LogicalDevice(), modelBufferMemory, 0, size, 0, &data);
			memcpy(data, &modelUbo, size);
			vkUnmapMemory(_vk->LogicalDevice(), modelBufferMemory);
		}

		
		std::vector<RenderableResourceId> opaqueObjects = {};
		std::map<f32, RenderableResourceId> depthSortedTransparentObjects = {};
		
		// Split renderables into an opaque and ordered transparent collections.
		for (size_t i = 0; i < renderableIds.size(); i++)
		{
			const auto& id = renderableIds[i];
			const auto& mat = _renderables[id.Id]->Mat;
			
			if (mat.UsingTransparencyMap())
			{
				// Depth sort transparent object
				
				// Calc depth of from camera to object transform - this isn't fullproof!
				const auto& tf = transforms[i];
				auto objPos = glm::vec3(tf[3]);
				glm::vec3 diff = objPos-camPos;
				float dist2 = glm::dot(diff,diff);

				auto [it, success] = depthSortedTransparentObjects.try_emplace(dist2, id);
				while (!success)
				{
					// HACK to nudge the dist a little. Doing this to avoid needing a more complicated sorted map
					dist2 += 0.001f * (float(rand())/RAND_MAX);
					std::tie(it, success) = depthSortedTransparentObjects.try_emplace(dist2, id);
					//std::cerr << "Failed to depth sort object\n";
				}
			}
			else // Opaque
			{
				opaqueObjects.emplace_back(id);
			}
		}


		// TODO Once it's working, run a perf test where we only sort a few transparent objects with the majority opaque
		

		// Record Command Buffer

		// Render region - Note: this region is the 3d viewport only. ImGui defines it's own viewport
		auto viewport = vki::Viewport((f32)regionPos.x,(f32)regionPos.y, (f32)regionSize.x,(f32)regionSize.y, 0,1);
		auto scissor = vki::Rect2D({ regionPos.x,regionPos.y }, { (u32)regionSize.x, (u32)regionSize.y });
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		
		// Draw Skybox
		if (skybox)
		{
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipeline);

			const auto& mesh = *_meshes[skybox->MeshId.Id];

			// Draw mesh
			VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout,
				0, 1, &skybox->FrameResources[frameIndex].DescriptorSet, 0, nullptr);
			vkCmdDrawIndexed(commandBuffer, (uint32_t)mesh.IndexCount, 1, 0, 0, 0);
		}


		// Draw Pbr Objects
		{
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pbrPipeline);

			// Draw Opaque objects
			for (const auto& opaqueObj : opaqueObjects)
			{
				const auto& renderable = _renderables[opaqueObj.Id].get();
				const auto& mesh = *_meshes[renderable->MeshId.Id];

				// Draw mesh
				VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindDescriptorSets(commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS, _pbrPipelineLayout, // TODO Use diff pipeline with blending disabled?
					0, 1, &renderable->FrameResources[frameIndex].DescriptorSet, 0, nullptr);

				/*const void* pValues;
				vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 1, pValues);*/

				vkCmdDrawIndexed(commandBuffer, (uint32_t)mesh.IndexCount, 1, 0, 0, 0);
			}

			// Draw transparent objects (reverse iterated)
			for (auto it = depthSortedTransparentObjects.rbegin(); it != depthSortedTransparentObjects.rend(); ++it)
			{
				auto [dist2, renderableId] = *it;
				
				const auto& renderable = _renderables[renderableId.Id].get();
				const auto& mesh = *_meshes[renderable->MeshId.Id];

				// Draw mesh
				VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindDescriptorSets(commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS, _pbrPipelineLayout,
					0, 1, &renderable->FrameResources[frameIndex].DescriptorSet, 0, nullptr);

				/*const void* pValues;
				vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 1, pValues);*/

				vkCmdDrawIndexed(commandBuffer, (uint32_t)mesh.IndexCount, 1, 0, 0, 0);
			}
		}
	}

	
	const std::chrono::duration<double, std::chrono::milliseconds::period> duration
		= std::chrono::steady_clock::now() - startBench;
	//std::cout << "# Update loop took:  " << std::setprecision(3) << duration.count() << "ms.\n";
}

void Renderer::CleanUp()
{
	vkDeviceWaitIdle(_vk->LogicalDevice());
	
	DestroyRenderResourcesDependentOnSwapchain();
	DestroyRenderer();
}

IblTextureResourceIds
Renderer::CreateIblTextureResources(const std::array<std::string, 6>& sidePaths)
{
	IblTextureResources iblRes = IblLoader::LoadIblFromCubemapPath(sidePaths, *_meshes[_skyboxMesh.Id], _shaderDir, 
		_vk->CommandPool(), _vk->GraphicsQueue(), _vk->PhysicalDevice(), _vk->LogicalDevice());

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
		_vk->CommandPool(), _vk->GraphicsQueue(), _vk->PhysicalDevice(), _vk->LogicalDevice());

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
			sidePaths, format, _vk->CommandPool(), _vk->GraphicsQueue(), _vk->PhysicalDevice(), _vk->LogicalDevice())));
	
	return id;
}

TextureResourceId Renderer::CreateTextureResource(const std::string& path)
{
	const TextureResourceId id = (u32)_textures.size();
	auto texRes = TextureResourceHelpers::LoadTexture(path, _vk->CommandPool(), _vk->GraphicsQueue(), _vk->PhysicalDevice(), _vk->LogicalDevice());
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
		= vkh::CreateVertexBuffer(meshDefinition.Vertices, _vk->GraphicsQueue(), _vk->CommandPool(), _vk->PhysicalDevice(), _vk->LogicalDevice());

	std::tie(mesh->IndexBuffer, mesh->IndexBufferMemory)
		= vkh::CreateIndexBuffer(meshDefinition.Indices, _vk->GraphicsQueue(), _vk->CommandPool(), _vk->PhysicalDevice(), _vk->LogicalDevice());


	const MeshResourceId id = (u32)_meshes.size();
	_meshes.emplace_back(std::move(mesh));

	return id;
}

SkyboxResourceId Renderer::CreateSkybox(const SkyboxCreateInfo& createInfo)
{
	auto skybox = std::make_unique<Skybox>();
	skybox->MeshId = _skyboxMesh;
	skybox->IblTextureIds = createInfo.IblTextureIds;
	skybox->FrameResources = CreateSkyboxModelFrameResources(_vk->SwapchainImageCount(), *skybox);

	const SkyboxResourceId id = (u32)_skyboxes.size();
	_skyboxes.emplace_back(std::move(skybox));

	return id;
}

RenderableResourceId Renderer::CreateRenderable(const MeshResourceId& meshId, const Material& material)
{
	auto model = std::make_unique<RenderableMesh>();
	model->MeshId = meshId;
	model->Mat = material;
	model->FrameResources = CreatePbrModelFrameResources(_vk->SwapchainImageCount(), *model);

	const RenderableResourceId id = (u32)_renderables.size();
	_renderables.emplace_back(std::move(model));

	return id;
}

void Renderer::SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat)
{
	auto& renderable = *_renderables[renderableResId.Id];
	auto& oldMat = renderable.Mat;

	// Existing ids
	const auto currentBasecolorMapId = oldMat.BasecolorMap.value_or(_placeholderTexture).Id;
	const auto currentNormalMapId = oldMat.NormalMap.value_or(_placeholderTexture).Id;
	const auto currentRoughnessMapId = oldMat.RoughnessMap.value_or(_placeholderTexture).Id;
	const auto currentMetalnessMapId = oldMat.MetalnessMap.value_or(_placeholderTexture).Id;
	const auto currentAoMapId = oldMat.AoMap.value_or(_placeholderTexture).Id;
	const auto currentEmissiveMapId = oldMat.EmissiveMap.value_or(_placeholderTexture).Id;
	const auto currentTransparencyMapId = oldMat.TransparencyMap.value_or(_placeholderTexture).Id;

	// New ids
	const auto basecolorMapId = newMat.BasecolorMap.value_or(_placeholderTexture).Id;
	const auto normalMapId = newMat.NormalMap.value_or(_placeholderTexture).Id;
	const auto roughnessMapId = newMat.RoughnessMap.value_or(_placeholderTexture).Id;
	const auto metalnessMapId = newMat.MetalnessMap.value_or(_placeholderTexture).Id;
	const auto aoMapId = newMat.AoMap.value_or(_placeholderTexture).Id;
	const auto emissiveMapId = newMat.EmissiveMap.value_or(_placeholderTexture).Id;
	const auto transparencyMapId = newMat.TransparencyMap.value_or(_placeholderTexture).Id;

	// Store new mat
	renderable.Mat = newMat;


	// Bail early if the new descriptor set is identical (eg, if not changing a Map id!)
	const bool descriptorSetsMatch =
		currentBasecolorMapId == basecolorMapId &&
		currentNormalMapId == normalMapId &&
		currentRoughnessMapId == roughnessMapId &&
		currentMetalnessMapId == metalnessMapId &&
		currentAoMapId == aoMapId &&
		currentEmissiveMapId == emissiveMapId &&
		currentTransparencyMapId == transparencyMapId;

	if (!descriptorSetsMatch)
	{
		// NOTE: This is heavy handed as it rebuilds ALL object descriptor sets, not just those using this material
		_refreshRenderableDescriptorSets = true;
	}
}

void Renderer::SetSkybox(const SkyboxResourceId& resourceId)
{
	// Set skybox
	_activeSkybox = resourceId;
	_refreshRenderableDescriptorSets = true; // Renderables depend on skybox resources for IBL
}

void Renderer::InitRenderer()
{
	// PBR pipe
	_pbrDescriptorSetLayout = CreatePbrDescriptorSetLayout(_vk->LogicalDevice());
	_pbrPipelineLayout = vkh::CreatePipelineLayout(_vk->LogicalDevice(), { _pbrDescriptorSetLayout });

	// Skybox pipe
	_skyboxDescriptorSetLayout = CreateSkyboxDescriptorSetLayout(_vk->LogicalDevice());
	_skyboxPipelineLayout = vkh::CreatePipelineLayout(_vk->LogicalDevice(), { _skyboxDescriptorSetLayout });
}
void Renderer::DestroyRenderer()
{
	// Resources
	for (auto& mesh : _meshes)
	{
		//mesh.Vertices.clear();
		//mesh.Indices.clear();
		vkDestroyBuffer(_vk->LogicalDevice(), mesh->IndexBuffer, nullptr);
		vkFreeMemory(_vk->LogicalDevice(), mesh->IndexBufferMemory, nullptr);
		vkDestroyBuffer(_vk->LogicalDevice(), mesh->VertexBuffer, nullptr);
		vkFreeMemory(_vk->LogicalDevice(), mesh->VertexBufferMemory, nullptr);
	}

	_textures.clear(); // RAII will cleanup


	// Renderer
	vkDestroyPipelineLayout(_vk->LogicalDevice(), _pbrPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(_vk->LogicalDevice(), _pbrDescriptorSetLayout, nullptr);

	vkDestroyPipelineLayout(_vk->LogicalDevice(), _skyboxPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(_vk->LogicalDevice(), _skyboxDescriptorSetLayout, nullptr);
}

void Renderer::InitRendererResourcesDependentOnSwapchain(u32 numImagesInFlight)
{
	_pbrPipeline = CreatePbrGraphicsPipeline(_shaderDir, _pbrPipelineLayout, _vk->MsaaSamples(), _vk->RenderPass(), _vk->LogicalDevice());

	_skyboxPipeline = CreateSkyboxGraphicsPipeline(_shaderDir, _skyboxPipelineLayout, _vk->MsaaSamples(), _vk->RenderPass(), _vk->LogicalDevice(),
		_vk->SwapchainExtent());


	_rendererDescriptorPool = CreateDescriptorPool(numImagesInFlight, _vk->LogicalDevice());


	// Create light uniform buffers per swapchain image
	std::tie(_lightBuffers, _lightBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(LightUbo), _vk->LogicalDevice(), _vk->PhysicalDevice());

	// Create model uniform buffers and descriptor sets per swapchain image
	for (auto& renderable : _renderables)
	{
		renderable->FrameResources = CreatePbrModelFrameResources(numImagesInFlight, *renderable);
	}

	// Create frame resources for skybox
	for (auto& skybox : _skyboxes)
	{
		skybox->FrameResources = CreateSkyboxModelFrameResources(numImagesInFlight, *skybox);
	}
}
void Renderer::DestroyRenderResourcesDependentOnSwapchain()
{
	for (auto& skybox : _skyboxes)
	{
		for (auto& info : skybox->FrameResources)
		{
			vkDestroyBuffer(_vk->LogicalDevice(), info.VertUniformBuffer, nullptr);
			vkFreeMemory(_vk->LogicalDevice(), info.VertUniformBufferMemory, nullptr);
			vkDestroyBuffer(_vk->LogicalDevice(), info.FragUniformBuffer, nullptr);
			vkFreeMemory(_vk->LogicalDevice(), info.FragUniformBufferMemory, nullptr);
			//vkFreeDescriptorSets(_vk->LogicalDevice(), _descriptorPool, (uint32_t)mesh.DescriptorSets.size(), mesh.DescriptorSets.data());
		}
	}

	for (auto& renderable : _renderables)
	{
		for (auto& info : renderable->FrameResources)
		{
			vkDestroyBuffer(_vk->LogicalDevice(), info.UniformBuffer, nullptr);
			vkFreeMemory(_vk->LogicalDevice(), info.UniformBufferMemory, nullptr);
			//vkFreeDescriptorSets(_vk->LogicalDevice(), _descriptorPool, (uint32_t)mesh.DescriptorSets.size(), mesh.DescriptorSets.data());
		}
	}

	for (auto& x : _lightBuffers) { vkDestroyBuffer(_vk->LogicalDevice(), x, nullptr); }
	for (auto& x : _lightBuffersMemory) { vkFreeMemory(_vk->LogicalDevice(), x, nullptr); }

	vkDestroyDescriptorPool(_vk->LogicalDevice(), _rendererDescriptorPool, nullptr);

	vkDestroyPipeline(_vk->LogicalDevice(), _pbrPipeline, nullptr);
	vkDestroyPipeline(_vk->LogicalDevice(), _skyboxPipeline, nullptr);
}

void Renderer::HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
{
	DestroyRenderResourcesDependentOnSwapchain();
	InitRendererResourcesDependentOnSwapchain(numSwapchainImages);
}



#pragma region Shared

VkDescriptorPool Renderer::CreateDescriptorPool(u32 numImagesInFlight, VkDevice device)
{
	const u32 maxPbrObjects = 10000; // Max scene objects! This is gross, but it'll do for now.
	const u32 maxSkyboxObjects = 1;

	// Match these to CreatePbrDescriptorSetLayout
	const auto numPbrUniformBuffers = 2;
	const auto numPbrCombinedImageSamplers = 10;

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
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(UniversalUbo), _vk->LogicalDevice(), _vk->PhysicalDevice());


	// Create descriptor sets
	auto descriptorSets = vkh::AllocateDescriptorSets(numImagesInFlight, _pbrDescriptorSetLayout, _rendererDescriptorPool, _vk->LogicalDevice());

	// Get the id of an existing texture, fallback to placeholder if necessary.
	const auto basecolorMapId = renderable.Mat.BasecolorMap.value_or(_placeholderTexture).Id;
	const auto normalMapId = renderable.Mat.NormalMap.value_or(_placeholderTexture).Id;
	const auto roughnessMapId = renderable.Mat.RoughnessMap.value_or(_placeholderTexture).Id;
	const auto metalnessMapId = renderable.Mat.MetalnessMap.value_or(_placeholderTexture).Id;
	const auto aoMapId = renderable.Mat.AoMap.value_or(_placeholderTexture).Id;
	const auto emissiveMapId = renderable.Mat.EmissiveMap.value_or(_placeholderTexture).Id;
	const auto transparencyMapId = renderable.Mat.TransparencyMap.value_or(_placeholderTexture).Id;

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
		*_textures[emissiveMapId],
		*_textures[transparencyMapId],
		GetIrradianceTextureResource(),
		GetPrefilterTextureResource(),
		GetBrdfTextureResource(),
		_vk->LogicalDevice()
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
		// irradiance map
		vki::DescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// prefilter map
		vki::DescriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// brdf map
		vki::DescriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// light ubo
		vki::DescriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
		// basecolor
		vki::DescriptorSetLayoutBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// normalMap
		vki::DescriptorSetLayoutBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// roughnessMap
		vki::DescriptorSetLayoutBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// metalnessMap
		vki::DescriptorSetLayoutBinding(8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// aoMap
		vki::DescriptorSetLayoutBinding(9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// emissiveMap
		vki::DescriptorSetLayoutBinding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// transparencyMap
		vki::DescriptorSetLayoutBinding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
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
	const TextureResource& emissiveMap,
	const TextureResource& transparencyMap,
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

		const auto& set = descriptorSets[i];
		
		std::vector<VkWriteDescriptorSet> descriptorWrites
		{
			vki::WriteDescriptorSet(set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &bufferUboInfo),
			vki::WriteDescriptorSet(set, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &irradianceMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &prefilterMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &brdfMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &lightUboInfo),
			vki::WriteDescriptorSet(set, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &basecolorMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &normalMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &roughnessMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &metalnessMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &aoMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &emissiveMap.DescriptorImageInfo()),
			vki::WriteDescriptorSet(set, 11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &transparencyMap.DescriptorImageInfo()),

		};

		vkUpdateDescriptorSets(device, (u32)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}
}

VkPipeline Renderer::CreatePbrGraphicsPipeline(const std::string& shaderDir,
	VkPipelineLayout pipelineLayout,
	VkSampleCountFlagBits msaaSamples,
	VkRenderPass renderPass,
	VkDevice device)
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
	auto vertBindingDesc = VertexHelper::BindingDescription();
	auto vertAttrDesc = VertexHelper::AttributeDescriptions();
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
		rasterizationCI.cullMode = VK_CULL_MODE_BACK_BIT;
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
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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

		graphicsPipelineCI.basePipelineHandle = nullptr; // is our pipeline derived from another?
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
	// Rebuild all objects descriptor sets
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
		const auto emissiveMapId = renderable->Mat.EmissiveMap.value_or(_placeholderTexture).Id;
		const auto transparencyMapId = renderable->Mat.TransparencyMap.value_or(_placeholderTexture).Id;


		// Write updated descriptor sets
		WritePbrDescriptorSets((u32)count, descriptorSets,
			modelBuffers,
			_lightBuffers,
			*_textures[basecolorMapId],
			*_textures[normalMapId],
			*_textures[roughnessMapId],
			*_textures[metalnessMapId],
			*_textures[aoMapId],
			*_textures[emissiveMapId],
			*_textures[transparencyMapId],
			GetIrradianceTextureResource(),
			GetPrefilterTextureResource(),
			GetBrdfTextureResource(),
			_vk->LogicalDevice());
	}
}

#pragma endregion Pbr


#pragma region Skybox

std::vector<SkyboxResourceFrame>
Renderer::CreateSkyboxModelFrameResources(u32 numImagesInFlight, const Skybox& skybox) const
{
	// Allocate descriptor sets
	std::vector<VkDescriptorSet> descriptorSets
		= vkh::AllocateDescriptorSets(numImagesInFlight, _skyboxDescriptorSetLayout, _rendererDescriptorPool, _vk->LogicalDevice());


	// Vert Uniform buffers
	std::vector<VkBuffer> skyboxVertBuffers;
	std::vector<VkDeviceMemory> skyboxVertBuffersMemory;
	
	std::tie(skyboxVertBuffers, skyboxVertBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(SkyboxVertUbo), _vk->LogicalDevice(), _vk->PhysicalDevice());

	
	// Frag Uniform buffers
	std::vector<VkBuffer> skyboxFragBuffers;
	std::vector<VkDeviceMemory> skyboxFragBuffersMemory;
	
	std::tie(skyboxFragBuffers, skyboxFragBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(SkyboxFragUbo), _vk->LogicalDevice(), _vk->PhysicalDevice());


	const auto textureId = _lastOptions.ShowIrradiance
		? skybox.IblTextureIds.IrradianceCubemapId.Id
		: skybox.IblTextureIds.EnvironmentCubemapId.Id;
	
	WriteSkyboxDescriptorSets(
		numImagesInFlight, descriptorSets, skyboxVertBuffers, skyboxFragBuffers, *_textures[textureId], _vk->LogicalDevice());


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
	auto vertBindingDesc = VertexHelper::BindingDescription();
	auto vertAttrDesc = VertexHelper::AttributeDescriptions();
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
		rasterizationCI.cullMode = VK_CULL_MODE_FRONT_BIT; // SKYBOX: flipped from norm
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

		graphicsPipelineCI.basePipelineHandle = nullptr; // is our pipeline derived from another?
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

		WriteSkyboxDescriptorSets((u32)count, descriptorSets, vertUbos, fragUbos, *_textures[textureId], _vk->LogicalDevice());
	}
}

#pragma endregion Skybox

