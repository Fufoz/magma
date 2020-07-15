#version 450

layout(location = 0) in vec3 inVertexCoord;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 jointIds;
layout(location = 4) in vec4 weights;

layout(location = 5) out vec3 outNormal;
layout(location = 6) out vec2 outUV;


layout(set = 0, binding = 0) uniform UBO {
	mat4 model;
	mat4 viewProjection;
}ubo;

layout(std430, set = 0, binding = 1) readonly buffer JointMatrices {
	mat4 jointMats[];
};

layout(std430, set = 0, binding = 3) readonly buffer InstanceInfos {
	mat4 instanceTransforms[];
};

void main()
{

	mat4 SkinMat = 
		weights.x * jointMats[int(jointIds.x)] +
		weights.y * jointMats[int(jointIds.y)] +
		weights.z * jointMats[int(jointIds.z)] +
		weights.w * jointMats[int(jointIds.w)];

	outUV = inUV;
	outNormal =  normalize(inverse(transpose(mat3(ubo.model * instanceTransforms[gl_InstanceIndex]))) * inNormal);
	gl_Position =  ubo.viewProjection * ubo.model * instanceTransforms[gl_InstanceIndex] *
		SkinMat * vec4(inVertexCoord, 1.0);

}