#pragma once

#include "App/AppTypes.h" // TextureType
#include <State/SceneManager.h> // for texture loading - TODO refactor this outta here

#include <glm/glm.hpp>

#include <vector>


struct MaterialViewState
{
	bool UseBasecolorMap = false;
	//bool UseNormalMap = false;
	bool UseMetalnessMap = false;
	bool UseRoughnessMap = false;
	//bool UseAoMap = false;
	

	std::string BasecolorMapPath{};
	std::string NormalMapPath{};
	std::string MetalnessMapPath{};
	std::string RoughnessMapPath{};
	std::string AoMapPath{};
	std::string EmissiveMapPath{};
	std::string TransparencyMapPath{};

	glm::vec3 Basecolor = glm::vec3{ 1 };
	float Metalness = 0.0f;
	float Roughness = 0.3f;
	float EmissiveIntensity = 1.0f;
	float TransparencyCutoffThreshold = 0;

	bool InvertNormalMapY = false;
	bool InvertNormalMapZ = false;
	bool InvertAoMap = false;
	bool InvertRoughnessMap = false;
	bool InvertMetalnessMap = false;

	int ActiveSolo = 0;
	int TransparencyMode = 0;

	inline static std::vector<std::string> MapChannels = { "Red", "Green", "Blue", "Alpha" };
	int ActiveMetalnessChannel = 0;
	int ActiveRoughnessChannel = 0;
	int ActiveAoChannel = 0;
	int ActiveTransparencyChannel = 0;

	
	static MaterialViewState CreateFrom(const Material& mat)
	{
		MaterialViewState state = {};

		state.UseBasecolorMap = mat.UseBasecolorMap;
		state.UseMetalnessMap = mat.UseMetalnessMap;
		state.UseRoughnessMap = mat.UseRoughnessMap;
		//rvm.UseNormalMap = mat.UseNormalMap;
		//rvm.UseAoMap = mat.UseAoMap;

		state.Basecolor = mat.Basecolor;
		state.Metalness = mat.Metalness;
		state.Roughness = mat.Roughness;
		state.EmissiveIntensity = mat.EmissiveIntensity;
		state.TransparencyCutoffThreshold = mat.TransparencyCutoffThreshold;
		
		state.InvertNormalMapY = mat.InvertNormalMapY;
		state.InvertNormalMapZ = mat.InvertNormalMapZ;
		state.InvertAoMap = mat.InvertAoMap;
		state.InvertRoughnessMap = mat.InvertRoughnessMap;
		state.InvertMetalnessMap = mat.InvertMetalnessMap;

		state.ActiveMetalnessChannel = int(mat.MetalnessMapChannel);
		state.ActiveRoughnessChannel = int(mat.RoughnessMapChannel);
		state.ActiveAoChannel = int(mat.AoMapChannel);
		state.ActiveTransparencyChannel = int(mat.TransparencyMapChannel);

		switch (mat.ActiveSolo)
		{
		case TextureType::Undefined:    state.ActiveSolo = 0;     break;
		case TextureType::Basecolor:    state.ActiveSolo = 1;     break;
		case TextureType::Normals:      state.ActiveSolo = 2;     break;
		case TextureType::Metalness:    state.ActiveSolo = 3;     break;
		case TextureType::Roughness:    state.ActiveSolo = 4;     break;
		case TextureType::AmbientOcclusion: state.ActiveSolo = 5; break;
		case TextureType::Emissive:     state.ActiveSolo = 6;     break;
		case TextureType::Transparency: state.ActiveSolo = 7;     break;
		default:
			throw std::out_of_range("Unsupported ActiveSolo");
		}

		switch (mat.TransparencyMode)
		{
		case TransparencyMode::Additive: state.TransparencyMode = 0; break;
		case TransparencyMode::Cutoff:  state.TransparencyMode = 1; break;
		default:
			throw std::out_of_range("Unsupported TransparencyMode");
		}

		auto GetPath = [](const std::optional<Material::Map>& map) { return map.has_value() ? map->Path : ""; };
		state.BasecolorMapPath = GetPath(mat.BasecolorMap);
		state.NormalMapPath = GetPath(mat.NormalMap);
		state.MetalnessMapPath = GetPath(mat.MetalnessMap);
		state.RoughnessMapPath = GetPath(mat.RoughnessMap);
		state.AoMapPath = GetPath(mat.AoMap);
		state.EmissiveMapPath = GetPath(mat.EmissiveMap);
		state.TransparencyMapPath = GetPath(mat.TransparencyMap);

		return state;
	}

