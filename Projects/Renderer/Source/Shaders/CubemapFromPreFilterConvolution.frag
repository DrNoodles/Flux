#version 450 core

// Constants
const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 4096;


layout(push_constant) uniform PushConsts
{
	//layout (offset = 0) mat4 mvp; // not used in frag
	layout (offset = 64) float envMapResPerFace;
	layout (offset = 68) float roughness;
} u;
layout(binding = 0) uniform samplerCube uEnvironmentMap;
layout(location = 0) in vec3 fragWorldPos;
layout(location = 0) out vec4 outColor;


float RadicalInverse_VdC(uint bits);
vec2 Hammersley(uint i, uint N);
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness);
float DistributionGGX(float NdotH, float roughness);

void main()
{
	vec3 normal = normalize(fragWorldPos);
	vec3 R = normal;
	vec3 toEye = R;

	float totalWeight = 0.0;
	vec3 prefilteredColor = vec3(0.0);

	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		vec3 halfway = ImportanceSampleGGX(Xi, normal, u.roughness);
		vec3 lightVec = normalize(2.0*dot(toEye,halfway)*halfway - toEye);

		float NdotL = max(dot(normal, lightVec), 0.0);
		
		if (NdotL > 0.0)
		{
			// Sample from the mip levels to reduce noise in the final PreFilter map
			// https://chetanjags.wordpress.com/2015/08/26/image-based-lighting/

			float NdotH = max(dot(normal, halfway), 0.0);
			float HdotV = max(dot(halfway, toEye), 0.0);
			
			float D = DistributionGGX(NdotH, u.roughness);
			float pdf = (D * NdotH / (4.0 * HdotV)) + 0.0001; 
			
			float saTexel = 4.0 * PI / (6.0 * u.envMapResPerFace * u.envMapResPerFace);
			float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
			
			float mipLevel = u.roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 
			
			
			prefilteredColor += textureLod(uEnvironmentMap, lightVec, mipLevel).rgb * NdotL;
			totalWeight += NdotL;
		}
	}

	prefilteredColor = prefilteredColor / max(totalWeight,0.001f);

	outColor = vec4(prefilteredColor, 1.0);
}




float RadicalInverse_VdC(uint bits) 
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
// Computes a "low-descrepancy sequence" for use in biased Quasi-Monte Carlo sampling.
// A low-descrepancy sequence is biased but converges quick than a pseudorandom sequence.
//	http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}  

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness*roughness;
	
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
	// from spherical coordinates to cartesian coordinates
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
	
	// from tangent-space vector to world-space sample vector
	vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);
	
	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

float DistributionGGX(float NdotH, float roughness)
{
	float a = roughness*roughness; // disney found rough^2 had more realistic results
	float a2 = a*a;
	float NdotH2 = NdotH*NdotH;
	
	float numerator = a2;
	float denominator = NdotH2 * (a2-1.0) + 1.0;
	denominator = PI * denominator * denominator;

	return numerator / denominator; // TODO: safe guard div0
}
