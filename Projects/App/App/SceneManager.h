#pragma once

#include "IModelLoaderService.h"
#include "Entity/Entity.h"

#include <Renderer/Renderer.h>

#include <unordered_map>
#include "Camera.h"

class RenderableComponent;

// GPU loaded resources
class SceneManager
{
public:
	SceneManager(IModelLoaderService& modelLoaderService, Renderer& renderer)
		: _modelLoaderService(modelLoaderService), _renderer(renderer)
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
		auto iterator = std::find_if(_entities.begin(), _entities.end(), [entId](std::unique_ptr<Entity>& e)
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
	IModelLoaderService& _modelLoaderService;
	Renderer& _renderer;

	// Scene
	Camera _camera;
	std::vector<std::unique_ptr<Entity>> _entities{};
	SkyboxResourceId _skybox;

	// Cache
	std::unordered_map<std::string, SkyboxResourceId> _loadedSkyboxesCache = {};
	std::unordered_map<std::string, TextureResourceId> _loadedTexturesCache = {};
};
