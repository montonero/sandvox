#version 410

in vec3 normal;

out vec4 out_color;

void main()
{
	out_color = vec4(normal * 0.5 + 0.5, 1);
}