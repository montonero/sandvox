#version 410

in vec3 position;
in vec3 normal;

out vec4 out_color;

uniform sampler2D AlbedoTop;
uniform sampler2D AlbedoSide;

vec4 textureTriplanar(sampler2D top, sampler2D side, vec3 p, vec3 w)
{
	return
		texture(top, p.xy) * w.z +
		texture(side, p.xz) * w.y +
		texture(side, p.yz) * w.x;
}

void main()
{
	vec3 w = normal * normal;
	w /= w.x + w.y + w.z;

	vec4 albedo = textureTriplanar(AlbedoTop, AlbedoSide, position / 2, w);
	float diffuse = max(0, dot(normal, normalize(vec3(1, 1, 1))));
	float ambient = 0.25f;

	out_color = vec4((ambient + diffuse) * albedo.rgb, 1);
	// out_color = vec4(normal * 0.5 + 0.5, 1);
}