#pragma once

#include <Renderer/HighLevel/CommonRendererHighLevel.h>

#include <State/Entity/LightComponent.h>

namespace Converters
{
	static Light ToLight(const Entity& entity)
	{
		assert(entity.Light.has_value());
		
		const auto& lightComp = *entity.Light;

		Light light = {};
		light.Pos = entity.Transform.GetPos();
		light.Color = lightComp.Color;
		light.Intensity = lightComp.Intensity;

		switch (lightComp.Type) {
		case LightComponent::Types::point:       light.Type = Light::LightType::Point;       break;
		case LightComponent::Types::directional: light.Type = Light::LightType::Directional; break;
		//case Types::spot: 
		default:
			throw std::invalid_argument("Unsupport light component type");
		}

		return light;
	}
}
