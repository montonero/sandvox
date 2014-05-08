#version 410

uniform vec3 Color;

in vec3 normal;

out vec4 out_color;

in vec4 worldPosition;

float grid(float v)
{
    return clamp(fract(v) * 32, 0, 1);
}

void main()
{
    float gf = grid(worldPosition.x) * grid(worldPosition.y) * grid(worldPosition.z);
    
	out_color = vec4(Color * gf, 0.25f + (1 - gf));
}