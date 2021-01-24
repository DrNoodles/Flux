#pragma once

#include <Framework/AABB.h>
#include <Framework/Material.h>
#include <Framework/Vertex.h>
#include <Framework/CommonTypes.h>

#include <vector>
#include <string>
#include <optional>

struct TextureDefinition
{
	TextureType Type{};
	std::string Path{};
};

struct MaterialDefinition
{
	std::string Name{};
	std::vector<TextureDefinition> Textures{};
};

struct MeshDefinition
{
	static const u32 InvalidMaterialIndex = 0xFFFFFFFF;
	
	std::string Name{};
	std::vector<Vertex> Vertices{};
	std::vector<u32> Indices{};
	AABB Bounds{};
	u32 MaterialIndex = InvalidMaterialIndex;
};

struct ModelDefinition
{
	std::vector<MeshDefinition> Meshes{};
	std::vector<MaterialDefinition> Materials{};
};

class IModelLoaderService
{
public:
	virtual ~IModelLoaderService() = default;
	virtual std::optional<ModelDefinition> LoadModel(const std::string& path) = 0;
};
