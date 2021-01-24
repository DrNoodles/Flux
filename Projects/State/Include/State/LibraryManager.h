#pragma once

#include "SceneManager.h"
#include "Entity/Entity.h"
#include "Entity/Actions/TurntableActionComponent.h"

#include <Framework/CommonTypes.h>
#include <Framework/IModelLoaderService.h>

#include <chrono>
#include <memory>
#include <utility>
#include <vector>


struct SkyboxInfo
{
	std::string Path;
	std::string Dir;
	std::string Name;
};

class ILibraryManagerDelegate
{
public:
	virtual ~ILibraryManagerDelegate() = default;
	virtual RenderableResourceId CreateRenderable(const MeshResourceId& meshId) = 0;
	virtual MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition) = 0;
};

class LibraryManager final
{
public:
	
	LibraryManager(ILibraryManagerDelegate& del, SceneManager& scene, IModelLoaderService& mls, std::string assetsDir)
		: _delegate{del}, _scene{scene}, _modelLoaderService{mls}, _libraryDir{std::move(assetsDir)}
	{
		srand(unsigned(std::chrono::system_clock::now().time_since_epoch().count()));

		for (const auto& filename : _skyboxFilenames)
		{
			SkyboxInfo info = {};
			info.Dir = _libraryDir + "IBL/";
			info.Name = filename;
			info.Path = info.Dir + filename;
			_skyboxInfos.emplace_back(std::move(info));
		}
	}
	//TODO ensure resources are cleaned up on exit


	const std::vector<SkyboxInfo>& GetSkyboxes() const
	{
		return _skyboxInfos;
	}


	std::unique_ptr<Entity> CreateSphere(MaterialId matId)
	{
		if (!_sphere.has_value())
		{
			auto modelDefinition = _modelLoaderService.LoadModel(_libraryDir + "Models/Sphere/Sphere.obj");
			auto& meshDefinition = modelDefinition.value().Meshes[0];

			LoadedMesh prim;
			prim.Id = _delegate.CreateMeshResource(meshDefinition);
			prim.Bounds = meshDefinition.Bounds;

			_sphere = prim;
		}

		return CreateEntity(_sphere->Id, matId, _sphere->Bounds, "Sphere");
	}

	std::unique_ptr<Entity> CreateCube(MaterialId matId)
	{
		if (!_cube.has_value())
		{
			auto modelDefinition = _modelLoaderService.LoadModel(_libraryDir + "Models/Cube/Cube.obj");
			auto& meshDefinition = modelDefinition.value().Meshes[0];
			
			LoadedMesh prim;
			prim.Id = _delegate.CreateMeshResource(meshDefinition);
			prim.Bounds = meshDefinition.Bounds;

			_cube = prim;
		}

		return CreateEntity(_cube->Id, matId, _cube->Bounds, "Cube");
	}

	std::unique_ptr<Entity> CreateBlob(MaterialId matId)
	{
		if (!_blob.has_value())
		{
			auto modelDefinition = _modelLoaderService.LoadModel(_libraryDir + "Models/Blob/Blob.obj");
			auto& meshDefinition = modelDefinition.value().Meshes[0];

			LoadedMesh prim;
			prim.Id = _delegate.CreateMeshResource(meshDefinition);
			prim.Bounds = meshDefinition.Bounds;

			_blob = prim;
		}

		return CreateEntity(_blob->Id, matId, _blob->Bounds, "Blob");
	}

	
	void LoadEmptyScene() const
	{
		_scene.LoadAndSetSkybox(GetSkyboxes()[0].Path);
	}
	
	void LoadDefaultScene()
	{
		_scene.LoadAndSetSkybox(GetSkyboxes()[0].Path);
		LoadObjectArray();

		// TEMP: Add directional light to help test shadowmaps
		{
			auto entity = std::make_unique<Entity>();
			entity->Name = "DirectionalLight" + std::to_string(entity->Id);
			entity->Light = LightComponent{};
			entity->Light->Type = LightComponent::Types::directional;
			entity->Light->Intensity = 5;
			entity->Transform.SetPos({10, 10, 10});
			_scene.AddEntity(std::move(entity));
		}
		

		// TEMP: Add a ground plane to catch shadows
		{
			Material* mat = _scene.CreateMaterial();
			
			auto x = CreateCube(mat->Id);
			x->Transform.SetScale({7.5, 2, 7.5});
			x->Transform.SetPos({0,-4,0});
			_scene.AddEntity(std::move(x));
		}
	}

