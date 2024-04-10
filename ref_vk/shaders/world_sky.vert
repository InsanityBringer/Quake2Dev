#version 450

layout(set = 0, binding = 0) uniform projection_modelviewtype
{
	mat4 projection;
	mat4 modelview;
	vec3 pos;
} projection_modelview;

layout(push_constant) uniform skyrotatetype
{
	vec3 axis;
	float amount;
} skyrotate;

layout(location = 0) in vec3 position;

layout(location = 0) out vec3 surfacePos;

//Goldman's formula, from Real Time Rendering 3rd edition, pg 71
//Given that this involves a normalize, I wonder if the more formal method would be more optimized. 
mat3 rotateAround(float angle, vec3 axis)
{
	float angcos = cos(angle);
	float angsin = sin(angle);
	return mat3(
		vec3(angcos + (1 - angcos) * (axis.x * axis.x), (1 - angcos) * axis.x * axis.y + axis.z * angsin, (1 - angcos) * axis.x * axis.z - axis.y * angsin),
		vec3((1 - angcos) * axis.x * axis.y - axis.z * angsin, angcos + (1 - angcos) * (axis.y * axis.y), (1 - angcos) * axis.y * axis.z + axis.x * angsin),
		vec3((1 - angcos) * axis.x * axis.z + axis.y * angsin, (1 - angcos) * axis.y * axis.z - axis.x * angsin, angcos + (1 - angcos) * (axis.z * axis.z)));
}

void main()
{
	surfacePos = rotateAround(-radians(skyrotate.amount), normalize(skyrotate.axis)) * (position - projection_modelview.pos);
	vec4 pt = projection_modelview.modelview * vec4(position.x, position.y, position.z, 1.0);
	gl_Position = projection_modelview.projection * pt;
	//Try to ensure the sky surface is behind anything else in the depth buffer. Because sometimes sky surfaces can be in front of things, like in xship
	gl_Position.z = 0.99999 * gl_Position.w;
}
