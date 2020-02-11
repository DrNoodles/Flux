#version 450 core
layout(binding = 1) uniform samplerCube uCubemap;

layout (location=0) in vec3 fragUVW; // direction vector representing a 3d texture coord

layout (location=0) out vec4 outColor;



// Tonemapping
vec3 ACESFitted(vec3 color);

void main()
{
	vec3 color = texture(uCubemap, fragUVW).rgb;

	// TODO Support blurring 

	float exposureBias = 1;

	// Post-processing - TODO Move to post pass shader
	color *= exposureBias; // Exposure
	color = ACESFitted(color); // Tonemap
	color = pow(color, vec3(1/2.2));  // Gamma: sRGB Linear -> 2.2

	outColor = vec4(color, 1.0);
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