#version 450

// TODO Optimise size via juicy packing
layout(std140, binding = 0) uniform UniversalUbo
{
	mat4 model;
	mat4 view;
	mat4 projection;
	// PBR
	vec3 camPos;

	// Material
	vec3 basecolor;
	vec4 roughness;          // float in [0]
	vec4 metalness;          // float in [0]

	vec4 useBasecolorMap;    // bool in [0] 
	vec4 useNormalMap;       // bool in [0]
	vec4 useRoughnessMap;    // bool in [0]
	vec4 useMetalnessMap;    // bool in [0]
	vec4 useAoMap;           // bool in [0]

	vec4 invertNormalMapZ;   // bool in [0]
	vec4 invertRoughnessMap; // bool in [0]
	vec4 invertMetalnessMap; // bool in [0]
	vec4 invertAoMap;        // bool in [0]
	 
	vec4 roughnessMapChannel;// int in [0] R=0,G,B,A
	vec4 metalnessMapChannel;// int in [0] R=0,G,B,A
	vec4 aoMapChannel;       // int in [0] R=0,G,B,A

	// Render options
	vec4 showNormalMap;      // bool in [0]
	vec4 exposureBias;       // float in [0]
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;

layout(location = 0) out vec3 fragPos; // in world space
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragNormal;
layout(location = 4) out mat3 fragTBN;

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
	gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 1.0);
}