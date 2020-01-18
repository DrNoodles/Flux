#pragma once
#include "App/Entity/IActionComponent.h"
#include "App/Entity/TransformComponent.h"

class TurntableAction final : public IActionComponent
{
public:
	float RotationsPerSecond = 0.10f;

	explicit TurntableAction(TransformComponent& transform): _pTransform(transform) 
	{
	}

	void Update(float dt) override
	{
		auto rot = _pTransform.GetRot();
		rot.y += RotationsPerSecond * 360 * dt;
		_pTransform.SetRot(rot);
	}

private:
	TransformComponent& _pTransform;
};
