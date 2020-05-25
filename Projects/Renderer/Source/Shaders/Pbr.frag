#version 450

// Constants
const float PI = 3.14159265359;
const int MAX_LIGHT_COUNT = 8; // TODO Convert to specialization constant to support N lights?
const int PREFILTER_MIP_COUNT = 6; // Must match PrefilterMap's # mip levels // TODO Pass this in.

// Types
struct LightPacked
{
	vec4 ColorIntensity;// floats [R,G,B,Intensity]
	vec4 PosType;       // floats [X,Y,Z], int [Type:Point=0,Directional=1]
};

// TODO Optimise size via juicy packing
layout(std140, binding = 0) uniform UniversalUbo
{
	//mat4 model;
	//mat4 view;
	//mat4 projection;
	
	// PBR
	layout(offset=192) vec3 camPos;

	layout(offset=208) float metalness;          // float in [0]
	int metalnessMapChannel;// int in [0] R=0,G,B,A
	bool useMetalnessMap;    // bool 4bytes
	bool invertMetalnessMap; // bool 4bytes

	// Material
	layout(offset=208) vec3 basecolor;
	vec4 useBasecolorMap;    // bool in [0] 

	vec4 useNormalMap;       // bool in [0]
	vec3 scaleNormalMap;	    // Scales the normals after the map has been transformed to [-1,1] per channel.

	vec4 roughness;          // float in [0]
	vec4 useRoughnessMap;	 // bool in [0]
	vec4 invertRoughnessMap; // bool in [0]
	vec4 roughnessMapChannel;// int in [0] R=0,G,B,A


	vec4 useAoMap;           // bool in [0]
	vec4 invertAoMap;        // bool in [0]
	vec4 aoMapChannel;       // int in [0] R=0,G,B,A

	vec4 emissivity;         // float in [0]
	vec4 useEmissiveMap;     // bool in [0]

	vec4 transparencyCutoffThreshold; // float in [0]
	vec4 useTransparencyMap; // bool in [0]
	vec4 transparencyMapChannel; // int in [0]
	vec4 transparencyMode;   // int in [0]. 0=Additive, 1=Cutoff

	// Render options
	vec4 showNormalMap;      // bool in [0]
	vec4 showClipping;       // bool in [0]
	vec4 exposureBias;       // float in [0]
	vec4 iblStrength;        // float in [0]
	mat4 cubemapRotation;
} ubo;

layout(binding = 1) uniform samplerCube IrradianceMap; // diffuse
layout(binding = 2) uniform samplerCube PrefilterMap; // spec
layout(binding = 3) uniform sampler2D BrdfLUT; // spec
layout(std140, binding = 4) uniform LightUbo
{
	LightPacked[MAX_LIGHT_COUNT] lights;
	// TODO see this post about dealing with N number of lights
	//  https://www.reddit.com/r/vulkan/comments/8vzpir/whats_the_best_practice_for_dealing_with/
} lightUbo;
layout(binding = 5) uniform sampler2D BasecolorMap;
layout(binding = 6) uniform sampler2D NormalMap;
layout(binding = 7) uniform sampler2D RoughnessMap;
layout(binding = 8) uniform sampler2D MetalnessMap;
layout(binding = 9) uniform sampler2D AmbientOcclusionMap;
layout(binding = 10) uniform sampler2D EmissiveMap;
layout(binding = 11) uniform sampler2D TransparencyMap;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;


// Material
vec3 uBasecolor;
float uRoughness;        
float uMetalness;         
float uEmissivity;
float uTransparencyCutoffThreshold;

const int TransparencyMode_Additive = 0;
const int TransparencyMode_Cutoff = 1;
int uTransparencyMode = 0;

bool uUseBasecolorMap;  
bool uUseNormalMap;		 
bool uUseRoughnessMap;	 
bool uUseMetalnessMap;		 
bool uUseAoMap;				 
bool uUseEmissiveMap;
bool uUseTransparencyMap;

