#pragma once

#include "Shared/CommonTypes.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL // for hash
#include <glm/gtx/hash.hpp>


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct UniversalUbo
{                            // Size  Effective Size
	alignas(16) glm::mat4 Model;         // 64    (64 > 64)
	alignas(16) glm::mat4 View;          // 64    (64 > 64)
	alignas(16) glm::mat4 Projection;    // 64    (64 > 64)
	alignas(16) f32 ShowNormalMap;
	alignas(16) f32 ExposureBias;
	//alignas(16) glm::vec4 Options;        // 4     ( 4 > 16)
	//alignas(16) bool DrawNormalMap;       // 4     ( 4 > 16)
	//alignas(16) char Pad[38];
};
