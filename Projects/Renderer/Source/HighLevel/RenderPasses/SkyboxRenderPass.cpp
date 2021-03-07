
#include "Renderer/HighLevel/RenderPasses/SkyboxRenderPass.h"

#include "Renderer/LowLevel/GpuTypes.h"
#include "Renderer/LowLevel/VulkanHelpers.h"
#include "Renderer/LowLevel/VulkanInitializers.h"
#include "Renderer/LowLevel/UniformBufferObjects.h"
#include "Renderer/LowLevel/RenderableMesh.h"
#include "Renderer/HighLevel/CubemapTextureLoader.h"
#include "Renderer/HighLevel/IblLoader.h"
#include "Renderer/LowLevel/VulkanService.h"
#include "Renderer/HighLevel/ResourceRegistry.h"

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

SkyboxRenderPass::SkyboxRenderPass(VulkanService& vulkanService, ResourceRegistry* registry, std::string shaderDir, const std::string& assetsDir, IModelLoaderService& modelLoaderService)
	: _vk(vulkanService), _resources(registry), _shaderDir(std::move(shaderDir))
{
	InitResources();
	InitResourcesDependentOnSwapchain(_vk.GetSwapchain().GetImageCount());
	
	_placeholderTextureId = _resources->CreateTextureResource(assetsDir + "placeholder.png");  // TODO Move this to some common resources code

	// Load a cube
	auto model = modelLoaderService.LoadModel(assetsDir + "skybox.obj");  // TODO Move this to some common resources code
	auto& meshDefinition = model.value().Meshes[0];
	_skyboxMeshId = _resources->CreateMeshResource(meshDefinition);
}

void SkyboxRenderPass::Destroy()
{
	vkDeviceWaitIdle(_vk.LogicalDevice());
	
	DestroyResourcesDependentOnSwapchain();
	DestroyResources();
}


