#pragma once

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL // for hash
#include <glm/gtx/hash.hpp>

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

