#version 450

layout(set = 0, binding = 1) uniform shaderglobalstype
{
	float intensity;
	float varg;
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

void main()
{
	color = vec4(1.0) * texture(lightmapTexture, vec3(outLightUV, outLightlayer)) * shaderglobals.modulate;
}
