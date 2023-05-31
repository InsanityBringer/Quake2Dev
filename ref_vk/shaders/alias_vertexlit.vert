#version 450

layout(set = 0, binding = 0) uniform projection_modelviewtype
{
	mat4 projection;
	vec4 translation;
	vec4 rotation;
} projection_modelview;

struct dlight
{
	vec3 origin;
	vec3 color;
	float intensity;
};

layout(set = 0, binding = 1) uniform shaderglobalstype
{
	float intensity;
	float varg;
	float modulate;
	int num_dlights;
	dlight lights[32];
} shaderglobals;

layout(set = 3, binding = 0) uniform modelview_type
{
	vec3 translation;
	int frameOffset;
	vec3 rotation;
	int oldFrameOffset;
	vec3 color;
	float backlerp;
	vec3 move;
	float swell;
	float xscale;
	float lightscale;
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

layout(location = 0) in int vertindex;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outLight;

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

//Lighting attenuation from the following post:
//https://lisyarus.github.io/blog/graphics/2022/07/30/point-light-attenuation.html
float sqr(float x)
{
	return x * x;
}

float attenuate_no_cusp(float distance, float radius, float max_intensity, float falloff)
{
	float s = distance / radius;

	if (s >= 1.0)
		return 0.0;

	float s2 = sqr(s);

	return max_intensity * sqr(1 - s2) / (1 + falloff * s2);
}

vec3 calc_dlights(vec3 position, vec3 normal)
{
	int i;
	
	vec3 accum = vec3(0);
	
	//This should be identical per thread group, right? The data coming in is the same for every single thread
	for (i = 0; i < shaderglobals.num_dlights; i++)
	{
		vec3 dir = position - shaderglobals.lights[i].origin;
		float dist = length(dir);
		
		//Ick! The intensity shouldn't just be vk_modulate at the origin, but what else do I do here?
		accum += shaderglobals.lights[i].color * attenuate_no_cusp(dist, shaderglobals.lights[i].intensity, shaderglobals.modulate, 1.0)
			* ((dot(normal, normalize(dir)) + 1) * 0.65 + .7);
	}
	
	return accum;
}

void main()
{
	mat4 rotation_matrix = rotationAroundZ(radians(localmodelview.rotation.y))
		* rotationAroundY(-radians(localmodelview.rotation.x))
		* rotationAroundX(-radians(localmodelview.rotation.z));
		
	mat4 lmodelview = translation(localmodelview.translation.xyz)
		* rotation_matrix;
		
	mat4 modelview = rotationAroundX(radians(-90))
		* rotationAroundZ(radians(90)) 
		* rotationAroundX(radians(projection_modelview.rotation.z)) 
		* rotationAroundY(radians(projection_modelview.rotation.x)) 
		* rotationAroundZ(-radians(projection_modelview.rotation.y))
		* translation(-projection_modelview.translation.xyz);

	vertex_entry vertex = vertbuffer.vertices[localmodelview.frameOffset + vertindex];
	vertex_entry oldvertex = vertbuffer.vertices[localmodelview.oldFrameOffset + vertindex];

	vec3 lerpvertex = mix(oldvertex.position, vertex.position, 1.0 - localmodelview.backlerp) + localmodelview.move * localmodelview.backlerp;
	vec3 lerpnormal = normalize(mix(oldvertex.normal, vertex.normal, 1.0 - localmodelview.backlerp)); //no idea if this is an ideal way to interpolate normals. 
	
	vec4 pos = lmodelview * vec4(lerpvertex + lerpnormal * localmodelview.swell, 1.0);
	gl_Position = projection_modelview.projection * (modelview * pos);
	
	vec4 normal = rotation_matrix * vec4(lerpnormal, 1.0);
	
	outLight = localmodelview.color;
	outLight *= ((dot(normal.xyz, vec3(0.681718, 0.147621, 0.716567)) + 1) * 0.65 + .7)
		* localmodelview.lightscale + (1.0 - localmodelview.lightscale);
	outLight += calc_dlights(pos.xyz, normal.xyz);
	//outLight = clamp(outLight, 0.0, 1.0);
	//outLight = localmodelview.color * (dot(normal.xyz, vec3(0.681718, 0.147621, 0.716567)) + 1);
	outUV = uv;
}
