#version 450

layout(set = 0, binding = 0) uniform projection_modelviewtype
{
	mat4 projection;
	mat4 modelview;
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

void main()
{		
	vec3 dir = localmodelview.end - localmodelview.start;
	vec3 bitangent = cross(normalize(dir), localmodelview.tangent);

	
	vec4 pos = projection_modelview.modelview * vec4(localmodelview.start + dir * position.x + localmodelview.tangent * localmodelview.diameter * position.y + bitangent * localmodelview.diameter * position.z, 1.0);
	gl_Position = projection_modelview.projection * pos;
	color = vec4(localmodelview.color, localmodelview.alpha);
}