	void LoadDemoScene()
	{
		std::cout << "Loading scene\n";

		LoadMaterialExamples();
		LoadGrapple();
		
		_scene.LoadAndSetSkybox(GetSkyboxes()[0].Path);
		
		auto ro = _scene.GetRenderOptions();
		ro.IblStrength = 2.5f;
		ro.SkyboxRotation = 290;
		ro.BackdropBrightness = 0.3f;
		_scene.SetRenderOptions(ro);
	}

	void LoadDemoSceneHeavy()
	{
		std::cout << "Loading scene\n";
		_scene.LoadAndSetSkybox(GetSkyboxes()[0].Path);
		LoadObjectArray({ 0,0,0 }, 30, 30);
	}

	void LoadObjectArray(const glm::vec3& offset = glm::vec3{ 0,0,0 }, u32 numRows = 2, u32 numColumns = 5)
	{
		std::cout << "Loading material array" << std::endl;

		const glm::vec3 center = offset;
		const auto rowSpacing = 2.1f;
		const auto colSpacing = 2.1f;

		u32 count = 0;
		
		for (u32 row = 0; row < numRows; row++)
		{
			const f32 metalness = row / f32(numRows - 1);

			const f32 height = f32(numRows - 1) * rowSpacing;
			const f32 hStart = height / 2.f;
			const f32 y = center.y + hStart + -rowSpacing * row;

			for (u32 col = 0; col < numColumns; col++)
			{
				const f32 roughness = col / f32(numColumns - 1);

				const f32 width = f32(numColumns - 1) * colSpacing;
				const f32 wStart = -width / 2.f;
				const f32 x = center.x + wStart + colSpacing * col;

				char name[256];
				sprintf_s(name, 256, "Obj M:%.2f R:%.2f", metalness, roughness);

				// Config Material
				Material& mat = *_scene.CreateMaterial();
				mat.Name = name;
				mat.Basecolor = glm::vec3{ 1 };
				mat.Roughness = roughness;
				mat.Metalness = metalness;

				// Create Entity
				auto entity = CreateBlob(mat.Id);
				entity->Name = name;
				entity->Transform.SetPos({ x,y,0.f });
				entity->Action = std::make_unique<TurntableAction>(entity->Transform);
				
				_scene.AddEntity(std::move(entity));

				++count;
			}
		}

		std::cout << "Material array obj count: " << count << std::endl; 
	}

	void LoadMaterialExamples()
	{
		std::cout << "Loading material example" << std::endl;

		std::string name;
		std::string basecolorPath;
		std::string normalPath;
		std::string ormPath;

		auto CreateMaterial = [&]() -> MaterialId
		{
			Material& mat = *_scene.CreateMaterial();
			mat.Name = name;
			
			// Load basecolor map
			mat.BasecolorMap = { *_scene.LoadTexture(basecolorPath), basecolorPath };
			mat.UseBasecolorMap = true;

			// Load normal map
			mat.NormalMap = { *_scene.LoadTexture(normalPath), normalPath };

			// Load occlusion map
			mat.AoMap = { *_scene.LoadTexture(ormPath), ormPath };
			mat.AoMapChannel = Material::Channel::Red;
			
			// Load roughness map
			mat.RoughnessMap = { *_scene.LoadTexture(ormPath), ormPath };
			mat.UseRoughnessMap = true;
			mat.RoughnessMapChannel = Material::Channel::Green;

			// Load metalness map
			mat.MetalnessMap = { *_scene.LoadTexture(ormPath), ormPath };
			mat.UseMetalnessMap = true;
			mat.MetalnessMapChannel = Material::Channel::Blue;

			return mat.Id;
		};
		
		{
			name = "Sphere";

			basecolorPath = _libraryDir + "Materials/ScuffedAluminum/BaseColor.png";
			ormPath = _libraryDir + "Materials/ScuffedAluminum/ORM.png";
			normalPath = _libraryDir + "Materials/ScuffedAluminum/Normal.png";
			auto matId = CreateMaterial();
			
			auto entity = CreateSphere(matId);
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(1));
			entity->Transform.SetPos(glm::vec3{ 0, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);
			_scene.AddEntity(std::move(entity));
		}
		
		{
			name = "GreasyPan";

			basecolorPath = _libraryDir + "Materials/GreasyPan/BaseColor.png";
			ormPath = _libraryDir + "Materials/GreasyPan/ORM.png";
			normalPath = _libraryDir + "Materials/GreasyPan/Normal.png";
			auto matId = CreateMaterial();
		
			auto entity = CreateBlob(matId);
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(.8f));
			entity->Transform.SetPos(glm::vec3{ 2, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);
			_scene.AddEntity(std::move(entity));
		}

		{
			name = "Gold";

			basecolorPath = _libraryDir + "Materials/GoldScuffed/BaseColor.png";
			ormPath = _libraryDir + "Materials/GoldScuffed/ORM.png";
			normalPath = _libraryDir + "Materials/GoldScuffed/Normal.png";
			auto matId = CreateMaterial();
			
			auto entity = CreateBlob(matId);
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(.8f));
			entity->Transform.SetPos(glm::vec3{ 4, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);
			_scene.AddEntity(std::move(entity));
		}

		{
			name = "Rusted Metal";
			
			basecolorPath = _libraryDir + "Materials/RustedMetal/BaseColor.png";
			ormPath = _libraryDir + "Materials/RustedMetal/ORM.png";
			normalPath = _libraryDir + "Materials/RustedMetal/Normal.png";
			auto matId = CreateMaterial();

			auto entity = CreateBlob(matId);
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(.8f));
			entity->Transform.SetPos(glm::vec3{ -2, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);
			_scene.AddEntity(std::move(entity));
		}

		{
			name = "Bumpy Plastic";
			
			basecolorPath = _libraryDir + "Materials/BumpyPlastic/BaseColor.png";
			ormPath = _libraryDir + "Materials/BumpyPlastic/ORM.png";
			normalPath = _libraryDir + "Materials/BumpyPlastic/Normal.png";
			auto matId = CreateMaterial();

			auto entity = CreateBlob(matId);
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(.8f));
			entity->Transform.SetPos(glm::vec3{ -4, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);
			_scene.AddEntity(std::move(entity));
		}
	}

