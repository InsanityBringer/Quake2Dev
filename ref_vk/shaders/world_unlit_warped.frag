#version 450

layout(set = 0, binding = 1) uniform shaderglobalstype
{
	float intensity;
	float time;
} shaderglobals;
layout(set = 1, binding = 0) uniform sampler2D colorTexture;

layout(location = 0) in vec2 outUV;
layout(location = 1) in float outAlpha;

layout(location = 0) out vec4 color;

void main()
{
	vec2 worldSpaceUV = outUV * 64;
	vec2 newUV = worldSpaceUV + vec2(sin((worldSpaceUV.y + shaderglobals.time * 20) * 6.283185307 / 128) * 8, sin((worldSpaceUV.x + shaderglobals.time * 20) * 6.283185307 / 128) * 8);
	newUV /= 64;
			
	color = vec4(texture(colorTexture, newUV).rgb, outAlpha);
}
