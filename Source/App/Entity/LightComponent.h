#pragma once
#include <glm/vec3.hpp>
#include "Renderer/GpuTypes.h"

struct LightComponent
{
	LightComponent() = default;

	enum class Types
	{
		point = 0, directional = 1, //spot = 2,
	};

	Types Type = Types::directional;

	glm::vec3 Color{ 1.0f };
	float Intensity = 500;
//	float AmbientStr{ 0.2f };
//	float DiffStr{ 0.8f };
//	float SpecStr{ 1.0f };
	
	// Falloff
//	float Constant{ 1 };
//	float Linear{ 0.005f };
//	float Quadratic{ .000514434f };

	// Directional Light
	//glm::vec3 Direction{ 0 };

	// Spotlight radius
//	float CutOff{ 0 };

	Light ToLight()
	{
		Light light{};
		light.Color = Color;
		light.Intensity = Intensity;
		switch (Type) {
			case Types::point:       light.Type = Light::LightType::Point;       break;
			case Types::directional: light.Type = Light::LightType::Directional; break;

			//case Types::spot: break;
			default:
				throw std::invalid_argument("Unsupport light component type");
		}

		return light;
	}
};
