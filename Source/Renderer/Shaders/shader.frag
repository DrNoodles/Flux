#version 450

layout(binding = 0, std140) uniform UniversalUbo
{
	bool drawNormalMap;
	float exposureBias;
	mat4 model;
	mat4 view;
	mat4 projection;
} ubo;
layout(binding = 1) uniform sampler2D basecolorTexSampler;
layout(binding = 2) uniform sampler2D normalTexSampler;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Directional Light (no attenuation)
const float dirLightStrength = 1;
const vec3 dirLightColor = vec3(0.2,0.2,0.8);
const vec3 dirLightDir = normalize(vec3(-1,-1,1));

// Point Light (with attenuation)
const float pointLightStrength = 40;
const vec3 pointLightColor = vec3(10,0.7,0.6);
const vec3 pointLightPos = vec3(20,5,0);



vec3 GetNormalFromMap()
{	
	vec3 N = texture(normalTexSampler, fragTexCoord).xyz; 
	N = normalize(N*2 - 1); // map [0,1] to [-1,1]
	N = normalize(fragTBN*N); // transform from tangent to world space
	return N;
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





void main() 
{
	vec3 normal = GetNormalFromMap();
	
	if (ubo.drawNormalMap)
	{
		// map from [-1,1] > [0,1]
		vec3 mappedNormal = (normal * 0.5) + 0.5;
		outColor = vec4(mappedNormal, 1.0);
		return;
	}

	vec3 ambientLight = vec3(0.1,0.1,0.1);
	vec3 lightContribution = vec3(0,0,0);

	// Directional light
	float dirLightfactor = max(dot(dirLightDir, normal),0);
	lightContribution += dirLightStrength * dirLightfactor * dirLightColor;


	// Point light
	vec3 pointLightDisplacement = pointLightPos - fragPos;
	float pointLightDistance = length(pointLightDisplacement);
	vec3 pointLightDir = pointLightDisplacement / pointLightDistance;
	float pointLightAttenuation = 1 / (pointLightDistance * pointLightDistance);
	float pointLightfactor = max(dot(pointLightDir, normal),0);
	lightContribution += pointLightStrength * pointLightAttenuation * pointLightfactor * pointLightColor;


	vec3 color = (ambientLight + lightContribution) * texture(basecolorTexSampler, fragTexCoord).rgb;

	// Tonemap  
	color *= ubo.exposureBias;
	color = ACESFitted(color);


	outColor = vec4(color,1.0);
}