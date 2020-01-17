#pragma once

#include "AppTypes.h"
#include "IModelLoaderService.h"
#include "Renderer/Renderer.h"

#include <unordered_map>
#include <memory>


// GPU loaded resources
class SceneManager
{
public:
	SceneManager(IModelLoaderService& modelLoaderService, Renderer& renderer)
		: _modelLoaderService(modelLoaderService), _renderer(renderer)
	{
	}
	
	RenderableComponent LoadRenderableComponentFromFile(const std::string& path);
	//u32 LoadTexture(const std::string& path);

private:
	// Dependencies
	IModelLoaderService& _modelLoaderService;
	Renderer& _renderer;

	// Track loaded textures to prevent loading the same texture more than once
	std::unordered_map<std::string, u32> _loadedTextures{};

	//std::unique_ptr<MeshResource> CreateMeshResource(const MeshDefinition& meshDefinition) const;
	//std::unique_ptr<TextureResource> CreateTextureResource(const std::string& path) const;
	//std::vector<ModelInfoResource> CreateModelInfoResources(const ModelResource& model) const;

//	u32 StoreMeshResource(std::unique_ptr<MeshResource> mesh);
//	u32 StoreTextureResource(std::unique_ptr<TextureResource> texture);
	//u32 StoreShader(const std::string& vert, const std::string& frag);
};
