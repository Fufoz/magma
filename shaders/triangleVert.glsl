#version 450

layout(location = 0) in vec3 vertexCoord;
layout(location = 1) out vec3 vertColor;

void main()
{
	gl_Position = vec4(vertexCoord, 1.0);
	vertColor = vec3(0.5, 0.2, 0.4);
}