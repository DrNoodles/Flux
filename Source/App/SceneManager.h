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

	
	std::vector<std::unique_ptr<Entity>>& GetEntities() { return _entities; }
	Camera& GetCamera() { return _camera; }

	
	RenderableComponent LoadRenderableComponentFromFile(const std::string& path);
	TextureResourceId LoadTexture(const std::string& path);
	const Material& GetMaterial(const RenderableResourceId& resourceId) const;
	void SetMaterial(const RenderableResourceId& renderableResId, const Material& newMat);

private:
	// Dependencies
	IModelLoaderService& _modelLoaderService;
	Renderer& _renderer;

	// Scene
	Camera _camera;
	std::vector<std::unique_ptr<Entity>> _entities{};
	


	// Track loaded textures to prevent loading the same texture more than once
	std::unordered_map<std::string, TextureResourceId> _loadedTextures{};

};
