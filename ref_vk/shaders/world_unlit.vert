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

layout(set = 3, binding = 0) uniform modelview_type
{
	vec4 translation;
	vec4 rotation;
} localmodelview;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in float scroll_speed;
layout(location = 3) in float alpha;

layout(location = 0) out vec2 outUV;
layout(location = 1) out float outAlpha;

mat4 rotationAroundX(float angle)
{
	return mat4(
		vec4(1.0, 0.0, 0.0, 0.0),
		vec4(0.0, cos(angle), -sin(angle), 0.0),
		vec4(0.0, sin(angle), cos(angle), 0.0),
		vec4(0.0, 0.0, 0.0, 1.0));
}

mat4 rotationAroundY(float angle)
{
	return mat4(
		vec4(cos(angle), 0.0, sin(angle), 0.0),
		vec4(0.0, 1.0, 0.0, 0.0),
		vec4(-sin(angle), 0.0, cos(angle), 0.0),
		vec4(0.0, 0.0, 0.0, 1.0));
}

mat4 rotationAroundZ(float angle)
{
	return mat4(
		vec4(cos(angle), sin(angle), 0.0, 0.0),
		vec4(-sin(angle), cos(angle), 0.0, 0.0),
		vec4(0.0, 0.0, 1.0, 0.0),
		vec4(0.0, 0.0, 0.0, 1.0));
}

mat4 translation(vec3 trans)
{
	return mat4(
		vec4(1.0, 0.0, 0.0, 0.0),
		vec4(0.0, 1.0, 0.0, 0.0),
		vec4(0.0, 0.0, 1.0, 0.0),
		vec4(trans.x, trans.y, trans.z, 1.0));
}

void main()
{
	mat4 modelview = projection_modelview.modelview
		* translation(localmodelview.translation.xyz)
		* rotationAroundZ(radians(localmodelview.rotation.y))
		* rotationAroundY(-radians(localmodelview.rotation.x))
		* rotationAroundX(-radians(localmodelview.rotation.z));
	
	vec4 pt = modelview * vec4(position.x, position.y, position.z, 1.0);
	gl_Position = projection_modelview.projection * pt;
	
	outUV = vec2(uv.x - shaderglobals.time * scroll_speed, uv.y);
	outAlpha = alpha;
}
