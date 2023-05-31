#version 450

layout(set = 1, binding = 0) uniform samplerCube colorTexture;

layout(location = 0) in vec3 surfacePos;

layout(location = 0) out vec4 color;

void main()
{
	vec3 vec = normalize(vec3(surfacePos.x, surfacePos.z, surfacePos.y));
	color = vec4(texture(colorTexture, vec).rgb, 1.0);
}
