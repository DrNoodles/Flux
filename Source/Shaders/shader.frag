#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

const vec3 lightDir = normalize(vec3(-1,1,1));

void main() 
{
//	outColor = vec4(fragNormal,1.0);
//	return;

	float lightStrength = clamp(dot(lightDir, fragNormal),0,1);

	vec3 color = lightStrength * texture(texSampler, fragTexCoord).rgb;
	outColor = vec4(color,1.0);
}