#version 450

// Constants
const float PI = 3.14159265359;
const int MAX_LIGHT_COUNT = 8;		// TODO Convert to specialization constant to support N lights?
const int PREFILTER_MIP_COUNT = 6;  // Must match PrefilterMap's # mip levels // TODO Pass this in.
const int TRANSPARENCY_MODE_ADDITIVE = 0;
const int TRANSPARENCY_MODE_CUTOFF = 1;

// Types
struct LightPacked
{
	vec4 ColorIntensity;// floats [R,G,B,Intensity]
	vec4 PosType;       // floats [X,Y,Z], int [Type:Point=0,Directional=1]
};

layout(std140, binding = 0) uniform UniversalUbo
{
	//mat4 model;
	//mat4 view;
	//mat4 projection;
	//mat4 lightSpaceMatrix;
	layout(offset=256) mat4 cubemapRotation;
	layout(offset=320) vec3 camPos;

	layout(offset=336) vec3 basecolor;
	layout(offset=352) vec3 scaleNormalMap; // Scales the normals after the map has been transformed to [-1,1] per channel.

	layout(offset=368) 
	bool  useBasecolorMap;   
	bool  useNormalMap;      

	float metalness; 
	int   metalnessMapChannel;	// R=0,G,B,A
	bool  useMetalnessMap;   
	bool  invertMetalnessMap;

	float roughness;        
	bool  useRoughnessMap;	
	bool  invertRoughnessMap;
	int   roughnessMapChannel;	// R=0,G,B,A

	bool  useAoMap;          
	bool  invertAoMap;       
	int   aoMapChannel;        // R=0,G,B,A

	float emissivity;      
	bool  useEmissiveMap;   

	float transparencyCutoffThreshold; 
	bool  useTransparencyMap;
	int   transparencyMapChannel;
	int   transparencyMode;		// 0=Additive, 1=Cutoff

	// Render options
	bool  showNormalMap;   
	bool  showClipping;    
	float exposureBias;   
	float iblStrength;    
} ubo;

layout(binding = 1) uniform samplerCube IrradianceMap; // diffuse
layout(binding = 2) uniform samplerCube PrefilterMap; // spec
layout(binding = 3) uniform sampler2D BrdfLUT; // spec
layout(std140, binding = 4) uniform LightUbo
{
	LightPacked[MAX_LIGHT_COUNT] lights;
	// TODO see this post about dealing with N number of lights OR just pass in N via a specialization constant?
	//  https://www.reddit.com/r/vulkan/comments/8vzpir/whats_the_best_practice_for_dealing_with/
} lightUbo;
layout(binding = 5)  uniform sampler2D BasecolorMap;
layout(binding = 6)  uniform sampler2D NormalMap;
layout(binding = 7)  uniform sampler2D RoughnessMap;
layout(binding = 8)  uniform sampler2D MetalnessMap;
layout(binding = 9)  uniform sampler2D AmbientOcclusionMap;
layout(binding = 10) uniform sampler2D EmissiveMap;
layout(binding = 11) uniform sampler2D TransparencyMap;
layout(binding = 12) uniform sampler2D ShadowMap;

layout(location = 0) in vec4 fragPosLightSpace;
layout(location = 1) in vec3 fragPos; // in world space
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in vec3 fragNormal;
layout(location = 5) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;


// PBR 
vec3 Fresnel_SchlickRoughness(float cosTheta, vec3 F0, float roughness);
vec3 Fresnel_Schlick(float cosTheta, vec3 F0);
float Distribution_GGX(float NdotH, float roughness);
float Geometry_SchlickGGX_Direct(float NdotV, float roughness);
float Geometry_Smith(float NdotV, float NdotL, float roughness);

// Material
vec3 GetBasecolor();
vec3 GetNormal();
float GetRoughness();
float GetMetalness();
float GetAmbientOcclusion();
vec3 GetEmissive();
float GetTransparency();

