#version 450

layout(binding = 1) uniform sampler2D basecolorTexSampler;
layout(binding = 2) uniform sampler2D normalTexSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;

// Directional Light (no attenuation)
const float dirLightStrength = 10;
const vec3 dirLightColor = vec3(1,1,1);
const vec3 dirLightDir = normalize(vec3(-1,1,1));

// Point Light (with attenuation)
const float pointLightStrength = 10;
const vec3 pointLightColor = vec3(1,1,1);
const vec3 pointLightPos = normalize(vec3(-10,-10,-10));

const bool doDisplayDebugNormalMap = false;

// Tonemapping
const float exposureBias = 0.5;


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
	
	if (doDisplayDebugNormalMap)
	{
		// map from [-1,1] > [0,1]
		vec3 mappedNormal = (normal * 0.5) + 0.5;
		outColor = vec4(mappedNormal, 1.0);
		return;
	}

	// Directional light
	float dirLightfactor = max(dot(dirLightDir, normal),0);
	vec3 dirLightContribution = dirLightStrength * dirLightfactor * dirLightColor;

	// Point light

	vec3 color = dirLightContribution * texture(basecolorTexSampler, fragTexCoord).rgb;



	// Tonemap  
	color *= exposureBias;
	color = ACESFitted(color);


	outColor = vec4(color,1.0);
}