void SkyboxRenderPass::InitResources()
{
	_renderPass = CreateRenderPass(VK_FORMAT_R16G16B16A16_SFLOAT, _vk);
	_descSetLayout = CreateDescSetLayout(_vk.LogicalDevice());
	_pipelineLayout = vkh::CreatePipelineLayout(_vk.LogicalDevice(), { _descSetLayout });
}
void SkyboxRenderPass::DestroyResources()
{
	vkDestroyPipelineLayout(_vk.LogicalDevice(), _pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(_vk.LogicalDevice(), _descSetLayout, nullptr);
	vkDestroyRenderPass(_vk.LogicalDevice(), _renderPass, nullptr);
	_pipelineLayout = nullptr;
	_descSetLayout = nullptr;
	_renderPass = nullptr;
}

void SkyboxRenderPass::InitResourcesDependentOnSwapchain(u32 numImagesInFlight)
{
	_pipeline = CreateGraphicsPipeline(_shaderDir, _pipelineLayout, _vk.MsaaSamples(), _renderPass, _vk.LogicalDevice());

	_descPool = CreateDescPool(numImagesInFlight, _vk.LogicalDevice());
	
	// Create frame resources for skybox
	for (auto& skybox : _skyboxes)
	{
		skybox->FrameResources = CreateModelFrameResources(numImagesInFlight, *skybox);
	}
}
void SkyboxRenderPass::DestroyResourcesDependentOnSwapchain()
{
	for (auto& skybox : _skyboxes)
	{
		for (auto& info : skybox->FrameResources)
		{
			vkDestroyBuffer(_vk.LogicalDevice(), info.VertUniformBuffer, nullptr);
			vkFreeMemory(_vk.LogicalDevice(), info.VertUniformBufferMemory, nullptr);
			vkDestroyBuffer(_vk.LogicalDevice(), info.FragUniformBuffer, nullptr);
			vkFreeMemory(_vk.LogicalDevice(), info.FragUniformBufferMemory, nullptr);
			//vkFreeDescriptorSets(_vk->LogicalDevice(), _descriptorPool, (uint32_t)mesh.DescriptorSets.size(), mesh.DescriptorSets.data());
		}
	}

	vkDestroyDescriptorPool(_vk.LogicalDevice(), _descPool, nullptr);
	vkDestroyPipeline(_vk.LogicalDevice(), _pipeline, nullptr);
}

void SkyboxRenderPass::HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
{
	DestroyResourcesDependentOnSwapchain();
	InitResourcesDependentOnSwapchain(numSwapchainImages);
}

VkRenderPass SkyboxRenderPass::CreateRenderPass(VkFormat format, VulkanService& vk)
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

bool SkyboxRenderPass::UpdateDescriptors(const RenderOptions& options)
{
	bool wasUpdated = false;
	
	// Diff render options and force state updates where needed
		// Process whether refreshing is required
	_refreshDescSets |= _lastOptions.ShowIrradiance != options.ShowIrradiance;


	_lastOptions = options;

	// Rebuild descriptor sets as needed
	if (_refreshDescSets)
	{
		_refreshDescSets = false;
		UpdateDescSets();
		wasUpdated = true;
	}

	return wasUpdated;
}

void SkyboxRenderPass::Draw(VkCommandBuffer commandBuffer, u32 frameIndex,
	const RenderOptions& options,
	const glm::mat4& view, const glm::mat4& projection) const
{
	const auto startBench = std::chrono::steady_clock::now();
	
	const Skybox* skybox = GetCurrentSkyboxOrNull();
	assert(skybox); // dont draw this at all if there's no skybox?

	
	// Update Vert UBO
	{
		// Populate
		auto skyboxVertUbo = SkyboxVertUbo{};
		skyboxVertUbo.Projection = projection; // same as camera
		skyboxVertUbo.Rotation = rotate(glm::radians(options.SkyboxRotation), glm::vec3{ 0,1,0 });
		skyboxVertUbo.View = glm::mat4{ glm::mat3{view} }; // only keep view rotation

		// Copy to gpu - TODO PERF Keep mem mapped 
		void* data;
		const auto size = sizeof(skyboxVertUbo);
		vkMapMemory(_vk.LogicalDevice(), skybox->FrameResources[frameIndex].VertUniformBufferMemory, 0, size, 0, &data);
		memcpy(data, &skyboxVertUbo, size);
		vkUnmapMemory(_vk.LogicalDevice(), skybox->FrameResources[frameIndex].VertUniformBufferMemory);
	}


	// Update Frag UBO
	{
		// Populate
		auto skyboxFragUbo = SkyboxFragUbo{};
		skyboxFragUbo.ExposureBias = options.ExposureBias;
		skyboxFragUbo.ShowClipping = options.ShowClipping;
		skyboxFragUbo.IblStrength = options.IblStrength;
		skyboxFragUbo.BackdropBrightness = options.BackdropBrightness;

		// Copy to gpu - TODO PERF Keep mem mapped 
		void* data;
		const auto size = sizeof(skyboxFragUbo);
		vkMapMemory(_vk.LogicalDevice(), skybox->FrameResources[frameIndex].FragUniformBufferMemory, 0, size, 0, &data);
		memcpy(data, &skyboxFragUbo, size);
		vkUnmapMemory(_vk.LogicalDevice(), skybox->FrameResources[frameIndex].FragUniformBufferMemory);
	}


	// Draw Skybox
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

		const auto& mesh = _resources->GetMesh(skybox->MeshId);

		// Draw mesh
		VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout,
			0, 1, &skybox->FrameResources[frameIndex].DescriptorSet, 0, nullptr);
		vkCmdDrawIndexed(commandBuffer, (uint32_t)mesh.IndexCount, 1, 0, 0, 0);
	}

	
	const std::chrono::duration<double, std::chrono::milliseconds::period> duration
		= std::chrono::steady_clock::now() - startBench;
	//std::cout << "# Update loop took:  " << std::setprecision(3) << duration.count() << "ms.\n";
}

IblTextureResourceIds SkyboxRenderPass::CreateIblTextureResources(const std::array<std::string, 6>& sidePaths) const
{
	return _resources->CreateIblTextureResources(sidePaths);
}

IblTextureResourceIds SkyboxRenderPass::CreateIblTextureResources(const std::string& path) const
{
	return _resources->CreateIblTextureResources(path);
}

SkyboxResourceId SkyboxRenderPass::CreateSkybox(const SkyboxCreateInfo& createInfo)
{
	auto skybox = std::make_unique<Skybox>();
	skybox->MeshId = _skyboxMeshId;
	skybox->IblTextureIds = createInfo.IblTextureIds;
	skybox->FrameResources = CreateModelFrameResources(_vk.GetSwapchain().GetImageCount(), *skybox);

	const auto id = SkyboxResourceId(static_cast<u32>(_skyboxes.size()));
	_skyboxes.emplace_back(std::move(skybox));

	return id;
}

void SkyboxRenderPass::SetSkybox(const SkyboxResourceId& resourceId)
{
	// Set skybox
	_activeSkybox = resourceId;
}

