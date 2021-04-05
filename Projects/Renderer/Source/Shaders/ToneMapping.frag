#version 450

layout(binding = 0) uniform sampler2D screenMap;
layout(std140, binding = 1) uniform Ubo
{
	layout(offset= 0) int   showClipping;
	layout(offset= 4) float exposureBias;

	layout(offset= 8) float vignetteInnerRadius;
	layout(offset=12) float vignetteOuterRadius;
	layout(offset=16) vec3  vignetteColor;
	layout(offset=32) int   enableVignette;

	layout(offset=36) int   enableGrain;
	layout(offset=40) float grainStr;
	layout(offset=44) float grainColorStr;
	layout(offset=48) float grainSize;     // (1.5 - 2.5)

	layout(offset=52) float time;          // seconds
} ubo;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

// ACES Tonemap: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
const mat3 ACESInputMat = mat3(0.59719, 0.07600, 0.02840, 0.35458, 0.90834, 0.13383, 0.04823, 0.01566, 0.83777);
const mat3 ACESOutputMat = mat3(1.60475, -0.10208, -0.00327,-0.53108, 1.10813, -0.07276, -0.07367, -0.00605,  1.07602);
vec3 RRTAndODTFit(vec3 v) { return (v * (v + 0.0245786f) - 0.000090537f) / (v * (0.983729f * v + 0.4329510f) + 0.238081f); }
vec3 ACESFitted(vec3 color) { return clamp(ACESOutputMat * RRTAndODTFit(ACESInputMat * color),0,1); }

void main()
{
	vec3 color = texture(screenMap, inTexCoord).rgb;

	color *= ubo.exposureBias;       // Exposure
	color = ACESFitted(color);       // Tonemap  
	color = pow(color, vec3(1/2.2)); // Gamma: sRGB Linear -> 2.2

	outColor = vec4(color,1);
}
