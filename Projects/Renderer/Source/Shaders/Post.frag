#version 450

layout(binding = 0) uniform sampler2D screenMap;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

bool _showClipping = false;
float _exposureBias = 1.;

// ACES Tonemap: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
const mat3 ACESInputMat = mat3(0.59719, 0.07600, 0.02840, 0.35458, 0.90834, 0.13383, 0.04823, 0.01566, 0.83777);
const mat3 ACESOutputMat = mat3(1.60475, -0.10208, -0.00327,-0.53108, 1.10813, -0.07276, -0.07367, -0.00605,  1.07602);
vec3 RRTAndODTFit(vec3 v) { return (v * (v + 0.0245786f) - 0.000090537f) / (v * (0.983729f * v + 0.4329510f) + 0.238081f); }
vec3 ACESFitted(vec3 color) { return clamp(ACESOutputMat * RRTAndODTFit(ACESInputMat * color),0,1); }

bool Equals3f(vec3 a, vec3 b, float threshold)// = 0.000001)
{
	return abs(a.r-b.r) < threshold 
		&& abs(a.g-b.g) < threshold 
		&& abs(a.b-b.b) < threshold;
}

void main()
{
	vec3 color = texture(screenMap, inTexCoord).rgb;

	color *= _exposureBias;          // Exposure
	color = ACESFitted(color);       // Tonemap  
	color = pow(color, vec3(1/2.2)); // Gamma: sRGB Linear -> 2.2

	// Shows values clipped at white or black as bold colours
	if (_showClipping)
	{
		if (Equals3f(color, vec3(1), 0.001)) color = vec3(1,0,1); // Magenta
		if (Equals3f(color, vec3(0), 0.001)) color = vec3(0,0,1); // Blue
	}

	outColor = vec4(color,1);
}