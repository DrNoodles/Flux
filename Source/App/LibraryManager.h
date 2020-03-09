#pragma once

#include "Entity/Actions/TurntableActionComponent.h"
#include "Entity/Entity.h"
#include "Renderer/Renderer.h"
#include "SceneManager.h"

#include <utility>
#include <vector>
#include <memory>
#include <cmath>
#include <chrono>

class LibraryManager
{
public:
	LibraryManager(Renderer& resources, IModelLoaderService* modelLoaderService, std::string assetsDir)
		: _modelLoaderService{modelLoaderService}, _resources{resources}, _libraryDir{std::move(assetsDir)}
	{
		srand(unsigned(std::chrono::system_clock::now().time_since_epoch().count()));
	}
	
	//TODO clean up resources on exit

	/*
	std::unique_ptr<Entity> CreateSphere()
	{
		if (!_spherePrimitiveLoaded)
		{
			_sphereModelId = _resources.LoadRenderableFromFile(_modelsDir + "Sphere/Sphere.obj").Meshes[0].MeshId;
			_spherePrimitiveLoaded = true;
		}

		auto entity = std::make_unique<Entity>();
		entity->Name = "Sphere" + std::to_string(entity->Id);
		entity->Renderable = Renderable{ RenderableMesh{_sphereModelId,CreateRandomMaterial()} };
		return entity;
	}*/

	//std::unique_ptr<Entity> CreateCube()
	//{
	//	if (!_cubePrimitiveLoaded)
	//	{
	//		_cubeModelId = _resources.StoreMesh(std::make_unique<CubeMesh>());
	//		_cubePrimitiveLoaded = true;
	//	}

	//	auto entity = std::make_unique<Entity>();
	//	entity->Name = "Cube" + std::to_string(entity->Id);
	//	entity->Renderable = Renderable{ RenderableMesh{_cubeModelId,CreateRandomMaterial()} };
	//	return entity;
	//}

	std::unique_ptr<Entity> CreateBlob()
	{
		if (!_blobPrimitiveLoaded)
		{
			auto model = _modelLoaderService->LoadModel(_libraryDir + "Models/Blob/Blob.obj");
			auto& meshDefinition = model.value().Meshes[0];
			_blobModelId = _resources.CreateMeshResource(meshDefinition);
		}

		// Create renderable resource
		RenderableCreateInfo info = {};
		info.MeshId = _blobModelId;
		info.Mat = CreateRandomMaterial();
		RenderableResourceId resourceId = _resources.CreateRenderable(info);

		// Create renderable component
		RenderableComponent comp = {};
		comp.RenderableId = resourceId;

		// Create entity
		auto entity = std::make_unique<Entity>();
		entity->Name = "Blob" + std::to_string(entity->Id);
		entity->Renderable = std::make_optional(comp);
		return entity;
	}

private:
	// Dependencies
	IModelLoaderService* _modelLoaderService;
	Renderer& _resources; // TODO Replace Renderer inclusion once a gpu resource service exists (that the renderer relies on also)
	const std::string _libraryDir;

	//bool _spherePrimitiveLoaded = false;
	//bool _cubePrimitiveLoaded = false;
	//unsigned _sphereModelId = 0;
	//unsigned _cubeModelId = 0;
	bool _blobPrimitiveLoaded = false;
	MeshResourceId _blobModelId = 0;
	

	static Material CreateRandomMaterial()
	{
		const auto RandF = [](float min, float max)
		{
			const float r = float(rand()) / float(RAND_MAX);
			const float range = max - min;
			return min + r * range;
		};

		const auto isMetallic = bool(rand() % 2);

		Material m{};
		m.Roughness = RandF(0.f, 0.7f);
		if (isMetallic)
		{
			m.Metalness = true;
			m.Basecolor = glm::vec3{ RandF(0.70f,1.f),RandF(0.70f,1.f),RandF(0.70f,1.f) };
		}
		else
		{
			m.Metalness = false;
			m.Basecolor = glm::vec3{ RandF(0.15f,0.95f),RandF(0.15f,0.95f),RandF(0.15f,0.95f) };
		}

		return m;
	}
};
