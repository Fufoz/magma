#version 450

layout(location = 0) in vec3 inVertexCoord;
layout(location = 1) in vec3 inColor;
layout(location = 2) out vec3 outColor;

layout(set = 0, binding = 0) uniform UBO {
	mat4 model;
	mat4 viewProjection;
}ubo;

void main()
{
	gl_Position = ubo.viewProjection * ubo.model * vec4(inVertexCoord, 1.0);
	outColor = inColor;
}