	static Material ToMaterial(const MaterialViewState& state, SceneManager& sm)
	{
		Material mat;
		
		mat.UseBasecolorMap = state.UseBasecolorMap;
		mat.UseMetalnessMap = state.UseMetalnessMap;
		mat.UseRoughnessMap = state.UseRoughnessMap;
		//mat.UseNormalMap = state.UseNormalMap;
		//mat.UseAoMap = state.UseAoMap;

		mat.Basecolor = state.Basecolor;
		mat.Metalness = state.Metalness;
		mat.Roughness = state.Roughness;
		mat.EmissiveIntensity = state.EmissiveIntensity;
		mat.TransparencyCutoffThreshold = state.TransparencyCutoffThreshold;

		mat.InvertNormalMapY = state.InvertNormalMapY;
		mat.InvertNormalMapZ = state.InvertNormalMapZ;
		mat.InvertAoMap = state.InvertAoMap;
		mat.InvertRoughnessMap = state.InvertRoughnessMap;
		mat.InvertMetalnessMap = state.InvertMetalnessMap;

		mat.MetalnessMapChannel = (Material::Channel)state.ActiveMetalnessChannel;
		mat.RoughnessMapChannel = (Material::Channel)state.ActiveRoughnessChannel;
		mat.AoMapChannel = (Material::Channel)state.ActiveAoChannel;
		mat.TransparencyMapChannel = (Material::Channel)state.ActiveTransparencyChannel;

		switch (state.ActiveSolo)
		{
		case 0: mat.ActiveSolo = TextureType::Undefined;        break;
		case 1: mat.ActiveSolo = TextureType::Basecolor;        break;
		case 2: mat.ActiveSolo = TextureType::Normals;          break;
		case 3: mat.ActiveSolo = TextureType::Metalness;        break;
		case 4: mat.ActiveSolo = TextureType::Roughness;        break;
		case 5: mat.ActiveSolo = TextureType::AmbientOcclusion; break;
		case 6: mat.ActiveSolo = TextureType::Emissive;         break;
		case 7: mat.ActiveSolo = TextureType::Transparency;     break;
		default:
			throw std::out_of_range("");
		}

		switch (state.TransparencyMode)
		{
		case 0: mat.TransparencyMode = TransparencyMode::Additive; break;
		case 1: mat.TransparencyMode = TransparencyMode::Cutoff; break;
		default:
			throw std::out_of_range("Unsupported TransparencyMode");
		}

		
		auto UpdateMap = [&](const std::string& newPath, Material& targetMat, const TextureType type)
		{
			std::optional<Material::Map>* pMap;
			
			switch (type)
			{
			case TextureType::Basecolor:        pMap = &targetMat.BasecolorMap;    break;
			case TextureType::Normals:          pMap = &targetMat.NormalMap;       break;
			case TextureType::Roughness:        pMap = &targetMat.RoughnessMap;    break;
			case TextureType::Metalness:        pMap = &targetMat.MetalnessMap;    break;
			case TextureType::AmbientOcclusion: pMap = &targetMat.AoMap;           break;
			case TextureType::Emissive:         pMap = &targetMat.EmissiveMap;     break;
			case TextureType::Transparency:     pMap = &targetMat.TransparencyMap; break;
			case TextureType::Undefined:
			default:
				throw std::invalid_argument("unhandled TextureType");
			}

			if (newPath.empty()) {
				*pMap = std::nullopt;
			}
			else // path is empty, make sure the material is also
			{
				if (!pMap->has_value()) {
					*pMap = Material::Map{}; // make sure we have something to assign to
				}
				
				const bool pathIsDifferent = (*pMap)->Path != newPath;
				if (pathIsDifferent)
				{
					(*pMap)->Id = *sm.LoadTexture(newPath);
					(*pMap)->Path = newPath;
				}
			}
		};

		UpdateMap(state.BasecolorMapPath, mat, TextureType::Basecolor);
		UpdateMap(state.NormalMapPath, mat, TextureType::Normals);
		UpdateMap(state.MetalnessMapPath, mat, TextureType::Metalness);
		UpdateMap(state.RoughnessMapPath, mat, TextureType::Roughness);
		UpdateMap(state.AoMapPath, mat, TextureType::AmbientOcclusion);
		UpdateMap(state.EmissiveMapPath, mat, TextureType::Emissive);
		UpdateMap(state.TransparencyMapPath, mat, TextureType::Transparency);


		return mat;
	}
};

