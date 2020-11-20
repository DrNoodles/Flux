#version 450

layout(std140, binding = 0) uniform UniversalUbo
{
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 lightSpaceMatrix;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;

layout(location = 0) out vec4 fragPosLightSpace;
layout(location = 1) out vec3 fragPos; // in world space
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out vec3 fragNormal;
layout(location = 5) out mat3 fragTBN;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main() 
{
	vec3 T = normalize(vec3(ubo.model * vec4(inTangent, 0)));
	vec3 N = normalize(vec3(ubo.model * vec4(inNormal, 0)));
	vec3 B = normalize(cross(N,T));

	mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));

	// Outputs
	fragPos = vec3(ubo.model * vec4(inPosition, 1.0));
	fragColor = inColor;
	fragTexCoord = inTexCoord;
	fragNormal = normalize(normalMatrix * inNormal);
	fragTBN = mat3(T, B, N);
	fragPosLightSpace = (biasMat * ubo.lightSpaceMatrix * ubo.model) * vec4(inPosition, 1);
	//fragPosLightSpace = (ubo.lightSpaceMatrix * ubo.model) * vec4(inPosition, 1);
	gl_Position = ubo.projection * ubo.view * vec4(fragPos, 1);
}