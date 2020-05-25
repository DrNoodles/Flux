#pragma once

#include <Framework/CommonRenderer.h>
#include <glm/glm.hpp>
#include <optional>


// TODO Move type to Renderer layer


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum class TransparencyMode : char
{
	// DO NOT CHANGE ORDER
	Additive = 0,
	Cutoff = 1,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum class TextureType : char
{
	// DO NOT CHANGE ORDER
	Undefined = 0,
	Basecolor = 1,
	Normals = 2,
	Roughness = 3,
	Metalness = 4,
	AmbientOcclusion = 5,
	Emissive = 6,
	Transparency = 7,
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Material
{
	struct Map
	{
		TextureResourceId Id = {};
		std::string Path = {};
	};
	
	// DO NOT CHANGE ORDER
	enum class Channel
	{
		Red = 0,
		Green = 1,
		Blue = 2,
		Alpha = 3,
	};

	glm::vec3 Basecolor = glm::vec3{ 1 };
	float Metalness = 0.0f;
	float Roughness = 0.3f;
	float EmissiveIntensity = 1;
	float TransparencyCutoffThreshold = 0.5f;

	bool UseBasecolorMap = false;
	//bool UseNormalMap = false;
	bool UseMetalnessMap = false;
	bool UseRoughnessMap = false;
	//bool UseAoMap = false;
	
	std::optional<Map> BasecolorMap = {};
	std::optional<Map> NormalMap = {};
	std::optional<Map> MetalnessMap = {};
	std::optional<Map> RoughnessMap = {};
	std::optional<Map> AoMap = {};
	std::optional<Map> EmissiveMap = {};
	std::optional<Map> TransparencyMap = {};

	bool InvertNormalMapY = false;
	bool InvertNormalMapZ = false;
	bool InvertAoMap = false;
	bool InvertRoughnessMap = false;
	bool InvertMetalnessMap = false;

	Channel MetalnessMapChannel = Channel::Red;
	Channel RoughnessMapChannel = Channel::Red;
	Channel AoMapChannel = Channel::Red;
	Channel TransparencyMapChannel = Channel::Alpha;

	TextureType ActiveSolo = TextureType::Undefined;
	TransparencyMode TransparencyMode = TransparencyMode::Additive;



	bool UsingBasecolorMap() const
	{
		return UseBasecolorMap && BasecolorMap.has_value() && 
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Basecolor);
	}

	bool UsingNormalMap() const
	{
		return /*UseNormalMap && */NormalMap.has_value() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Normals);
	}

	bool UsingMetalnessMap() const
	{
		return UseMetalnessMap && MetalnessMap.has_value() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Metalness);
	}

	bool UsingRoughnessMap() const
	{
		return UseRoughnessMap && RoughnessMap.has_value() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Roughness);
	}

	bool UsingAoMap() const
	{
		return /*UseAoMap && */AoMap.has_value() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::AmbientOcclusion);
	}

	bool UsingEmissiveMap() const
	{
		return EmissiveMap.has_value() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Emissive);
	}

	bool UsingTransparencyMap() const
	{
		return TransparencyMap.has_value() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Transparency);
	}
};
