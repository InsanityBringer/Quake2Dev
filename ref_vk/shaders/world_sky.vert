#version 450

layout(set = 0, binding = 0) uniform projection_modelviewtype
{
	mat4 projection;
	vec4 translation;
	vec4 rotation;
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
	//This sucks a lot, I should optimize it, but for now replicate the soup of matrix ops id did
	mat4 modelview = rotationAroundX(radians(-90))
		* rotationAroundZ(radians(90)) 
		* rotationAroundX(radians(projection_modelview.rotation.z)) 
		* rotationAroundY(radians(projection_modelview.rotation.x)) 
		* rotationAroundZ(-radians(projection_modelview.rotation.y))
		* translation(-projection_modelview.translation.xyz);
	
	surfacePos = rotateAround(-radians(skyrotate.amount), normalize(skyrotate.axis)) * (position - projection_modelview.translation.xyz);
	vec4 pt = modelview * vec4(position.x, position.y, position.z, 1.0);
	gl_Position = projection_modelview.projection * pt;
	//Try to ensure the sky surface is behind anything else in the depth buffer. Because sometimes sky surfaces can be in front of things, like in xship
	gl_Position.z = 0.99999 * gl_Position.w;
}
