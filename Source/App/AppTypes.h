#pragma once

#include <string>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct AppOptions
{
	std::string ShaderDir{};
	std::string DataDir{};
	std::string AssetsDir{};
	std::string ModelsDir{};
	std::string CubemapsDir{};
	bool EnabledVulkanValidationLayers = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO Move type to Renderer layer
enum class TextureType : char
{
	Undefined = 0,
	Basecolor,
	Normals,
	Roughness,
	Metalness,
	AmbientOcclusion,
};
