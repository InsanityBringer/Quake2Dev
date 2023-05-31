#version 450

layout(set = 0, binding = 0) uniform projection_modelviewtype
{
	mat4 projection;
	vec4 translation;
	vec4 rotation;
} projection_modelview;

layout(set = 3, binding = 0) uniform modelview_type
{
	vec3 translation;
	int pad;
	vec3 spriteOrigin;
	int pad2;
	vec3 spriteSize;
	float alpha;
} localmodelview;

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

layout(location = 0) out vec2 uv;
layout(location = 1) out float alpha;

void main()
{
	const vec2 sprite_vertices[] = { vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1) };
	
	//Need to calculate the rotation matrix to extract right and up vectors for the screen
	mat4 rotationMatrix = rotationAroundX(radians(-90))
		* rotationAroundZ(radians(90)) 
		* rotationAroundX(radians(projection_modelview.rotation.z)) 
		* rotationAroundY(radians(projection_modelview.rotation.x)) 
		* rotationAroundZ(-radians(projection_modelview.rotation.y));
		
	mat4 modelview = rotationMatrix
		* translation(-projection_modelview.translation.xyz);
		//* translation(localmodelview.translation.xyz);
		
	vec4 pt = modelview * vec4(localmodelview.translation, 1.0);
	pt += vec4(-localmodelview.spriteOrigin.x + localmodelview.spriteSize.x * sprite_vertices[gl_VertexIndex].x,
				   -localmodelview.spriteOrigin.y + localmodelview.spriteSize.y * sprite_vertices[gl_VertexIndex].y,
				   0.0, 0.0);
				   
	gl_Position = projection_modelview.projection * pt;
	uv = sprite_vertices[gl_VertexIndex];
	alpha = localmodelview.alpha;
}
