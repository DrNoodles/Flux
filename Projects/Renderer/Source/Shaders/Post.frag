#version 450

layout(binding = 0) uniform sampler2D screenMap;
layout(std140, binding = 1) uniform Ubo
{
	layout(offset= 0) int   showClipping;
	layout(offset= 4) float exposureBias;
	layout(offset= 8) float vignetteInnerRadius;
	layout(offset=12) float vignetteOuterRadius;
	layout(offset=16) vec3  vignetteColor;
	layout(offset=32) int   enableVignette;
} ubo;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;



// ACES Tonemap: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
const mat3 ACESInputMat = mat3(0.59719, 0.07600, 0.02840, 0.35458, 0.90834, 0.13383, 0.04823, 0.01566, 0.83777);
const mat3 ACESOutputMat = mat3(1.60475, -0.10208, -0.00327,-0.53108, 1.10813, -0.07276, -0.07367, -0.00605,  1.07602);
vec3 RRTAndODTFit(vec3 v) { return (v * (v + 0.0245786f) - 0.000090537f) / (v * (0.983729f * v + 0.4329510f) + 0.238081f); }
vec3 ACESFitted(vec3 color) { return clamp(ACESOutputMat * RRTAndODTFit(ACESInputMat * color),0,1); }


bool Equals3f(vec3 a, vec3 b, float threshold)// = 0.000001)
{
	return abs(a.r-b.r) < threshold 
		&& abs(a.g-b.g) < threshold 
		&& abs(a.b-b.b) < threshold;
}

float linearstep(float a, float b, float v)
{
	float len = b-a;
	return clamp((v-a)/len, 0, 1);
}







// TODO pass in time
float timer = 2;

const float permTexUnit = 1.0/256.0;		// Perm texture texel-size
const float permTexUnitHalf = 0.5/256.0;	// Half perm texture texel-size

const float grainamount = 0.05; //grain amount
bool colored = true; //colored noise?
float coloramount = 0.6;
float grainsize = 2.5; //grain particle size (1.5 - 2.5)
float lumamount = 1.0; //
    
//a random texture generator, but you can also use a pre-computed perturbation texture
vec4 rnm(in vec2 tc) 
{
    float noise =  sin(dot(tc + vec2(timer,timer),vec2(12.9898,78.233))) * 43758.5453;

	float noiseR =  fract(noise)*2.0-1.0;
	float noiseG =  fract(noise*1.2154)*2.0-1.0; 
	float noiseB =  fract(noise*1.3453)*2.0-1.0;
	float noiseA =  fract(noise*1.3647)*2.0-1.0;
	
	return vec4(noiseR,noiseG,noiseB,noiseA);
}

float fade(in float t) {
	return t*t*t*(t*(t*6.0-15.0)+10.0);
}

