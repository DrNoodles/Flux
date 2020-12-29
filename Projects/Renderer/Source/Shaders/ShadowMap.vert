#version 450

layout(std140, push_constant) uniform PushConstants
{
	layout (offset = 0) mat4 shadowMatrix;
} pushConsts;

layout(location = 0) in vec3 inPosition;


void main() 
{
	gl_Position = pushConsts.shadowMatrix * vec4(inPosition, 1.0);
}