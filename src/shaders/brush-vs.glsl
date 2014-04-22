#version 410

uniform mat4 World;
uniform mat4 ViewProjection;

layout (location = 0) in vec3 in_position;

void main()
{
	gl_Position = ViewProjection * (World * vec4(in_position, 1));
}