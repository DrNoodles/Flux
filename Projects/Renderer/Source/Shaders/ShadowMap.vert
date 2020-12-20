#version 450

layout(std140, binding = 0) uniform UniversalUbo
{
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 lightSpaceMatrix;
} ubo;

layout(location = 0) in vec3 inPosition;


void main() 
{
	gl_Position = ubo.lightSpaceMatrix * ubo.model * vec4(inPosition, 1.0);
}