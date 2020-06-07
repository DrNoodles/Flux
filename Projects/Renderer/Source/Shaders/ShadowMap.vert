#version 450

layout(binding = 0) uniform ShadowUbo
{
	mat4 projectionViewModel;
} ubo;

layout(location = 0) in vec3 inPosition;


void main() 
{
	gl_Position = ubo.projectionViewModel * vec4(inPosition, 1.0);
}