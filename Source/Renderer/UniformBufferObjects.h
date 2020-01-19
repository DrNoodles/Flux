#pragma once

#include "Shared/CommonTypes.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL // for hash
#include <glm/gtx/hash.hpp>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct UniversalUbo
{                                   // Offets
	alignas(16) bool DrawNormalMap;  // 16
	alignas(16) f32 ExposureBias;    // 0
	alignas(16) glm::mat4 Model;     // 32
	alignas(16) glm::mat4 View;      // 96
	alignas(16) glm::mat4 Projection;// 160
};
