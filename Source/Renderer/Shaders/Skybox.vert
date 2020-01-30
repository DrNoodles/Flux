#version 450 core

layout(std140, binding = 0) uniform SkyboxVertUbo
{
	mat4 projection;
	mat4 view;
//	mat3 rotation;
} u;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;

layout (location = 0) out vec3 fragWorldPos;



void main()
{
	// The position in the cube also works out to be the UV coordinate. Cubemap magic!
	fragWorldPos = inPosition; 

	vec4 pos = u.projection * u.view * vec4(inPosition.xyz, 1.0f);//vec4(inverse(u.rotation)*inPosition, 1.0f);
	gl_Position = pos.xyww; // Setting z = w means the perspective division (z/w) into NDC will make z=1 always, at max depth.
}