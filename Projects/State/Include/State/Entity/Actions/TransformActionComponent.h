#pragma once

#include <State/Entity/IActionComponent.h>
#include <State/Entity/TransformComponent.h>
#include <functional>

class TransformAction final : public IActionComponent
{
public:
	explicit TransformAction(
		TransformComponent* const transform, 
		std::function<void(TransformComponent*, float, float)> doIt)
	{
		_pTransform = transform;
		_doIt = std::move(doIt);
	}

	void Update(float dt) override
	{
		_time += dt;
		_doIt(_pTransform, _time, dt);
	}

private:
	float _time = 0;
	TransformComponent* _pTransform = nullptr;
	std::function<void(TransformComponent*, float, float)> _doIt = nullptr;
};
