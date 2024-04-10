#version 450

layout(set = 0, binding = 0) uniform projection_modelviewtype
{
	mat4 projection;
	mat4 modelview;
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

layout(location = 0) out vec2 uv;
layout(location = 1) out float alpha;

void main()
{
	const vec2 sprite_vertices[] = { vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1) };
	
	//Need to calculate the rotation matrix to extract right and up vectors for the screen
	//Do this by scratching out the translation from the camera's modelview matrix.
	/*mat4 rotationMatrix = modelview;
	rotationMatrix[3] = vec4(0, 0, 0, 1);
		
	mat4 modelview = rotationMatrix
		* translation(-projection_modelview.translation.xyz);*/
		
	vec4 pt = projection_modelview.modelview * vec4(localmodelview.translation, 1.0);
	pt += vec4(-localmodelview.spriteOrigin.x + localmodelview.spriteSize.x * sprite_vertices[gl_VertexIndex].x,
				   -localmodelview.spriteOrigin.y + localmodelview.spriteSize.y * sprite_vertices[gl_VertexIndex].y,
				   0.0, 0.0);
				   
	gl_Position = projection_modelview.projection * pt;
	uv = sprite_vertices[gl_VertexIndex];
	alpha = localmodelview.alpha;
}
