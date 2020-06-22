#version 450

layout(location = 0) out vec4 outputColor;
// layout(location = 2) in vec3 inFragColor;
layout(location = 3) in vec3 inFragNormal;
layout(location = 4) in vec2 inUV;

layout(push_constant) uniform ConstantBlock
{
	vec3 viewDir;
}Light;

layout (binding = 1) uniform sampler2D samplerColor;

void main()
{
	vec3 inFragColor = texture(samplerColor, inUV).rgb;
	vec3 ambient = 0.5 * inFragColor;
	vec3 diffuse = max(-dot(Light.viewDir, inFragNormal), 0.0) * inFragColor;
	outputColor = vec4(ambient + diffuse, 1.0);
}