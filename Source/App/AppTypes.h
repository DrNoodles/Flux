#pragma once

#include <string>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct AppOptions
{
	std::string ShaderDir{};
	std::string DataDir{};
	std::string AssetsDir{};
	std::string ModelsDir{};
	std::string IblDir{};
	bool EnabledVulkanValidationLayers = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO Move type to Renderer layer
// DO NOT CHANGE ORDER
enum class TextureType : char
{
	Undefined = 0,
	Basecolor = 1,
	Normals = 2,
	Roughness = 3,
	Metalness = 4,
	AmbientOcclusion = 5,
};
