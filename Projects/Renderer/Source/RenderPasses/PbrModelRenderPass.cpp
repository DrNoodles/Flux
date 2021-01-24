
#include "RenderPasses/PbrModelRenderPass.h"
#include "GpuTypes.h"
#include "VulkanHelpers.h"
#include "VulkanInitializers.h"
#include "UniformBufferObjects.h"
#include "RenderableMesh.h"
#include "IblLoader.h"
#include "VulkanService.h"

#include <Framework/FileService.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE // to comply with vulkan
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <utility>
#include <vector>
#include <string>
#include <chrono>
#include <map>


using vkh = VulkanHelpers;

PbrMaterialResource MaterialResourceManager::CreateMaterialFrameResources(const Material& material) const
{
	const auto numImagesInFlight = 1;
		
	// Create uniform buffers
	auto [materialBuffers, materialBuffersMemory] = vkh::CreateUniformBuffers(numImagesInFlight, sizeof(PbrMaterialUbo),
		_vk->LogicalDevice(), _vk->PhysicalDevice());


	// Create descriptor sets
	const auto materialDescSets = vkh::AllocateDescriptorSets(numImagesInFlight, _descSetLayout, _pool, _vk->LogicalDevice());


	// Get the id of an existing texture, fallback to placeholder if necessary.
	auto GetTexture = [&](const std::optional<Material::Map>& map) -> const TextureResource&
	{
		const auto id =  map.has_value() ? map->Id : _placeholderTexture;
		return _resourceRegistry->GetTexture(id);
	};
	for (u32 i = 0; i < numImagesInFlight; i++)
	{
		// Write updated descriptor sets
		WriteMaterialDescriptorSet(
			materialDescSets[i],
			materialBuffers[i],
			GetTexture(material.BasecolorMap),
			GetTexture(material.NormalMap),
			GetTexture(material.RoughnessMap),
			GetTexture(material.MetalnessMap),
			GetTexture(material.AoMap),
			GetTexture(material.EmissiveMap),
			GetTexture(material.TransparencyMap),
			_vk->LogicalDevice());
	}

	return PbrMaterialResource{_vk, materialDescSets[0], materialBuffers[0], materialBuffersMemory[0] };
}

PbrModelRenderPass::PbrModelRenderPass(VulkanService& vulkanService, ResourceRegistry* registry, IPbrModelRenderPassDelegate& delegate, std::string shaderDir, const std::string& assetsDir)
	: _vk(vulkanService), _delegate(delegate), _shaderDir(std::move(shaderDir)), _resourceRegistry(registry)
{
	_placeholderTexture = _resourceRegistry->CreateTextureResource(assetsDir + "placeholder.png"); // TODO Move this to some common resources code

	InitRenderer();
	InitRendererResourcesDependentOnSwapchain(_vk.GetSwapchain().GetImageCount());
}

void PbrModelRenderPass::Destroy() // TODO Make this RAII
{
	vkDeviceWaitIdle(_vk.LogicalDevice());
	
	DestroyRenderResourcesDependentOnSwapchain();
	DestroyRenderer();
}


