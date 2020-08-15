#version 450

layout(location = 0) out vec4 outputColor;
layout(location = 2) in vec3 fragColor;

void main()
{
	outputColor = vec4(fragColor, 1.0);
}