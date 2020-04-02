#pragma once

class IActionComponent
{
public:
	virtual ~IActionComponent() = default;
	virtual void Update(float dt) = 0;
};


