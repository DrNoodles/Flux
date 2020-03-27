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
	
	virtual std::optional<ModelDefinition> LoadModel(const std::string& path) = 0;
	
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
	SceneManager(ISceneManagerDelegate* delegate)
		: _delegate(delegate)
	{}

	
	//std::vector<std::unique_ptr<Entity>>& GetEntities() { return _entities; }
	Camera& GetCamera() { return _camera; }

	
	RenderableComponent LoadRenderableComponentFromFile(const std::string& path);
	TextureResourceId LoadTexture(const std::string& path);
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

	SkyboxResourceId LoadSkybox(const std::string& path);
	void SetSkybox(const SkyboxResourceId& id);
	SkyboxResourceId GetSkybox() const;


private:
	// Dependencies
	ISceneManagerDelegate* _delegate = nullptr;

	// Scene
	Camera _camera;
	std::vector<std::unique_ptr<Entity>> _entities{};
	SkyboxResourceId _skybox;

	// Cache
	std::unordered_map<std::string, SkyboxResourceId> _loadedSkyboxesCache = {};
	std::unordered_map<std::string, TextureResourceId> _loadedTexturesCache = {};
};
