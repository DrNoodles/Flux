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
	alignas(4) f32  ExposureBias;
	alignas(4) f32  IblStrength;
	alignas(4) f32  BackdropBrightness;
	alignas(4) bool ShowClipping;
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
			const auto& l = lights[i];
			ubo.Lights[i].LightColorIntensity[0] = l.Color.r;
			ubo.Lights[i].LightColorIntensity[1] = l.Color.g;
			ubo.Lights[i].LightColorIntensity[2] = l.Color.b;
			ubo.Lights[i].LightColorIntensity[3] = l.Intensity;
			ubo.Lights[i].LightPosType[0] = l.Pos.x;
			ubo.Lights[i].LightPosType[1] = l.Pos.y;
			ubo.Lights[i].LightPosType[2] = l.Pos.z;
			ubo.Lights[i].LightPosType[3] = f32(l.Type);
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
	glm::mat4 LightSpaceMatrix{};
	glm::vec3 CamPos{};

	// Render options
	bool ShowNormalMap = false;
	bool ShowClipping = false;
	float IblStrength = 1.0f;
	float ExposureBias = 1.0f;
	float CubemapRotation = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct UniversalUbo
{
	// Data types and alignment to match shader exactly.
	alignas(16) glm::mat4 Model;         
	alignas(16) glm::mat4 View;          
	alignas(16) glm::mat4 Projection;
	alignas(16) glm::mat4 LightSpaceMatrix;
	
	alignas(16) glm::mat4 CubemapRotation;
	alignas(16) glm::vec3 CamPos;

	// Material
	alignas(16) glm::vec3 Basecolor;
	alignas(16) glm::vec3 ScaleNormalMap;
	
	alignas(16) bool UseBasecolorMap; 
	alignas(4)  bool UseNormalMap;    
				   
	alignas(4)  f32  Metalness;   
	alignas(4)  i32  MetalnessMapChannel; // R=0,G,B,A
	alignas(4)  bool UseMetalnessMap;    
	alignas(4)  bool InvertMetalnessMap; 
				   
	alignas(4)  f32  Roughness;          
	alignas(4)  bool UseRoughnessMap;    
	alignas(4)  bool InvertRoughnessMap; 
	alignas(4)  i32  RoughnessMapChannel; // R=0,G,B,A
				   
	alignas(4)  bool UseAoMap;        
	alignas(4)  bool InvertAoMap;     
	alignas(4)  i32  AoMapChannel;        // R=0,G,B,A
			  	   
	alignas(4)  f32  Emissivity;       
	alignas(4)  bool UseEmissiveMap;   
			  	   
	alignas(4)  f32  TransparencyCutoffThreshold; 
	alignas(4)  bool UseTransparencyMap;
	alignas(4)  i32  TransparencyMapChannel; 
	alignas(4)  i32  TransparencyMode;    // 0=Additive, 1=Cutoff
			  
	// Renderoptions
	alignas(4)  bool ShowNormalMap;
	alignas(4)  bool ShowClipping; 
	alignas(4)  f32  ExposureBias;  
	alignas(4)  f32  IblStrength;   


	
	// Create a UniversalUbo packed to match shader standards. MUST unpack in shader.
	static UniversalUbo Create(const UniversalUboCreateInfo& info, const Material& material)
	{
		UniversalUbo ubo{};
		
		ubo.Model = info.Model;
		ubo.View = info.View;
		ubo.Projection = info.Projection;
		ubo.LightSpaceMatrix = info.LightSpaceMatrix;
		ubo.CamPos = info.CamPos;

		// Material
		ubo.Basecolor = material.Basecolor;
		ubo.UseBasecolorMap = material.UsingBasecolorMap();

		ubo.UseNormalMap = material.UsingNormalMap();
		ubo.ScaleNormalMap = glm::vec3(1, material.InvertNormalMapY ? -1 : 1, material.InvertNormalMapZ ? -1 : 1);

		ubo.Roughness = material.Roughness;
		ubo.UseRoughnessMap = material.UsingRoughnessMap();
		ubo.InvertRoughnessMap = material.InvertRoughnessMap;
		ubo.RoughnessMapChannel = (int)material.RoughnessMapChannel;

		ubo.Metalness = material.Metalness;
		ubo.UseMetalnessMap = material.UsingMetalnessMap();
		ubo.InvertMetalnessMap = material.InvertMetalnessMap;
		ubo.MetalnessMapChannel = (int)material.MetalnessMapChannel;

		ubo.UseAoMap = material.UsingAoMap();
		ubo.InvertAoMap = material.InvertAoMap;
		ubo.AoMapChannel = int(material.AoMapChannel);

		ubo.Emissivity = material.EmissiveIntensity;
		ubo.UseEmissiveMap = material.UsingEmissiveMap();

		ubo.TransparencyCutoffThreshold = material.TransparencyCutoffThreshold;
		ubo.UseTransparencyMap = material.UsingTransparencyMap();
		ubo.TransparencyMapChannel = int(material.TransparencyMapChannel);
		ubo.TransparencyMode = int(material.TransparencyMode);
		
		// Render options
		ubo.ShowNormalMap = info.ShowNormalMap;
		ubo.ShowClipping = info.ShowClipping;
		ubo.ExposureBias = info.ExposureBias;
		ubo.IblStrength = info.IblStrength;
		ubo.CubemapRotation = glm::rotate(glm::radians(info.CubemapRotation), glm::vec3{ 0,1,0 });
		
		return ubo;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PostUbo
{
	alignas(4)  i32       ShowClipping = false;
	alignas(4)  f32       ExposureBias = 1;
	
	alignas(4)  f32       VignetteInnerRadius = 0.8f;
	alignas(4)  f32       VignetteOuterRadius = 1.5f;
	alignas(16) glm::vec3 VignetteColor = glm::vec3(0);
	alignas(16) i32       EnableVignette = false;

	alignas(4)  i32       EnableGrain = true;
	alignas(4)  f32       GrainStrength = 0.05f;
	alignas(4)  f32       GrainColorStrength = 0.6f;
	alignas(4)  f32       GrainSize = 1.6f; // (1.5 - 2.5)
	
	alignas(4)  f32       Time = 2; // TODO pass in running time
};