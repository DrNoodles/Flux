#pragma once

#include <optional>
#include <vector>
#include <array>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

struct Vertex
{
	glm::vec2 Pos;
	glm::vec3 Color;

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

	static std::array<VkVertexInputAttributeDescription, 2> AttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 2> attrDesc = {};
		{
			// Pos
			attrDesc[0].binding = 0;
			attrDesc[0].location = 0;
			attrDesc[0].format = VK_FORMAT_R32G32_SFLOAT;
			attrDesc[0].offset = offsetof(Vertex, Pos);
			// Color
			attrDesc[1].binding = 0;
			attrDesc[1].location = 1;
			attrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attrDesc[1].offset = offsetof(Vertex, Color);
		}
		return attrDesc;
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR Capabilities{};
	std::vector<VkSurfaceFormatKHR> Formats{};
	std::vector<VkPresentModeKHR> PresentModes{};
};

struct QueueFamilyIndices
{
	std::optional<uint32_t> GraphicsFamily = std::nullopt;
	std::optional<uint32_t> PresentFamily = std::nullopt;

	bool IsComplete() const
	{
		return GraphicsFamily.has_value() && PresentFamily.has_value();
	}
};
