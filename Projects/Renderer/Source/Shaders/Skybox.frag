#version 450 core
layout(std140, binding = 1) uniform SkyboxFragUbo
{
	vec4 exposureBias_showClipping_iblStrength_displayBrightness; // [float,bool...,float,float]
} ubo;
layout(binding = 2) uniform samplerCube uCubemap;

layout (location=0) in vec3 fragUVW; // direction vector representing a 3d texture coord

layout (location=0) out vec4 outColor;



// Tonemapping
vec3 ACESFitted(vec3 color);

bool Equals3f(vec3 a, vec3 b, float threshold)// = 0.000001)
{
	return abs(a.r-b.r) < threshold 
		&& abs(a.g-b.g) < threshold 
		&& abs(a.b-b.b) < threshold;
}




void main()
{
	vec3 color = texture(uCubemap, fragUVW).rgb;

	// TODO Support blurring 


	// Post-processing - TODO Move to post pass shader
	color *= ubo.exposureBias_showClipping_iblStrength_displayBrightness[2]; // IblStrength
	color *= ubo.exposureBias_showClipping_iblStrength_displayBrightness[0]; // Exposure
	color *= ubo.exposureBias_showClipping_iblStrength_displayBrightness[3]; // Display Brightness
	color = ACESFitted(color);    // Tonemap
	color = pow(color, vec3(1/2.2));  // Gamma: sRGB Linear -> 2.2
	
	// Shows values clipped at white or black as bold colours
	if (bool(ubo.exposureBias_showClipping_iblStrength_displayBrightness[1]))
	{
		if (Equals3f(color, vec3(1), 0.001)) color = vec3(1,0,1); // Magenta
		if (Equals3f(color, vec3(0), 0.001)) color = vec3(0,0,1); // Blue
	}

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