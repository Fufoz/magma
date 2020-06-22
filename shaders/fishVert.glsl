#version 450

layout(location = 0) in vec3 inVertexCoord;
layout(location = 1) in vec3 inNormal;
// layout(location = 2) out vec3 outFragColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec2 outUV;

layout(set = 0, binding = 0) uniform UBO {
	mat4 MVP;
}ubo;

void main()
{
	outUV = inUV;
	outNormal = inNormal;
	gl_Position =  ubo.MVP * vec4(inVertexCoord, 1.0);
	// outFragColor = vec3(0.745, 0.733, 0.733);
}