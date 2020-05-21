#pragma once

#include "App/AppTypes.h" // TextureType

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
	
};

