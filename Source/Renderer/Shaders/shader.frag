#version 450

// Types
struct LightPacked
{
	vec4 lightColorIntensity;// floats [R,G,B,Intensity]
	vec4 lightPosType;       // floats [X,Y,Z], int [Type]
};
struct Light
{
	vec3 pos;
	vec3 color;
	float intensity;
	int type; // Point=0, Directional=1
};

// TODO Optimise size via juicy packing
layout(std140, binding = 0) uniform UniversalUbo
{
	mat4 model;
	mat4 view;
	mat4 projection;
	
	// PBR
	vec3 camPos;

	// Material
	vec3 basecolor;
	vec4 roughness;          // float in [0]
	vec4 metalness;          // float in [0]

	vec4 useBasecolorMap;    // bool in [0] 
	vec4 useNormalMap;		 // bool in [0]
	vec4 useRoughnessMap;	 // bool in [0]
	vec4 useMetalnessMap;    // bool in [0]
	vec4 useAoMap;				 // bool in [0]

	vec4 invertNormalMapZ;	 // bool in [0]
	vec4 invertRoughnessMap; // bool in [0]
	vec4 invertMetalnessMap; // bool in [0]
	vec4 invertAoMap;			 // bool in [0]
	 
	vec4 roughnessMapChannel;// int in [0] R=0,G,B,A
	vec4 metalnessMapChannel;// int in [0] R=0,G,B,A
	vec4 aoMapChannel;       // int in [0] R=0,G,B,A

	// Render options
	vec4 showNormalMap;      // bool in [0]
	vec4 exposureBias;       // float in [0]
} ubo;
layout(binding = 1) uniform sampler2D BasecolorMap;
layout(binding = 2) uniform sampler2D NormalMap;
layout(binding = 3) uniform sampler2D RoughnessMap;
layout(binding = 4) uniform sampler2D MetalnessMap;
layout(binding = 5) uniform sampler2D AmbientOcclusionMap;
layout(std140, binding = 6) uniform LightUbo
{
	vec4 lightCount;           // int in [0]
	LightPacked[4] lights;
} lightUbo;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const int MAX_LIGHT_COUNT = 4;


// Material
vec3 uBasecolor;
float uRoughness;        
float uMetalness;         

bool uUseBasecolorMap;  
bool uUseNormalMap;		 
bool uUseRoughnessMap;	 
bool uUseMetalnessMap;		 
bool uUseAoMap;				 

bool uInvertNormalMapZ;	 
bool uInvertAoMap;			 
bool uInvertRoughnessMap; 
bool uInvertMetalnessMap; 

int uRoughnessMapChannel;
int uMetalnessMapChannel;
int uAoMapChannel;       

// Render Options
bool uShowNormalMap;
float uExposureBias;

// Lights
int uLightCount = 0; // TODO Load from UBO
Light uLights[MAX_LIGHT_COUNT];


// UBOs - must match packing on CPU side
void UnpackUbos()
{
	// Material
	uBasecolor = ubo.basecolor;
	uRoughness = float(ubo.roughness[0]);
	uMetalness = float(ubo.metalness[0]);

	uUseBasecolorMap = bool(ubo.useBasecolorMap[0]);
	uUseNormalMap = bool(ubo.useNormalMap[0]);
	uUseRoughnessMap = bool(ubo.useRoughnessMap[0]);
	uUseMetalnessMap = bool(ubo.useMetalnessMap[0]);
	uUseAoMap = bool(ubo.useAoMap[0]);
	
	uInvertNormalMapZ = bool(ubo.invertNormalMapZ[0]);
	uInvertRoughnessMap = bool(ubo.invertRoughnessMap[0]);
	uInvertMetalnessMap = bool(ubo.invertMetalnessMap[0]);
	uInvertAoMap = bool(ubo.invertAoMap[0]);
	
	uRoughnessMapChannel = int(ubo.roughnessMapChannel[0]);
	uMetalnessMapChannel = int(ubo.metalnessMapChannel[0]);
	uAoMapChannel = int(ubo.aoMapChannel[0]);

	// Render options
	uShowNormalMap = bool(ubo.showNormalMap[0]);
	uExposureBias = ubo.exposureBias[0];

	// Light
	uLightCount = int(lightUbo.lightCount[0]);
	for (int i = 0; i < uLightCount; i++) // this loop has GOTTA be bad for perf. maybe even catastrophic?
	{
		uLights[i].color = lightUbo.lights[i].lightColorIntensity.rgb;
		uLights[i].intensity = lightUbo.lights[i].lightColorIntensity.a;
		uLights[i].pos = lightUbo.lights[i].lightPosType.xyz;
		uLights[i].type = int(lightUbo.lights[i].lightPosType.w);
	}
}


// Tonemapping
vec3 ACESFitted(vec3 color);

