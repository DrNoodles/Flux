#pragma once

#include "IActionComponent.h"
#include "LightComponent.h"
#include "TransformComponent.h"
#include "RenderableComponent.h"

#include <string>
#include <optional>

struct Entity
{
	int Id;
	std::string Name;
	TransformComponent Transform;
	std::optional<RenderableComponent> Renderable = std::nullopt;
	std::optional<LightComponent> Light = std::nullopt;
	std::unique_ptr<IActionComponent> Action = nullptr; // null means it isn't available. optional can't take abstract

	Entity()
	{
		Id = ++EntityCount;
		Name = "Entity" + std::to_string(Id);
	}

	inline static int EntityCount = 0; // cuz i CBF importing a GUID lib
};
