#pragma once

#include "../../Renderer/GpuResources/ResourceManager.h"
#include "../../Renderer/Entity/Light.h"
#include <algorithm>

class LightVm
{
public:
	LightVm() = delete;
	explicit LightVm(Light* target) : _target(target)
	{
		Update();
	}

	Light::Types Type{};
	glm::vec3 Color{};
	float Intensity = 1;

	void Update()
	{
		Type = _target->Type;
		Color = _target->Color;
		Intensity = _target->Intensity;
	}
	void CommitChanges() const
	{
		_target->Color = Color;
		_target->Intensity = std::clamp(Intensity, 0.f, 1000000.f);
	}

	
private:
	Light* _target = nullptr;
};
