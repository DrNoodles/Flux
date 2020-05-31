#version 450 core
layout(std140, binding = 1) uniform SkyboxFragUbo
{
	layout(offset= 0) float ExposureBias;
	layout(offset= 4) float IblStrength;
	layout(offset= 8) float BackdropBrightness;
	layout(offset= 12) bool ShowClipping;
} ubo;
layout(binding = 2) uniform samplerCube uCubemap;

layout (location=0) in vec3 fragUVW; // direction vector representing a 3d texture coord

layout (location=0) out vec4 outColor;



void main()
{
	vec3 color = texture(uCubemap, fragUVW).rgb;

	// TODO Support blurring 

	color *= ubo.IblStrength;
	color *= ubo.BackdropBrightness;
	
	outColor = vec4(color, 1.0);
}
