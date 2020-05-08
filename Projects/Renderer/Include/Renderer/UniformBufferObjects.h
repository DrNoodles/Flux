#pragma once

#include "GpuTypes.h"
#include <Framework/CommonTypes.h>
#include <Framework/Material.h>

#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#define GLM_ENABLE_EXPERIMENTAL // for hash
#include <glm/gtx/hash.hpp>



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SkyboxVertUbo
{
	alignas(16) glm::mat4 Projection;
	alignas(16) glm::mat4 View;
	alignas(16) glm::mat4 Rotation;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SkyboxFragUbo
{
	alignas(16) glm::vec4 ExposureBias_ShowClipping_IblStrength; // [float,bool...,float,-]
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct LightPacked
{
	alignas(16) glm::vec4 LightColorIntensity;// floats [R,G,B,Intensity]
	alignas(16) glm::vec4 LightPosType;       // floats [X,Y,Z], int [Type]
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct LightUbo
{
	alignas(16)	LightPacked Lights[8];

	static LightUbo Create(const std::vector<Light>& lights)
	{
		const u32 maxLights = 8;
		assert(lights.size() <= maxLights);
		
		LightUbo ubo{};
		
		for (size_t i = 0; i < lights.size(); i++)
		{
			auto& light = lights[i];
			ubo.Lights[i].LightColorIntensity[0] = light.Color.r;
			ubo.Lights[i].LightColorIntensity[1] = light.Color.g;
			ubo.Lights[i].LightColorIntensity[2] = light.Color.b;
			ubo.Lights[i].LightColorIntensity[3] = light.Intensity;
			ubo.Lights[i].LightPosType[0] = light.Pos.x;
			ubo.Lights[i].LightPosType[1] = light.Pos.y;
			ubo.Lights[i].LightPosType[2] = light.Pos.z;
			ubo.Lights[i].LightPosType[3] = float(light.Type);
		}

		return ubo;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct UniversalUboCreateInfo
{
	glm::mat4 Model{};
	glm::mat4 View{};
	glm::mat4 Projection{};
	glm::vec3 CamPos{};

	// Render options
	bool ShowNormalMap = false;
	bool ShowClipping = false;
	float IblStrength = 1.0f;
	float ExposureBias = 1.0f;
	float CubemapRotation = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO use union to achieve packing data into vec4s
struct UniversalUbo
{
	// Data types and alignment to match shader exactly.
	alignas(16) glm::mat4 Model;         
	alignas(16) glm::mat4 View;          
	alignas(16) glm::mat4 Projection;    
	alignas(16) glm::vec3 CamPos;

	// Material
	alignas(16) glm::vec3 Basecolor;
	alignas(16) glm::vec4 UseBasecolorMap;    // bool in [0] 

	alignas(16) glm::vec4 UseNormalMap;       // bool in [0]
	alignas(16) glm::vec3 ScaleNormalMap;     // 
	
	alignas(16) glm::vec4 Roughness;          // float in [0]
	alignas(16) glm::vec4 UseRoughnessMap;    // bool in [0]
	alignas(16) glm::vec4 InvertRoughnessMap; // bool in [0]
	alignas(16) glm::vec4 RoughnessMapChannel;// int in [0] R=0,G,B,A

	alignas(16) glm::vec4 Metalness;          // float in [0]
	alignas(16) glm::vec4 UseMetalnessMap;    // bool in [0]
	alignas(16) glm::vec4 InvertMetalnessMap; // bool in [0]
	alignas(16) glm::vec4 MetalnessMapChannel;// int in [0] R=0,G,B,A

	alignas(16) glm::vec4 UseAoMap;           // bool in [0]
	alignas(16) glm::vec4 InvertAoMap;        // bool in [0]
	alignas(16) glm::vec4 AoMapChannel;       // int in [0] R=0,G,B,A

	alignas(16) glm::vec4 Emissivity;         // float in [0]
	alignas(16) glm::vec4 UseEmissiveMap;     // bool in [0]
	
	// Render options
	alignas(16) glm::vec4 ShowNormalMap;      // bool in [0]
	alignas(16) glm::vec4 ShowClipping;       // bool in [0]
	alignas(16) glm::vec4 ExposureBias;       // float in [0]
	alignas(16) glm::vec4 IblStrength;        // float in [0]
	alignas(16) glm::mat4 CubemapRotation;


	
	// Create a UniversalUbo packed to match shader standards. MUST unpack in shader.
	static UniversalUbo Create(const UniversalUboCreateInfo& info, const Material& material)
	{
		UniversalUbo ubo{};
		
		ubo.Model = info.Model;
		ubo.View = info.View;
		ubo.Projection = info.Projection;
		ubo.CamPos = info.CamPos;

		// Material
		ubo.Basecolor = material.Basecolor;
		ubo.UseBasecolorMap[0] = float(material.UsingBasecolorMap());

		ubo.UseNormalMap[0] = float(material.UsingNormalMap());
		ubo.ScaleNormalMap = glm::vec3(1, material.InvertNormalMapY ? -1 : 1, material.InvertNormalMapZ ? -1 : 1);

		ubo.Roughness[0] = material.Roughness;
		ubo.UseRoughnessMap[0] = float(material.UsingRoughnessMap());
		ubo.InvertRoughnessMap[0] = float(material.InvertRoughnessMap);
		ubo.RoughnessMapChannel[0] = float(material.RoughnessMapChannel);

		ubo.Metalness[0] = material.Metalness;
		ubo.UseMetalnessMap[0] = float(material.UsingMetalnessMap());
		ubo.InvertMetalnessMap[0] = float(material.InvertMetalnessMap);
		ubo.MetalnessMapChannel[0] = float(material.MetalnessMapChannel);

		ubo.UseAoMap[0] = float(material.UsingAoMap());
		ubo.InvertAoMap[0] = float(material.InvertAoMap);
		ubo.AoMapChannel[0] = float(material.AoMapChannel);

		ubo.Emissivity[0] = material.EmissiveIntensity;
		ubo.UseEmissiveMap[0] = float(material.UsingEmissiveMap());
		
		// Render options
		ubo.ShowNormalMap[0] = float(info.ShowNormalMap);
		ubo.ShowClipping[0] = float(info.ShowClipping);
		ubo.ExposureBias[0] = info.ExposureBias;
		ubo.IblStrength[0] = info.IblStrength;
		ubo.CubemapRotation = glm::rotate(glm::radians(info.CubemapRotation), glm::vec3{ 0,1,0 });
		
		return ubo;
	}
};
