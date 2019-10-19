#version 450

layout(location = 0) out vec4 outputColor;
layout(location = 1) in vec3 pixelColor;

void main()
{
	outputColor = vec4(pixelColor, 1.0);
}