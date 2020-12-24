#pragma once

#include <Framework/Vertex.h>
#include <Framework/CommonTypes.h>

#include <vulkan/vulkan.h>

#include <array>
#include <optional>
#include <vector>



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Light
{
	enum class LightType : i32
	{
		Point = 0, Directional = 1
	};
	f32 Intensity;
	glm::vec3 Color;
	glm::vec3 Pos;
	LightType Type;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace VertexHelper
{
	static VkVertexInputBindingDescription BindingDescription()
	{
		VkVertexInputBindingDescription bindingDesc = {};
		{
			bindingDesc.binding = 0;
			bindingDesc.stride = sizeof(Vertex);
			bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}
		return bindingDesc;
	}

	static std::array<VkVertexInputAttributeDescription, 5> AttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 5> attrDesc = {};
		{
			// Pos
			attrDesc[0].binding = 0;
			attrDesc[0].location = 0;
			attrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attrDesc[0].offset = offsetof(Vertex, Pos);
			// Normal
			attrDesc[1].binding = 0;
			attrDesc[1].location = 1;
			attrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attrDesc[1].offset = offsetof(Vertex, Normal);
			// Color
			attrDesc[2].binding = 0;
			attrDesc[2].location = 2;
			attrDesc[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			attrDesc[2].offset = offsetof(Vertex, Color);
			// TexCoord
			attrDesc[3].binding = 0;
			attrDesc[3].location = 3;
			attrDesc[3].format = VK_FORMAT_R32G32_SFLOAT;
			attrDesc[3].offset = offsetof(Vertex, TexCoord);
			// Tangent
			attrDesc[4].binding = 0;
			attrDesc[4].location = 4;
			attrDesc[4].format = VK_FORMAT_R32G32B32_SFLOAT;
			attrDesc[4].offset = offsetof(Vertex, Tangent);
			//// Bitangent
			//attrDesc[5].binding = 0;
			//attrDesc[5].location = 5;
			//attrDesc[5].format = VK_FORMAT_R32G32B32_SFLOAT;
			//attrDesc[5].offset = offsetof(Vertex, Bitangent);
		}
		return attrDesc;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MeshResource
{
	size_t VertexCount = 0;
	size_t IndexCount = 0;
	VkBuffer VertexBuffer = nullptr;
	VkDeviceMemory VertexBufferMemory = nullptr;
	VkBuffer IndexBuffer = nullptr;
	VkDeviceMemory IndexBufferMemory = nullptr;
	//AABB Bounds;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SkyboxResourceFrame // for lack of a better name...
{
	VkDescriptorSet DescriptorSet;
	VkBuffer VertUniformBuffer;
	VkDeviceMemory VertUniformBufferMemory;
	VkBuffer FragUniformBuffer;
	VkDeviceMemory FragUniformBufferMemory;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PbrModelResourceFrame // for lack of a better name...
{
	VkDescriptorSet DescriptorSet;
	VkBuffer UniformBuffer;
	VkDeviceMemory UniformBufferMemory;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR Capabilities{};
	std::vector<VkSurfaceFormatKHR> Formats{};
	std::vector<VkPresentModeKHR> PresentModes{};
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct QueueFamilyIndices
{
	std::optional<uint32_t> GraphicsFamily = std::nullopt;
	std::optional<uint32_t> PresentFamily = std::nullopt;

	bool IsComplete() const
	{
		return GraphicsFamily.has_value() && PresentFamily.has_value();
	}
};