#pragma region Shared

VkDescriptorPool SkyboxRenderPass::CreateDescPool(u32 numImagesInFlight, VkDevice device)
{
	const u32 maxSkyboxObjects = 100; // User can load 100 skyboxes in 1 session

	// Match these to CreateDescSetLayout
	const auto numSkyboxUniformBuffers = 2;
	const auto numSkyboxCombinedImageSamplers = 1;
	
	// Define which descriptor types our descriptor sets contain
	const std::vector<VkDescriptorPoolSize> poolSizes
	{
		// Skybox Object
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numSkyboxUniformBuffers * maxSkyboxObjects * numImagesInFlight},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numSkyboxCombinedImageSamplers * maxSkyboxObjects * numImagesInFlight},
	};

	const auto totalDescSets = (maxSkyboxObjects) * numImagesInFlight;

	return vkh::CreateDescriptorPool(poolSizes, totalDescSets, device);
}

const TextureResource& SkyboxRenderPass::GetIrradianceTextureResource() const
{
	const auto* skybox = GetCurrentSkyboxOrNull();
	return _resources->GetTexture(skybox ? skybox->IblTextureIds.IrradianceCubemapId : _placeholderTextureId);
}

const TextureResource& SkyboxRenderPass::GetPrefilterTextureResource() const
{
	const auto* skybox = GetCurrentSkyboxOrNull();
	return _resources->GetTexture(skybox ? skybox->IblTextureIds.PrefilterCubemapId : _placeholderTextureId);
}

const TextureResource& SkyboxRenderPass::GetBrdfTextureResource() const
{
	const auto* skybox = GetCurrentSkyboxOrNull();
	return _resources->GetTexture(skybox ? skybox->IblTextureIds.BrdfLutId : _placeholderTextureId);
}

#pragma endregion Shared



#pragma region Skybox

std::vector<SkyboxResourceFrame>
SkyboxRenderPass::CreateModelFrameResources(u32 numImagesInFlight, const Skybox& skybox) const
{
	// Allocate descriptor sets
	std::vector<VkDescriptorSet> descriptorSets
		= vkh::AllocateDescriptorSets(numImagesInFlight, _descSetLayout, _descPool, _vk.LogicalDevice());


	// Vert Uniform buffers
	auto [skyboxVertBuffers, skyboxVertBuffersMemory]
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(SkyboxVertUbo), _vk.LogicalDevice(), _vk.PhysicalDevice());

	
	// Frag Uniform buffers
	auto [skyboxFragBuffers, skyboxFragBuffersMemory]
		= vkh::CreateUniformBuffers(numImagesInFlight, sizeof(SkyboxFragUbo), _vk.LogicalDevice(), _vk.PhysicalDevice());


	const auto textureId = _lastOptions.ShowIrradiance
		? skybox.IblTextureIds.IrradianceCubemapId
		: skybox.IblTextureIds.EnvironmentCubemapId;
	
	WriteDescSets(
		numImagesInFlight, descriptorSets, skyboxVertBuffers, skyboxFragBuffers, _resources->GetTexture(textureId), _vk.LogicalDevice());


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

VkDescriptorSetLayout SkyboxRenderPass::CreateDescSetLayout(VkDevice device)
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

void SkyboxRenderPass::WriteDescSets(
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

		const auto& set = descriptorSets[i];
		
		vkh::UpdateDescriptorSet(device, {
			vki::WriteDescriptorSet(set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &vertBufferUboInfo),
			vki::WriteDescriptorSet(set, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, nullptr, &fragBufferUboInfo),
			vki::WriteDescriptorSet(set, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, &skyboxMap.ImageInfo()),
			});
	}
}

VkPipeline SkyboxRenderPass::CreateGraphicsPipeline(const std::string& shaderDir,
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
	//	viewport.x = 0;
	//	viewport.y = (float)swapchainExtent.height;
	//	viewport.width = (float)swapchainExtent.width;
	//	viewport.height = -(float)swapchainExtent.height;
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

void SkyboxRenderPass::UpdateDescSets()
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

		const auto& skyboxTexture = _resources->GetTexture(_lastOptions.ShowIrradiance
			? skybox->IblTextureIds.IrradianceCubemapId
			: skybox->IblTextureIds.EnvironmentCubemapId);

		WriteDescSets((u32)count, descriptorSets, vertUbos, fragUbos, skyboxTexture, _vk.LogicalDevice());
	}
}

#pragma endregion Skybox

