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
	virtual RenderableResourceId CreateRenderable(const MeshResourceId& meshId, const Material& material) = 0;
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


	std::unique_ptr<Entity> CreateSphere()
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

		return CreateEntity(_sphere->Id, _sphere->Bounds, "Sphere");
	}

	std::unique_ptr<Entity> CreateCube()
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

		return CreateEntity(_cube->Id, _cube->Bounds, "Cube");
	}

	std::unique_ptr<Entity> CreateBlob()
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

		return CreateEntity(_blob->Id, _blob->Bounds, "Blob");
	}

	
	void LoadEmptyScene() const
	{
		_scene.LoadAndSetSkybox(GetSkyboxes()[0].Path);
	}
	
	void LoadDefaultScene()
	{
		_scene.LoadAndSetSkybox(GetSkyboxes()[0].Path);
		//LoadObjectArray();

		// TEMP: Add directional light to help test shadowmaps
		//{
		//	auto entity = std::make_unique<Entity>();
		//	entity->Name = "DirectionalLight" + std::to_string(entity->Id);
		//	entity->Light = LightComponent{};
		//	entity->Light->Type = LightComponent::Types::directional;
		//	entity->Light->Intensity = 5;
		//	entity->Transform.SetPos({10, 10, 10});
		//	_scene.AddEntity(std::move(entity));
		//}
		//

		//// TEMP: Add a ground plane to catch shadows
		//{
		//	auto x = CreateCube();
		//	x->Transform.SetScale({7.5, 2, 7.5});
		//	x->Transform.SetPos({0,-4,0});
		//	_scene.AddEntity(std::move(x));
		//}
	}

	void LoadDemoScene()
	{
		std::cout << "Loading scene\n";

		//LoadMaterialExamples();
		//LoadGrapple();
		
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
		//LoadAxis();
		//LoadObjectArray({ 0,0,0 }, 30, 30);
	}

	/*void LoadObjectArray(const glm::vec3& offset = glm::vec3{ 0,0,0 }, u32 numRows = 2, u32 numColumns = 5)
	{
		std::cout << "Loading material array" << std::endl;

		glm::vec3 center = offset;
		auto rowSpacing = 2.1f;
		auto colSpacing = 2.1f;

		u32 count = 0;
		
		for (u32 row = 0; row < numRows; row++)
		{
			f32 metalness = row / f32(numRows - 1);

			f32 height = f32(numRows - 1) * rowSpacing;
			f32 hStart = height / 2.f;
			f32 y = center.y + hStart + -rowSpacing * row;

			for (u32 col = 0; col < numColumns; col++)
			{
				f32 roughness = col / f32(numColumns - 1);

				f32 width = f32(numColumns - 1) * colSpacing;
				f32 wStart = -width / 2.f;
				f32 x = center.x + wStart + colSpacing * col;

				char name[256];
				sprintf_s(name, 256, "Obj M:%.2f R:%.2f", metalness, roughness);
				
				auto entity = CreateBlob();
				entity->Name = name;
				entity->Transform.SetPos({ x,y,0.f });
				entity->Action = std::make_unique<TurntableAction>(entity->Transform);
				
				// config mat
				Material mat = {};
				mat.Name = name;
				mat.Basecolor = glm::vec3{ 1 };
				mat.Roughness = roughness;
				mat.Metalness = metalness;

				_scene.AssignMaterial(*entity->Renderable, mat);

				_scene.AddEntity(std::move(entity));

				++count;
			}
		}

		std::cout << "Material array obj count: " << count << std::endl; 
	}*/

	/*void LoadMaterialExamples()
	{
		std::cout << "Loading axis" << std::endl;

		std::string name;
		std::string basecolorPath;
		std::string normalPath;
		std::string ormPath;

		auto ApplyMat = [&](RenderableComponent& renComp)
		{
			Material mat;
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

			_scene.AssignMaterial(renComp, mat);
		};

		
		{
			name = "Sphere";
			
			auto entity = CreateSphere();
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(1));
			entity->Transform.SetPos(glm::vec3{ 0, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);

			basecolorPath = _libraryDir + "Materials/ScuffedAluminum/BaseColor.png";
			ormPath = _libraryDir + "Materials/ScuffedAluminum/ORM.png";
			normalPath = _libraryDir + "Materials/ScuffedAluminum/Normal.png";
			ApplyMat(*entity->Renderable);
			
			_scene.AddEntity(std::move(entity));
		}
		
		{
			name = "GreasyPan";
			
			auto entity = CreateBlob();
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(.8f));
			entity->Transform.SetPos(glm::vec3{ 2, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);

			basecolorPath = _libraryDir + "Materials/GreasyPan/BaseColor.png";
			ormPath = _libraryDir + "Materials/GreasyPan/ORM.png";
			normalPath = _libraryDir + "Materials/GreasyPan/Normal.png";
			ApplyMat(*entity->Renderable);
			
			_scene.AddEntity(std::move(entity));
		}

		{
			name = "Gold";
			
			auto entity = CreateBlob();
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(.8f));
			entity->Transform.SetPos(glm::vec3{ 4, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);

			basecolorPath = _libraryDir + "Materials/GoldScuffed/BaseColor.png";
			ormPath = _libraryDir + "Materials/GoldScuffed/ORM.png";
			normalPath = _libraryDir + "Materials/GoldScuffed/Normal.png";
			ApplyMat(*entity->Renderable);
			
			_scene.AddEntity(std::move(entity));
		}

		{
			name = "Rusted Metal";
			
			auto entity = CreateBlob();
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(.8f));
			entity->Transform.SetPos(glm::vec3{ -2, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);

			basecolorPath = _libraryDir + "Materials/RustedMetal/BaseColor.png";
			ormPath = _libraryDir + "Materials/RustedMetal/ORM.png";
			normalPath = _libraryDir + "Materials/RustedMetal/Normal.png";
			ApplyMat(*entity->Renderable);
			
			_scene.AddEntity(std::move(entity));
		}

		{
			name = "Bumpy Plastic";
			
			auto entity = CreateBlob();
			entity->Name = name;
			entity->Transform.SetScale(glm::vec3(.8f));
			entity->Transform.SetPos(glm::vec3{ -4, 0, 0 });
			entity->Action = std::make_unique<TurntableAction>(entity->Transform);

			basecolorPath = _libraryDir + "Materials/BumpyPlastic/BaseColor.png";
			ormPath = _libraryDir + "Materials/BumpyPlastic/ORM.png";
			normalPath = _libraryDir + "Materials/BumpyPlastic/Normal.png";
			ApplyMat(*entity->Renderable);
			
			_scene.AddEntity(std::move(entity));
		}
	}*/

	/*void LoadGrapple()
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

		RenderableResourceId resourceId;
		std::string basecolorPath;
		std::string normalPath;
		std::string ormPath;
		std::string emissivePath;

		auto ApplyMat = [&]()
		{
			auto GetOptionalRes = [&](const std::string& texturePath)
			{
				auto optRes = _scene.LoadTexture(texturePath);
				return optRes ? std::optional(Material::Map{optRes.value(), texturePath}) : std::nullopt;
			};

			auto matCopy = _scene.GetMaterial(resourceId);
			
			// Load basecolor map
			matCopy.BasecolorMap = GetOptionalRes(basecolorPath);
			matCopy.UseBasecolorMap = matCopy.BasecolorMap.has_value();

			// Load normal map
			matCopy.NormalMap = { *_scene.LoadTexture(normalPath), normalPath };

			// Load occlusion map
			matCopy.AoMap = GetOptionalRes(ormPath);
			matCopy.AoMapChannel = Material::Channel::Red;
			
			// Load roughness map
			matCopy.RoughnessMap = GetOptionalRes(ormPath);
			matCopy.UseRoughnessMap = matCopy.RoughnessMap.has_value();
			matCopy.RoughnessMapChannel = Material::Channel::Green;

			// Load metalness map
			matCopy.MetalnessMap = GetOptionalRes(ormPath);
			matCopy.UseMetalnessMap = matCopy.MetalnessMap.has_value();
			matCopy.MetalnessMapChannel = Material::Channel::Blue;

			// Load emissive map
			matCopy.EmissiveMap = GetOptionalRes(emissivePath);
			matCopy.EmissiveIntensity = 5;

			_scene.AssignMaterial(resourceId, matCopy);
		};

		
		// Add maps to material
		{
			// Barrel
			{
				resourceId = entity->Renderable->GetSubmeshes()[0].Id;
				basecolorPath = _libraryDir + "Models/" + "grapple/export/Barrel_Basecolor.png";
				normalPath = _libraryDir + "Models/" + "grapple/export/Barrel_Normal.png";
				ormPath = _libraryDir + "Models/" + "grapple/export/Barrel_ORM.png";
				emissivePath = _libraryDir + "Models/" + "grapple/export/Barrel_Emissive.png";
				ApplyMat();
			}
			// Hook
			{
				resourceId = entity->Renderable->GetSubmeshes()[1].Id;
				basecolorPath = _libraryDir + "Models/" + "grapple/export/Hook_Basecolor.png";
				normalPath = _libraryDir + "Models/" + "grapple/export/Hook_Normal.png";
				ormPath = _libraryDir + "Models/" + "grapple/export/Hook_ORM.png";
				emissivePath = "";
				ApplyMat();
			}
			// Stock
			{
				resourceId = entity->Renderable->GetSubmeshes()[2].Id;
				basecolorPath = _libraryDir + "Models/" + "grapple/export/Stock_Basecolor.png";
				normalPath = _libraryDir + "Models/" + "grapple/export/Stock_Normal.png";
				ormPath = _libraryDir + "Models/" + "grapple/export/Stock_ORM.png";
				emissivePath = _libraryDir + "Models/" + "grapple/export/Stock_Emissive.png";
				ApplyMat();
			}
		}

		
		_scene.AddEntity(std::move(entity));
	}*/

	/*void LoadAxis()
	{
		std::cout << "Loading axis" << std::endl;

		auto scale = glm::vec3{ 0.5f };
		f32 dist = 1;

		// Pivot
		{
			auto entity = CreateSphere();
			entity->Name = "Axis-Pivot";
			entity->Transform.SetScale(scale*0.5f);
			entity->Transform.SetPos(glm::vec3{ 0, 0, 0 });


			{
				Material mat;

				auto basePath = _libraryDir + "Materials/ScuffedAluminum/BaseColor.png";
				auto ormPath = _libraryDir + "Materials/ScuffedAluminum/ORM.png";
				auto normalPath = _libraryDir + "Materials/ScuffedAluminum/Normal.png";
				
				mat.UseBasecolorMap = true;
				mat.BasecolorMap = { *_scene.LoadTexture(basePath), basePath };

				//mat.UseNormalMap = true;
				mat.NormalMap = { *_scene.LoadTexture(normalPath), normalPath };
				
				//mat.UseAoMap = true;
				mat.AoMap = { *_scene.LoadTexture(ormPath), ormPath };
				mat.AoMapChannel = Material::Channel::Red;
				
				mat.UseRoughnessMap = true;
				mat.RoughnessMap = { *_scene.LoadTexture(ormPath), ormPath };
				mat.RoughnessMapChannel = Material::Channel::Green;

				mat.UseMetalnessMap = true;
				mat.MetalnessMap = { *_scene.LoadTexture(ormPath), ormPath };
				mat.MetalnessMapChannel = Material::Channel::Blue;

				_scene.AssignMaterial(*entity->Renderable, mat);
			}

			_scene.AddEntity(std::move(entity));
		}
		
		// X
		{
			auto entity = CreateSphere();
			auto name = "Axis-X";
			entity->Name = name;
			entity->Transform.SetScale(scale);
			entity->Transform.SetPos(glm::vec3{ dist, 0, 0 });

			Material mat{};
			{
				mat.Name = name;

				mat.Basecolor = { 1,0,0 };

				const auto normalPath = _libraryDir + "Materials/BumpyPlastic/Normal.png";
				const auto ormPath = _libraryDir + "Materials/BumpyPlastic/ORM.png";
				
				//mat.UseNormalMap = true;
				mat.NormalMap = { *_scene.LoadTexture(normalPath), normalPath };

				//mat.UseAoMap = true;
				mat.AoMap = { *_scene.LoadTexture(ormPath), ormPath };
				mat.AoMapChannel = Material::Channel::Red;

				mat.UseRoughnessMap = true;
				mat.RoughnessMap = { *_scene.LoadTexture(ormPath), ormPath };
				mat.RoughnessMapChannel = Material::Channel::Green;

				mat.UseMetalnessMap = true;
				mat.MetalnessMap = { *_scene.LoadTexture(ormPath), ormPath };
				mat.MetalnessMapChannel = Material::Channel::Blue;
			}
			_scene.AssignMaterial(*entity->Renderable, mat);

			_scene.AddEntity(std::move(entity));
		}
		
		// Y
		{
			auto entity = CreateSphere();
			auto name = "Axis-Y";
			entity->Name = name;
			entity->Transform.SetScale(scale);
			entity->Transform.SetPos(glm::vec3{ 0, dist, 0 });
			
			Material mat{};
			mat.Name = name;
			mat.Basecolor = { 0,1,0 };
			mat.Roughness = 0;
			_scene.AssignMaterial(*entity->Renderable, mat);
			
			_scene.AddEntity(std::move(entity));
		}
		
		// Z
		{
			auto entity = CreateSphere();
			auto name = "Axis-Z";
			entity->Name = name;
			entity->Transform.SetScale(scale);
			entity->Transform.SetPos(glm::vec3{ 0, 0, dist });
			
			Material mat{};
			mat.Name = name;
			mat.Basecolor = { 0,0,1 };
			mat.Roughness = 0;
			_scene.AssignMaterial(*entity->Renderable, mat);
			
			_scene.AddEntity(std::move(entity));
		}
	}*/


	/*static Material CreateRandomDielectricMaterial()
	{
		Material m{};
		m.Name = "RandomDielectricMaterial_" + std::to_string(RandF(0,1));
		m.Roughness = RandF(0.f, 0.7f);
		m.Metalness = 0;
		m.Basecolor = glm::vec3{ RandF(0.15f,0.95f),RandF(0.15f,0.95f),RandF(0.15f,0.95f) };
		return m;
	}
	
	static Material CreateRandomMetalMaterial()
	{
		Material m{};
		m.Name = "CreateRandomMetalMaterial_" + std::to_string(RandF(0,1));
		m.Roughness = RandF(0.f, 0.7f);
		m.Metalness = 1;
		m.Basecolor = glm::vec3{ RandF(0.70f,1.f),RandF(0.70f,1.f),RandF(0.70f,1.f) };
		return m;
	}
	
	static Material CreateRandomMaterial()
	{
		const auto isMetallic = bool(rand() % 2);
		return isMetallic ? CreateRandomMetalMaterial() : CreateRandomDielectricMaterial();
	}*/

	
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

	std::unique_ptr<Entity> CreateEntity(const MeshResourceId& meshId, const AABB& bounds, const std::string& name) const
	{
		Material* material = _scene.CreateMaterial();
		
		//Material material{};
		const auto renderableResId = _delegate.CreateRenderable(meshId, *material);

		
		
		// Create renderable component
		const RenderableComponentSubmesh submesh = { renderableResId, name, material->Id };
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
	};
};
