#version 450

//layout(binding = 0) uniform sampler2D screenMap;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main()
{
	//vec3 color = texture(screenMap, inTexCoord).rgb;
	outColor = vec4(1,1,0,1);
}