#pragma once

#include "Material.h"
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

	// Render options
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

	// Material
	alignas(16) glm::vec3 Basecolor;
	alignas(16) glm::vec4 Roughness;          // float in [0]
	alignas(16) glm::vec4 Metalness;          // float in [0]
	
	alignas(16) glm::vec4 UseBasecolorMap;    // bool in [0] 
	alignas(16) glm::vec4 UseNormalMap;       // bool in [0]
	alignas(16) glm::vec4 UseRoughnessMap;    // bool in [0]
	alignas(16) glm::vec4 UseMetalnessMap;    // bool in [0]
	alignas(16) glm::vec4 UseAoMap;           // bool in [0]
	
	alignas(16) glm::vec4 InvertNormalMapZ;   // bool in [0]
	alignas(16) glm::vec4 InvertRoughnessMap; // bool in [0]
	alignas(16) glm::vec4 InvertMetalnessMap; // bool in [0]
	alignas(16) glm::vec4 InvertAoMap;        // bool in [0]
	
	alignas(16) glm::vec4 RoughnessMapChannel;// int in [0] R=0,G,B,A
	alignas(16) glm::vec4 MetalnessMapChannel;// int in [0] R=0,G,B,A
	alignas(16) glm::vec4 AoMapChannel;       // int in [0] R=0,G,B,A
	
	// Render options
	alignas(16) glm::vec4 ShowNormalMap;
	alignas(16) glm::vec4 ExposureBias;

	
	// Create a UniversalUbo packed to match shader standards. MUST unpack in shader.
	static UniversalUbo CreatePacked(const UniversalUboCreateInfo& info, const Material& material)
	{
		UniversalUbo ubo{};
		
		ubo.Model = info.Model;
		ubo.View = info.View;
		ubo.Projection = info.Projection;
		ubo.CamPos = info.CamPos;

		// Material
		ubo.Basecolor = material.Basecolor;
		ubo.Roughness[0] = material.Roughness;
		ubo.Metalness[0] = material.Metalness;

		ubo.UseBasecolorMap[0] = float(material.UseBasecolorMap);
		ubo.UseNormalMap[0] = float(material.UseNormalMap);
		ubo.UseRoughnessMap[0] = float(material.UseRoughnessMap);
		ubo.UseMetalnessMap[0] = float(material.UseMetalnessMap);
		ubo.UseAoMap[0] = float(material.UseAoMap);
		
		ubo.InvertNormalMapZ[0] = float(material.InvertNormalMapZ);
		ubo.InvertRoughnessMap[0] = float(material.InvertRoughnessMap);
		ubo.InvertMetalnessMap[0] = float(material.InvertMetalnessMap);
		ubo.InvertAoMap[0] = float(material.InvertAoMap);

		ubo.RoughnessMapChannel[0] = float(material.RoughnessMapChannel);
		ubo.MetalnessMapChannel[0] = float(material.MetalnessMapChannel);
		ubo.AoMapChannel[0] = float(material.AoMapChannel);
		
		// Render options
		ubo.ShowNormalMap[0] = float(info.ShowNormalMap);
		ubo.ExposureBias[0] = info.ExposureBias;
		
		return ubo;
	}
};
