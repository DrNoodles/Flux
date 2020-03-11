#pragma once

#include "Entity/Actions/TurntableActionComponent.h"
#include "Entity/Entity.h"
#include "Renderer/Renderer.h"

#include <utility>
#include <vector>
#include <memory>
#include <chrono>

struct SkyboxInfo
{
	std::string Path;
	std::string Dir;
	std::string Name;
};

class LibraryManager
{
public:
	
	
	LibraryManager(Renderer& resources, IModelLoaderService* modelLoaderService, std::string assetsDir)
		: _modelLoaderService{modelLoaderService}, _resources{resources}, _libraryDir{std::move(assetsDir)}
	{
		srand(unsigned(std::chrono::system_clock::now().time_since_epoch().count()));

		for (auto& filename : _skyboxFilenames)
		{
			SkyboxInfo info = {};
			info.Dir = _libraryDir + "IBL/";
			info.Name = filename;
			info.Path = info.Dir + filename;
			_skyboxInfos.emplace_back(std::move(info));
		}
	}


	const std::vector<SkyboxInfo>& GetSkyboxes() const
	{
		return _skyboxInfos;
	}
	
	
	//TODO clean up resources on exit
	
	std::unique_ptr<Entity> CreateSphere()
	{
		if (!_spherePrimitiveLoaded)
		{
			auto modelDefinition = _modelLoaderService->LoadModel(_libraryDir + "Models/Sphere/Sphere.obj");
			auto& meshDefinition = modelDefinition.value().Meshes[0];

			_sphereModelId = _resources.CreateMeshResource(meshDefinition);
			_spherePrimitiveLoaded = true;
		}

		return CreateEntity(_sphereModelId, "Sphere");
	}

	std::unique_ptr<Entity> CreateCube()
	{
		if (!_cubePrimitiveLoaded)
		{
			auto modelDefinition = _modelLoaderService->LoadModel(_libraryDir + "Models/Cube/Cube.obj");
			auto& meshDefinition = modelDefinition.value().Meshes[0];
			
			_cubeModelId = _resources.CreateMeshResource(meshDefinition);
			_cubePrimitiveLoaded = true;
		}

		return CreateEntity(_cubeModelId, "Cube");
	}

	std::unique_ptr<Entity> CreateBlob()
	{
		if (!_blobPrimitiveLoaded)
		{
			auto modelDefinition = _modelLoaderService->LoadModel(_libraryDir + "Models/Blob/Blob.obj");
			auto& meshDefinition = modelDefinition.value().Meshes[0];
			
			_blobModelId = _resources.CreateMeshResource(meshDefinition);
			_blobPrimitiveLoaded = true;
		}

		return CreateEntity(_blobModelId, "Blob");
	}
	
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
	
private:
	// Dependencies
	IModelLoaderService* _modelLoaderService;
	Renderer& _resources; // TODO Replace Renderer inclusion once a gpu resource service exists (that the renderer relies on also)
	const std::string _libraryDir;

	bool _spherePrimitiveLoaded = false;
	bool _cubePrimitiveLoaded = false;
	bool _blobPrimitiveLoaded = false;
	MeshResourceId _sphereModelId = 0;
	MeshResourceId _cubeModelId = 0;
	MeshResourceId _blobModelId = 0;

	std::vector<SkyboxInfo> _skyboxInfos = {};
	const std::vector<std::string> _skyboxFilenames =
	{
		"ChiricahuaPath.hdr",
		"DitchRiver.hdr",
		"debug/equirectangular.hdr",
	};
	
	std::unique_ptr<Entity> CreateEntity(const MeshResourceId& meshId, const std::string& name) const
	{
		// Create renderable resource
		RenderableCreateInfo info = {};
		info.MeshId = meshId;
		info.Mat = Material();// CreateRandomMaterial();
		const RenderableResourceId resourceId = _resources.CreateRenderable(info);

		// Create renderable component
		RenderableComponent comp = {};
		comp.RenderableId = resourceId;

		// Create entity
		auto entity = std::make_unique<Entity>();
		entity->Name = name + std::to_string(entity->Id);
		entity->Renderable = std::make_optional(comp);
		return entity;
	}
};
