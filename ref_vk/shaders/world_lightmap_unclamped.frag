#version 450

struct dlight
{
	vec3 origin;
	vec3 color;
	float intensity;
};

layout(set = 0, binding = 1) uniform shaderglobalstype
{
	float intensity;
	float varg;
	float modulate;
	int num_dlights;
	dlight lights[32];
} shaderglobals;

layout(set = 1, binding = 0) uniform sampler2D colorTexture;
layout(set = 2, binding = 0) uniform sampler2DArray lightmapTexture;

/*layout(push_constant) uniform lightinfotype
{
	float modulate;
} lightinfo;*/

layout(location = 0) in vec2 outUV;
layout(location = 1) in vec2 outLightUV;
layout(location = 2) in vec3 outWorldPos;
layout(location = 3) flat in uint outLightlayer;

layout(location = 0) out vec4 color;

//Lighting attenuation from the following post:
//https://lisyarus.github.io/blog/graphics/2022/07/30/point-light-attenuation.html
float sqr(float x)
{
	return x * x;
}

float attenuate_no_cusp(float distance, float radius, float max_intensity, float falloff)
{
	float s = distance / radius;

	if (s >= 1.0)
		return 0.0;

	float s2 = sqr(s);

	return max_intensity * sqr(1 - s2) / (1 + falloff * s2);
}

vec4 calc_dlights()
{
	int i;
	
	vec3 accum = vec3(0);
	
	//This should be identical per thread group, right? The data coming in is the same for every single thread
	for (i = 0; i < shaderglobals.num_dlights; i++)
	{
		float dist = distance(outWorldPos, shaderglobals.lights[i].origin);
		//Ick! The intensity shouldn't just be vk_modulate at the origin, but what else do I do here?
		accum += shaderglobals.lights[i].color * attenuate_no_cusp(dist, shaderglobals.lights[i].intensity, shaderglobals.modulate, 1.0);
	}
	
	return vec4(accum, 0.0);
}

void main()
{
	color = clamp(texture(colorTexture, outUV) * shaderglobals.intensity, 0, 1)
		* (texture(lightmapTexture, vec3(outLightUV, outLightlayer)) 
		* shaderglobals.modulate
		+ calc_dlights());
}
