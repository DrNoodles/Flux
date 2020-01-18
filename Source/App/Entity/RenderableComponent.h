#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Renderer/GpuTypes.h"


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
	ModelResourceId ModelResId;
	//std::vector<RenderableMesh> Meshes; // TODO Support N submeshes
	//u32 ModelId = u32_max;

	//// Material
	//u32 BasecolorMapId = u32_max;
	//u32 NormalMapId = u32_max;
};
