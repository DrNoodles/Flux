#pragma once

#include "Camera.h"
#include "Entity/Entity.h"

#include <Framework/IModelLoaderService.h> 
#include <Framework/CommonRenderer.h>

#include <unordered_map>

struct Material;
class RenderableComponent;

class ISceneManagerDelegate
{
public:
	virtual ~ISceneManagerDelegate() = default;
		
	virtual MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition) = 0;
	virtual RenderableResourceId CreateRenderable(const MeshResourceId& meshId) = 0;
	virtual TextureResourceId CreateTextureResource(const std::string& path) = 0;
	virtual IblTextureResourceIds CreateIblTextureResources(const std::string& path) = 0;
	virtual SkyboxResourceId CreateSkybox(const SkyboxCreateInfo& createInfo) = 0;
	virtual void SetSkybox(const SkyboxResourceId& resourceId) = 0;
};

// GPU loaded resources
class SceneManager
{
public:
	explicit SceneManager(ISceneManagerDelegate& delegate, IModelLoaderService& mls)
		: _delegate(delegate), _modelLoaderService(mls)
	{}
	
	Camera& GetCamera() { return _camera; }
	
	std::optional<RenderableComponent> LoadRenderableComponentFromFile(const std::string& path);
	std::optional<TextureResourceId> LoadTexture(const std::string& path);

	Material* CreateMaterial();
	Material* GetMaterial(MaterialId id) const;
	std::vector<Material*> GetMaterials() const;

	const std::vector<std::unique_ptr<Entity>>& EntitiesView() const { return _entities; }
	void AddEntity(std::unique_ptr<Entity> e) { _entities.emplace_back(std::move(e)); }
	void RemoveEntity(int entId);

	SkyboxResourceId LoadAndSetSkybox(const std::string& path);
	void SetSkybox(const SkyboxResourceId& id);
	SkyboxResourceId GetSkybox() const;

	RenderOptions GetRenderOptions() const { return _renderOptions; }
	void SetRenderOptions(const RenderOptions& ro) { _renderOptions = ro; }

private:
	// Dependencies
	ISceneManagerDelegate& _delegate;
	IModelLoaderService& _modelLoaderService;

	// Scene
	Camera _camera;
	std::vector<std::unique_ptr<Entity>> _entities{};
	SkyboxResourceId _skybox;
	RenderOptions _renderOptions;
	std::unordered_map<u32, std::unique_ptr<Material>> _materials{}; //TODO Make type id usable as hash key (convertable to u32?)

	// Cache
	std::unordered_map<std::string, SkyboxResourceId> _loadedSkyboxesCache = {};
	std::unordered_map<std::string, TextureResourceId> _loadedTexturesCache = {};
};
