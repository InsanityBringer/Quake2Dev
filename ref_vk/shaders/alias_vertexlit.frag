#version 450

layout(set = 0, binding = 1) uniform shaderglobalstype
{
	float intensity;
} shaderglobals;
layout(set = 1, binding = 0) uniform sampler2D colorTexture;

layout(set = 3, binding = 0) uniform modelview_type
{
	vec3 translation;
	int frameOffset;
	vec3 rotation;
	int oldFrameOffset;
	vec3 color;
	float backlerp;
	vec3 move;
	float swell;
	vec3 pad;
	float alpha;
} localmodelview;

layout(location = 0) in vec2 outUV;
layout(location = 1) in vec3 outLight;

layout(location = 0) out vec4 color;

void main()
{
	color = vec4(clamp(texture(colorTexture, outUV).rgb * shaderglobals.intensity, 0, 1) * outLight, localmodelview.alpha);
}
