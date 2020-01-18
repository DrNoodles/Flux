#pragma once
#include <glm/vec3.hpp>

struct LightComponent
{
	LightComponent() = default;

	enum class Types
	{
		directional = 0,
		point,
		spot,
	};

	Types Type = Types::directional;

	glm::vec3 Color{ 1.0f };
	float Intensity = 500;
	float AmbientStr{ 0.2f };
	float DiffStr{ 0.8f };
	float SpecStr{ 1.0f };
	
	// Falloff
	float Constant{ 1 };
	float Linear{ 0.005f };
	float Quadratic{ .000514434f };

	// Directional Light
	glm::vec3 Direction{ 0 };

	// Spotlight radius
	float CutOff{ 0 };
};