	void LoadGrapple()
	{
		const auto path = _libraryDir + "Models/" + "grapple/export/grapple.gltf";
		std::cout << "Loading model:" << path << std::endl;


		// Load renderable
		auto renderableComponent = _scene.LoadRenderableComponentFromFile(path);
		if (!renderableComponent.has_value())
		{
			throw std::invalid_argument("Couldn't load model"); // Throwing here cuz this is a bug, not user data error
		}

		auto entity = std::make_unique<Entity>();
		entity->Name = "GrappleHook";
		entity->Transform.SetPos(glm::vec3{ 0, -3, 0 });
		entity->Transform.SetRot(glm::vec3{ 0, 30, 0 });
		entity->Renderable = std::move(renderableComponent);
		//entity->Action = std::make_unique<TurntableAction>(entity->Transform);

		RenderableComponentSubmesh* pSubmesh = nullptr;
		MaterialId matId;
		std::string basecolorPath;
		std::string normalPath;
		std::string ormPath;
		std::string emissivePath;

		auto ApplyMat = [&]()
		{
			auto GetOptionalTexture = [&](const std::string& texturePath) -> std::optional<Material::Map>
			{
				auto optRes = _scene.LoadTexture(texturePath);
				return optRes ? std::optional(Material::Map{optRes.value(), texturePath}) : std::nullopt;
			};

			Material& mat = *_scene.GetMaterial(matId);
			
			// Load basecolor map
			mat.BasecolorMap = GetOptionalTexture(basecolorPath);
			mat.UseBasecolorMap = mat.BasecolorMap.has_value();

			// Load normal map
			mat.NormalMap = { *_scene.LoadTexture(normalPath), normalPath };

			// Load occlusion map
			mat.AoMap = GetOptionalTexture(ormPath);
			mat.AoMapChannel = Material::Channel::Red;
			
			// Load roughness map
			mat.RoughnessMap = GetOptionalTexture(ormPath);
			mat.UseRoughnessMap = mat.RoughnessMap.has_value();
			mat.RoughnessMapChannel = Material::Channel::Green;

			// Load metalness map
			mat.MetalnessMap = GetOptionalTexture(ormPath);
			mat.UseMetalnessMap = mat.MetalnessMap.has_value();
			mat.MetalnessMapChannel = Material::Channel::Blue;

			// Load emissive map
			mat.EmissiveMap = GetOptionalTexture(emissivePath);
			mat.EmissiveIntensity = 5;

			pSubmesh->AssignMaterial(mat.Id);
		};

		
		// Add maps to material
		{
			// Barrel
			{
				pSubmesh = &entity->Renderable->GetSubmeshes()[0];
				matId = pSubmesh->MatId;
				basecolorPath = _libraryDir + "Models/" + "grapple/export/Barrel_Basecolor.png";
				normalPath = _libraryDir + "Models/" + "grapple/export/Barrel_Normal.png";
				ormPath = _libraryDir + "Models/" + "grapple/export/Barrel_ORM.png";
				emissivePath = _libraryDir + "Models/" + "grapple/export/Barrel_Emissive.png";
				ApplyMat();
			}
			// Hook
			{
				pSubmesh = &entity->Renderable->GetSubmeshes()[1];
				matId = pSubmesh->MatId;
				basecolorPath = _libraryDir + "Models/" + "grapple/export/Hook_Basecolor.png";
				normalPath = _libraryDir + "Models/" + "grapple/export/Hook_Normal.png";
				ormPath = _libraryDir + "Models/" + "grapple/export/Hook_ORM.png";
				emissivePath = "";
				ApplyMat();
			}
			// Stock
			{
				pSubmesh = &entity->Renderable->GetSubmeshes()[2];
				matId = pSubmesh->MatId;
				basecolorPath = _libraryDir + "Models/" + "grapple/export/Stock_Basecolor.png";
				normalPath = _libraryDir + "Models/" + "grapple/export/Stock_Normal.png";
				ormPath = _libraryDir + "Models/" + "grapple/export/Stock_ORM.png";
				emissivePath = _libraryDir + "Models/" + "grapple/export/Stock_Emissive.png";
				ApplyMat();
			}
		}

		_scene.AddEntity(std::move(entity));
	}

