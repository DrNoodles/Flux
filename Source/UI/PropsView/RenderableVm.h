#pragma once

#include "App/IModelLoaderService.h" // TextureType
#include "App/Entity/RenderableComponent.h"

#include <iostream>
#include <exception>
#include <optional>
#include <vector>


class RenderableVm
{
public:
};


//class RenderableVm
//{
//public:
//	// Sub-Meshes
//	std::vector<std::string> MeshVms{};
//
//	// Material
//	bool UseBaseColorMap = false;
//	//bool UseNormalMap = false;
//	bool UseMetalnessMap = false;
//	bool UseRoughnessMap = false;
//	//bool UseAoMap = false;
//
//	std::string BaseColorMapPath{};
//	std::string NormalMapPath{};
//	std::string MetalnessMapPath{};
//	std::string RoughnessMapPath{};
//	std::string AoMapPath{};
//
//	glm::vec3 BaseColor = glm::vec3{ 1 };
//	float Metalness = 0.0f;
//	float Roughness = 0.3f;
//
//	bool InvertNormalMapZ = false;
//	bool InvertAoMap = false;
//	bool InvertRoughnessMap = false;
//	bool InvertMetalnessMap = false;
//
//	TextureType ActiveSolo = TextureType::Undefined;
//
//	inline static std::vector<std::string> MapChannels = { "Red", "Green", "Blue", "Alpha" };
//	int ActiveMetalnessChannel = 0;
//	int ActiveRoughnessChannel = 0;
//	int ActiveAoChannel = 0;
//
//	RenderableVm() = delete;
//	
//	RenderableVm(ResourceManager* res, RenderableComponent* target, 
//		const std::vector<TextureResource*>* textures, 
//		const std::vector<MeshResource*>* models)
//		: _target(target), /*_textures(textures), */_res(res)
//	/*, _models(models)*/
//	{
//		assert(!target->Meshes.empty());
//		
//		for (auto& renderableMesh : target->Meshes)
//		{
//			MeshResource* model = models->at(renderableMesh.MeshId);
//			MeshVms.emplace_back(model->Name());
//		}
//
//		SelectSubMesh(0);
//	}
//
//	void SelectSubMesh(int i)
//	{
//		_selectedSubMesh = i;
//		auto& mat = _target->Meshes[i].Mat;
//
//		UseBaseColorMap = mat.UseAlbedoMap;
//		UseMetalnessMap = mat.UseMetallicMap;
//		UseRoughnessMap = mat.UseRoughnessMap;
//		//UseNormalMap = mat.UseNormalMap;
//		//UseAoMap = mat.UseAoMap;
//		
//		BaseColor = mat.Albedo;
//		Metalness = mat.Metallic;
//		Roughness = mat.Roughness;
//
//		InvertNormalMapZ = mat.InvertNormalMapZ;
//		InvertAoMap = mat.InvertAoMap;
//		InvertRoughnessMap = mat.InvertRoughnessMap;
//		InvertMetalnessMap = mat.InvertMetalnessMap;
//
//		ActiveMetalnessChannel = int(mat.MetalnessMapChannel);
//		ActiveRoughnessChannel = int(mat.RoughnessMapChannel);
//		ActiveAoChannel = int(mat.AoMapChannel);
//
//		ActiveSolo = mat.ActiveSolo;
//
//		BaseColorMapPath = mat.HasAlbedoMap() ? mat.AlbedoMap->Path : "";
//		NormalMapPath = mat.HasNormalMap() ? mat.NormalMap->Path : "";
//		MetalnessMapPath = mat.HasMetallicMap() ? mat.MetalnessMap->Path : "";
//		RoughnessMapPath = mat.HasRoughnessMap() ? mat.RoughnessMap->Path : "";
//		AoMapPath= mat.HasAoMap() ? mat.AoMap->Path : "";
//	}
//
//	void CommitChanges() const
//	{
//		if (!_target) return;
//
//		// Update material properties
//		auto& mat = _target->Meshes[_selectedSubMesh].Mat;
//
//		mat.UseAlbedoMap = UseBaseColorMap;
//		mat.UseMetallicMap = UseMetalnessMap;
//		mat.UseRoughnessMap = UseRoughnessMap;
//		//mat.UseNormalMap = UseNormalMap;
//		//mat.UseAoMap = UseAoMap;
//		
//		mat.Albedo = BaseColor;
//		mat.Metallic = Metalness;
//		mat.Roughness = Roughness;
//
//		mat.InvertNormalMapZ = InvertNormalMapZ;
//		mat.InvertAoMap = InvertAoMap;
//		mat.InvertRoughnessMap = InvertRoughnessMap;
//		mat.InvertMetalnessMap = InvertMetalnessMap;
//
//		mat.MetalnessMapChannel = (Material::Channel)ActiveMetalnessChannel;
//		mat.RoughnessMapChannel = (Material::Channel)ActiveRoughnessChannel;
//		mat.AoMapChannel = (Material::Channel)ActiveAoChannel;
//
//		mat.ActiveSolo = ActiveSolo;
//		
//		auto UpdateMap = [&](const std::string& newPath, std::optional<Texture>& targetMap, const TextureType type)
//		{
//			if (newPath.empty())
//			{
//				targetMap = std::nullopt;
//			}
//			else // path is empty, make sure the material is also
//			{
//				const bool pathIsDifferent = !(targetMap.has_value() && targetMap->Path == newPath);
//				if (pathIsDifferent)
//				{
//					targetMap = LoadTexture(mat, newPath, type, *_res);
//				}
//			}
//		};
//
//		UpdateMap(BaseColorMapPath, mat.AlbedoMap, TextureType::BaseColor);
//		UpdateMap(NormalMapPath, mat.NormalMap, TextureType::Normals);
//		UpdateMap(MetalnessMapPath, mat.MetalnessMap, TextureType::Metalness);
//		UpdateMap(RoughnessMapPath, mat.RoughnessMap, TextureType::Roughness);
//		UpdateMap(AoMapPath, mat.AoMap, TextureType::AmbientOcclusion);
//	}
//
//	int GetSelectedSubMesh() const { return _selectedSubMesh; }
//
//	
//private:
//	int _selectedSubMesh = 0;
//	RenderableComponent* _target = nullptr;
//	ResourceManager* _res = nullptr;
//
//	static std::optional<Texture> LoadTexture(Material& mat, const std::string& path, TextureType type, ResourceManager& res)
//	{
//		// Load the texture into memory
//		const size_t idx = path.find_last_of("/\\");
//		// TODO safety check this actually exists :)
//		const auto directory = path.substr(0, idx + 1);
//		const auto filename = path.substr(idx + 1);
//
//		try
//		{
//			Texture t{};
//			t.ResourceId = res.LoadTexture(path);
//			t.Path = path;
//			t.Type = type;
//			return t;
//		}
//		catch (std::exception& e)
//		{
//			std::cerr << "Failed to load texture" << e.what() << std::endl;
//			return std::nullopt; // protect against shitty input data
//		}
//	}
//};
