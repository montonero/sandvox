#version 410

in vec2 uv;
in vec4 color;

out vec4 out_color;

uniform sampler2D Texture;

void main()
{
    float alpha = uv.x > 2 ? 1 : texture(Texture, uv).r;
    
	out_color = vec4(color.rgb, color.a * alpha);
}