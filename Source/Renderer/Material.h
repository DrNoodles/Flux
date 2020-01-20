#pragma once
#include "GpuTypes.h"

#include <glm/detail/type_vec3.hpp>
#include <glm/vec3.hpp>

#include <optional>
#include "App/AppTypes.h"

struct Material
{
	enum class Channel
	{
		Red = 0,
		Green = 1,
		Blue = 2,
		Alpha = 3,
	};
	bool UseBasecolorMap = false;
	bool UseNormalMap = false;
	bool UseMetalnessMap = false;
	bool UseRoughnessMap = false;
	bool UseAoMap = false;

	bool HasBasecolorMap() const { return BasecolorMap.has_value(); }
	bool HasNormalMap() const { return NormalMap.has_value(); }
	bool HasMetalnessMap() const { return MetalnessMap.has_value(); }
	bool HasRoughnessMap() const { return RoughnessMap.has_value(); }
	bool HasAoMap() const { return AoMap.has_value(); }
	
	std::optional<TextureResourceId> BasecolorMap = std::nullopt;
	std::optional<TextureResourceId> NormalMap = std::nullopt;
	std::optional<TextureResourceId> MetalnessMap = std::nullopt;
	std::optional<TextureResourceId> RoughnessMap = std::nullopt;
	std::optional<TextureResourceId> AoMap = std::nullopt;

	glm::vec3 Basecolor = glm::vec3{ 1 };
	float Metalness = 0.0f;
	float Roughness = 0.3f;
	float AmbientOcclusion = 1;

	bool InvertNormalMapZ = false;
	bool InvertAoMap = false;
	bool InvertRoughnessMap = false;
	bool InvertMetalnessMap = false;

	Channel MetalnessMapChannel = Channel::Red;
	Channel RoughnessMapChannel = Channel::Red;
	Channel AoMapChannel = Channel::Red;

	TextureType ActiveSolo = TextureType::Undefined;

	bool UsingBasecolorMap() const
	{
		return UseBasecolorMap && HasBasecolorMap() && 
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::Basecolor);
	}

	bool UsingNormalMap() const
	{
		return UseNormalMap && HasNormalMap() &&
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
		return UseAoMap && HasAoMap() &&
			(ActiveSolo == TextureType::Undefined || ActiveSolo == TextureType::AmbientOcclusion);
	}

	//void Bind(const ShaderResource& shader, const std::vector<TextureResource*>& textures) const
	//{
	//	// Set material uniforms
	//	shader.SetVec3("uAlbedo", Albedo);
	//	shader.SetFloat("uMetallic", Metallic);
	//	shader.SetFloat("uRoughness", Roughness);
	//	shader.SetFloat("uAo", AmbientOcclusion);

	//	// Don't set use map to true unless there's a map to use! This avoids a corrupt look
	//	shader.SetBool("useAlbedoMap", UsingAlbedoMap());
	//	shader.SetBool("useNormalMap", UsingNormalMap());
	//	shader.SetBool("useMetallicMap", UsingMetalnessMap());
	//	shader.SetBool("useRoughnessMap", UsingRoughnessMap());
	//	shader.SetBool("useAoMap", UsingAoMap());
	//	
	//	shader.SetBool("invertNormalMapZ", InvertNormalMapZ);
	//	shader.SetBool("invertAoMap", InvertAoMap);
	//	shader.SetBool("invertRoughnessMap", InvertRoughnessMap);
	//	shader.SetBool("invertMetalnessMap", InvertMetalnessMap);

	//	shader.SetInt("metalnessMapChannel", (int)MetalnessMapChannel);
	//	shader.SetInt("roughnessMapChannel", (int)RoughnessMapChannel);
	//	shader.SetInt("aoMapChannel", (int)AoMapChannel);
	//	
	//	// Bind textures
	//	auto textureUnit = 0;
	//	if (UsingAlbedoMap()) BindMap(textureUnit++, "AlbedoMap", textures[AlbedoMap->ResourceId]->Id(), shader);
	//	if (UsingNormalMap()) BindMap(textureUnit++, "NormalMap", textures[NormalMap->ResourceId]->Id(), shader);
	//	if (UsingMetalnessMap()) BindMap(textureUnit++, "MetalnessMap", textures[MetalnessMap->ResourceId]->Id(), shader);
	//	if (UsingRoughnessMap()) BindMap(textureUnit++, "RoughnessMap", textures[RoughnessMap->ResourceId]->Id(), shader);
	//	if (UsingAoMap()) BindMap(textureUnit++, "AmbientOcclusionMap", textures[AoMap->ResourceId]->Id(), shader);
	//}

	//static void BindMap(int textureUnit, const std::string& textureUnitName, UINT textureId, 
	//	const ShaderResource& shader)
	//{
	//	glActiveTexture(GL_TEXTURE0 + textureUnit);
	//	glBindTexture(GL_TEXTURE_2D, textureId);
	//	shader.SetInt(textureUnitName, textureUnit);
	//}
};
