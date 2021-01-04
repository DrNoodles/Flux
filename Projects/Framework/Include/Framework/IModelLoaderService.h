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

struct MeshDefinition
{
	std::string MeshName{};
	std::string MaterialName{};
	std::vector<Vertex> Vertices{};
	std::vector<u32> Indices{};
	std::vector<TextureDefinition> Textures{};
	AABB Bounds{};
};

struct ModelDefinition
{
	std::vector<MeshDefinition> Meshes{};
};

class IModelLoaderService
{
public:
	virtual ~IModelLoaderService() = default;
	virtual std::optional<ModelDefinition> LoadModel(const std::string& path) = 0;
};
