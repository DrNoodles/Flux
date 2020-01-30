#version 450 core
layout(binding = 1) uniform samplerCube uCubemap;

layout (location=0) in vec3 fragWorldPos; // direction vector representing a 3d texture coord

layout (location=0) out vec4 outColor;

void main()
{
	outColor = vec4(0,1,0,1);
	return;
	vec3 color = textureLod(uCubemap, fragWorldPos, 1).rgb;

	// TODO Support blurring 

	outColor = vec4(color, 1.0);
}