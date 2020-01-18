#version 450

layout(binding = 0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 projection;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;
//layout(location = 5) in vec3 inBitangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out mat3 fragTBN;

void main() 
{
	vec3 T = normalize(vec3(ubo.model * vec4(inTangent, 0)));
//	vec3 B = normalize(vec3(ubo.model * vec4(inBitangent, 0)));
	vec3 N = normalize(vec3(ubo.model * vec4(inNormal, 0)));
	vec3 B = normalize(cross(N,T));

	mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));

	// Outputs
	fragColor = inColor;
	fragTexCoord = inTexCoord;
	fragNormal = normalize(normalMatrix * inNormal);
	fragTBN = mat3(T, B, N);
	gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 1.0);
}