#version 450

layout(std140, binding = 0) uniform UniversalUbo
{
	mat4 model;
	mat4 view;
	mat4 projection;
	vec3 camPos;
	vec4 showNormalMap; // bool in [0]
	vec4 exposureBias;  // float in [0]
} ubo;
layout(binding = 1) uniform sampler2D basecolorTexSampler;
layout(binding = 2) uniform sampler2D normalTexSampler;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;

// Unpack UniversalUbo - must match packing on CPU side
bool showNormalMap;
float exposureBias;
void UnpackUbos()
{
	showNormalMap = bool(ubo.showNormalMap[0]);
	exposureBias = ubo.exposureBias[0];
}


// Define lights
struct Light
{
	vec3 pos;
	vec3 color;
	float intensity;
};

const int pointLightCount = 3;
const Light lights[pointLightCount] = Light[](
	Light(vec3(20,10,10), vec3(0.9,0.3,0.3), 2500)  // key
	,Light(vec3(-20,-5,10), vec3(0.2,0.2,0.7), 700)  // fill
	,Light(vec3(0,5,-20), vec3(1.0,1.0,1.0), 3000)  // rim
);

const float PI = 3.14159265359;

//// Directional Light (no attenuation)
//const float dirLightStrength = 1;
//const vec3 dirLightColor = vec3(0.2,0.2,0.8);
//const vec3 dirLightDir = normalize(vec3(-1,-1,1));


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


// PBR 
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
float DistributionGGX(float NdotH, float roughness);
float GeometrySchlickGGX_Direct(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);


vec3 GetNormalFromMap()
{	
	vec3 N = texture(normalTexSampler, fragTexCoord).xyz; 
	N = normalize(N*2 - 1); // map [0,1] to [-1,1]
	N = normalize(fragTBN*N); // transform from tangent to world space
	return N;
}


void main() 
{
	UnpackUbos();

	vec3 albedo = texture(basecolorTexSampler, fragTexCoord).xyz;
	float metallic = 0;
	float roughness = 0.3;
	//float ao = 1.0;
	vec3 normal = GetNormalFromMap();
	
	if (showNormalMap)
	{
		// map from [-1,1] > [0,1]
		vec3 mappedNormal = (normal * 0.5) + 0.5;
		outColor = vec4(mappedNormal, 1.0);
		return;
	}

	

	vec3 V = normalize(ubo.camPos - fragPos); // view vector


	vec3 F0 = vec3(0.04); // good average value for common dielectrics
	F0 = mix(F0, albedo, metallic);

	
	// Reflectance equation for direct lighting
	vec3 Lo = vec3(0.0);
	
	for(int i = 0; i < pointLightCount; i++)
	{
		vec3 L = normalize(lights[i].pos - fragPos); // light direction
		vec3 H = normalize(V + L);
		

		// Compute Radiance //
		float dist = length(lights[i].pos - fragPos);
		float attenuation = 1.0 / (dist * dist);
		vec3 radiance = lights[i].color * lights[i].intensity * attenuation;


		// BRDF - Cook-Torrance//
		float NdotH = max(dot(normal,H),0.0);

		float NDF = DistributionGGX(NdotH, roughness);
		float G = GeometrySmith(normal, V, L, roughness);
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
		float denominator = 4.0 * max(dot(V,normal),0.0) * max(dot(L,normal),0.0);
		vec3 specular = NDF*G*F / max(denominator, 0.0000001); // safe guard div0

		// Spec/Diff contributions 
		vec3 kS = F; // fresnel already represents spec contribution
		vec3 kD = vec3(1.0) - kS; // (kS + kD = 1)
		kD *= 1.0 - metallic; // remove diffuse contribution for metals

		// Outgoing radiance due to light hitting surface
		float NdotL = max(dot(normal,L), 0.0);
		Lo += (kD*albedo/PI + specular) * radiance * NdotL;
	}


	vec3 ambient = vec3(0.1);

	vec3 color = ambient + Lo;
	
	// Tonemap  
	color *= exposureBias;
	color = ACESFitted(color);


	outColor = vec4(color,1.0);
}





vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	cosTheta = min(cosTheta,1); // fixes issue where cosTheta is slightly > 1.0. a floating point issue that causes black pixels where the half and view dirs align
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
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




//	vec3 ambientLight = vec3(0.1,0.1,0.1);
//	vec3 lightContribution = vec3(0,0,0);
//
//	// Directional light
//	float dirLightfactor = max(dot(dirLightDir, normal),0);
//	lightContribution += dirLightStrength * dirLightfactor * dirLightColor;
//
//
//	// Point light
//	vec3 pointLightDisplacement = pointLightPos - fragPos;
//	float pointLightDistance = length(pointLightDisplacement);
//	vec3 pointLightDir = pointLightDisplacement / pointLightDistance;
//	float pointLightAttenuation = 1 / (pointLightDistance * pointLightDistance);
//	float pointLightfactor = max(dot(pointLightDir, normal),0);
//	lightContribution += pointLightStrength * pointLightAttenuation * pointLightfactor * pointLightColor;
//
//
//	vec3 color = (ambientLight + lightContribution) * texture(basecolorTexSampler, fragTexCoord).rgb;
