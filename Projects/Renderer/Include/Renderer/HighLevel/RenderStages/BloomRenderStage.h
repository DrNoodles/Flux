#pragma once

#include <vulkan/vulkan.h>

#include "Framework/FileService.h"
#include "Renderer/LowLevel/GpuTypes.h"
#include "Renderer/LowLevel/UniformBufferObjects.h"
#include "Renderer/LowLevel/VulkanService.h"
#include "Renderer/LowLevel/TextureResource.h"

struct InputFramebuffers
{
	VkDescriptorImageInfo Texture;
};

struct OutputFramebuffers
{
	VkImage ColorImage;
	VkDescriptorImageInfo ColorInfo;
};

template <class TInput, class TOutput>
class RenderStage
{
public:
	virtual ~RenderStage() = default;
	virtual void SetIntput(TInput input) = 0;
	virtual TOutput GetOutput() = 0;
};

class BloomRenderStage : public RenderStage<InputFramebuffers, OutputFramebuffers>
{
private: // Types
	struct DescriptorResources // TODO Extract this for all RenderStages to use.
	{
		std::vector<VkDescriptorSet> DescriptorSets = {};
		std::vector<VkBuffer> UboBuffers = {};
		std::vector<VkDeviceMemory> UboBuffersMemory = {};
		VkDescriptorPool DescriptorPool = nullptr;

		void Destroy(VkDevice device, VkAllocationCallbacks* allocator)
		{
			for (auto& buffer : UboBuffers)
				vkDestroyBuffer(device, buffer, allocator);

			for (auto& deviceMemory : UboBuffersMemory)
				vkFreeMemory(device, deviceMemory, allocator);

			vkDestroyDescriptorPool(device, DescriptorPool, allocator);

			// Note: VkDescriptorSets do not need to be destroyed.
		}
	};

public:  // Data
private: // Data
	VulkanService* _vk = nullptr;

	// Compute descriptor crap
	DescriptorResources _descriptorResources;

	VkPipeline _computePipeline = nullptr;
	VkPipelineLayout _computePipelineLayout = nullptr;
	VkDescriptorSetLayout _descSetLayout = nullptr;

	std::unique_ptr<TextureResource> _texture = nullptr;


public: // Methods
	BloomRenderStage() = delete;

	explicit BloomRenderStage(const std::string& shaderDir, VulkanService* vk) : _vk(vk)
	{
		// Create Descriptor Set Layout
		VkDescriptorSetLayout descSetLayout = vkh::CreateDescriptorSetLayout(_vk->LogicalDevice(), { 
			vki::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT),
			vki::DescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT),
			});

		// Create Compute Pipeline Layout
		VkPipelineLayout computePipelineLayout = vkh::CreatePipelineLayout(_vk->LogicalDevice(), { descSetLayout }, {});
		
		// Create Compute Pipeline
		VkPipeline computePipeline = nullptr;
		{
			VkPipelineShaderStageCreateInfo shaderStageInfo = {};
			shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageInfo.pName = "main";
			shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			shaderStageInfo.module = vkh::CreateShaderModule(FileService::ReadFile(shaderDir + "BloomExtractBright.comp.spv"), _vk->LogicalDevice());
			shaderStageInfo.pSpecializationInfo = nullptr;

			// TODO Extract into 1 liner vkh:: func
			VkComputePipelineCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			createInfo.layout = computePipelineLayout;
			createInfo.stage = shaderStageInfo;
			createInfo.flags = 0;
			createInfo.basePipelineHandle = nullptr;
			createInfo.basePipelineIndex = 0;
			if (vkCreateComputePipelines(_vk->LogicalDevice(), nullptr, 1, &createInfo, _vk->Allocator(), &computePipeline) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to Create Compute Pipeline!");
			}
		}

		_descSetLayout = descSetLayout;
		_computePipeline = computePipeline;
		_computePipelineLayout = computePipelineLayout;
	}

	void Destroy()
	{
		if (_vk)
		{
			DestroyDescriptorResources();
			vkDestroyPipelineLayout(_vk->LogicalDevice(), _computePipelineLayout, _vk->Allocator());
			vkDestroyPipeline(_vk->LogicalDevice(), _computePipeline, _vk->Allocator());
			_vk = nullptr;
		}
	}

	void CreateDescriptorResources(InputFramebuffers input, VkExtent2D extent)
	{
		// Create Output image
		{
			auto format = VK_FORMAT_R16G16B16A16_SFLOAT;
			auto finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			auto sampler = vkh::CreateSampler(_vk->LogicalDevice());
			auto mipLevels = 1;

			auto [image, memory] = vkh::CreateImage2D(extent.width, extent.height, mipLevels,
				VK_SAMPLE_COUNT_1_BIT,
				format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				_vk->PhysicalDevice(), _vk->LogicalDevice());
			
			auto view = vkh::CreateImage2DView(image, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, _vk->LogicalDevice());
			
			auto cmdBuffer = vkh::BeginSingleTimeCommands(_vk->CommandPool(), _vk->LogicalDevice());
			vkh::TransitionImageLayout(cmdBuffer, image,
				VK_IMAGE_LAYOUT_UNDEFINED, // from
				finalLayout, // to
				VK_IMAGE_ASPECT_COLOR_BIT); // TODO Optimise pipeline stage flags
			vkh::EndSingeTimeCommands(cmdBuffer, _vk->CommandPool(), _vk->GraphicsQueue(), _vk->LogicalDevice());

			_texture = std::make_unique<TextureResource>(_vk->LogicalDevice(),
				extent.width, extent.height, mipLevels, 1,
				image, memory, view, sampler,
				format, finalLayout);
		}
	
		
		auto imageCount = _vk->GetSwapchain().GetImageCount();

		// Create descriptor pool
		VkDescriptorPool descPool;
		{
			const std::vector<VkDescriptorPoolSize> poolSizes = {
				VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}, // TODO Look at whether i need imageCount number at all. Might just need 1
				VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
			};
			descPool = vkh::CreateDescriptorPool(poolSizes, 1, _vk->LogicalDevice());
		}

		// Create Descriptor Set
		VkDescriptorSet descSet = vkh::AllocateDescriptorSets(1, _descSetLayout, descPool, _vk->LogicalDevice())[0]; // NOTE: [0]
		vkh::UpdateDescriptorSet(_vk->LogicalDevice(), {
			vki::WriteDescriptorSet(descSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, 0, &input.Texture),
			vki::WriteDescriptorSet(descSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, 0, &_texture->ImageInfo()),
			});

		_descriptorResources = DescriptorResources{};
		_descriptorResources.DescriptorPool = descPool;
		_descriptorResources.DescriptorSets = { descSet };
	}

	void DestroyDescriptorResources()
	{
		_texture = nullptr; // RAII
		_descriptorResources.Destroy(_vk->LogicalDevice(), _vk->Allocator());
	}

	void Draw(VkCommandBuffer commandBuffer, i32 imageIndex, const RenderOptions& ro)
	{
		//vkCmdPushConstants(commandBuffer, m_computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(), 0)
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, _descriptorResources.DescriptorSets.data(), 0, nullptr);
		vkCmdDispatch(commandBuffer, (_texture->Width() + 15) / 16, (_texture->Height() + 15) / 16, 1);
	}

	void SetIntput(InputFramebuffers input)
	{
	}

	OutputFramebuffers GetOutput() override
	{
		return OutputFramebuffers{ _texture->Image(), _texture->ImageInfo() };
	}

private: // Methods
};
