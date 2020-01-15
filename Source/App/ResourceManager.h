#pragma once

#include "IModelLoaderService.h"
#include "AppTypes.h"

#include <vector>
#include <unordered_map>
#include <memory>




// GPU loaded resources
class ResourceManager
{
public:
	explicit ResourceManager(
		std::unique_ptr<IModelLoaderService>&& modelLoaderService)
		: _modelLoaderService(std::move(modelLoaderService))
	{
	}
	
		// TODO Make these shared_ptr for auto unloading!
	// TODO Protect these behind read only getters
	//std::vector<std::unique_ptr<ShaderResource>> Shaders{};
	
	RenderableComponent LoadRenderableFromFile(const std::string& path);
	std::tuple<u32, u32, u32, u32, u32> LoadEnvironmentMap(const std::string& path, const std::string& shadersDir);
	u32 LoadTexture(const std::string& path);

	
private:
	// Dependencies
	const std::unique_ptr<IModelLoaderService> _modelLoaderService;

	// Data
	std::unordered_map<std::string, u32> _loadedTextures{};
	
	std::vector<std::unique_ptr<MeshResource>> _meshResources{};
	std::vector<std::unique_ptr<TextureResource>> _textureResources{};


	std::unique_ptr<TextureResource> LoadTextureResource(const std::string& path) const;
	std::unique_ptr<MeshResource> LoadMeshResource(const ModelDefinition& modelDefinition) const;
	std::vector<ModelInfoResource> CreateModelInfoResources(const Model& model) const;

	
	u32 StoreTexture(std::unique_ptr<TextureResource> t);
	u32 StoreMesh(std::unique_ptr<MeshResource> mesh);
	u32 StoreShader(const std::string& vert, const std::string& frag);
};
