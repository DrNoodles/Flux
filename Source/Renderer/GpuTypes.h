#pragma once

#include "Shared/CommonTypes.h"
#include "Shared/AABB.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL // for hash
#include <glm/gtx/hash.hpp>

#include <array>
#include <optional>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct ResourceId
{
	u32 Id = u32_max;
	ResourceId() = default;
	ResourceId(u32 id) : Id{ id } {}
	//bool IsValid() const { return Id != u32_max; }
	// TODO equality checks
};

struct SkyboxIdType;
struct RenderableIdType;
struct MeshIdType;
struct TextureIdType;
//struct ShaderIdType;
typedef ResourceId<SkyboxIdType> SkyboxResourceId;
typedef ResourceId<RenderableIdType> RenderableResourceId;
typedef ResourceId<MeshIdType> MeshResourceId;
typedef ResourceId<TextureIdType> TextureResourceId;
//typedef ResourceId<ShaderIdType> ShaderResourceId;

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
struct Vertex
{
	glm::vec3 Pos;
	glm::vec3 Normal;
	glm::vec3 Color;
	glm::vec2 TexCoord;
	glm::vec3 Tangent;

	bool operator==(const Vertex& other) const
	{
		return Pos == other.Pos &&
			Normal == other.Normal &&
			Color == other.Color &&
			TexCoord == other.TexCoord &&
			Tangent == other.Tangent;
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
			const size_t tangentCoordHash = hash<glm::vec2>()(vertex.Tangent);
			//	const size_t bitangentCoordHash = hash<glm::vec2>()(vertex.Bitangent);

			const size_t join1 = (posHash ^ (normalHash << 1)) >> 1;
			const size_t join2 = (join1 ^ (colorHash << 1)) >> 1;
			const size_t join3 = (join2 ^ (texCoordHash << 1)) >> 1;
			const size_t join4 = (join3 ^ (tangentCoordHash << 1));// >> 1;
		//	const size_t join5 = (join4 ^ (bitangentCoordHash << 1));

			return join4;
		}
	};
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
	AABB Bounds;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SkyboxResourceFrame // for lack of a better name...
{
	VkDescriptorSet DescriptorSet;
	VkBuffer VertUniformBuffer;
	VkDeviceMemory VertUniformBufferMemory;
	//VkBuffer FragUniformBuffer;
	//VkDeviceMemory VertUniformBufferMemory;
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
