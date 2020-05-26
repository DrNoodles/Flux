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
	bool VSync = false;
	bool LoadDemoScene = false;
};

