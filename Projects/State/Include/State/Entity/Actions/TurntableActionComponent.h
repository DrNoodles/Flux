#pragma once
#include <State/Entity/IActionComponent.h>
#include <State/Entity/TransformComponent.h>

class TurntableAction final : public IActionComponent
{
public:
	float RotationsPerSecond = 0.08f;

	explicit TurntableAction(TransformComponent& transform): _pTransform(transform) 
	{
	}

	void Update(float dt) override
	{
		const auto degreesPerRotation = 360.f;
		const auto rotationDelta = RotationsPerSecond * degreesPerRotation * dt;
		
		auto rot = _pTransform.GetRot();
		rot.y += rotationDelta;
		_pTransform.SetRot(rot);

		//printf_s("rot.y:%f dt:%f\n", rot.y, dt);
	}

private:
	TransformComponent& _pTransform;
};
