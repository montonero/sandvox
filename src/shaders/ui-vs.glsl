#version 410

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec4 in_color;

out vec2 uv;
out vec4 color;

void main()
{
	gl_Position = vec4(in_pos, 0, 1);

	uv = in_uv / 8192;
	color = in_color;
}