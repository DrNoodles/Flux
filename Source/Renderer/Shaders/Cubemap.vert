#version 450 core 

layout(std140, binding = 0) uniform CubemapVertUbo
{
	mat4 projection;
	mat4 view;
} u;

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 fragWorldPos;

void main()
{
	fragWorldPos = inPos;
	gl_Position = u.projection * u.view * vec4(inPos, 1.0);
}