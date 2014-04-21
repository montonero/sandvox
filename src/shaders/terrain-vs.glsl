#version 410

uniform mat4 ViewProjection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;

out vec3 normal;

void main()
{
	gl_Position = ViewProjection * vec4(in_position, 1);
	normal = in_normal;
}