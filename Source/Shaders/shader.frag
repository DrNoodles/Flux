#version 450

layout(binding = 1) uniform sampler2D basecolorTexSampler;
layout(binding = 2) uniform sampler2D normalTexSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;

const vec3 lightDir = normalize(vec3(-1,1,1));
const bool doDisplayDebugNormalMap = false;


vec3 GetNormalFromMap()
{	
	vec3 N = texture(normalTexSampler, fragTexCoord).xyz; 
	N = normalize(N*2 - 1); // map [0,1] to [-1,1]
	N = normalize(fragTBN*N); // transform from tangent to world space
	return N;
}


void main() 
{
	vec3 normal = GetNormalFromMap();
	
	if (doDisplayDebugNormalMap)
	{
		// map from [-1,1] > [0,1]
		vec3 mappedNormal = (normal * 0.5) + 0.5;
		outColor = vec4(mappedNormal, 1.0);
		return;
	}


	float lightStrength = max(dot(lightDir, normal),0);

	vec3 color = lightStrength * texture(basecolorTexSampler, fragTexCoord).rgb;

	outColor = vec4(color,1.0);
}