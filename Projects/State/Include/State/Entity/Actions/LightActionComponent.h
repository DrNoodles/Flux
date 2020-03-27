#pragma once

#include <State/Entity/IActionComponent.h>
#include <State/Entity/LightComponent.h>
#include <functional>

class LightAction final : public IActionComponent
{
public:
	float RotationsPerSecond = 0.10f;

	LightAction(
		LightComponent& light, 
		std::function<void(LightComponent&, float)> doIt)
		: _pLight(light), _doIt(std::move(doIt))
	{
	}

	void Update(float dt) override
	{
		_doIt(_pLight, dt);
	}

private:
	LightComponent& _pLight;
	std::function<void(LightComponent&, float)> _doIt = nullptr;
};
