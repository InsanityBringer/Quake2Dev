#version 450

layout(set = 1, binding = 0) uniform sampler2D colorTexture;

layout(location = 0) in vec2 outUV;
layout(location = 1) in float outAlpha;

layout(location = 0) out vec4 color;

void main()
{
	color = vec4(texture(colorTexture, outUV).rgb, outAlpha);
}