void PbrModelRenderPass::InitRenderer()
{
	_renderPass = CreateRenderPass(VK_FORMAT_R16G16B16A16_SFLOAT, _vk);
	
	// PBR pipe
	_materialDescriptorSetLayout = CreateMaterialDescriptorSetLayout(_vk.LogicalDevice());
	_pbrDescriptorSetLayout = CreatePbrDescriptorSetLayout(_vk.LogicalDevice());
	_pbrPipelineLayout = vkh::CreatePipelineLayout(_vk.LogicalDevice(), { 
		_materialDescriptorSetLayout,
		_pbrDescriptorSetLayout,
	});
}
void PbrModelRenderPass::DestroyRenderer()
{
	vkDestroyPipelineLayout(_vk.LogicalDevice(), _pbrPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(_vk.LogicalDevice(), _pbrDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(_vk.LogicalDevice(), _materialDescriptorSetLayout, nullptr);
	vkDestroyRenderPass(_vk.LogicalDevice(), _renderPass, nullptr);
}

void PbrModelRenderPass::InitRendererResourcesDependentOnSwapchain(u32 numImagesInFlight)
{
	_pbrPipeline = CreatePbrGraphicsPipeline(_shaderDir, _pbrPipelineLayout, _vk.MsaaSamples(), _renderPass, _vk.LogicalDevice());

	_rendererDescriptorPool = CreateDescriptorPool(numImagesInFlight, _vk.LogicalDevice());


	// Create light uniform buffers per swapchain image
	std::tie(_lightBuffers, _lightBuffersMemory)
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(LightUbo), _vk.LogicalDevice(), _vk.PhysicalDevice());

	// Create model uniform buffers and descriptor sets per swapchain image
	for (auto& renderable : _renderables)
	{
		renderable->CommonFrameResources = CreateCommonFrameResources(numImagesInFlight);
	}

	_materialFrameResources = std::make_unique<MaterialResourceManager>(_vk, _rendererDescriptorPool, _materialDescriptorSetLayout, _resourceRegistry, _placeholderTexture);
}
void PbrModelRenderPass::DestroyRenderResourcesDependentOnSwapchain()
{
	_materialFrameResources = nullptr; // RAII
	
	for (auto& renderable : _renderables)
	{
		for (auto& info : renderable->CommonFrameResources)
		{
			vkDestroyBuffer(_vk.LogicalDevice(), info.MeshUniformBuffer, nullptr);
			vkFreeMemory(_vk.LogicalDevice(), info.MeshUniformBufferMemory, nullptr);
		}
	}

	for (auto& x : _lightBuffers) { vkDestroyBuffer(_vk.LogicalDevice(), x, nullptr); }
	for (auto& x : _lightBuffersMemory) { vkFreeMemory(_vk.LogicalDevice(), x, nullptr); }

	vkDestroyDescriptorPool(_vk.LogicalDevice(), _rendererDescriptorPool, nullptr);

	vkDestroyPipeline(_vk.LogicalDevice(), _pbrPipeline, nullptr);
}

void PbrModelRenderPass::HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
{
	DestroyRenderResourcesDependentOnSwapchain();
	InitRendererResourcesDependentOnSwapchain(numSwapchainImages);
}

VkRenderPass PbrModelRenderPass::CreateRenderPass(VkFormat format, VulkanService& vk)
{
	auto* physicalDevice = vk.PhysicalDevice();
	auto* device = vk.LogicalDevice();
	const auto msaaSamples = vk.MsaaSamples();
	auto usingMsaa = msaaSamples > VK_SAMPLE_COUNT_1_BIT;

	// Color attachment
	VkAttachmentDescription colorAttachmentDesc = {};
	{
		colorAttachmentDesc.format = format;
		colorAttachmentDesc.samples = msaaSamples;
		colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // what to do with color/depth data before rendering
		colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // what to do with color/depth data after rendering
		colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // not using stencil
		colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	VkAttachmentReference colorAttachmentRef = {};
	{
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}


	// Depth attachment  -  multisample depth doesn't need to be resolved as it won't be displayed
	VkAttachmentDescription depthAttachmentDesc = {};
	{
		depthAttachmentDesc.format = vkh::FindDepthFormat(physicalDevice);
		depthAttachmentDesc.samples = msaaSamples;
		depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // not used after drawing
		depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 
		depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}
	VkAttachmentReference depthAttachmentRef = {};
	{
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	// Resolve attachment  -  Resolve MSAA to single sample image
	VkAttachmentDescription resolveAttachDesc = {};
	{
		resolveAttachDesc.format = format;
		resolveAttachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		resolveAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		resolveAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
		resolveAttachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
		resolveAttachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		resolveAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		resolveAttachDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	VkAttachmentReference resolveAttachRef = {};
	{
		resolveAttachRef.attachment = 2;
		resolveAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	
	
	// Associate color and depth attachements with a subpass
	VkSubpassDescription subpassDesc = {};
	{
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc.colorAttachmentCount = 1;
		subpassDesc.pColorAttachments = &colorAttachmentRef;
		subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;
		subpassDesc.pResolveAttachments = usingMsaa ? &resolveAttachRef : nullptr;
	}


	// TODO Review these dependencies!
	
	
	// Set subpass dependency for the implicit external subpass to wait for the swapchain to finish reading from it
	VkSubpassDependency subpassDependency = {};
	{
		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL; // implicit subpass before render
		subpassDependency.dstSubpass = 0; // this pass
		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.srcAccessMask = 0;
		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}


	// Create render pass
	std::vector<VkAttachmentDescription> attachments = { colorAttachmentDesc, depthAttachmentDesc };
	if (usingMsaa)	{
		attachments.push_back(resolveAttachDesc);
	}

	
	VkRenderPassCreateInfo renderPassCI = {};
	{
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCI.attachmentCount = (u32)attachments.size();
		renderPassCI.pAttachments = attachments.data();
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDesc;
		renderPassCI.dependencyCount = 1;
		renderPassCI.pDependencies = &subpassDependency;
	}

	VkRenderPass renderPass;
	if (vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass");
	}

	return renderPass;
}

bool PbrModelRenderPass::UpdateDescriptors(u32 imageIndex, const RenderOptions& options, bool skyboxUpdated, const SceneRendererPrimitives& scene)
{
	// HACK HACK HACK TODO Optimise this so we only update descriptor sets when needed :)
	_refreshRenderableDescriptorSets = true;
	// HACK HACK HACK

	const bool updateDescriptors = skyboxUpdated || _refreshRenderableDescriptorSets;

	_lastOptions = options;
	_refreshRenderableDescriptorSets = false;

	// Rebuild descriptor sets if needed
	if (!updateDescriptors)
		return updateDescriptors;


	// Get the id of an existing texture, fallback to placeholder if necessary.
	auto GetTexture = [&](const std::optional<Material::Map>& map) -> const TextureResource&
	{
		const auto id =  map.has_value() ? map->Id : _placeholderTexture;
		return _resourceRegistry->GetTexture(id);
	};

	// TODO
	
	/*
	 * DONE! [x] v1  -  unique descset per mat, updated every frame
	 * 
	 * 1. Add scene.Materials of each unique mat.
	 * 2. scene.Objects should have an index ref into scene.Materials
	 * 3. add unordered_map of <matId, matDescSet>
	 * 4. each mat.id has a desc set that's updated every frame
	 */
	
	/*
	 * [ ] v2  -  only update when changed
	 * 
	 * 1. add code in this layer that gens a hash id for material desc set
	 * 2. add unordered_map of <matId, pair<matHash, matDescSet>>
	 */

	for (auto&& mat : scene.Materials)
	{
		/*
		THIS REALLY DOES JUST NEED TO BE ABOUT FREQUENCY OF UPDATE
		SPLITTING THEM UP IS PURELY TO MINIMISE UPDATES IN GENERAL

		PER FRAME: time, ibl, punctual lightingS
			PER MATERIAL: material textures and ubo
				PER MESH: transform
		*/
		
		const PbrMaterialResource& matResources = _materialFrameResources->GetOrCreate(*mat, imageIndex);
		//matResources->UpdateDescriptorSet(mat);
		
		_materialFrameResources->WriteMaterialDescriptorSet(
			matResources.GetMaterialDescriptorSet(),
			matResources.GetMaterialUniformBuffer(),
			GetTexture(mat->BasecolorMap),
			GetTexture(mat->NormalMap),
			GetTexture(mat->RoughnessMap),
			GetTexture(mat->MetalnessMap),
			GetTexture(mat->AoMap),
			GetTexture(mat->EmissiveMap),
			GetTexture(mat->TransparencyMap),
			_vk.LogicalDevice());
	}

	
	for (auto&& object : scene.Objects)
	{
		const auto& commonResources = _renderables[object.RenderableId.Value()]->CommonFrameResources[imageIndex];

		WriteCommonDescriptorSet(
			commonResources.PbrDescriptorSet,
			commonResources.MeshUniformBuffer,
			_lightBuffers[imageIndex],
			_delegate.GetIrradianceTextureResource(),
			_delegate.GetPrefilterTextureResource(),
			_delegate.GetBrdfTextureResource(),
			_delegate.GetShadowmapDescriptor(),
			_vk.LogicalDevice());
	}

	return updateDescriptors;
}

void PbrModelRenderPass::Draw(VkCommandBuffer commandBuffer, u32 frameIndex,
	const RenderOptions& options,
	const std::vector<SceneRendererPrimitives::RenderableObject>& objects,
	const std::vector<Light>& lights,
	const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, const glm::mat4& lightSpaceMatrix)
{
	const auto startBench = std::chrono::steady_clock::now();

	// Update UBOs
	{
		// Light ubo - TODO PERF Keep mem mapped
		{
			auto lightsUbo = LightUbo::Create(lights);

			void* data;
			auto size = sizeof(lightsUbo);
			vkMapMemory(_vk.LogicalDevice(), _lightBuffersMemory[frameIndex], 0, size, 0, &data);
			memcpy(data, &lightsUbo, size);
			vkUnmapMemory(_vk.LogicalDevice(), _lightBuffersMemory[frameIndex]);
		}

	}




	// Update material UBOs
	for (auto&& object : objects)
	{
		
	}


	// Determine draw order - Split renderables into an opaque and ordered transparent buckets.
	std::vector<SceneRendererPrimitives::RenderableObject> opaqueObjects = {};
	std::map<f32, SceneRendererPrimitives::RenderableObject> depthSortedTransparentObjects = {}; // map sorts by keys, so use dist as key
	for (const auto& object : objects)
	{
		const auto& renderable = *_renderables[object.RenderableId.Value()];
		
		// Update UBOs
		{
			PbrUboCreateInfo info = {};
			info.Model = object.Transform;
			info.View = view;
			info.Projection = projection;
			info.LightSpaceMatrix = lightSpaceMatrix;
			info.CamPos = camPos;
			info.ExposureBias = options.ExposureBias;
			info.IblStrength = options.IblStrength;
			info.ShowClipping = options.ShowClipping;
			info.ShowNormalMap = false;
			info.CubemapRotation = options.SkyboxRotation;

			
			// Update Pbr Mesh ubos
			{
				const auto& bufferMemory = renderable.CommonFrameResources[frameIndex].MeshUniformBufferMemory;
				const auto ubo = PbrMeshVsUbo::Create(info);

				// Copy to gpu - TODO PERF Keep mem mapped 
				void* data;
				auto size = sizeof(ubo);
				vkMapMemory(_vk.LogicalDevice(), bufferMemory, 0, size, 0, &data);
				memcpy(data, &ubo, size);
				vkUnmapMemory(_vk.LogicalDevice(), bufferMemory);
			}

			
			// Update Pbr Material ubos
			{
				const auto& matResources = _materialFrameResources->GetOrCreate(object.Material, frameIndex);
				auto* bufferMemory = matResources.GetMaterialUniformBufferMemory();
				const auto ubo = PbrMaterialUbo::Create(info, object.Material);

				// Copy to gpu - TODO PERF Keep mem mapped 
				void* data;
				auto size = sizeof(ubo);
				vkMapMemory(_vk.LogicalDevice(), bufferMemory, 0, size, 0, &data);
				memcpy(data, &ubo, size);
				vkUnmapMemory(_vk.LogicalDevice(), bufferMemory);
			}
		}

		
		// Depth sort transparent object
		if (object.Material.UsingTransparencyMap())
		{

			// Calc depth of from camera to object transform - this isn't fullproof!
			const auto& tf = object.Transform;
			const auto objPos = glm::vec3(tf[3]);
			const glm::vec3 displacement = objPos - camPos;
			float distSquared = glm::dot(displacement, displacement);

			auto [it, success] = depthSortedTransparentObjects.try_emplace(distSquared, object);
			while (!success)
			{
				// HACK to nudge the dist a little. Doing this to avoid needing a more complicated sorted map
				distSquared += 0.001f * (float(rand()) / RAND_MAX);
				std::tie(it, success) = depthSortedTransparentObjects.try_emplace(distSquared, object);
				//std::cerr << "Failed to depth sort object\n";
			}
		}
		else // Opaque
		{
			opaqueObjects.emplace_back(object);
		}
	}

	
	// Draw Pbr Objects
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pbrPipeline);

		auto DrawMesh = [&](const SceneRendererPrimitives::RenderableObject& obj)
		{
			const auto& renderable = _renderables[obj.RenderableId.Value()].get();
			const auto& mesh = _resourceRegistry->GetMesh(renderable->MeshId);
			const auto& materialResource = _materialFrameResources->GetOrCreate(obj.Material, frameIndex);
			
			std::array<VkDescriptorSet, 2> descSets = {
				materialResource.GetMaterialDescriptorSet(),
				renderable->CommonFrameResources[frameIndex].PbrDescriptorSet
			};
			
			// Draw mesh
			VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS, _pbrPipelineLayout, // TODO Use diff pipeline with blending disabled?
				0, (u32)descSets.size(), descSets.data(), 0, nullptr);
			vkCmdDrawIndexed(commandBuffer, (u32)mesh.IndexCount, 1, 0, 0, 0);
		};

		
		// Draw Opaque objects
		for (auto&& opaqueObj : opaqueObjects)
		{
			DrawMesh(opaqueObj);
		}

		// Draw transparent objects (reverse iterated)
		for (auto it = depthSortedTransparentObjects.rbegin(); it != depthSortedTransparentObjects.rend(); ++it)
		{
			auto [_, transparentObj] = *it;
			DrawMesh(transparentObj);
		}
	}

	
	const std::chrono::duration<double, std::chrono::milliseconds::period> duration
		= std::chrono::steady_clock::now() - startBench;
	//std::cout << "# Update loop took:  " << std::setprecision(3) << duration.count() << "ms.\n";
}


[[deprecated("use ResourceRegistry directly and not through PbrModelRenderPass")]] // TODO Remove this method
TextureResourceId PbrModelRenderPass::Hack_CreateTextureResource(const std::string& path)
{
	return _resourceRegistry->CreateTextureResource(path);
}

[[deprecated("use ResourceRegistry directly and not through PbrModelRenderPass")]] // TODO Remove this method
MeshResourceId PbrModelRenderPass::Hack_CreateMeshResource(const MeshDefinition& meshDefinition)
{
	return _resourceRegistry->CreateMeshResource(meshDefinition);
}

RenderableResourceId PbrModelRenderPass::CreateRenderable(const MeshResourceId& meshId)
{
	auto model = std::make_unique<RenderableMesh>();
	model->MeshId = meshId;
	model->CommonFrameResources = CreateCommonFrameResources(_vk.GetSwapchain().GetImageCount()); 

	const auto id = RenderableResourceId((u32)_renderables.size());
	_renderables.emplace_back(std::move(model));

	return id;
}

/*
// TODO - KEEP THIS CODE-  The comparison below could be used to determine whether a mat descriptor needs to be written

void PbrModelRenderPass::SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat)
{
		
	auto& renderable = *_renderables[renderableResId.Id];
	auto& oldMat = renderable.Mat;

	const auto pid = _placeholderTexture.Id;
	auto GetId = [pid](const std::optional<Material::Map>& map) { return map.has_value() ? map->Id.Id : pid; };
	
	// Bail early if the new descriptor set is identical (eg, if not changing a Map id!)
	const bool descriptorSetsMatch =
		GetId(oldMat.BasecolorMap)    == GetId(newMat.BasecolorMap) &&
		GetId(oldMat.NormalMap)       == GetId(newMat.NormalMap)    &&
		GetId(oldMat.RoughnessMap)    == GetId(newMat.RoughnessMap) &&
		GetId(oldMat.MetalnessMap)    == GetId(newMat.MetalnessMap) &&
		GetId(oldMat.AoMap)           == GetId(newMat.AoMap)        &&
		GetId(oldMat.EmissiveMap)     == GetId(newMat.EmissiveMap)  &&
		GetId(oldMat.TransparencyMap) == GetId(newMat.TransparencyMap);

	
	// Store new mat
	renderable.Mat = newMat;

	
	if (!descriptorSetsMatch)
	{
		// NOTE: This is heavy handed as it rebuilds ALL object descriptor sets, not just those using this material
		_refreshRenderableDescriptorSets = true;
	}
}
*/


VkDescriptorPool PbrModelRenderPass::CreateDescriptorPool(u32 numImagesInFlight, VkDevice device)
{
	const u32 maxPbrObjects = 10000; // Max scene objects! This is gross, but it'll do for now.
	//const u32 maxSkyboxObjects = 1;

	// Match these to CreatePbrDescriptorSetLayout
	const auto numPbrUniformBuffers = 3;
	const auto numPbrCombinedImageSamplers = 11;

	// Match these to CreateSkyboxDescriptorSetLayout
	//const auto numSkyboxUniformBuffers = 2;
	//const auto numSkyboxCombinedImageSamplers = 1;
	
	// Define which descriptor types our descriptor sets contain
	const std::vector<VkDescriptorPoolSize> poolSizes
	{
		// PBR Objects
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numPbrUniformBuffers * maxPbrObjects * numImagesInFlight},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numPbrCombinedImageSamplers * maxPbrObjects * numImagesInFlight},

		// Skybox Object
		//{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numSkyboxUniformBuffers * maxSkyboxObjects * numImagesInFlight},
		//{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numSkyboxCombinedImageSamplers * maxSkyboxObjects * numImagesInFlight},
	};

	const auto totalDescSets = (maxPbrObjects/* + maxSkyboxObjects*/) * numImagesInFlight;

	return vkh::CreateDescriptorPool(poolSizes, totalDescSets, device);
}


