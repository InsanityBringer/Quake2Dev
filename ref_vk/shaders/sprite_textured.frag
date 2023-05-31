#version 450

layout(set = 1, binding = 0) uniform sampler2D colorTexture;

layout(location = 0) in vec2 outUV;
layout(location = 1) in float alpha;

layout(location = 0) out vec4 color;

void main()
{
	color = texture(colorTexture, outUV);
	if (color.a <= 0) //alpha test on non-translucent sprites
		discard;
	
	color.a *= alpha;
}
