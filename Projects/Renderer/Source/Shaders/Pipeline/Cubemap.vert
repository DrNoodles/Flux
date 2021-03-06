#version 450 core 

layout(std140, push_constant) uniform PushConsts 
{
	layout (offset = 0) mat4 mvp;
} pushConsts;

layout (location = 0) in vec3 inPos;
layout (location = 0) out vec3 fragWorldPos;

void main()
{
	fragWorldPos = inPos;
	gl_Position = pushConsts.mvp * vec4(inPos, 1.0);
}