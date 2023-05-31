#version 450

layout(set = 0, binding = 0) uniform projectionBuffer
{
	mat4 projectionMatrix;
} projection;

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 outColor;

void main()
{
	gl_Position = projection.projectionMatrix * vec4(position, 1.0);
	outColor = color;
}
