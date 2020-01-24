#version 450 core

layout(std140, binding = 0) uniform SkyboxVertUbo
{
	mat4 projection;
	mat4 view;
	mat3 rotation;
} u;

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 fragWorldPos;



void main()
{
	// The position in the cube also works out to be the UV coordinate. Cubemap magic!
	fragWorldPos = inPos; 

	vec4 pos = u.projection * u.view * vec4(inverse(u.rotation)*inPos, 1.0f);
	gl_Position = pos.xyww; // Setting z = w means the perspective division (z/w) into NDC will make z=1 always, at max depth.
}