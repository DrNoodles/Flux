#pragma once

#include "Shared/CommonTypes.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL // for hash
#include <glm/gtx/hash.hpp>


struct UniversalUboCreateInfo
{
	glm::mat4 Model{};
	glm::mat4 View{};
	glm::mat4 Projection{};
	glm::vec3 CamPos{};
	bool ShowNormalMap = false;
	float ExposureBias = 1.0f;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct UniversalUbo
{
	// Data types and alignment to match shader exactly.
	alignas(16) glm::mat4 Model;         
	alignas(16) glm::mat4 View;          
	alignas(16) glm::mat4 Projection;    
	alignas(16) glm::vec3 CamPos;
	alignas(16) glm::vec4 ShowNormalMap;
	alignas(16) glm::vec4 ExposureBias;

	// Create a UniversalUbo packed to match shader standards. MUST unpack in shader.
	static UniversalUbo CreatePacked(UniversalUboCreateInfo info)
	{
		UniversalUbo ubo{};
		
		ubo.Model = info.Model;
		ubo.View = info.View;
		ubo.Projection = info.Projection;
		ubo.CamPos = info.CamPos;
		ubo.ShowNormalMap[0] = float(info.ShowNormalMap);
		ubo.ExposureBias[0] = info.ExposureBias;

		return ubo;
	}
};
