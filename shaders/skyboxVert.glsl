#version 440

//cube vertex positions
layout(location = 0) in vec3 vertexCoord;
layout(location = 1) out vec3 texCoord;

layout(set = 0, binding = 0) uniform UBO {
	mat4 model;
	mat4 viewProjection;
}ubo;

void main()
{
	texCoord = vertexCoord;
	gl_Position = ubo.viewProjection * ubo.model * vec4(vertexCoord, 1.0);
}