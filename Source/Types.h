#pragma once

#include <optional>
#include <vector>
#include <array>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL // for hash
#include <glm/gtx/hash.hpp>
#include "Transform.h"
#include "AABB.h"

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
	glm::vec3 Normal;
	glm::vec3 Color;
	glm::vec2 TexCoord;

	bool operator==(const Vertex& other) const
	{
		return Pos == other.Pos && Color == other.Color && TexCoord == other.TexCoord;
	}
	
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

	static std::array<VkVertexInputAttributeDescription, 4> AttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 4> attrDesc = {};
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
		}
		return attrDesc;
	}
};
namespace std
{
	template<> struct hash<Vertex>
	{
		size_t operator()(Vertex const& vertex) const noexcept
		{
			// based on hash technique recommendation from https://en.cppreference.com/w/cpp/utility/hash
			// ... which is apparently flawed... good enough? probably!
			const size_t posHash = hash<glm::vec3>()(vertex.Pos);
			const size_t normalHash = hash<glm::vec3>()(vertex.Normal);
			const size_t colorHash = hash<glm::vec3>()(vertex.Color);
			const size_t texCoordHash = hash<glm::vec2>()(vertex.TexCoord);

			const size_t join1 = (posHash ^ (normalHash << 1)) >> 1;
			const size_t join2 = (join1 ^ (colorHash << 1)) >> 1;
			const size_t join3 = join2 ^ (texCoordHash << 1);
			
			return join3;
		}
	};
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Texture
{
	VkImage Image;
	VkDeviceMemory Memory;
	VkImageView View;
	VkSampler Sampler;
	uint32_t Width;
	uint32_t Height;
	uint32_t MipLevels;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Mesh
{
	size_t VertexCount = 0;
	size_t IndexCount = 0;
	VkBuffer VertexBuffer = nullptr;
	VkDeviceMemory VertexBufferMemory = nullptr;
	VkBuffer IndexBuffer = nullptr;
	VkDeviceMemory IndexBufferMemory = nullptr;
	AABB Bounds;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ModelInfo // for lack of a better name...
{
	VkDescriptorSet DescriptorSet;
	VkBuffer UniformBuffer;
	VkDeviceMemory UniformBufferMemory;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Model
{
	Mesh* Mesh = nullptr;
	Texture* Texture = nullptr;
	Transform Transform;

	// Array containing one per frame in flight
	std::vector<ModelInfo> Infos{};
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
		return _buffer.size() / _secPerFrameAccumulator;
	}

private:
	double _secPerFrameAccumulator = 0;
	std::vector<double> _buffer{};
	size_t _index = 0;
};

