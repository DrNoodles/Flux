#pragma once

#include "IModelLoaderService.h"
#include "AppTypes.h"

#include <vector>
#include <unordered_map>
#include <memory>



template <typename T>
struct ResourceId
{
	u32 Value;
	ResourceId(u32 id) : Value{ id } {}
	// TODO equality checks
};

struct ModelIdType;
struct MeshIdType;
struct TextureIdType;
struct ShaderIdType;

typedef ResourceId<ModelIdType> ModelResourceId;
typedef ResourceId<MeshIdType> MeshResourceId;
typedef ResourceId<TextureIdType> TextureResourceId;
typedef ResourceId<ShaderIdType> ShaderResourceId;



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
	
	RenderableComponent LoadRenderableComponentFromFile(const std::string& path);
	//u32 LoadTexture(const std::string& path);

	const std::vector<std::unique_ptr<ModelResource>>& GetModelResources() const { return _modelResources; }


private:
	// Dependencies
	const std::unique_ptr<IModelLoaderService> _modelLoaderService;

	// Data
	std::unordered_map<std::string, u32> _loadedTextures{};
	
	std::vector<std::unique_ptr<ModelResource>> _modelResources{};
	std::vector<std::unique_ptr<MeshResource>> _meshResources{};
	std::vector<std::unique_ptr<TextureResource>> _textureResources{};


	std::unique_ptr<MeshResource> CreateMeshResource(const MeshDefinition& meshDefinition) const;
	std::unique_ptr<TextureResource> CreateTextureResource(const std::string& path) const;
	std::vector<ModelInfoResource> CreateModelInfoResources(const ModelResource& model) const;

	u32 StoreMeshResource(std::unique_ptr<MeshResource> mesh);
	u32 StoreTextureResource(std::unique_ptr<TextureResource> texture);
	//u32 StoreShader(const std::string& vert, const std::string& frag);
};
