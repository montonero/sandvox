#version 410

layout (location = 0) in vec4 in_posuv;
layout (location = 1) in vec4 in_color;

out vec2 uv;
out vec4 color;

void main()
{
	gl_Position = vec4(in_posuv.xy, 0, 1);

	uv = in_posuv.zw;
	color = in_color;
}