float textureProj2(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( ShadowMap, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = 0.0;
		}
	}
	return shadow;
}
//
//float ShadowCalculation(vec4 fragPosLightSpace)
//{
//	vec4 projCoords = fragPosLightSpace / fragPosLightSpace.w;
//	//projCoords = projCoords * 0.5 + 0.5;
//	float closestDepth = texture(ShadowMap, projCoords.xy).r;
//	float currentDepth = projCoords.z;
//	float shadow = currentDepth > closestDepth ? 1.0 : 0.0;
//	return shadow;
//}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// main

void main() 
{
//	float shadow = textureProj(fragPosLightSpace / fragPosLightSpace.w, vec2(0.0));
//	if (shadow < 0.90)
//	{
//		outColor = vec4(1,0,0,1);
//		return;
//	}
//	if (shadow > 0.50)
//	{
//		outColor = vec4(0,1,0,1);
//		return;
//	}
//	if (shadow > 0.001)
//	{
//		outColor = vec4(0,0,1,1);
//		return;
//	}

	vec3 normal = GetNormal();
	vec3 basecolor = GetBasecolor();
	float metalness = GetMetalness();
	float roughness = GetRoughness();
	float ao = GetAmbientOcclusion();
	vec3 emissive = GetEmissive();
	float transparency = GetTransparency();
	
	if (ubo.showNormalMap)
	{
		// map from [-1,1] > [0,1]
		vec3 mappedNormal = (normal * 0.5) + 0.5;
		outColor = vec4(mappedNormal, 1.0);
		return;
	}
	
	// Handle cutoff transparency
	if (ubo.transparencyMode == TRANSPARENCY_MODE_CUTOFF)
	{
		if (transparency < ubo.transparencyCutoffThreshold) {
			discard;
		}

		transparency = 1.;
	}


	const vec3 V = normalize(ubo.camPos - fragPos); // View vector
	const float NdotV = max(dot(normal,V),0.0);

	const vec3 F0Default = vec3(0.04); // Good average value for common dielectrics
	vec3 F0 = mix(F0Default, basecolor, metalness); 

	
	// Reflectance equation for direct lighting
	vec3 Lo = vec3(0.0);
	for(int i = 0; i < MAX_LIGHT_COUNT; i++) // Always looping max light count even if there aren't any (it's faster...)
	{
		// Unpack light values
		const vec3 lightPos = lightUbo.lights[i].PosType.xyz;
		const int lightType = int(lightUbo.lights[i].PosType.w);
		const vec3 lightColor = lightUbo.lights[i].ColorIntensity.rgb; 
		const float lightIntensity = lightUbo.lights[i].ColorIntensity.w;

		if (lightIntensity < 0.01) continue; // pretty good optimisation

		// Incoming light direction - point or directional
		const vec3 pointDir = lightPos - fragPos;
		const vec3 directionalDir = lightPos;
		const vec3 L = normalize(mix(pointDir, directionalDir, lightType));
		const float NdotL = max(dot(normal,L), 0.0);
		
		// Compute Radiance - Li
		vec3 incomingRadiance;
		{
			const float dist = length(lightPos - fragPos);
			const float attenuation = mix(1.0 / (dist * dist), 1, lightType); // no attenuation for directional lights
			incomingRadiance = lightColor * lightIntensity * attenuation;
		}

		// Compute BRDF - fr - Cook-Torrance material response
		vec3 brdf;
		{
			const vec3 H = normalize(V + L); // half vec
			const float NdotH = max(dot(normal,H), 0.0);
			const float HdotV = max(dot(H, V), 0.0);

			const float NDF = Distribution_GGX(NdotH, roughness);
			const float G = Geometry_Smith(NdotV, NdotL, roughness);
			const vec3 F = Fresnel_Schlick(HdotV, F0); // Note: HdotV is correct for direct lighting, based on discussion in http://disq.us/p/1etzl77
			const float denominator = 4.0 * NdotV * NdotL;
			const vec3 specular = NDF*G*F / max(denominator, 0.0000001); // safe guard div0

			// Spec/Diff contributions 
			//const vec3 kS = F; // fresnel already represents spec contribution
			vec3 kD = vec3(1.0) - F; // ensure kS+kD=1
			kD *= 1.0 - metalness; // remove diffuse contribution for metals

			vec3 diffuse = kD*basecolor/PI;

			brdf = diffuse + specular;
		}

		// Outgoing radiance due to light hitting surface
		float shadow = textureProj2(fragPosLightSpace / fragPosLightSpace.w, vec2(0.0));
		Lo += brdf * incomingRadiance * NdotL * shadow; // TODO Shadows should be limted shadow casting lights
	}


	vec3 iblAmbient = vec3(0);

	const bool useIbl = true;
	if (useIbl)
	{
		// Compute ratio of diffuse/specular for IBL
		const vec3 F = Fresnel_SchlickRoughness(NdotV, F0, roughness);
		vec3 kD = vec3(1.0) - F; // F = kS term
		kD *= 1.0 - metalness;	

		mat3 cubemapRotationMat3 = mat3(ubo.cubemapRotation);

		// Compute diffuse IBL
		vec3 diffuse;
		{
			const vec3 irradiance = texture(IrradianceMap, cubemapRotationMat3*normal).rgb;
			diffuse = kD * irradiance * basecolor;
		}

		// Compute specular IBL
		vec3 specular;
		{
			// Sample the reflection color from the prefiltered map 
			const vec3 R = reflect(-V, normal); // reflection vector
			const float maxReflectionLod = PREFILTER_MIP_COUNT-1; 
			const vec3 prefilteredColor = textureLod(PrefilterMap, cubemapRotationMat3*R, roughness*maxReflectionLod).rgb;

			// Sample BRDF LUT
			vec2 envBRDF = texture(BrdfLUT, vec2(NdotV, 1-roughness)).rg;

			// Final specular by combining prefilter color and BRDF LUT
			specular = prefilteredColor * (F*envBRDF.x + envBRDF.y);
		}

		// Compute ambient term
		iblAmbient = (diffuse + specular) * ao * ubo.iblStrength; 
	}


	// PUT IT ALL TOGETHER BABY!
	vec3 color = iblAmbient + Lo + emissive;

	outColor = vec4(color, transparency);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PBR Get Properties

vec3 GetBasecolor()
{
	vec3 basecolor = ubo.useBasecolorMap ? texture(BasecolorMap, fragTexCoord).rgb : ubo.basecolor; 
	return pow(basecolor, vec3(2.2)); // sRGB 2.2 -> Linear
}

vec3 GetNormal()
{
	if (ubo.useNormalMap)
	{
		vec3 n = texture(NormalMap, fragTexCoord).xyz;
		n = normalize((n*2 - 1) * ubo.scaleNormalMap); // map [0,1] to [-1,1] then apply any axis scaling.
		n = normalize(fragTBN*n); // transform from tangent to world space
		return n;
	}
	else {
		return fragNormal;
	}
}

float GetRoughness()
{
	float roughness = ubo.roughness; 
	if (ubo.useRoughnessMap)
	{	
		if (ubo.roughnessMapChannel == 0) roughness = texture(RoughnessMap, fragTexCoord).r;
		else if (ubo.roughnessMapChannel == 1) roughness = texture(RoughnessMap, fragTexCoord).g;
		else if (ubo.roughnessMapChannel == 2) roughness = texture(RoughnessMap, fragTexCoord).b;
		else roughness = texture(RoughnessMap, fragTexCoord).a; // assume == 3

		if (ubo.invertRoughnessMap) {
			roughness = 1-roughness;
		}
	}
	return roughness;
}

float GetMetalness()
{
	float metalness = ubo.metalness; 
	if (ubo.useMetalnessMap)
	{	
		if (ubo.metalnessMapChannel == 0) metalness = texture(MetalnessMap, fragTexCoord).r;
		else if (ubo.metalnessMapChannel == 1) metalness = texture(MetalnessMap, fragTexCoord).g;
		else if (ubo.metalnessMapChannel == 2) metalness = texture(MetalnessMap, fragTexCoord).b;
		else metalness = texture(MetalnessMap, fragTexCoord).a; // assume == 3

		if (ubo.invertMetalnessMap) { 
			metalness = 1-metalness; 
		}
	}
	return metalness;
}

float GetAmbientOcclusion()
{
	float ao = 1;
	if (ubo.useAoMap)
	{	
		if (ubo.aoMapChannel == 0) ao = texture(AmbientOcclusionMap, fragTexCoord).r;
		else if (ubo.aoMapChannel == 1) ao = texture(AmbientOcclusionMap, fragTexCoord).g;
		else if (ubo.aoMapChannel == 2) ao = texture(AmbientOcclusionMap, fragTexCoord).b;
		else ao = texture(AmbientOcclusionMap, fragTexCoord).a; // assume == 3 

		if (ubo.invertAoMap) {
			ao = 1-ao;
		}
	}
	return ao;
}

vec3 GetEmissive()
{
	return ubo.useEmissiveMap ? texture(EmissiveMap, fragTexCoord).rgb * ubo.emissivity : vec3(0);
}

float GetTransparency()
{
	float alpha = 1;
	if (ubo.useTransparencyMap)
	{
		if (ubo.transparencyMapChannel == 0) alpha = texture(TransparencyMap, fragTexCoord).r;
		else if (ubo.transparencyMapChannel == 1) alpha = texture(TransparencyMap, fragTexCoord).g;
		else if (ubo.transparencyMapChannel == 2) alpha = texture(TransparencyMap, fragTexCoord).b;
		else alpha = texture(TransparencyMap, fragTexCoord).a; // assume == 3 
	}
	return alpha;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IBL
vec3 Fresnel_Schlick(float cosTheta, vec3 F0)
{
	cosTheta = min(cosTheta,1); // fixes issue where cosTheta is slightly > 1.0. a floating point issue that causes black pixels where the half and view dirs align
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 Fresnel_SchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	cosTheta = min(cosTheta,1); // fixes issue where cosTheta is slightly > 1.0. a floating point issue that causes black pixels where the half and view dirs align
	vec3 factor = max(vec3(1.0 - roughness), F0); // make rough surfaces reflect less strongly on glancing angles
	return F0 + (factor - F0) * pow(1.0 - cosTheta, 5.0);
}

// Calc how much of the microsurface area has its normal exactly aligned with H
float Distribution_GGX(float NdotH, float roughness)
{
	float a = roughness*roughness; // disney found rough^2 had more realistic results
	float a2 = a*a;
	float NdotH2 = NdotH*NdotH;
	float numerator = a2;
	float denominator = NdotH2 * (a2-1.0) + 1.0;
	denominator = PI * denominator * denominator;
	return numerator / max(denominator, 0.0000001);
}

float Geometry_SchlickGGX_Direct(float NdotV, float roughness)
{
	float r = roughness + 1.0; 
	float k = (r*r) / 8; // k computed for direct lighting. we use a diff constant for IBL
	return NdotV / (NdotV * (1.0-k) + k); // bug: div0 if NdotV=0 and k=0?
}

// Calc how much the microsurface area is self shadowing or occluded in the viewing direction.
float Geometry_Smith(float NdotV, float NdotL, float roughness)
{
	float ggx2 = Geometry_SchlickGGX_Direct(NdotV,roughness);
	float ggx1 = Geometry_SchlickGGX_Direct(NdotL,roughness);
	return ggx1*ggx2;
}
