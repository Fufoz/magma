#version 450

layout(location = 1) in vec3 pixelColor;
out vec4 outputColor;

void main()
{
	outputColor = vec4(pixelColor, 1.0);
}