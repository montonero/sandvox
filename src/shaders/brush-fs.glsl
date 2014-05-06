#version 410

uniform vec3 Color;

in vec3 normal;

out vec4 out_color;

void main()
{
	out_color = vec4(Color, 0.25f);
}