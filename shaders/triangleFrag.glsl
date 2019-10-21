#version 450

layout(location = 2) in vec3 pixelColor;
layout(location = 2) out vec4 outputColor;

void main()
{
	outputColor = vec4(pixelColor, 1.0);
}