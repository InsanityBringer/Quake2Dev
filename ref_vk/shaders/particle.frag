#version 450

layout(push_constant) uniform particlesizetype
{
	float basesize;
	float min, max;
	int mode;
} particleinfo;

layout(location = 0) in vec4 outColor;

layout(location = 0) out vec4 color;

void main()
{
	vec2 uv = gl_PointCoord * 2 - vec2(1, 1);
	color = outColor;
	if (particleinfo.mode == 1 && length(uv) >= 1)
		color.a = 0;
}
