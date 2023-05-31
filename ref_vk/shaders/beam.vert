#version 450

layout(set = 0, binding = 0) uniform projection_modelviewtype
{
	mat4 projection;
	vec4 translation;
	vec4 rotation;
} projection_modelview;

layout(set = 3, binding = 0) uniform modelview_type
{
	vec3 start;
	int pad;
	vec3 end;
	int pad2;
	vec3 color;
	float alpha;
	vec3 tangent;
	float diameter;
} localmodelview;

struct vertex_entry
{
	vec3 position;
	vec3 normal;
};

layout(set = 2, binding = 0) buffer vertbuffer_type
{
	vertex_entry vertices[];
} vertbuffer;

layout(location = 0) in vec3 position;

layout(location = 0) out vec4 color;

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
	mat4 modelview = rotationAroundX(radians(-90))
		* rotationAroundZ(radians(90)) 
		* rotationAroundX(radians(projection_modelview.rotation.z)) 
		* rotationAroundY(radians(projection_modelview.rotation.x)) 
		* rotationAroundZ(-radians(projection_modelview.rotation.y))
		* translation(-projection_modelview.translation.xyz);
		
	vec3 dir = localmodelview.end - localmodelview.start;
	vec3 bitangent = cross(normalize(dir), localmodelview.tangent);

	
	vec4 pos = modelview * vec4(localmodelview.start + dir * position.x + localmodelview.tangent * localmodelview.diameter * position.y + bitangent * localmodelview.diameter * position.z, 1.0);
	gl_Position = projection_modelview.projection * pos;
	color = vec4(localmodelview.color, localmodelview.alpha);
}
