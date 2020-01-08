#pragma once

#include <optional>
#include <vector>
#include <array>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct UniformBufferObject
{
	alignas(16) glm::mat4 Model;
	alignas(16) glm::mat4 View;
	alignas(16) glm::mat4 Projection;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Vertex
{
	glm::vec3 Pos;
	glm::vec3 Color;
	glm::vec2 TexCoord;

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

	static std::array<VkVertexInputAttributeDescription, 3> AttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attrDesc = {};
		{
			// Pos
			attrDesc[0].binding = 0;
			attrDesc[0].location = 0;
			attrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attrDesc[0].offset = offsetof(Vertex, Pos);
			// Color
			attrDesc[1].binding = 0;
			attrDesc[1].location = 1;
			attrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attrDesc[1].offset = offsetof(Vertex, Color);
			// TexCoord
			attrDesc[2].binding = 0;
			attrDesc[2].location = 2;
			attrDesc[2].format = VK_FORMAT_R32G32_SFLOAT;
			attrDesc[2].offset = offsetof(Vertex, TexCoord);
		}
		return attrDesc;
	}
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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FpsCounter
{
public:
	FpsCounter() : FpsCounter(120) {}
	explicit FpsCounter(size_t bufferSize)
	{
		_buffer.resize(bufferSize, 0);
	}
	void AddFrameTime(double dt)
	{
		_secPerFrameAccumulator += dt - _buffer[_index];
		_buffer[_index] = dt;
		_index = (_index + 1) % _buffer.size();
	}

	double GetFps() const
	{
		return 1.0 / (_secPerFrameAccumulator / (double)_buffer.size());
		return _frameRate;
		
	}

private:
	double _frameRate = 0;
	double _secPerFrameAccumulator = 0;
	std::vector<double> _buffer{};
	size_t _index = 0;
};

