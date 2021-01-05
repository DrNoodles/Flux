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
	virtual RenderableResourceId CreateRenderable(const MeshResourceId& meshId, const Material& mat) = 0;
	virtual TextureResourceId CreateTextureResource(const std::string& path) = 0;
	virtual const Material& GetMaterial(const RenderableResourceId& id) = 0;
	virtual void SetMaterial(const RenderableResourceId& id, const Material& newMat) = 0;
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

	
	//std::vector<std::unique_ptr<Entity>>& GetEntities() { return _entities; }
	Camera& GetCamera() { return _camera; }

	
	std::optional<RenderableComponent> LoadRenderableComponentFromFile(const std::string& path);
	std::optional<TextureResourceId> LoadTexture(const std::string& path);

	//std::vector<Material> GatherAllMaterials() const;
	const Material& GetMaterial(const RenderableResourceId& resourceId) const;
	void SetMaterial(const RenderableComponent& renderableComp, const Material& newMat) const;
	void SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat) const;

	const std::vector<std::unique_ptr<Entity>>& EntitiesView() const
	{
		return _entities;
	}
	void AddEntity(std::unique_ptr<Entity> e)
	{
		_entities.emplace_back(std::move(e));
	}
	void RemoveEntity(int entId)
	{
		// Find item
		const auto iterator = std::find_if(_entities.begin(), _entities.end(), [entId](std::unique_ptr<Entity>& e)
		{
			return entId == e->Id;
		});

		if (iterator == _entities.end())
		{
			assert(false); // trying to erase bogus entId
			return;
		}

		
		// Cleanup entity references in app
		Entity* e = iterator->get();
		if (e->Renderable.has_value())
		{
			auto& r = e->Renderable.value();
			// TODO Clean up rendereable shit
		}

		
		_entities.erase(iterator);
	}

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

	// Cache
	std::unordered_map<std::string, SkyboxResourceId> _loadedSkyboxesCache = {};
	std::unordered_map<std::string, TextureResourceId> _loadedTexturesCache = {};
};
