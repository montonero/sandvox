#version 410

in vec3 position;
in vec3 normal;

out vec4 out_color;

float faketex(vec2 uv)
{
    return (int(floor(uv.x)) ^ int(floor(uv.y))) & 1;
}

void main()
{
	vec3 w = normal * normal;

	float m =
		faketex(position.xy) * w.z +
		faketex(position.xz) * w.y +
		faketex(position.yz) * w.x;

	out_color = vec4((normal * 0.5 + 0.5), 1);
}