#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Renderer/GpuTypes.h"
#include "Renderer/Renderable.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//struct RenderableMesh
//{
//	u32 ModelId = u32_max;
//
//	// Material
//	u32 BasecolorMapId = u32_max;
//	u32 NormalMapId = u32_max;
//};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class RenderableComponent
{
public:
	RenderableResourceId RenderableId; // TODO Support N Renderables

	// TODO Use this space to add additional data used for the App/Ui layer
};