//vec3 uInvertNormalMap;	 
bool uInvertAoMap;			 
bool uInvertRoughnessMap; 
bool uInvertMetalnessMap; 

int uRoughnessMapChannel;
int uMetalnessMapChannel;
int uAoMapChannel;       
int uTransparencyMapChannel;

// Render Options
bool uShowNormalMap;
bool uShowClipping;
float uExposureBias;
float uIblStrength;

void UnpackUbos();

// ACES Tonemap: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
const mat3 ACESInputMat = mat3(0.59719, 0.07600, 0.02840, 0.35458, 0.90834, 0.13383, 0.04823, 0.01566, 0.83777);
const mat3 ACESOutputMat = mat3(1.60475, -0.10208, -0.00327,-0.53108, 1.10813, -0.07276, -0.07367, -0.00605,  1.07602);
vec3 RRTAndODTFit(vec3 v) { return (v * (v + 0.0245786f) - 0.000090537f) / (v * (0.983729f * v + 0.4329510f) + 0.238081f); }
vec3 ACESFitted(vec3 color) { return clamp(ACESOutputMat * RRTAndODTFit(ACESInputMat * color),0,1); }

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

bool Equals3f(vec3 a, vec3 b, float threshold)// = 0.000001f)
{
	return abs(a.r-b.r) < threshold 
		&& abs(a.g-b.g) < threshold 
		&& abs(a.b-b.b) < threshold;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// main

void main() 
{
	UnpackUbos();

	vec3 normal = GetNormal();
	vec3 basecolor = GetBasecolor();
	float metalness = GetMetalness();
	float roughness = GetRoughness();
	float ao = GetAmbientOcclusion();
	vec3 emissive = GetEmissive();
	float transparency = GetTransparency();
	
	if (uShowNormalMap)
	{
		// map from [-1,1] > [0,1]
		vec3 mappedNormal = (normal * 0.5) + 0.5;
		outColor = vec4(mappedNormal, 1.0);
		return;
	}
	
	// Handle cutoff transparency
	if (uTransparencyMode == TransparencyMode_Cutoff)
	{
		if (transparency < uTransparencyCutoffThreshold) {
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
		const vec3 directionalDir = -lightPos;
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
		Lo += brdf * incomingRadiance * NdotL;
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
		iblAmbient = (diffuse + specular) * ao * uIblStrength; 
	}


	// PUT IT ALL TOGETHER BABY!
	vec3 color = iblAmbient + Lo + emissive;

	
	// Post-processing - TODO Move to post pass shader
	color *= uExposureBias;	// Exposure
	color = ACESFitted(color); // Tonemap  
	color = pow(color, vec3(1/2.2)); // Gamma: sRGB Linear -> 2.2

	//	// Shows values clipped at white or black as bold colours
	if (uShowClipping)
	{
		if (Equals3f(color, vec3(1), 0.001)) color = vec3(1,0,1); // Magenta
		if (Equals3f(color, vec3(0), 0.001)) color = vec3(0,0,1); // Blue
	}

	outColor = vec4(color, transparency);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PBR Get Properties

vec3 GetBasecolor()
{
	vec3 basecolor = uUseBasecolorMap ? texture(BasecolorMap, fragTexCoord).rgb : uBasecolor; 
	return pow(basecolor, vec3(2.2)); // sRGB 2.2 -> Linear
}
vec3 GetNormal()
{
	if (uUseNormalMap)
	{
		vec3 n = texture(NormalMap, fragTexCoord).xyz;
		n = normalize((n*2 - 1) * ubo.scaleNormalMap); // map [0,1] to [-1,1] then apply any axis scaling.
		n = normalize(fragTBN*n); // transform from tangent to world space
		return n;
	}
	else
	{
		return fragNormal;
	}
}
float GetRoughness()
{
	float roughness = uRoughness; 
	if (uUseRoughnessMap)
	{	
		if (uRoughnessMapChannel == 0) roughness = texture(RoughnessMap, fragTexCoord).r;
		else if (uRoughnessMapChannel == 1) roughness = texture(RoughnessMap, fragTexCoord).g;
		else if (uRoughnessMapChannel == 2) roughness = texture(RoughnessMap, fragTexCoord).b;
		else roughness = texture(RoughnessMap, fragTexCoord).a; // assume == 3

		if (uInvertRoughnessMap)
			roughness = 1-roughness;
	}
	return roughness;
}
float GetMetalness()
{
	float metalness = uMetalness; 
	if (uUseMetalnessMap)
	{	
		if (uMetalnessMapChannel == 0) metalness = texture(MetalnessMap, fragTexCoord).r;
		else if (uMetalnessMapChannel == 1) metalness = texture(MetalnessMap, fragTexCoord).g;
		else if (uMetalnessMapChannel == 2) metalness = texture(MetalnessMap, fragTexCoord).b;
		else metalness = texture(MetalnessMap, fragTexCoord).a; // assume == 3

		if (uInvertMetalnessMap)
			metalness = 1-metalness;
	}
	return metalness;
}
float GetAmbientOcclusion()
{
	float ao = 1;
	if (uUseAoMap)
	{	
		if (uAoMapChannel == 0) ao = texture(AmbientOcclusionMap, fragTexCoord).r;
		else if (uAoMapChannel == 1) ao = texture(AmbientOcclusionMap, fragTexCoord).g;
		else if (uAoMapChannel == 2) ao = texture(AmbientOcclusionMap, fragTexCoord).b;
		else ao = texture(AmbientOcclusionMap, fragTexCoord).a; // assume == 3 

		if (uInvertAoMap)
			ao = 1-ao;
	}
	return ao;
}
vec3 GetEmissive()
{
	return uUseEmissiveMap ? texture(EmissiveMap, fragTexCoord).rgb * uEmissivity : vec3(0);
}
float GetTransparency()
{
	float alpha = 1;
	if (uUseTransparencyMap)
	{
		if (uTransparencyMapChannel == 0) alpha = texture(TransparencyMap, fragTexCoord).r;
		else if (uTransparencyMapChannel == 1) alpha = texture(TransparencyMap, fragTexCoord).g;
		else if (uTransparencyMapChannel == 2) alpha = texture(TransparencyMap, fragTexCoord).b;
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



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UBOs - must match packing on CPU side

void UnpackUbos()
{
	uMetalness = ubo.metalness;
	uUseMetalnessMap = ubo.useMetalnessMap; // int to bool
	uInvertMetalnessMap = ubo.invertMetalnessMap; // int to bool
	uMetalnessMapChannel = ubo.metalnessMapChannel;

	// Material
	uBasecolor = ubo.basecolor;
	uRoughness = ubo.roughness[0];
	uEmissivity = ubo.emissivity[0];
	uTransparencyCutoffThreshold = ubo.transparencyCutoffThreshold[0];
	uTransparencyMode = int(ubo.transparencyMode[0]);

	uUseBasecolorMap = bool(ubo.useBasecolorMap[0]);
	uUseNormalMap = bool(ubo.useNormalMap[0]);
	uUseRoughnessMap = bool(ubo.useRoughnessMap[0]);
	uUseAoMap = bool(ubo.useAoMap[0]);
	uUseEmissiveMap = bool(ubo.useEmissiveMap[0]);
	uUseTransparencyMap = bool(ubo.useTransparencyMap[0]);

	uInvertRoughnessMap = bool(ubo.invertRoughnessMap[0]);
	uInvertAoMap = bool(ubo.invertAoMap[0]);
	
	uRoughnessMapChannel = int(ubo.roughnessMapChannel[0]);
	uAoMapChannel = int(ubo.aoMapChannel[0]);
	uTransparencyMapChannel = int(ubo.transparencyMapChannel[0]);
	

	// Render options
	uShowNormalMap = bool(ubo.showNormalMap[0]);
	uShowClipping = bool(ubo.showClipping[0]);
	uExposureBias = ubo.exposureBias[0];
	uIblStrength = ubo.iblStrength[0];
}

