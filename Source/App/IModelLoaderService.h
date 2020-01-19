#pragma once

#include "AppTypes.h"

#include "Renderer/GpuTypes.h" // Vertex

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
	virtual std::optional<ModelDefinition> LoadModel(const std::string& path) = 0;
};