// PBR 
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);
vec3 FresnelSchlick(float cosTheta, vec3 F0);
float DistributionGGX(float NdotH, float roughness);
float GeometrySchlickGGX_Direct(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

// Material
vec3 GetBasecolor();
vec3 GetNormal();
float GetRoughness();
float GetMetalness();
float GetAmbientOcclusion();



void main() 
{
	UnpackUbos();

	vec3 normal = GetNormal();
	vec3 basecolor = GetBasecolor();
	float metalness = GetMetalness();
	float roughness = GetRoughness();
	float ao = GetAmbientOcclusion();
	
	if (uShowNormalMap)
	{
		// map from [-1,1] > [0,1]
		vec3 mappedNormal = (normal * 0.5) + 0.5;
		outColor = vec4(mappedNormal, 1.0);
		return;
	}


	vec3 V = normalize(ubo.camPos - fragPos); // view vector

	vec3 F0 = vec3(0.04); // good average value for common dielectrics
	F0 = mix(F0, basecolor, metalness);

	
	// Reflectance equation for direct lighting
	vec3 Lo = vec3(0.0);
	
	for(int i = 0; i < uLightCount; i++)
	{
		vec3 L = normalize(uLights[i].pos - fragPos); // light direction
		vec3 H = normalize(V + L);
		

		// Compute Radiance //
		float dist = length(uLights[i].pos - fragPos);
		float attenuation = 1.0 / (dist * dist);
		vec3 radiance = uLights[i].color * uLights[i].intensity * attenuation;


		// BRDF - Cook-Torrance//
		float NdotH = max(dot(normal,H),0.0);

		float NDF = DistributionGGX(NdotH, roughness);
		float G = GeometrySmith(normal, V, L, roughness);
		vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
		float denominator = 4.0 * max(dot(V,normal),0.0) * max(dot(L,normal),0.0);
		vec3 specular = NDF*G*F / max(denominator, 0.0000001); // safe guard div0

		// Spec/Diff contributions 
		vec3 kS = F; // fresnel already represents spec contribution
		vec3 kD = vec3(1.0) - kS; // (kS + kD = 1)
		kD *= 1.0 - metalness; // remove diffuse contribution for metals

		// Outgoing radiance due to light hitting surface
		float NdotL = max(dot(normal,L), 0.0);
		Lo += (kD*basecolor/PI + specular) * radiance * NdotL;
	}


	vec3 ambient = vec3(0.0);

	vec3 color = ambient + Lo;
	
	// Tonemap  
	color *= uExposureBias;
	color = ACESFitted(color);


	outColor = vec4(color,1.0);
}


vec3 GetBasecolor()
{
	vec3 basecolor = uUseBasecolorMap ? texture(BasecolorMap, fragTexCoord).rgb : uBasecolor; 
	basecolor = pow(basecolor, vec3(2.2)); // sRGB 2.2 > Linear
	return basecolor;
}
vec3 GetNormal()
{
	if (uUseNormalMap)
	{
		vec3 n = texture(NormalMap, fragTexCoord).xyz;
		n = normalize(n*2 - 1); // map [0,1] to [-1,1]
		
		if (uInvertNormalMapZ)
			n.z = -n.z;
		
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



vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	cosTheta = min(cosTheta,1); // fixes issue where cosTheta is slightly > 1.0. a floating point issue that causes black pixels where the half and view dirs align
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	cosTheta = min(cosTheta,1); // fixes issue where cosTheta is slightly > 1.0. a floating point issue that causes black pixels where the half and view dirs align
	vec3 factor = max(vec3(1.0 - roughness), F0); // make rough surfaces reflect less strongly on glancing angles
	return F0 + (factor - F0) * pow(1.0 - cosTheta, 5.0);
}
float DistributionGGX(float NdotH, float roughness)
{
	float a = roughness*roughness; // disney found rough^2 had more realistic results
	float a2 = a*a;
	float NdotH2 = NdotH*NdotH;
	
	float numerator = a2;
	float denominator = NdotH2 * (a2-1.0) + 1.0;
	denominator = PI * denominator * denominator;

	return numerator / denominator; // TODO: safe guard div0
}
float GeometrySchlickGGX_Direct(float NdotV, float roughness)
{
	float r = roughness + 1.0; 
	float k = (r*r) / 8; // k computed for direct lighting. we use a diff constant for IBL
	return NdotV / (NdotV * (1.0-k) + k); // bug: div0 if NdotV=0 and k=0?
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N,V), 0.0);
	float NdotL = max(dot(N,L), 0.0);
	float ggx2 = GeometrySchlickGGX_Direct(NdotV,roughness);
	float ggx1 = GeometrySchlickGGX_Direct(NdotL,roughness);
	return ggx1*ggx2;
}



// ACES Tonemap: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
const mat3 ACESInputMat = mat3(
	0.59719, 0.07600, 0.02840, 
	0.35458, 0.90834, 0.13383,
	0.04823, 0.01566, 0.83777
);
const mat3 ACESOutputMat = mat3(
	 1.60475, -0.10208, -0.00327,
	-0.53108,  1.10813, -0.07276,
	-0.07367, -0.00605,  1.07602
);
vec3 RRTAndODTFit(vec3 v)
{
	vec3 a = v * (v + 0.0245786f) - 0.000090537f;
	vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}
vec3 ACESFitted(vec3 color)
{
	// Aces pipe: Input
	// > InputDeviceTransform (IDT)
	// > Look Modification Transform (LMT) 
	// > Reference Rendering Transform (RRT) 
	// > Output Device Transform (ODT) - per output type (eg, SDR, HDR10, etc..)
	color = ACESInputMat * color;
	color = RRTAndODTFit(color);
	color = ACESOutputMat * color;
	color = clamp(color,0,1);
	return color;
}

