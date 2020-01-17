#pragma once

#include "Shared/Transform.h"

#include <glm/vec3.hpp>

class TransformVm
{
public:
	glm::vec3 Pos{};
	glm::vec3 Rot{};
	glm::vec3 Scale{};
	bool UniformScale = true;
	
	TransformVm() = default;
	explicit TransformVm(Transform* target)
		: _target(target)
	{
		Refresh();
	}
	void Refresh()
	{
		Pos = _target->GetPos();
		Rot = _target->GetRot();
		Scale = _target->GetScale();
	}
	void Commit() const
	{
		if (!_target) return;

		_target->SetPos(Pos);
		_target->SetRot(Rot);

		
		if (UniformScale)
		{
			const auto oldScale = _target->GetScale();


			// Find the axis with the largest change
			int changedAxis = -1; // x = 0, y = 1, z = 2, no change = -1
			if (abs(Scale.x - oldScale.x) > 0.00001)
				changedAxis = 0;
			else if (abs(Scale.y - oldScale.y) > 0.00001)
				changedAxis = 1;
			else if (abs(Scale.z - oldScale.z) > 0.00001)
				changedAxis = 2;
			if (changedAxis == -1)
			{
				// No change to scale
				return;
			}

			// newPrimary scales the out values
			auto CalcNewScale = [](const float newPrimary, float& outPrimary, float& outSecondary, float& outTertiary)
			{
				// Special case: Scale TO 0 sets all values to 0
				if (newPrimary == 0)
				{
					outPrimary = outSecondary = outTertiary = 0;
					return;
				}

				// Special case: Allows intuitive dragging of scale from 0 to a non-0
				if (outPrimary == 0)
				{
					outPrimary = newPrimary;
					outSecondary = (outSecondary == 0) ? newPrimary : outSecondary * newPrimary;
					outTertiary = (outTertiary == 0) ? newPrimary : outTertiary* newPrimary;
					return;
				}
				
				const auto factor = newPrimary / outPrimary;
				outPrimary *= factor;
				outSecondary *= factor;
				outTertiary *= factor;
			};
		

			glm::vec3 newScale = oldScale;
			if (changedAxis == 0) // x
			{
				CalcNewScale(Scale.x, newScale.x, newScale.y, newScale.z);
			}
			else if (changedAxis == 1) // y
			{
				CalcNewScale(Scale.y, newScale.y, newScale.x, newScale.z);
			}
			else // assume z
			{
				CalcNewScale(Scale.z, newScale.z, newScale.x, newScale.y);
			}
			
			_target->SetScale(newScale);
		}
		else
		{
			_target->SetScale(Scale);
		}
	}
private:
	Transform* _target = nullptr;
};

