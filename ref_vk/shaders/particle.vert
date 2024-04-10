#version 450

layout(set = 0, binding = 0) uniform projection_modelviewtype
{
	mat4 projection;
	mat4 modelview;
} projection_modelview;

layout(push_constant) uniform particlesizetype
{
	float basesize;
	float min, max;
} particlesize;

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 outColor;

void main()
{		
	vec4 pos = projection_modelview.modelview * vec4(position, 1.0);
	gl_Position = projection_modelview.projection * pos;
	
	gl_PointSize = clamp(particlesize.basesize / gl_Position.w, particlesize.min, particlesize.max);
	
	outColor = color;
}