float pnoise3D(in vec3 p)
{
	vec3 pi = permTexUnit*floor(p)+permTexUnitHalf; // Integer part, scaled so +1 moves permTexUnit texel
	// and offset 1/2 texel to sample texel centers
	vec3 pf = fract(p);     // Fractional part for interpolation

	// Noise contributions from (x=0, y=0), z=0 and z=1
	float perm00 = rnm(pi.xy).a ;
	vec3  grad000 = rnm(vec2(perm00, pi.z)).rgb * 4.0 - 1.0;
	float n000 = dot(grad000, pf);
	vec3  grad001 = rnm(vec2(perm00, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
	float n001 = dot(grad001, pf - vec3(0.0, 0.0, 1.0));

	// Noise contributions from (x=0, y=1), z=0 and z=1
	float perm01 = rnm(pi.xy + vec2(0.0, permTexUnit)).a ;
	vec3  grad010 = rnm(vec2(perm01, pi.z)).rgb * 4.0 - 1.0;
	float n010 = dot(grad010, pf - vec3(0.0, 1.0, 0.0));
	vec3  grad011 = rnm(vec2(perm01, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
	float n011 = dot(grad011, pf - vec3(0.0, 1.0, 1.0));

	// Noise contributions from (x=1, y=0), z=0 and z=1
	float perm10 = rnm(pi.xy + vec2(permTexUnit, 0.0)).a ;
	vec3  grad100 = rnm(vec2(perm10, pi.z)).rgb * 4.0 - 1.0;
	float n100 = dot(grad100, pf - vec3(1.0, 0.0, 0.0));
	vec3  grad101 = rnm(vec2(perm10, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
	float n101 = dot(grad101, pf - vec3(1.0, 0.0, 1.0));

	// Noise contributions from (x=1, y=1), z=0 and z=1
	float perm11 = rnm(pi.xy + vec2(permTexUnit, permTexUnit)).a ;
	vec3  grad110 = rnm(vec2(perm11, pi.z)).rgb * 4.0 - 1.0;
	float n110 = dot(grad110, pf - vec3(1.0, 1.0, 0.0));
	vec3  grad111 = rnm(vec2(perm11, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
	float n111 = dot(grad111, pf - vec3(1.0, 1.0, 1.0));

	// Blend contributions along x
	vec4 n_x = mix(vec4(n000, n001, n010, n011), vec4(n100, n101, n110, n111), fade(pf.x));

	// Blend contributions along y
	vec2 n_xy = mix(n_x.xy, n_x.zw, fade(pf.y));

	// Blend contributions along z
	float n_xyz = mix(n_xy.x, n_xy.y, fade(pf.z));

	// We're done, return the final noise value.
	return n_xyz;
}

//2d coordinate orientation thing
vec2 coordRot(in vec2 tc, in float angle, in float width, in float height)
{
	float aspect = width/height;
	float rotX = ((tc.x*2.0-1.0)*aspect*cos(angle)) - ((tc.y*2.0-1.0)*sin(angle));
	float rotY = ((tc.y*2.0-1.0)*cos(angle)) + ((tc.x*2.0-1.0)*aspect*sin(angle));
	rotX = ((rotX/aspect)*0.5+0.5);
	rotY = rotY*0.5+0.5;
	return vec2(rotX,rotY);
}

void mainer(vec2 texCoord, float width, float height, inout vec3 col) 
{
	vec3 rotOffset = vec3(1.425,3.892,5.835); //rotation offset values	
	vec2 rotCoordsR = coordRot(texCoord, timer + rotOffset.x, width, height);
	vec3 noise = vec3(pnoise3D(vec3(rotCoordsR*vec2(width/grainsize,height/grainsize),0.0)));
  
	if (colored)
	{
		vec2 rotCoordsG = coordRot(texCoord, timer + rotOffset.y, width, height);
		vec2 rotCoordsB = coordRot(texCoord, timer + rotOffset.z, width, height);
		noise.g = mix(noise.r,pnoise3D(vec3(rotCoordsG*vec2(width/grainsize,height/grainsize),1.0)),coloramount);
		noise.b = mix(noise.r,pnoise3D(vec3(rotCoordsB*vec2(width/grainsize,height/grainsize),2.0)),coloramount);
	}

	//noisiness response curve based on scene luminance
	vec3 lumcoeff = vec3(0.299,0.587,0.114);
	float luminance = mix(0.0,dot(col, lumcoeff),lumamount);
	float lum = smoothstep(0.2,0.0,luminance);
	lum += luminance;
	
	
	noise = mix(noise,vec3(0.0),pow(lum,4.0));
	col = col+noise*grainamount;
}























void main()
{
	ivec2 res = textureSize(screenMap, 0);

	vec3 color = texture(screenMap, inTexCoord).rgb;

	color *= ubo.exposureBias;          // Exposure
	color = ACESFitted(color);       // Tonemap  
	color = pow(color, vec3(1/2.2)); // Gamma: sRGB Linear -> 2.2



	
	// Transform a uv so that middle = (0,0), topLeft = (-1, -1/aspect), botRight = (1, 1/aspect)
	float aspect = res.x/float(res.y);
	vec2 uv = (inTexCoord-.5) * 2 * vec2(aspect, 1);


	// Preview UV coordinates
	const bool previewUvCoords = false;
	if (previewUvCoords)
	{
		color.r = mix(0, 1, uv.x > 0);
		color.g = mix(0, 1, uv.y > 0);
		color.b = mix(0, 1, uv.x < 0 && uv.y < 0);

		color = mix(vec3(1,0,1), color, smoothstep(0.05, 0.051, length(uv-vec2(0))));
		color = mix(vec3(0,1,1), color, smoothstep(0.05, 0.051, length(uv-vec2(-aspect, -1.))));
		color = mix(vec3(0,0,1), color, smoothstep(0.05, 0.051, length(uv-vec2(aspect, 1.))));
	}

	
	// Vignette
	
	if (bool(ubo.enableVignette))
	{
		//vec3 vinCol = vec3(0,0,0);
		float innerRadius = ubo.vignetteInnerRadius*aspect;
		float outerRadius = ubo.vignetteOuterRadius*aspect;

		float dist = distance(uv, vec2(0));
		//color = dist > innerRadius*aspect ? vinCol : color;
		color = mix(color, ubo.vignetteColor, smoothstep(innerRadius, outerRadius, dist));
	}

//void mainer(vec2 texCoord, float width, float height, inout vec3 col) 

	mainer(uv, float(res.x), float(res.y), color);

	// Shows values clipped at white or black as bold colours
	if (bool(ubo.showClipping))
	{
		if (Equals3f(color, vec3(1), 0.001)) color = vec3(1,0,1); // Magenta
		if (Equals3f(color, vec3(0), 0.001)) color = vec3(0,0,1); // Blue
	}

	outColor = vec4(color,1);
}

