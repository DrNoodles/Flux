#version 450 core

const float PI = 3.14159265359;

layout(binding = 0) uniform samplerCube uEnvironmentMap;
layout(location = 0) in vec3 fragWorldPos;
layout(location = 0) out vec4 outColor;

//layout(std140, push_constant) uniform PushConsts {
//	layout (offset = 64) float deltaPhi;
//	layout (offset = 68) float deltaTheta;
//} consts;

// Computes the irradiance in a hemisphere about a view direction
void main()
{
	vec3 normal = normalize(fragWorldPos);
	vec3 irradiance = vec3(0.0);

	vec3 up    = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));


	// Riemann sum adding radiance at fixed intervals on a hemisphere oriented about the normal.
	float deltaPhi = 0.025;
	float deltaTheta = 0.025;
	float nrSamples = 0.0; 

	for(float phi = 0.0; phi < 2.0 * PI; phi += deltaPhi)
	{
		for(float theta = 0.0; theta < 0.5 * PI; theta += deltaTheta)
		{
			// spherical to cartesian (in tangent space)
			vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
			// tangent space to world
			vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 

			// Note: cos(theta) scales the radiance based on the view angle (physical property).
			// Note: sin(theta) scales the radiance to equally weight the radiance results. Needed as samples on the sphere are tightly packed towards the poles.
			irradiance += texture(uEnvironmentMap, sampleVec).rgb * cos(theta) * sin(theta);
			nrSamples++;
		}
	}

	irradiance = PI * irradiance * (1.0 / float(nrSamples));

	outColor = vec4(irradiance, 1.0);
}