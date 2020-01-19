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
	 alignas(16) f32 ExposureBias;        // 4     ( 4 > 16)
	 alignas(16) f32 DrawNormalMap;       // 4     ( 4 > 16)
	 alignas(16) char Pad[32];

private:
	//alignas(16) glm::vec4 _padding{}; // 224   ( 16 > 16)
	//alignas(16) glm::vec4 _padding2{};// 224   ( 16 > 16)
	//alignas(16) glm::vec4 _padding3{};// 224   ( 16 > 16)
	//alignas(16) glm::vec4 _padding4{};// 224   ( 16 > 16)
												 // Total 256
};
