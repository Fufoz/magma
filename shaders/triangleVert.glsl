#version 450

//attribute locations in vertex buffer
layout(location = 0) in vec3 vertexCoord;
layout(location = 1) in vec3 vertexColor;
layout(location = 2) out vec3 fragColor;

void main()
{
	gl_Position = vec4(vertexCoord, 1.0);
	fragColor = vertexColor;
}