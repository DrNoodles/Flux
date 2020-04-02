#version 450 core

// Generates a BRDF 2D LUT with x axis=n*v and y axis=roughness

const float PI = 3.14159265359;

layout (location = 0) in vec2 fragTexCoords;

layout (location = 0) out vec2 outColor;



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


float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
	// note that we compute a different k for IBL
	float k = (roughness * roughness) / 2.0;
	return NdotV / (NdotV * (1.0-k) + k); // bug: div0 if NdotV=0 and k=0?
}


float GeometrySmith(float NdotV, float NdotL, float roughness)
{
	float ggx2 = GeometrySchlickGGX_IBL(NdotV,roughness);
	float ggx1 = GeometrySchlickGGX_IBL(NdotL,roughness);
	return ggx1*ggx2;
}


vec2 IntegrateBRDF(float NdotV, float roughness)
{
	// Generate a sample view vector
	vec3 toEye;
	toEye.x = sqrt(1.0 - NdotV*NdotV);
	toEye.y = 0.0;
	toEye.z = NdotV;

	float A = 0.0;
	float B = 0.0;

	vec3 N = vec3(0.0, 0.0, 1.0);

	const uint SAMPLE_COUNT = 1024u;
	for(uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		vec3 halfway  = ImportanceSampleGGX(Xi, N, roughness);
		vec3 light  = normalize(2.0*dot(toEye, halfway)*halfway - toEye);

		float NdotL = max(light.z, 0.0); // optimised with N=<0,0,1>
		if(NdotL > 0.0)
		{
			float NdotH = max(halfway.z, 0.0); // optimised with N=<0,0,1>
			float VdotH = max(dot(toEye, halfway), 0.0);
			//float NdotV = max(toEye.z, 0.0); // TODO Confirm these computations are the same as input NdotV

			float G = GeometrySmith(NdotV, NdotL, roughness);
			float G_Vis = (G * VdotH) / (NdotH * NdotV);
			float Fc = pow(1.0 - VdotH, 5.0);

			A += (1.0 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}
	A /= float(SAMPLE_COUNT);
	B /= float(SAMPLE_COUNT);
	return vec2(A, B);
}


void main()
{
	outColor = IntegrateBRDF(fragTexCoords.x, fragTexCoords.y);
}
