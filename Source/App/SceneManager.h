#pragma once

#include "AppTypes.h"
#include "IModelLoaderService.h"
#include "Entity/RenderableComponent.h"

#include "Renderer/Renderer.h"

#include <unordered_map>


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
};