	MaterialId CreateRandomDielectricMaterial() const
	{
		Material& m = *_scene.CreateMaterial();
		m.Name = "RandomDielectricMaterial_" + std::to_string(RandF(0,1));
		m.Roughness = RandF(0.f, 0.7f);
		m.Metalness = 0;
		m.Basecolor = glm::vec3{ RandF(0.15f,0.95f),RandF(0.15f,0.95f),RandF(0.15f,0.95f) };
		return m.Id;
	}
	
	MaterialId CreateRandomMetalMaterial() const
	{
		Material& m = *_scene.CreateMaterial();
		m.Name = "CreateRandomMetalMaterial_" + std::to_string(RandF(0,1));
		m.Roughness = RandF(0.f, 0.7f);
		m.Metalness = 1;
		m.Basecolor = glm::vec3{ RandF(0.70f,1.f),RandF(0.70f,1.f),RandF(0.70f,1.f) };
		return m.Id;
	}
	
	MaterialId CreateRandomMaterial() const
	{
		const auto isMetallic = bool(rand() % 2);
		return isMetallic ? CreateRandomMetalMaterial() : CreateRandomDielectricMaterial();
	}

	
private:
	struct LoadedMesh
	{
		MeshResourceId Id = {};
		AABB Bounds = {};
	};
	
	// Dependencies
	ILibraryManagerDelegate& _delegate;
	SceneManager& _scene;
	IModelLoaderService& _modelLoaderService;
	
	const std::string _libraryDir;

	std::optional<LoadedMesh> _cube = {};
	std::optional<LoadedMesh> _sphere = {};
	std::optional<LoadedMesh> _blob = {};


	std::vector<SkyboxInfo> _skyboxInfos = {};
	const std::vector<std::string> _skyboxFilenames =
	{
		"ChiricahuaPath.hdr",
		"DitchRiver.hdr",
		"debug/equirectangular.hdr",
	};

	std::unique_ptr<Entity> CreateEntity(const MeshResourceId& meshId, MaterialId matId, const AABB& bounds, const std::string& name) const
	{
		const auto renderableResId = _delegate.CreateRenderable(meshId);
		
		// Create renderable component
		const RenderableComponentSubmesh submesh = { renderableResId, name, _scene.GetMaterial(matId)->Id };
		RenderableComponent comp{ submesh, bounds };

		// Create entity
		auto entity = std::make_unique<Entity>();
		entity->Name = name + std::to_string(entity->Id);
		entity->Renderable = std::make_optional(comp);
		return entity;
	}

	// todo find a better home for this
	static float RandF(float min, float max) 
	{
		const float r = float(rand()) / float(RAND_MAX);
		const float range = max - min;
		return min + r * range;
	}
};
