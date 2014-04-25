#version 410

in vec2 uv;
in vec4 color;

out vec4 out_color;

uniform sampler2D Texture;

void main()
{
	out_color = vec4(color.rgb, color.a * texture(Texture, uv).r);
}