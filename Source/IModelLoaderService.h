#pragma once
#include <vector>
#include <string>

#include "Types.h"

enum class TextureType : char
{
	Undefined = 0,
	BaseColor,
	Metalness,
	Roughness,
	AmbientOcclusion,
	Normals,
};

struct TextureDefinition
{
	TextureType Type{};
	std::string Path{};
};

struct MeshDefinition
{
	std::string Name{};
	std::vector<Vertex> Vertices{};
	std::vector<u32> Indices{};
	std::vector<TextureDefinition> Textures{};
};

struct ModelDefinition
{
	std::vector<MeshDefinition> Meshes{};
};

class IModelLoaderService
{
public:
	virtual ~IModelLoaderService() = default;
	virtual ModelDefinition LoadModel(const std::string& path) = 0;
};
