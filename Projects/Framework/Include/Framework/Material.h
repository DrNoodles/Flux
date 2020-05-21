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
	// DO NOT CHANGE ORDER
	enum class Channel
	{
		Red = 0,
		Green = 1,
		Blue = 2,
		Alpha = 3,
	};
	
	bool UseBasecolorMap = false;
	//bool UseNormalMap = false;
	bool UseMetalnessMap = false;
	bool UseRoughnessMap = false;
	//bool UseAoMap = false;

	std::string BasecolorMapPath = {};
	std::string NormalMapPath = {};
	std::string MetalnessMapPath = {};
	std::string RoughnessMapPath = {};
	std::string AoMapPath = {};
	std::string EmissiveMapPath = {};
	std::string TransparencyMapPath = {};

	std::optional<TextureResourceId> BasecolorMap = std::nullopt;
	std::optional<TextureResourceId> NormalMap = std::nullopt;
	std::optional<TextureResourceId> MetalnessMap = std::nullopt;
	std::optional<TextureResourceId> RoughnessMap = std::nullopt;
	std::optional<TextureResourceId> AoMap = std::nullopt;
	std::optional<TextureResourceId> EmissiveMap = std::nullopt;
	std::optional<TextureResourceId> TransparencyMap = std::nullopt;

	glm::vec3 Basecolor = glm::vec3{ 1 };
	float Metalness = 0.0f;
	float Roughness = 0.3f;
	float EmissiveIntensity = 1;
	float TransparencyCutoffThreshold = 0.5f;

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


	bool HasBasecolorMap() const { return BasecolorMap.has_value(); }
	bool HasNormalMap() const { return NormalMap.has_value(); }
	bool HasMetalnessMap() const { return MetalnessMap.has_value(); }
	bool HasRoughnessMap() const { return RoughnessMap.has_value(); }
	bool HasAoMap() const { return AoMap.has_value(); }
	bool HasEmissiveMap() const { return EmissiveMap.has_value(); }
	bool HasTransparencyMap() const { return TransparencyMap.has_value(); }


	bool UsingBasecolorMap() const
	{
		return UseBasecolorMap && HasBasecolorMap() && 
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Basecolor);
	}

	bool UsingNormalMap() const
	{
		return /*UseNormalMap && */HasNormalMap() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Normals);
	}

	bool UsingMetalnessMap() const
	{
		return UseMetalnessMap && HasMetalnessMap() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Metalness);
	}

	bool UsingRoughnessMap() const
	{
		return UseRoughnessMap && HasRoughnessMap() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Roughness);
	}

	bool UsingAoMap() const
	{
		return /*UseAoMap && */HasAoMap() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::AmbientOcclusion);
	}

	bool UsingEmissiveMap() const
	{
		return HasEmissiveMap() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Emissive);
	}

	bool UsingTransparencyMap() const
	{
		return HasTransparencyMap() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Transparency);
	}
};
