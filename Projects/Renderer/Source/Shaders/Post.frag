#version 450

layout(binding = 0) uniform sampler2D screenMap;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main()
{
	vec3 color = texture(screenMap, inTexCoord).rgb;

	// Invert right half for kicks
	if (inTexCoord.x > 0.5) {
		color = 1 - color; 
	}

	outColor = vec4(color,1);
}