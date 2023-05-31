#version 450

layout(set = 0, binding = 1) uniform shaderglobalstype
{
	float intensity;
	float time;
	float modulate;
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

vec4 clamptolargest(vec4 inv)
{
	float largest = inv.r;
	if (inv.g > largest)
		largest = inv.g;
	if (inv.b > largest)
		largest = inv.b;
		
	if (largest < 1.0)
		return inv;
		
	float scale = 1.0 / largest;
	
	return vec4(inv.rgb * scale, 1.0);
}

/*vec4 clamptolargest(vec4 inv)
{
	float largest = inv.r;
	if (inv.g > largest)
		largest = inv.g;
	if (inv.b > largest)
		largest = inv.b;
	
	return vec4(largest, largest, largest, 1.0);
}*/

void main()
{
	color = clamp(texture(colorTexture, outUV) * shaderglobals.intensity, 0, 1)
		* clamptolargest(vec4(texture(lightmapTexture, vec3(outLightUV, outLightlayer)).rgb 
		* shaderglobals.modulate, 1.0));
}
