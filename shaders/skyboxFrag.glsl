#version 440

layout(location = 0) out vec4 outputColor;
layout(location = 1) in vec3 texCoord;

layout(binding = 1) uniform samplerCube cubeSampler;

void main()
{
	outputColor = texture(cubeSampler, texCoord);
}