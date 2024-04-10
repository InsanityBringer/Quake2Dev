#version 450

layout(set = 0, binding = 0) uniform projection_modelviewtype
{
	mat4 projection;
	mat4 modelview;
} projection_modelview;

layout(set = 0, binding = 1) uniform shaderglobalstype
{
	float intensity;
	float time;
} shaderglobals;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in float scroll_speed;
layout(location = 3) in vec2 light_uv;
layout(location = 4) in uint light_layer;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec2 outLightUV;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) flat out uint outLightlayer;

void main()
{
	outWorldPos = position;

	vec4 pt = projection_modelview.modelview * vec4(position.x, position.y, position.z, 1.0);
	gl_Position = projection_modelview.projection * pt;
	
	outUV = vec2(uv.x - fract(shaderglobals.time * scroll_speed), uv.y);
	outLightUV = light_uv;
	outLightlayer = light_layer;
}