VkPipeline PbrModelRenderPass::CreatePbrGraphicsPipeline(const std::string& shaderDir,
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
		const auto vertShaderCode = FileService::ReadFile(shaderDir + "Pbr.vert.spv");
		const auto fragShaderCode = FileService::ReadFile(shaderDir + "Pbr.frag.spv");

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
		depthStencilCI.depthWriteEnable = true; // can new depth tests write to buffer?
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


#pragma region Material Descriptor Sets

VkDescriptorSetLayout PbrModelRenderPass::CreateMaterialDescriptorSetLayout(VkDevice device)
{
	return vkh::CreateDescriptorSetLayout(device, {
		// pbr material ubo
		vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
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
		// emissiveMap
		vki::DescriptorSetLayoutBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// transparencyMap
		vki::DescriptorSetLayoutBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
	});
}

#pragma endregion Material Descriptor Sets


#pragma region Common Descriptor Sets

std::vector<PbrCommonResourceFrame> PbrModelRenderPass::CreateCommonFrameResources(u32 numImagesInFlight) const
{
	// Create uniform buffers
	auto [meshBuffers, meshBuffersMemory] = vkh::CreateUniformBuffers(numImagesInFlight, sizeof(PbrMeshVsUbo), 
		_vk.LogicalDevice(), _vk.PhysicalDevice());

	// Create descriptor sets
	const auto pbrDescSets = vkh::AllocateDescriptorSets(numImagesInFlight, _pbrDescriptorSetLayout, _rendererDescriptorPool, _vk.LogicalDevice());


	for (u32 i = 0; i < numImagesInFlight; i++)
	{
		WriteCommonDescriptorSet(
			pbrDescSets[i],
			meshBuffers[i],
			_lightBuffers[i],
			_delegate.GetIrradianceTextureResource(),
			_delegate.GetPrefilterTextureResource(),
			_delegate.GetBrdfTextureResource(),
			_delegate.GetShadowmapDescriptor(),
			_vk.LogicalDevice());
	}

	// Group data for return
	std::vector<PbrCommonResourceFrame> ret(numImagesInFlight);
	for (size_t i = 0; i < numImagesInFlight; i++)
	{
		ret[i].MeshUniformBuffer = meshBuffers[i];
		ret[i].MeshUniformBufferMemory = meshBuffersMemory[i];
		ret[i].PbrDescriptorSet = pbrDescSets[i];
	}

	return ret;
}

VkDescriptorSetLayout PbrModelRenderPass::CreatePbrDescriptorSetLayout(VkDevice device)
{
	return vkh::CreateDescriptorSetLayout(device, {
		// pbr model ubo
		vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),

		// irradiance map
		vki::DescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// prefilter map
		vki::DescriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// brdf map
		vki::DescriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),

		// light ubo
		vki::DescriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
		// shadowMap
		vki::DescriptorSetLayoutBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
	});
}

