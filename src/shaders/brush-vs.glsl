#version 410

uniform mat4 World;
uniform mat4 ViewProjection;

layout (location = 0) in vec3 in_position;

out vec4 worldPosition;

void main()
{
    worldPosition = World * vec4(in_position, 1);
    
	gl_Position = ViewProjection * worldPosition;
}