void PbrModelRenderPass::WriteCommonDescriptorSet(
	VkDescriptorSet descriptorSet,
	VkBuffer meshUbo,
	VkBuffer lightUbo,
	const TextureResource& irradianceMap,
	const TextureResource& prefilterMap,
	const TextureResource& brdfMap,
	VkDescriptorImageInfo shadowmapDescriptor,
	VkDevice device)
{

	// Configure our new descriptor sets to point to our buffer/image data
	VkDescriptorBufferInfo meshUboInfo = {};
	{
		meshUboInfo.buffer = meshUbo;
		meshUboInfo.offset = 0;
		meshUboInfo.range = sizeof(PbrMeshVsUbo);
	}

	VkDescriptorBufferInfo lightUboInfo = {};
	{
		lightUboInfo.buffer = lightUbo;
		lightUboInfo.offset = 0;
		lightUboInfo.range = sizeof(LightUbo);
	}

	const auto& s = descriptorSet;

	vkh::UpdateDescriptorSet(device, {
		// Mesh
		vki::WriteDescriptorSet(s, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &meshUboInfo),
		// IBL
		vki::WriteDescriptorSet(s, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &irradianceMap.ImageInfo()),
		vki::WriteDescriptorSet(s, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &prefilterMap.ImageInfo()),
		vki::WriteDescriptorSet(s, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &brdfMap.ImageInfo()),
		// Discrete lighting
		vki::WriteDescriptorSet(s, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &lightUboInfo),
		vki::WriteDescriptorSet(s, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &shadowmapDescriptor),
		});
}


#pragma endregion Common Descriptor Sets

