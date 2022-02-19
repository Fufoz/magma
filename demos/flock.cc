#include <magma.h>

#include <memory>
#include <vector>
#include <array>
#include <random>
#include <climits>

//demo specific information
struct InstanceData
{
	mat4x4 isntanceTransform;
};

struct BoidTransform
{
	Quat orientation;
	Vec4 position;//keep them all vec4 for glsl aligning purposes
	Vec4 direction;
	Vec4 up;
};

struct Plane
{
	Vec4 normal;
	Vec4 point;
};

struct DebugInfo
{
	Vec4 linePoint;
	Vec4 color;
};

struct BoidsGlobals
{
	float    minDistance = 5.f;//separation
	float    flockRadius = 1.f;
	float    tankSize = 25.f;
	float    deltaTime = 0.f;
	uint32_t boidsCount = 25;
	uint32_t spherePointsCount = 1000;
}boidsGlobals;

static_assert(sizeof(BoidsGlobals) <= 128, 
	"Push constant block is larger than minimum API supported size of 128 bytes"
);

struct ComputePipeData
{
	VkPipeline            pipeline;
	VkPipelineLayout      pipelineLayout;
	VkCommandPool         commandPool;
	VkCommandBuffer       commandBuffer;
	VkDescriptorPool      descriptorPool;
	VkDescriptorSetLayout descrSetLayout;
	VkDescriptorSet       descriptorSet;
	Buffer                instanceTransformsDeviceBuffer;
	Buffer                debugBuffer;
	Buffer                boidsStateDeviceBuffer;
	Buffer                deviceSpherePointsBuffer;
	Buffer                devicePlaneUniformBuffer;
	uint32_t              debugVertexCount;
	int                   workGroupSize;
};

struct DebugPipeData
{
	VkPipeline            pipeline;
	VkDescriptorPool      descrPool;
	VkDescriptorSetLayout descrSetLayout;
	VkDescriptorSet       descrSet;
	VkPipelineLayout      pipelineLayout;
	Buffer                tankBuffer;
};

struct SkyBoxPipeData
{
	VkPipeline pipeline;
	VkPipelineLayout pipeLayout;
	VkDescriptorPool descrPool;
	VkDescriptorSetLayout descrSetLayout;
	VkDescriptorSet descrSet;
	Buffer gpuVertexBuffer;
	Buffer gpuIndexBuffer;
	ImageResource gpuCubeMap;
	VkSampler sampler;
	std::array<std::uint32_t, 12 * 3> indices;
};

struct FishPipeData
{
	Mesh mesh;
	Animation animation;
	Texture fishTexture;
	ImageResource depthImage;
	VkPipeline fishPipeline;
	VkPipelineLayout pipeLayout;
	VkDescriptorSetLayout descrSetLayout;
	VkDescriptorPool descrPool;
	VkDescriptorSet descrSet;
	VkViewport viewport;
	Buffer vertexBuffer;
	Buffer indexBuffer;
	Buffer ubo;
	Buffer jointMatrices;
};

static constexpr int SWAPCHAIN_IMAGE_COUNT = 2;
struct FlockContext
{
	VulkanGlobalContext vkCtx;
	SwapChain swapChain;
	WindowInfo windowInfo;

	FishPipeData fishPipeData;
	ComputePipeData computePipeData;
	DebugPipeData debugPipeData;
	SkyBoxPipeData skyboxPipeData;

	VkRenderPass renderPass;
	VkCommandPool commandPool;
	std::array<VkFramebuffer, SWAPCHAIN_IMAGE_COUNT> frameBuffers;
	std::array<VkCommandBuffer, SWAPCHAIN_IMAGE_COUNT> commandBuffers;
};

static std::vector<BoidTransform> generate_boids(uint32_t numberOfBoids)
{
	std::default_random_engine generator;
	std::uniform_real_distribution<float> distribution(-boidsGlobals.tankSize/2.f + 2.f, boidsGlobals.tankSize/2.f - 2.f);
	auto randomValue = [&]()
	{
		return distribution(generator);
	};
	
	std::uniform_real_distribution<float> normDistr(0.f, 1.f);
	auto randomNormalisedValue = [&]()
	{
		return distribution(generator);
	};

	Vec3 defaultUp = {0., 1.f, 0.f};
	Vec3 defaultDirection = {0.f, 0.f, 1.f};
	std::vector<BoidTransform> out = {};

	for(uint32_t i = 0; i < numberOfBoids; i++)
	{
		BoidTransform transform = {};
		Vec3 newDirection = normaliseVec3({randomValue(), randomValue(), randomNormalisedValue()});
		// Vec3 newDirection = {0.f, 0.f, -1.f};
		// Vec3 newDirection = defaultDirection;
		transform.direction = toVec4(newDirection);
		// transform.direction = toVec4(defaultDirection);
		transform.position = {randomValue(), randomValue(), randomValue(), 1.0};
		transform.orientation = rotateFromTo(defaultDirection, newDirection);
		transform.up = toVec4(normaliseVec3(cross(newDirection, cross(defaultUp, newDirection))));
		// transform.position = {0.f, 0.f, boidsGlobals.tankSize / 2.f - 1.f};
		out.push_back(transform);
		magma::log::info("dir = {} pos = {} orient = {} up {}",
		transform.direction, transform.position, transform.orientation, transform.up);
	}

	return out;
}

static std::vector<Vec4> generate_points_on_sphere()
{
	const std::size_t sampleCount = boidsGlobals.spherePointsCount;
	const float goldenRatio = (1 + sqrt(5.f)) / 2.f;

	std::vector<Vec4> out = {};
	out.reserve(sampleCount);

	for(std::size_t i = 0; i < sampleCount; i++)
	{
		Vec4 spherePoint = {};
		const float theta = 2 * M_PI * (i + 0.5f) / goldenRatio;
		const float phi = acosf(1 - 2 * (i + 0.5f) / (float)sampleCount);
		spherePoint.x = sinf(phi) * sin(theta);
		spherePoint.y = cosf(phi);
		spherePoint.z = sinf(phi) * cosf(theta);
		spherePoint.w = 1.f;
		out.push_back(spherePoint);

		int currentIndex = i;
		int previous = currentIndex - 1;

		//transform
		while(previous >= 0 && out[currentIndex].z > out[previous].z)
		{
			Vec4 tmp = out[previous];
			out[previous] = out[currentIndex];
			out[currentIndex] = tmp;
			currentIndex--;
			previous--;
		}

	}

	return out;
}

static std::array<Plane, 6> generate_tank_planes()
{
	std::array<Plane, 6> tankPlanes = {};
	const float halfDistance = boidsGlobals.tankSize / 2.f;
	
	tankPlanes[0].normal = {0.f, 0.f, -1.f, 0.f};
	tankPlanes[0].point  = {0.f, 0.f, halfDistance, 0.f};

	tankPlanes[1].normal = {1.f, 0.f, 0.f, 0.f};
	tankPlanes[1].point  = {-halfDistance, 0.f, 0.f, 0.f};

	tankPlanes[2].normal = {0.f, 0.f, 1.f, 0.f};
	tankPlanes[2].point  = {0.f, 0.f, -halfDistance, 0.f};

	tankPlanes[3].normal = {-1.f, 0.f, 0.f, 0.f};
	tankPlanes[3].point  = {halfDistance, 0.f, 0.f, 0.f};

	tankPlanes[4].normal = {0.f, -1.f, 0.f, 0.f};
	tankPlanes[4].point  = {0.f, halfDistance, 0.f, 0.f};

	tankPlanes[5].normal = {0.f, 1.f, 0.f, 0.f};
	tankPlanes[5].point  = {0.f, -halfDistance, 0.f, 0.f};
	
	return tankPlanes;
}

static std::array<DebugInfo, 25> generate_tank_borders()
{
	std::array<DebugInfo, 25> out = {};
	const float sz = boidsGlobals.tankSize/2.f;
	
	out[0].linePoint  = {-sz, sz, -sz, 1.f};
	out[1].linePoint  = {-sz, sz, sz, 1.f};
	
	out[2].linePoint  = {-sz, sz, -sz, 1.f};
	out[3].linePoint  = {-sz, -sz, -sz, 1.f};
	
	out[4].linePoint  = {-sz, -sz, -sz, 1.f};
	out[5].linePoint  = {-sz, -sz, sz, 1.f};

	out[6].linePoint  = {-sz, -sz, sz, 1.f};
	out[7].linePoint  = {-sz, sz, sz, 1.f};
	
	//right square
	out[8].linePoint  = {sz, sz, -sz, 1.f};
	out[9].linePoint  = {sz, sz, sz, 1.f};
	
	
	out[10].linePoint  = {sz, sz, -sz, 1.f};
	out[11].linePoint  = {sz, -sz, -sz, 1.f};
	
	out[12].linePoint = {sz, -sz, -sz, 1.f};
	out[13].linePoint = {sz, -sz, sz, 1.f};
	
	out[14].linePoint = {sz, -sz, sz, 1.f};
	out[15].linePoint = {sz, sz, sz, 1.f};

	out[16].linePoint = {-sz, sz, sz, 1.f};
	out[17].linePoint = {sz, sz, sz, 1.f};
	
	
	out[18].linePoint = {-sz, -sz, sz, 1.f};
	out[19].linePoint = {sz, -sz, sz, 1.f};
	
	out[20].linePoint = {-sz, -sz, -sz, 1.f};
	out[21].linePoint = {sz, -sz, -sz, 1.f};
	
	out[22].linePoint = {-sz, sz, -sz, 1.f};
	out[23].linePoint = {sz, sz, -sz, 1.f};

	out[0].color  = {1.f, 1.f, 1.f, 1.f}; 
	out[1].color  = {1.f, 1.f, 1.f, 1.f};
	out[2].color  = {1.f, 1.f, 1.f, 1.f};
	out[3].color  = {1.f, 1.f, 1.f, 1.f}; 
	out[4].color  = {1.f, 1.f, 1.f, 1.f}; 
	out[5].color  = {1.f, 1.f, 1.f, 1.f}; 
	out[4].color  = {1.f, 1.f, 1.f, 1.f}; 
	out[5].color  = {1.f, 1.f, 1.f, 1.f};
	out[6].color  = {1.f, 1.f, 1.f, 1.f};
	out[7].color  = {1.f, 1.f, 1.f, 1.f};
	out[8].color  = {1.f, 1.f, 1.f, 1.f};
	out[9].color  = {1.f, 1.f, 1.f, 1.f}; 
	out[10].color = {1.f, 1.f, 1.f, 1.f}; 
	out[11].color = {1.f, 1.f, 1.f, 1.f};
	out[12].color = {1.f, 1.f, 1.f, 1.f};
	out[13].color = {1.f, 1.f, 1.f, 1.f};
	out[14].color = {1.f, 1.f, 1.f, 1.f};
	out[15].color = {1.f, 1.f, 1.f, 1.f};
	out[16].color = {1.f, 1.f, 1.f, 1.f}; 
	out[17].color = {1.f, 1.f, 1.f, 1.f};
	out[18].color = {1.f, 1.f, 1.f, 1.f}; 
	out[19].color = {1.f, 1.f, 1.f, 1.f}; 
	out[20].color = {1.f, 1.f, 1.f, 1.f}; 
	out[21].color = {1.f, 1.f, 1.f, 1.f};
	out[22].color = {1.f, 1.f, 1.f, 1.f};
	out[23].color = {1.f, 1.f, 1.f, 1.f};
	out[24].color = {1.f, 1.f, 1.f, 1.f};

	return out;
}

static void update_fish_descriptor_set(FlockContext* ctx)
{
	auto& vkCtx = ctx->vkCtx;

	//creating descriptor pool to allocate descriptor sets from
	VkDescriptorPoolSize descriptorPoolSizes[3] = {};
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSizes[0].descriptorCount = 1;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorPoolSizes[1].descriptorCount = 2;
	descriptorPoolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSizes[2].descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.maxSets = 3;
	descriptorPoolCreateInfo.poolSizeCount = 3;
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VK_CALL(vkCreateDescriptorPool(vkCtx.logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

	VkDescriptorSetAllocateInfo descrSetAllocInfo = {};
	descrSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descrSetAllocInfo.pNext = nullptr;
	descrSetAllocInfo.descriptorPool = descriptorPool;
	descrSetAllocInfo.descriptorSetCount = 1;
	descrSetAllocInfo.pSetLayouts = &ctx->fishPipeData.descrSetLayout;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VK_CALL(vkAllocateDescriptorSets(vkCtx.logicalDevice, &descrSetAllocInfo, &descriptorSet));
	
	//create ubos large enough to store data for each swapchain image to avoid ubo update synchronization
	Buffer ubo = createBuffer(vkCtx,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		ctx->swapChain.imageCount * sizeof(mat4x4) * 2
	);

	Buffer jointMatrices = createBuffer(vkCtx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		ctx->swapChain.imageCount * sizeof(mat4x4) * ctx->fishPipeData.animation.bindPose.size()
	); 
	
	//write data to descriptor set
	VkDescriptorBufferInfo descriptorBufferInfoMVP = {};
	descriptorBufferInfoMVP.buffer = ubo.buffer;
	descriptorBufferInfoMVP.offset = 0;
	descriptorBufferInfoMVP.range = ubo.bufferSize;

	VkDescriptorBufferInfo descriptorBufferInfoJoints = {};
	descriptorBufferInfoJoints.buffer = jointMatrices.buffer;
	descriptorBufferInfoJoints.offset = 0;
	descriptorBufferInfoJoints.range = jointMatrices.bufferSize;

	VkDescriptorBufferInfo descriptorBufferInfoInstances = {};
	descriptorBufferInfoInstances.buffer = ctx->computePipeData.instanceTransformsDeviceBuffer.buffer;
	descriptorBufferInfoInstances.offset = 0;
	descriptorBufferInfoInstances.range = ctx->computePipeData.instanceTransformsDeviceBuffer.bufferSize;


	VkDescriptorImageInfo descriptorImageInfo = {};
	descriptorImageInfo.sampler = ctx->fishPipeData.fishTexture.textureSampler;
	descriptorImageInfo.imageView = ctx->fishPipeData.fishTexture.imageInfo.view;
	descriptorImageInfo.imageLayout = ctx->fishPipeData.fishTexture.imageInfo.layout;

	std::array<VkWriteDescriptorSet, 4> writeDescrSets = {};
	writeDescrSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[0].pNext = nullptr;
	writeDescrSets[0].dstSet = descriptorSet;
	writeDescrSets[0].dstBinding = 0;
	writeDescrSets[0].dstArrayElement = 0;
	writeDescrSets[0].descriptorCount = 1;
	writeDescrSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescrSets[0].pImageInfo = nullptr;
	writeDescrSets[0].pBufferInfo = &descriptorBufferInfoMVP;
	writeDescrSets[0].pTexelBufferView = nullptr;

	writeDescrSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[1].pNext = nullptr;
	writeDescrSets[1].dstSet = descriptorSet;
	writeDescrSets[1].dstBinding = 1;
	writeDescrSets[1].dstArrayElement = 0;
	writeDescrSets[1].descriptorCount = 1;
	writeDescrSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescrSets[1].pImageInfo = nullptr;
	writeDescrSets[1].pBufferInfo = &descriptorBufferInfoJoints;
	writeDescrSets[1].pTexelBufferView = nullptr;

	writeDescrSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[2].pNext = nullptr;
	writeDescrSets[2].dstSet = descriptorSet;
	writeDescrSets[2].dstBinding = 2;
	writeDescrSets[2].dstArrayElement = 0;
	writeDescrSets[2].descriptorCount = 1;
	writeDescrSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescrSets[2].pImageInfo = &descriptorImageInfo;
	writeDescrSets[2].pBufferInfo = nullptr;
	writeDescrSets[2].pTexelBufferView = nullptr;

	writeDescrSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[3].pNext = nullptr;
	writeDescrSets[3].dstSet = descriptorSet;
	writeDescrSets[3].dstBinding = 3;
	writeDescrSets[3].dstArrayElement = 0;
	writeDescrSets[3].descriptorCount = 1;
	writeDescrSets[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescrSets[3].pImageInfo = nullptr;
	writeDescrSets[3].pBufferInfo = &descriptorBufferInfoInstances;
	writeDescrSets[3].pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(vkCtx.logicalDevice, writeDescrSets.size(), writeDescrSets.data(), 0, nullptr);

	ctx->fishPipeData.descrPool = descriptorPool;
	ctx->fishPipeData.descrSet = descriptorSet;
	ctx->fishPipeData.ubo = ubo;
	ctx->fishPipeData.jointMatrices = jointMatrices;
}

static void build_fish_pipeline(FlockContext* ctx)
{
	auto& vkCtx = ctx->vkCtx;
	auto& windowInfo = ctx->windowInfo;
	auto& swapChain = ctx->swapChain;
	Mesh mesh = {};
	Animation animation = {};
	animation.playbackRate = 4.f;
	if(!loadGLTF("resources/fish.gltf", &mesh, &animation))
	{
		return;
	}

	TextureInfo fishTexture = {};
	if(!loadTexture("resources/fish.png", &fishTexture, false))
	{
		return;
	}

	std::uint32_t width = windowInfo.windowExtent.width;
	std::uint32_t height = windowInfo.windowExtent.height;

	VkDescriptorSetLayoutBinding layoutBindings[4] = {};
	//mvp
	layoutBindings[0].binding = 0;
	layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBindings[0].descriptorCount = 1;
	layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBindings[0].pImmutableSamplers = nullptr;
	
	//ssbo for joint matrices
	layoutBindings[1].binding = 1;
	layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[1].descriptorCount = 1;
	layoutBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBindings[1].pImmutableSamplers = nullptr;

	//sampler
	layoutBindings[2].binding = 2;
	layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBindings[2].descriptorCount = 1;
	layoutBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layoutBindings[2].pImmutableSamplers = nullptr;

	//ssbo storing per-instance information
	layoutBindings[3].binding = 3;
	layoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[3].descriptorCount = 1;
	layoutBindings[3].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBindings[3].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
	descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutInfo.pNext = nullptr;
	descriptorSetLayoutInfo.bindingCount = 4;
	descriptorSetLayoutInfo.pBindings = layoutBindings;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreateDescriptorSetLayout(vkCtx.logicalDevice, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

	//for now store only camera direction vector
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(Vec3);

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2] = {};
	shaderStageCreateInfos[0] = fillShaderStageCreateInfo(vkCtx.logicalDevice, "shaders/spv/fishVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStageCreateInfos[1] = fillShaderStageCreateInfo(vkCtx.logicalDevice, "shaders/spv/fishFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	
	VkVertexInputAttributeDescription attribDescriptions[5] = {};
	//positions
	attribDescriptions[0].location = 0;
	attribDescriptions[0].binding = 0;
	attribDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDescriptions[0].offset = offsetof(Vertex, position);
	//normals
	attribDescriptions[1].location = 1;
	attribDescriptions[1].binding = 0;
	attribDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDescriptions[1].offset = offsetof(Vertex, normal);
	//uvs
	attribDescriptions[2].location = 2;
	attribDescriptions[2].binding = 0;
	attribDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attribDescriptions[2].offset = offsetof(Vertex, uv);

	attribDescriptions[3].location = 3;
	attribDescriptions[3].binding = 0;
	attribDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribDescriptions[3].offset = offsetof(Vertex, jointIds);
	
	attribDescriptions[4].location = 4;
	attribDescriptions[4].binding = 0;
	attribDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribDescriptions[4].offset = offsetof(Vertex, weights);

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = VK_FLAGS_NONE;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 5;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = attribDescriptions;


	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.f;
	viewport.y = height;
	viewport.width = (float)width;
	viewport.height = -(float)height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;
	
	VkRect2D scissors = {};
	scissors.offset = {0, 0};
	scissors.extent = {width, height};

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissors;
	
	VkPipelineRasterizationStateCreateInfo rasterStateCreateInfo = {};
	rasterStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterStateCreateInfo.pNext = nullptr;
	rasterStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterStateCreateInfo.depthBiasConstantFactor = 0.f;
	rasterStateCreateInfo.depthBiasClamp = 0.f;
	rasterStateCreateInfo.depthBiasSlopeFactor = 0.f;
	rasterStateCreateInfo.lineWidth = 1.f;
	
	VkPipelineMultisampleStateCreateInfo msStateCreateInfo = {};
	msStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msStateCreateInfo.pNext = nullptr;
	msStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	msStateCreateInfo.sampleShadingEnable = VK_FALSE;
	msStateCreateInfo.pSampleMask = nullptr;
	msStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	msStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	//override depth buffer if frame sample's depth is greater or equal to the one stored in depth buffer.
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

	//color blending is used for mixing color of transparent objects
	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.colorWriteMask = 0xf;
	blendAttachmentState.blendEnable = VK_FALSE;
	// Color blend state describes how blend factors are calculated (if used)
	// We need one blend attachment state per color attachment (even if blending is not used)
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &blendAttachmentState;

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	VkDynamicState dynamicState = VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.dynamicStateCount = 1;
	dynamicStateCreateInfo.pDynamicStates = &dynamicState;
	
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;
	VK_CHECK(getSupportedDepthFormat(vkCtx.physicalDevice, &depthFormat));

	ImageResource depthImage = createResourceImage(vkCtx, {width, height, 1},
		depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	VkAttachmentDescription attachments[2] = {};
	//presentable image
	attachments[0].format = swapChain.imageFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	//depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	

	VkSubpassDescription subpassDescr = {};
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	subpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescr.inputAttachmentCount = 0;
	subpassDescr.pInputAttachments = nullptr;
	subpassDescr.colorAttachmentCount = 1;
	subpassDescr.pColorAttachments = &colorAttachmentRef;
	subpassDescr.pResolveAttachments = nullptr;
	subpassDescr.pDepthStencilAttachment = &depthAttachmentRef;
	subpassDescr.preserveAttachmentCount = 0;
	subpassDescr.pPreserveAttachments = nullptr;

	VkSubpassDependency selfDependency = {};
 	selfDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
 	selfDependency.dstSubpass = 0;
 	selfDependency.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
 	selfDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
 	selfDependency.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
 	selfDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
 	selfDependency.dependencyFlags = 0;

	/*RENDER PASS*/
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pNext = nullptr;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = attachments;	
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescr;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &selfDependency;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VK_CALL(vkCreateRenderPass(vkCtx.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass));

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = VK_FLAGS_NONE;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreatePipelineLayout(vkCtx.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipeLayout));

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pNext = nullptr;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pTessellationState = nullptr;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;

	pipelineCreateInfo.pRasterizationState = &rasterStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &msStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = pipeLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;

	VkPipeline pipeline;
	VK_CALL(vkCreateGraphicsPipelines(vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline));

	//create texture image
	//1. create staging buffer to copy texture contents to gpu local memory
	Buffer stagingTextureBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		fishTexture.numc * fishTexture.extent.width * fishTexture.extent.height
	);

	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, fishTexture.data, stagingTextureBuffer.bufferSize, &stagingTextureBuffer));


	//create image on device local memory to recieve texture data
	ImageResource textureImage = createResourceImage(
		vkCtx, fishTexture.extent,
		fishTexture.format,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
	);
	
	//copy texture data to device local memory
	VkCommandPool cmdPool = createCommandPool(vkCtx);
	VK_CHECK(pushTextureToDeviceLocalImage(cmdPool, vkCtx, stagingTextureBuffer, fishTexture.extent, &textureImage));
	destroyBuffer(vkCtx.logicalDevice, &stagingTextureBuffer);

	// //create sampler to sample the fish texture
	VkSampler textureSampler = createDefaultSampler(vkCtx.logicalDevice);

	Texture texture = {};
	texture.imageInfo = textureImage;
	texture.textureSampler = textureSampler;
	texture.imageInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	//push vertices to device local memory
	Buffer stagingVertexBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		mesh.vertexBuffer.size() * sizeof(Vertex)
	);
	Buffer deviceLocalVertexBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		stagingVertexBuffer.bufferSize);
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, mesh.vertexBuffer.data(), stagingVertexBuffer.bufferSize, &stagingVertexBuffer));
	VK_CHECK(pushDataToDeviceLocalBuffer(cmdPool, vkCtx, stagingVertexBuffer, &deviceLocalVertexBuffer));
	
	destroyBuffer(vkCtx.logicalDevice, &stagingVertexBuffer);

	//push indices to device local memory
	Buffer stagingIndexBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		mesh.indexBuffer.size() * sizeof(unsigned int)
	);

	Buffer deviceLocalIndexBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		stagingIndexBuffer.bufferSize);
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, mesh.indexBuffer.data(), stagingIndexBuffer.bufferSize, &stagingIndexBuffer));
	VK_CHECK(pushDataToDeviceLocalBuffer(cmdPool, vkCtx, stagingIndexBuffer, &deviceLocalIndexBuffer));
	destroyBuffer(vkCtx.logicalDevice, &stagingIndexBuffer);
	destroyCommandPool(vkCtx.logicalDevice, cmdPool);

	for(auto&& shader : shaderStageCreateInfos)
	{
		vkDestroyShaderModule(vkCtx.logicalDevice, shader.module, nullptr);
	}

	FishPipeData out = {};
	ctx->fishPipeData.mesh = mesh;
	ctx->fishPipeData.animation = animation;
	ctx->fishPipeData.fishTexture = texture;
	ctx->fishPipeData.pipeLayout = pipeLayout;
	ctx->fishPipeData.descrSetLayout = descriptorSetLayout;
	ctx->fishPipeData.fishPipeline = pipeline;
	ctx->fishPipeData.viewport = viewport;
	ctx->fishPipeData.vertexBuffer = deviceLocalVertexBuffer;
	ctx->fishPipeData.indexBuffer = deviceLocalIndexBuffer;
	ctx->fishPipeData.depthImage = depthImage;
	ctx->renderPass = renderPass;

	update_fish_descriptor_set(ctx);

}

static void build_compute_pipeline(FlockContext* ctx)
{
	auto&& vkCtx = ctx->vkCtx;

	std::vector<BoidTransform> boidTransforms = generate_boids(boidsGlobals.boidsCount);
	std::vector<Vec4> spherePoints = generate_points_on_sphere();
	std::array<Plane, 6> tankPlanes = generate_tank_planes();

	std::vector<mat4x4> instanceTransforms = {boidsGlobals.boidsCount, loadIdentity()};
	Shader computeShader = {};
	VK_CHECK(loadShader(vkCtx.logicalDevice, "shaders/spv/boids.spv", VK_SHADER_STAGE_COMPUTE_BIT, &computeShader));

	VkSpecializationMapEntry specMapEntry = {};
	specMapEntry.constantID = 100;
	specMapEntry.offset = 0;
	specMapEntry.size = sizeof(int);

	const int workGroupSize = 64;
	VkSpecializationInfo specInfo = {};
	specInfo.mapEntryCount = 1;
	specInfo.pMapEntries = &specMapEntry;
	specInfo.dataSize = sizeof(int);
	specInfo.pData = &workGroupSize;


	VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
	shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo.pNext = nullptr;
	shaderStageCreateInfo.flags = VK_FLAGS_NONE;
	shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shaderStageCreateInfo.module = computeShader.handle;
	shaderStageCreateInfo.pName = "main";
	shaderStageCreateInfo.pSpecializationInfo = &specInfo;

	std::array<VkDescriptorSetLayoutBinding, 5> descrSetLayoutBindings = {};

	descrSetLayoutBindings[0].binding = 0;
	descrSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descrSetLayoutBindings[0].descriptorCount = 1;
	descrSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	descrSetLayoutBindings[0].pImmutableSamplers = nullptr;

	descrSetLayoutBindings[1].binding = 1;
	descrSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descrSetLayoutBindings[1].descriptorCount = 1;
	descrSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	descrSetLayoutBindings[1].pImmutableSamplers = nullptr;

	descrSetLayoutBindings[2].binding = 2;
	descrSetLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descrSetLayoutBindings[2].descriptorCount = 1;
	descrSetLayoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	descrSetLayoutBindings[2].pImmutableSamplers = nullptr;

	descrSetLayoutBindings[3].binding = 3;
	descrSetLayoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descrSetLayoutBindings[3].descriptorCount = 1;
	descrSetLayoutBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	descrSetLayoutBindings[3].pImmutableSamplers = nullptr;

	descrSetLayoutBindings[4].binding = 4;
	descrSetLayoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descrSetLayoutBindings[4].descriptorCount = 1;
	descrSetLayoutBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	descrSetLayoutBindings[4].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descrSetLayoutCreateInfo = {};
	descrSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descrSetLayoutCreateInfo.pNext = nullptr;
	descrSetLayoutCreateInfo.flags = VK_FLAGS_NONE;
	descrSetLayoutCreateInfo.bindingCount = descrSetLayoutBindings.size();
	descrSetLayoutCreateInfo.pBindings = descrSetLayoutBindings.data();

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreateDescriptorSetLayout(vkCtx.logicalDevice, &descrSetLayoutCreateInfo, nullptr, &descriptorSetLayout));


	VkPushConstantRange pushConstants = {};
	pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstants.offset = 0;
	pushConstants.size = sizeof(BoidsGlobals);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = VK_FLAGS_NONE;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstants;

	VkPipelineLayout computePipeLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreatePipelineLayout(vkCtx.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &computePipeLayout));

	VkComputePipelineCreateInfo computePipeCreateInfo = {};
	computePipeCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipeCreateInfo.pNext = nullptr;
	computePipeCreateInfo.flags = VK_FLAGS_NONE;
	computePipeCreateInfo.stage = shaderStageCreateInfo;
	computePipeCreateInfo.layout = computePipeLayout;

	VkPipeline computePipeline = VK_NULL_HANDLE;
	VK_CALL(vkCreateComputePipelines(vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &computePipeCreateInfo, nullptr, &computePipeline));

	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[0].descriptorCount = 4;

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.maxSets = 5;
	descriptorPoolCreateInfo.poolSizeCount = 2;
	descriptorPoolCreateInfo.pPoolSizes = poolSizes;
	
	VkDescriptorPool descrPool = VK_NULL_HANDLE;
	VK_CALL(vkCreateDescriptorPool(vkCtx.logicalDevice, &descriptorPoolCreateInfo, nullptr, &descrPool));
	
	VkDescriptorSetAllocateInfo descrSetAllocInfo = {};
	descrSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descrSetAllocInfo.pNext = nullptr;
	descrSetAllocInfo.descriptorPool = descrPool;
	descrSetAllocInfo.descriptorSetCount = 1;
	descrSetAllocInfo.pSetLayouts = &descriptorSetLayout;

	VkDescriptorSet descrSet = VK_NULL_HANDLE;
	VK_CALL(vkAllocateDescriptorSets(vkCtx.logicalDevice, &descrSetAllocInfo, &descrSet));


	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.pNext = nullptr;
	cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmdPoolCreateInfo.queueFamilyIndex = vkCtx.computeQueueFamIdx;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VK_CALL(vkCreateCommandPool(vkCtx.logicalDevice, &cmdPoolCreateInfo, nullptr, &commandPool));

	Buffer boidsStateStagingBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		boidsGlobals.boidsCount * sizeof(BoidTransform));

	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, boidTransforms.data(),
		boidsGlobals.boidsCount * sizeof(BoidTransform), &boidsStateStagingBuffer)
	);

	Buffer boidsStateDeviceBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		boidsGlobals.boidsCount * sizeof(BoidTransform));

	VK_CHECK(pushDataToDeviceLocalBuffer(commandPool, vkCtx, boidsStateStagingBuffer, &boidsStateDeviceBuffer, vkCtx.computeQueue));
	destroyBuffer(vkCtx.logicalDevice, &boidsStateStagingBuffer);

	Buffer stagingInstanceMatrices = createBuffer(vkCtx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		boidsGlobals.boidsCount * sizeof(mat4x4)
	);
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, instanceTransforms.data(),
		boidsGlobals.boidsCount * sizeof(mat4x4), &stagingInstanceMatrices)
	);

	Buffer instanceMatricesDeviceBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		boidsGlobals.boidsCount * sizeof(mat4x4));

	VK_CHECK(pushDataToDeviceLocalBuffer(commandPool, vkCtx, stagingInstanceMatrices, &instanceMatricesDeviceBuffer, vkCtx.computeQueue));
	destroyBuffer(vkCtx.logicalDevice, &stagingInstanceMatrices);

	Buffer stagingSpherePointsBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		spherePoints.size() * sizeof(Vec4)
	);
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, spherePoints.data(),
		spherePoints.size() * sizeof(Vec4), &stagingSpherePointsBuffer
	));

	Buffer deviceSpherePointsBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		spherePoints.size() * sizeof(Vec4)
	);
	VK_CHECK(pushDataToDeviceLocalBuffer(commandPool, vkCtx, stagingSpherePointsBuffer, &deviceSpherePointsBuffer, vkCtx.computeQueue));
	destroyBuffer(vkCtx.logicalDevice, &stagingSpherePointsBuffer);

	Buffer stagingPlaneUniformBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		tankPlanes.size() * sizeof(Plane)
	);
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, tankPlanes.data(),
		stagingPlaneUniformBuffer.bufferSize, &stagingPlaneUniformBuffer
	));

	Buffer devicePlaneUniformBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		stagingPlaneUniformBuffer.bufferSize
	);
	VK_CHECK(pushDataToDeviceLocalBuffer(commandPool, vkCtx, stagingPlaneUniformBuffer, &devicePlaneUniformBuffer, vkCtx.computeQueue));
	destroyBuffer(vkCtx.logicalDevice, &stagingPlaneUniformBuffer);

	//debugBufferSize = each instance has 2 structures of debugInfo data to describe a single vector ( ),
	// we have 3 vectors for each instance, hence (* 3)
	const std::size_t debugBufferSize = boidsGlobals.boidsCount * 2 * sizeof(DebugInfo) * 3;
	Buffer deviceDebugBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		debugBufferSize
	);


	//write data to descriptor set
	VkDescriptorBufferInfo descriptorBufferInfoBoidTransforms = {};
	descriptorBufferInfoBoidTransforms.buffer = boidsStateDeviceBuffer.buffer;
	descriptorBufferInfoBoidTransforms.offset = 0;
	descriptorBufferInfoBoidTransforms.range = boidsStateDeviceBuffer.bufferSize;

	VkDescriptorBufferInfo descriptorBufferInfoComputeInstanceTransforms = {};
	descriptorBufferInfoComputeInstanceTransforms.buffer = instanceMatricesDeviceBuffer.buffer;
	descriptorBufferInfoComputeInstanceTransforms.offset = 0;
	descriptorBufferInfoComputeInstanceTransforms.range = instanceMatricesDeviceBuffer.bufferSize;

	VkDescriptorBufferInfo descriptorBufferInfoSpherePoints = {};
	descriptorBufferInfoSpherePoints.buffer = deviceSpherePointsBuffer.buffer;
	descriptorBufferInfoSpherePoints.offset = 0;
	descriptorBufferInfoSpherePoints.range = deviceSpherePointsBuffer.bufferSize;

	VkDescriptorBufferInfo descriptorBufferInfoPlanes = {};
	descriptorBufferInfoPlanes.buffer = devicePlaneUniformBuffer.buffer;
	descriptorBufferInfoPlanes.offset = 0;
	descriptorBufferInfoPlanes.range = devicePlaneUniformBuffer.bufferSize;

	VkDescriptorBufferInfo descriptorBufferInfoDebug = {};
	descriptorBufferInfoDebug.buffer = deviceDebugBuffer.buffer;
	descriptorBufferInfoDebug.offset = 0;
	descriptorBufferInfoDebug.range = deviceDebugBuffer.bufferSize;

	std::array<VkWriteDescriptorSet, 5> writeDescrSets = {};
	writeDescrSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[0].pNext = nullptr;
	writeDescrSets[0].dstSet = descrSet;
	writeDescrSets[0].dstBinding = 0;
	writeDescrSets[0].dstArrayElement = 0;
	writeDescrSets[0].descriptorCount = 1;
	writeDescrSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescrSets[0].pImageInfo = nullptr;
	writeDescrSets[0].pBufferInfo = &descriptorBufferInfoBoidTransforms;
	writeDescrSets[0].pTexelBufferView = nullptr;

	writeDescrSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[1].pNext = nullptr;
	writeDescrSets[1].dstSet = descrSet;
	writeDescrSets[1].dstBinding = 1;
	writeDescrSets[1].dstArrayElement = 0;
	writeDescrSets[1].descriptorCount = 1;
	writeDescrSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescrSets[1].pImageInfo = nullptr;
	writeDescrSets[1].pBufferInfo = &descriptorBufferInfoComputeInstanceTransforms;
	writeDescrSets[1].pTexelBufferView = nullptr;

	writeDescrSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[2].pNext = nullptr;
	writeDescrSets[2].dstSet = descrSet;
	writeDescrSets[2].dstBinding = 2;
	writeDescrSets[2].dstArrayElement = 0;
	writeDescrSets[2].descriptorCount = 1;
	writeDescrSets[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescrSets[2].pImageInfo = nullptr;
	writeDescrSets[2].pBufferInfo = &descriptorBufferInfoSpherePoints;
	writeDescrSets[2].pTexelBufferView = nullptr;

	writeDescrSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[3].pNext = nullptr;
	writeDescrSets[3].dstSet = descrSet;
	writeDescrSets[3].dstBinding = 3;
	writeDescrSets[3].dstArrayElement = 0;
	writeDescrSets[3].descriptorCount = 1;
	writeDescrSets[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescrSets[3].pImageInfo = nullptr;
	writeDescrSets[3].pBufferInfo = &descriptorBufferInfoPlanes;
	writeDescrSets[3].pTexelBufferView = nullptr;

	writeDescrSets[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[4].pNext = nullptr;
	writeDescrSets[4].dstSet = descrSet;
	writeDescrSets[4].dstBinding = 4;
	writeDescrSets[4].dstArrayElement = 0;
	writeDescrSets[4].descriptorCount = 1;
	writeDescrSets[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescrSets[4].pImageInfo = nullptr;
	writeDescrSets[4].pBufferInfo = &descriptorBufferInfoDebug;
	writeDescrSets[4].pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(vkCtx.logicalDevice, writeDescrSets.size(), writeDescrSets.data(), 0, nullptr);
	VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
	createCommandBuffer(vkCtx.logicalDevice, commandPool, &cmdBuffer);

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	vkBeginCommandBuffer(cmdBuffer, &commandBufferBeginInfo);

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeLayout, 0, 1, &descrSet, 0, nullptr);
		vkCmdPushConstants(cmdBuffer, computePipeLayout, 
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BoidsGlobals), &boidsGlobals);

		vkCmdDispatch(cmdBuffer, boidsGlobals.boidsCount / workGroupSize + 1, 1, 1);
		
		if(vkCtx.queueFamIdx != vkCtx.computeQueueFamIdx)
		{
			VkBufferMemoryBarrier releaseBarrier = {};
			releaseBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			releaseBarrier.pNext = nullptr;
			releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			releaseBarrier.dstAccessMask = 0;
			releaseBarrier.srcQueueFamilyIndex = vkCtx.computeQueueFamIdx;
			releaseBarrier.dstQueueFamilyIndex = vkCtx.queueFamIdx;
			releaseBarrier.buffer = instanceMatricesDeviceBuffer.buffer;
			releaseBarrier.offset = 0;
			releaseBarrier.size = instanceMatricesDeviceBuffer.bufferSize;

			vkCmdPipelineBarrier(cmdBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
				0,
				0, nullptr,
				1, &releaseBarrier,
				0, nullptr
			);

			VkBufferMemoryBarrier debugReleaseBarrier = {};
			debugReleaseBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			debugReleaseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			debugReleaseBarrier.dstAccessMask = 0;
			debugReleaseBarrier.srcQueueFamilyIndex = vkCtx.computeQueueFamIdx;
			debugReleaseBarrier.dstQueueFamilyIndex = vkCtx.queueFamIdx;
			debugReleaseBarrier.buffer = deviceDebugBuffer.buffer;
			debugReleaseBarrier.offset = 0;
			debugReleaseBarrier.size = deviceDebugBuffer.bufferSize;

			vkCmdPipelineBarrier(cmdBuffer, 
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,
				0, nullptr,
				1, &debugReleaseBarrier,
				0, nullptr);

		}

	vkEndCommandBuffer(cmdBuffer);
	vkDestroyShaderModule(vkCtx.logicalDevice, computeShader.handle, nullptr);

	ctx->computePipeData.pipeline = computePipeline;
	ctx->computePipeData.pipelineLayout = computePipeLayout;
	ctx->computePipeData.commandPool = commandPool;
	ctx->computePipeData.commandBuffer = cmdBuffer;
	ctx->computePipeData.descriptorSet = descrSet;
	ctx->computePipeData.descrSetLayout = descriptorSetLayout;
	ctx->computePipeData.descriptorPool = descrPool;
	ctx->computePipeData.instanceTransformsDeviceBuffer = instanceMatricesDeviceBuffer;
	ctx->computePipeData.debugBuffer = deviceDebugBuffer;
	ctx->computePipeData.workGroupSize = workGroupSize;
	ctx->computePipeData.debugVertexCount = 6 * boidsGlobals.boidsCount;
	ctx->computePipeData.boidsStateDeviceBuffer = boidsStateDeviceBuffer;
	ctx->computePipeData.deviceSpherePointsBuffer = deviceSpherePointsBuffer;
	ctx->computePipeData.devicePlaneUniformBuffer = devicePlaneUniformBuffer;
}

static void build_debug_pipeline(FlockContext* ctx)
{
	auto& vkCtx = ctx->vkCtx;
	auto& windowInfo = ctx->windowInfo;
	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2] = {};
	shaderStageCreateInfos[0] = fillShaderStageCreateInfo(vkCtx.logicalDevice, "shaders/spv/debugVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStageCreateInfos[1] = fillShaderStageCreateInfo(vkCtx.logicalDevice, "shaders/spv/debugFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	
	VkVertexInputBindingDescription bindingDescr = {};
	bindingDescr.binding = 0;
	bindingDescr.stride = sizeof(DebugInfo);
	bindingDescr.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	
	std::array<VkVertexInputAttributeDescription, 2> vertexAttribs = {};
	vertexAttribs[0].location = 0;
	vertexAttribs[0].binding = 0;
	vertexAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttribs[0].offset = offsetof(DebugInfo, linePoint);

	vertexAttribs[1].location = 1;
	vertexAttribs[1].binding = 0;
	vertexAttribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttribs[1].offset = offsetof(DebugInfo, color);

	VkPipelineVertexInputStateCreateInfo vertexInpuStateCreateInfo = 
		fillVertexInputStateCreateInfo(&bindingDescr, 1, vertexAttribs.data(), vertexAttribs.size());

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
		fillInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	
	
	VkViewport viewport = createViewPort(windowInfo.windowExtent);
	VkRect2D scissors = {};
	scissors.offset = {0, 0};
	scissors.extent = windowInfo.windowExtent;
	
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo =
		fillViewportStateCreateInfo(viewport, scissors);

	VkPipelineRasterizationStateCreateInfo rasterStateCreateInfo = 
		fillRasterizationStateCreateInfo(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	
	VkPipelineMultisampleStateCreateInfo msStateCreateInfo = 
		fillMultisampleStateCreateInfo();

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = 
		fillDepthStencilStateCreateInfo(VK_COMPARE_OP_LESS_OR_EQUAL);


	//color blending is used for mixing color of transparent objects
	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.colorWriteMask = 0xf;
	blendAttachmentState.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = 
		fillColorBlendStateCreateInfo(blendAttachmentState);
	
	std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH};
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = 
		fillDynamicStateCreateInfo(dynamicStates.data(), dynamicStates.size());

	VkDescriptorSetLayoutBinding descrSetLayoutBinding = {};
	descrSetLayoutBinding.binding = 0;
	descrSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descrSetLayoutBinding.descriptorCount = 1;
	descrSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	descrSetLayoutBinding.pImmutableSamplers = nullptr;
	

	VkDescriptorSetLayoutCreateInfo descrSetLayoutCreateInfo = {};
	descrSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descrSetLayoutCreateInfo.bindingCount = 1;
	descrSetLayoutCreateInfo.pBindings = &descrSetLayoutBinding;

	VkDescriptorSetLayout descrSetLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreateDescriptorSetLayout(vkCtx.logicalDevice, &descrSetLayoutCreateInfo, nullptr, &descrSetLayout));
	VkPipelineLayout pipeLayout = createPipelineLayout(vkCtx.logicalDevice, &descrSetLayout, 1, nullptr, 0);

	VkGraphicsPipelineCreateInfo debugPipeCreateInfo = {};
	debugPipeCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	debugPipeCreateInfo.pNext = nullptr;
	debugPipeCreateInfo.stageCount = 2;
	debugPipeCreateInfo.pStages = shaderStageCreateInfos;
	debugPipeCreateInfo.pVertexInputState = &vertexInpuStateCreateInfo;
	debugPipeCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	debugPipeCreateInfo.pTessellationState = nullptr;
	debugPipeCreateInfo.pViewportState = &viewportStateCreateInfo;
	debugPipeCreateInfo.pRasterizationState = &rasterStateCreateInfo;
	debugPipeCreateInfo.pMultisampleState = &msStateCreateInfo;
	debugPipeCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	debugPipeCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	debugPipeCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	debugPipeCreateInfo.layout = pipeLayout;
	debugPipeCreateInfo.renderPass = ctx->renderPass;
	debugPipeCreateInfo.subpass = 0;

	VkPipeline debugPipeline = VK_NULL_HANDLE;
	VK_CALL(vkCreateGraphicsPipelines(vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &debugPipeCreateInfo, nullptr, &debugPipeline));


	VkDescriptorPoolSize descrPoolSize = {};
	descrPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descrPoolSize.descriptorCount = 1; 

	VkDescriptorPoolCreateInfo descrPoolCreateInfo = {};
	descrPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descrPoolCreateInfo.maxSets = 1;
	descrPoolCreateInfo.poolSizeCount = 1;
	descrPoolCreateInfo.pPoolSizes = &descrPoolSize;

	VkDescriptorPool descrPool = VK_NULL_HANDLE;
	VK_CALL(vkCreateDescriptorPool(vkCtx.logicalDevice, &descrPoolCreateInfo, nullptr, &descrPool));
	
	VkDescriptorSetAllocateInfo descrAllocateInfo = {};
	descrAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descrAllocateInfo.descriptorPool = descrPool;
	descrAllocateInfo.descriptorSetCount = 1;
	descrAllocateInfo.pSetLayouts = &descrSetLayout;

	VkDescriptorSet descrSet = VK_NULL_HANDLE;
	VK_CALL(vkAllocateDescriptorSets(vkCtx.logicalDevice, &descrAllocateInfo, &descrSet));

	VkDescriptorBufferInfo uboBufferInfo = {};
	uboBufferInfo.buffer = ctx->fishPipeData.ubo.buffer;
	uboBufferInfo.offset = 0;
	uboBufferInfo.range = ctx->fishPipeData.ubo.bufferSize;

	VkWriteDescriptorSet uboWriteDescrSet = {};
	uboWriteDescrSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	uboWriteDescrSet.dstSet = descrSet;
	uboWriteDescrSet.dstBinding = 0;
	uboWriteDescrSet.dstArrayElement = 0;
	uboWriteDescrSet.descriptorCount = 1;
	uboWriteDescrSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboWriteDescrSet.pImageInfo = nullptr;
	uboWriteDescrSet.pBufferInfo = &uboBufferInfo;
	uboWriteDescrSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(vkCtx.logicalDevice, 1, &uboWriteDescrSet, 0, nullptr);


	std::array<DebugInfo, 25> planes = generate_tank_borders();
	
	Buffer hostTankBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		planes.size() * sizeof(DebugInfo)
	);
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, planes.data(), hostTankBuffer.bufferSize, &hostTankBuffer));
	
	Buffer deviceTankBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		hostTankBuffer.bufferSize
	);

	VkCommandPool cmdPool = createCommandPool(vkCtx);
	VK_CHECK(pushDataToDeviceLocalBuffer(cmdPool, vkCtx, hostTankBuffer, &deviceTankBuffer));
	destroyBuffer(vkCtx.logicalDevice, &hostTankBuffer);
	vkDestroyCommandPool(vkCtx.logicalDevice, cmdPool, nullptr);
	
	for(auto&& shader : shaderStageCreateInfos)
	{
		vkDestroyShaderModule(vkCtx.logicalDevice, shader.module, nullptr);
	}

	ctx->debugPipeData.pipeline = debugPipeline;
	ctx->debugPipeData.descrSet = descrSet;
	ctx->debugPipeData.pipelineLayout = pipeLayout;
	ctx->debugPipeData.tankBuffer = deviceTankBuffer;
	ctx->debugPipeData.descrSetLayout = descrSetLayout;
	ctx->debugPipeData.descrPool = descrPool;
}

static bool build_skybox_pipeline(FlockContext* ctx)
{
	auto& vkCtx = ctx->vkCtx;
	auto& windowInfo = ctx->windowInfo;
	auto& swapChain = ctx->swapChain;

	VkVertexInputBindingDescription bindingDescr = {};
	bindingDescr.binding = 0;
	bindingDescr.stride = sizeof(Vec3);
	bindingDescr.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	
	VkVertexInputAttributeDescription attribDescr = {};
	attribDescr.location = 0;
	attribDescr.binding = 0;
	attribDescr.format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDescr.offset = 0;

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
		fillVertexInputStateCreateInfo(&bindingDescr, 1, &attribDescr, 1);

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = 
		fillInputAssemblyCreateInfo();

	VkViewport viewport = createViewPort(windowInfo.windowExtent);
	VkRect2D scissors = {};
	scissors.offset = {0, 0};
	scissors.extent = windowInfo.windowExtent;
	
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissors;

	// VkPipelineViewportStateCreateInfo viewportStateCreateInfo =
		// fillViewportStateCreateInfo(viewport, scissors);

	VkPipelineRasterizationStateCreateInfo rasterStateCreateInfo = 
		fillRasterizationStateCreateInfo(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	
	VkPipelineMultisampleStateCreateInfo msStateCreateInfo = 
		fillMultisampleStateCreateInfo();

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = 
		fillDepthStencilStateCreateInfo(VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.colorWriteMask = 0xf;
	blendAttachmentState.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendState = fillColorBlendStateCreateInfo(blendAttachmentState);

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageInfos = {};
	shaderStageInfos[0] = fillShaderStageCreateInfo(vkCtx.logicalDevice, "shaders/spv/skyboxVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStageInfos[1] = fillShaderStageCreateInfo(vkCtx.logicalDevice, "shaders/spv/skyboxFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDynamicState dynState = VK_DYNAMIC_STATE_VIEWPORT;
	VkPipelineDynamicStateCreateInfo dynStateCreateInfo = fillDynamicStateCreateInfo(&dynState, 1);

	std::array<VkDescriptorSetLayoutBinding, 2> descrSetLayoutBinding = {};
	descrSetLayoutBinding[0].binding = 0;
	descrSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descrSetLayoutBinding[0].descriptorCount = 1;
	descrSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	descrSetLayoutBinding[0].pImmutableSamplers = nullptr;

	descrSetLayoutBinding[1].binding = 1;
	descrSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descrSetLayoutBinding[1].descriptorCount = 1;
	descrSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	descrSetLayoutBinding[1].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descrSetLayoutCreateInfo = {};
	descrSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descrSetLayoutCreateInfo.bindingCount = descrSetLayoutBinding.size();
	descrSetLayoutCreateInfo.pBindings = descrSetLayoutBinding.data();

	VkDescriptorSetLayout descrSetLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreateDescriptorSetLayout(vkCtx.logicalDevice, &descrSetLayoutCreateInfo, nullptr, &descrSetLayout));

	VkPipelineLayoutCreateInfo pipeLayoutCreateInfo = {};
	pipeLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutCreateInfo.setLayoutCount = 1;
	pipeLayoutCreateInfo.pSetLayouts = &descrSetLayout;
	
	VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreatePipelineLayout(vkCtx.logicalDevice, &pipeLayoutCreateInfo, nullptr, &pipeLayout));

	VkGraphicsPipelineCreateInfo graphicsPipeCreateInfo = {};
	graphicsPipeCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipeCreateInfo.pNext = nullptr;
	graphicsPipeCreateInfo.flags = VK_FLAGS_NONE;
	graphicsPipeCreateInfo.stageCount = shaderStageInfos.size();
	graphicsPipeCreateInfo.pStages = shaderStageInfos.data();
	graphicsPipeCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipeCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	graphicsPipeCreateInfo.pTessellationState = nullptr;
	graphicsPipeCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipeCreateInfo.pRasterizationState = &rasterStateCreateInfo;
	graphicsPipeCreateInfo.pMultisampleState = &msStateCreateInfo;
	graphicsPipeCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipeCreateInfo.pColorBlendState = &colorBlendState;
	graphicsPipeCreateInfo.pDynamicState = &dynStateCreateInfo;
	graphicsPipeCreateInfo.layout = pipeLayout;
	graphicsPipeCreateInfo.renderPass = ctx->renderPass;
	graphicsPipeCreateInfo.subpass = 0;

	VkPipeline graphicsPipe = VK_NULL_HANDLE;
	VK_CALL(vkCreateGraphicsPipelines(vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipeCreateInfo, nullptr, &graphicsPipe));

	std::array<Vec3, 8> cubeVertices =
	{
		Vec3{ 1.f, -1.f,  1.f},
		Vec3{ 1.f, -1.f, -1.f},
		Vec3{-1.f, -1.f, -1.f},
		Vec3{-1.f, -1.f,  1.f},
		Vec3{ 1.f,  1.f,  1.f},
		Vec3{ 1.f,  1.f, -1.f},
		Vec3{-1.f,  1.f, -1.f},
		Vec3{-1.f,  1.f,  1.f}
	};
	const Vec3 scale = {boidsGlobals.tankSize * 10, boidsGlobals.tankSize * 10, boidsGlobals.tankSize* 10};
	auto mat = loadScale(scale);
	for(auto& vertex : cubeVertices)
	{
		vertex *= mat;
	}

	std::array<std::uint32_t, 12 * 3> indices = {
		//bottom
		0,1,3,
		1,2,3,
		//top
		5,4,6,
		4,7,6,
		//sides
		4,5,0,
		0,5,1,
		2,5,6,
		2,1,5,
		3,6,7,
		3,2,6,
		3,4,0,
		3,7,4
	};

	//transfer cube geom to gpu
	Buffer vertexBuffer = createBuffer(
		vkCtx, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		cubeVertices.size() * sizeof(Vec3)
	);

	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, cubeVertices.data(), vertexBuffer.bufferSize, &vertexBuffer));

	Buffer gpuVertexBuffer = createBuffer(
		vkCtx,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer.bufferSize
	);
	auto cmdPool = createCommandPool(vkCtx);
	VK_CHECK(pushDataToDeviceLocalBuffer(cmdPool, vkCtx, vertexBuffer, &gpuVertexBuffer));
	destroyBuffer(vkCtx.logicalDevice, &vertexBuffer);

	//transfer cube geom indices to gpu
	Buffer indexBuffer = createBuffer(
		vkCtx, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indices.size() * sizeof(std::uint32_t)
	);

	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, indices.data(), indexBuffer.bufferSize, &indexBuffer));

	Buffer gpuIndexBuffer = createBuffer(
		vkCtx,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer.bufferSize
	);

	VK_CHECK(pushDataToDeviceLocalBuffer(cmdPool, vkCtx, indexBuffer, &gpuIndexBuffer));
	destroyBuffer(vkCtx.logicalDevice, &indexBuffer);

	VkSampler sampler = createDefaultSampler(vkCtx.logicalDevice);

	bool planesAreLoaded = true;
	std::array<TextureInfo, 6> textures = {};
	std::array<const char*, 6> planes = 
	{
		"resources/negx.png",
		"resources/posx.png",
		"resources/negy.png",
		"resources/posy.png",
		"resources/negz.png",
		"resources/posz.png",
	};

	planesAreLoaded &= loadTexture(planes[0], &textures[0]);

	const std::size_t planeStride = textures[0].extent.width * textures[0].extent.height * textures[0].numc;
	const std::size_t cubemapSize = textures.size() * planeStride;

	std::vector<uint8_t> hostPtr = {};
	hostPtr.resize(cubemapSize);
	uint8_t* dstPtr = hostPtr.data();
	
	VkExtent3D planeExtent = textures[0].extent;
	
	for(uint32_t i = 0; i < planes.size(); i++)
	{
		planesAreLoaded &= loadTexture(planes[i], &textures[i], false);
		memcpy(dstPtr, textures[i].data, planeStride);
		dstPtr += planeStride;
	}

	if(!planesAreLoaded)
	{
		return false;
	}

	//creating big-ass staging buffer
	Buffer stagingCubeMapBuffer = createBuffer(
		vkCtx, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		cubemapSize
	);
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, hostPtr.data(), cubemapSize, &stagingCubeMapBuffer));

	// createResourceImage
	ImageResource cubemapGpuImage = createCubemapImage(
		vkCtx,
		textures[0].extent, textures[0].format,
		VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT
	);
	VK_CHECK(pushCubemapTextureToDeviceLocalImage(cmdPool, vkCtx, stagingCubeMapBuffer, planeExtent, planeStride, &cubemapGpuImage));
	destroyBuffer(vkCtx.logicalDevice, &stagingCubeMapBuffer);
	vkDestroyCommandPool(vkCtx.logicalDevice, cmdPool, nullptr);

	//updating descriptor sets
	std::array<VkDescriptorPoolSize, 2> descrPoolSizes = {};
	descrPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descrPoolSizes[0].descriptorCount = 1;
	descrPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descrPoolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo descrPoolCreateInfo = {};
	descrPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descrPoolCreateInfo.maxSets = 1;
	descrPoolCreateInfo.poolSizeCount = descrPoolSizes.size();
	descrPoolCreateInfo.pPoolSizes = descrPoolSizes.data();

	VkDescriptorPool descrPool = VK_NULL_HANDLE;
	VK_CALL(vkCreateDescriptorPool(vkCtx.logicalDevice, &descrPoolCreateInfo, nullptr, &descrPool));

	VkDescriptorSetAllocateInfo descrSetAllocInfo = {};
	descrSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descrSetAllocInfo.descriptorPool = descrPool;
	descrSetAllocInfo.descriptorSetCount = 1;
	descrSetAllocInfo.pSetLayouts = &descrSetLayout;

	VkDescriptorSet descrSet = VK_NULL_HANDLE;
	VK_CALL(vkAllocateDescriptorSets(vkCtx.logicalDevice, &descrSetAllocInfo, &descrSet));
	
	VkDescriptorBufferInfo uboDescrBufferInfo = {};
	uboDescrBufferInfo.buffer = ctx->fishPipeData.ubo.buffer;
	uboDescrBufferInfo.range = ctx->fishPipeData.ubo.bufferSize;
	
	std::array<VkWriteDescriptorSet, 2> writeDescrSets = {};
	writeDescrSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[0].dstSet = descrSet;
	writeDescrSets[0].dstBinding = 0;
	writeDescrSets[0].dstArrayElement = 0;
	writeDescrSets[0].descriptorCount = 1;
	writeDescrSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescrSets[0].pBufferInfo = &uboDescrBufferInfo;

	VkDescriptorImageInfo descrImageInfo = {};
	descrImageInfo.sampler = sampler;
	descrImageInfo.imageView = cubemapGpuImage.view;
	descrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	writeDescrSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescrSets[1].dstSet = descrSet;
	writeDescrSets[1].dstBinding = 1;
	writeDescrSets[1].dstArrayElement = 0;
	writeDescrSets[1].descriptorCount = 1;
	writeDescrSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescrSets[1].pImageInfo = &descrImageInfo;
	vkUpdateDescriptorSets(vkCtx.logicalDevice, writeDescrSets.size(), writeDescrSets.data(), 0, nullptr);

	for(auto&& shader : shaderStageInfos)
	{
		vkDestroyShaderModule(vkCtx.logicalDevice, shader.module, nullptr);
	}

	ctx->skyboxPipeData.pipeline = graphicsPipe;
	ctx->skyboxPipeData.pipeLayout = pipeLayout;
	ctx->skyboxPipeData.descrSetLayout = descrSetLayout;
	ctx->skyboxPipeData.descrPool = descrPool;
	ctx->skyboxPipeData.descrSet = descrSet;
	ctx->skyboxPipeData.gpuVertexBuffer = gpuVertexBuffer;
	ctx->skyboxPipeData.gpuIndexBuffer = gpuIndexBuffer;
	ctx->skyboxPipeData.gpuCubeMap = cubemapGpuImage;
	ctx->skyboxPipeData.indices = indices;
	ctx->skyboxPipeData.sampler = sampler;

	if(vkCtx.hasDebugUtilsExtension)
	{
		VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
		objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		objectNameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
		objectNameInfo.objectHandle = (uint64_t)gpuIndexBuffer.buffer;
		objectNameInfo.pObjectName = "SkyBox Index Buffer";
		vkSetDebugUtilsObjectNameEXT(vkCtx.logicalDevice, &objectNameInfo);

		VkDebugUtilsObjectNameInfoEXT objectNameInfo2 = {};
		objectNameInfo2.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		objectNameInfo2.objectType = VK_OBJECT_TYPE_BUFFER;
		objectNameInfo2.objectHandle = (uint64_t)gpuVertexBuffer.buffer;
		objectNameInfo2.pObjectName = "SkyBox Vertex Buffer";
		vkSetDebugUtilsObjectNameEXT(vkCtx.logicalDevice, &objectNameInfo2);

	}
	return true;
}

static bool init_flock_context(FlockContext* out)
{
	magma::log::initLogging();
	
	std::vector<const char*> desiredLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	std::vector<const char*> desiredExtensions = {
		"VK_EXT_debug_utils"
	};

	VulkanGlobalContext vkCtx = {};
	VK_CHECK(initVulkanGlobalContext(desiredLayers, desiredExtensions, &vkCtx));

	const std::size_t width = 640;
	const std::size_t height = 480;
	
	WindowInfo windowInfo = {};
	VK_CHECK(initPlatformWindow(vkCtx, width, height, "Magma", &windowInfo));	

	SwapChain swapChain = {};
	VK_CHECK(createSwapChain(vkCtx, windowInfo, 2, &swapChain));

	out->swapChain = swapChain;
	out->windowInfo = windowInfo;
	out->vkCtx = vkCtx;

	return true;
}

static void create_frame_buffers(FlockContext* ctx)
{
	//build actual frame buffers for render pass commands
	for(std::size_t i = 0; i < ctx->swapChain.imageCount; i++)
	{
		VkImageView imageViews[2] = {
			ctx->swapChain.runtime.imageViews[i],
			ctx->fishPipeData.depthImage.view
		};

		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = nullptr;
		frameBufferCreateInfo.flags = VK_FLAGS_NONE;
		frameBufferCreateInfo.renderPass = ctx->renderPass;
		frameBufferCreateInfo.attachmentCount = 2;
		frameBufferCreateInfo.pAttachments = imageViews;
		frameBufferCreateInfo.width = ctx->windowInfo.windowExtent.width;
		frameBufferCreateInfo.height = ctx->windowInfo.windowExtent.height;
		frameBufferCreateInfo.layers = 1;
		VK_CALL(vkCreateFramebuffer(ctx->vkCtx.logicalDevice, &frameBufferCreateInfo, nullptr, &ctx->swapChain.runtime.frameBuffers[i]));
	}
}

static void destroy_flock_context(FlockContext* ctx)
{
	auto& vkCtx = ctx->vkCtx;
	vkDeviceWaitIdle(vkCtx.logicalDevice);

	for(auto&& frameBuffer : ctx->frameBuffers)
	{
		vkDestroyFramebuffer(vkCtx.logicalDevice, frameBuffer, nullptr);
	}
	vkDestroyRenderPass(vkCtx.logicalDevice, ctx->renderPass, nullptr);
	vkFreeCommandBuffers(vkCtx.logicalDevice, ctx->commandPool, ctx->commandBuffers.size(), ctx->commandBuffers.data());
	vkDestroyCommandPool(vkCtx.logicalDevice, ctx->commandPool, nullptr);

	destroy_image_resource(vkCtx.logicalDevice, &ctx->fishPipeData.fishTexture.imageInfo);
	destroy_image_resource(vkCtx.logicalDevice, &ctx->fishPipeData.depthImage);
	vkDestroySampler(vkCtx.logicalDevice, ctx->fishPipeData.fishTexture.textureSampler, nullptr);
	vkDestroyPipelineLayout(vkCtx.logicalDevice, ctx->fishPipeData.pipeLayout, nullptr);
	vkDestroyPipeline(vkCtx.logicalDevice, ctx->fishPipeData.fishPipeline, nullptr);
	vkDestroyDescriptorSetLayout(vkCtx.logicalDevice, ctx->fishPipeData.descrSetLayout, nullptr);
	vkDestroyDescriptorPool(vkCtx.logicalDevice, ctx->fishPipeData.descrPool, nullptr);
	destroyBuffer(vkCtx.logicalDevice, &ctx->fishPipeData.vertexBuffer);
	destroyBuffer(vkCtx.logicalDevice, &ctx->fishPipeData.indexBuffer);
	destroyBuffer(vkCtx.logicalDevice, &ctx->fishPipeData.ubo);
	destroyBuffer(vkCtx.logicalDevice, &ctx->fishPipeData.jointMatrices);

	vkDestroyPipelineLayout(vkCtx.logicalDevice, ctx->computePipeData.pipelineLayout, nullptr);
	vkDestroyPipeline(vkCtx.logicalDevice, ctx->computePipeData.pipeline, nullptr);
	vkDestroyDescriptorSetLayout(vkCtx.logicalDevice, ctx->computePipeData.descrSetLayout, nullptr);
	vkDestroyDescriptorPool(vkCtx.logicalDevice, ctx->computePipeData.descriptorPool, nullptr);
	vkFreeCommandBuffers(vkCtx.logicalDevice, ctx->computePipeData.commandPool, 1, &ctx->computePipeData.commandBuffer);
	vkDestroyCommandPool(vkCtx.logicalDevice, ctx->computePipeData.commandPool, nullptr);
	destroyBuffer(vkCtx.logicalDevice, &ctx->computePipeData.instanceTransformsDeviceBuffer);
	destroyBuffer(vkCtx.logicalDevice, &ctx->computePipeData.debugBuffer);
	destroyBuffer(vkCtx.logicalDevice, &ctx->computePipeData.boidsStateDeviceBuffer);
	destroyBuffer(vkCtx.logicalDevice, &ctx->computePipeData.deviceSpherePointsBuffer);
	destroyBuffer(vkCtx.logicalDevice, &ctx->computePipeData.devicePlaneUniformBuffer);

	vkDestroyPipelineLayout(vkCtx.logicalDevice, ctx->debugPipeData.pipelineLayout, nullptr);
	vkDestroyPipeline(vkCtx.logicalDevice, ctx->debugPipeData.pipeline, nullptr);
	vkDestroyDescriptorSetLayout(vkCtx.logicalDevice, ctx->debugPipeData.descrSetLayout, nullptr);
	vkDestroyDescriptorPool(vkCtx.logicalDevice, ctx->debugPipeData.descrPool, nullptr);
	destroyBuffer(vkCtx.logicalDevice, &ctx->debugPipeData.tankBuffer);

	vkDestroyPipelineLayout(vkCtx.logicalDevice, ctx->skyboxPipeData.pipeLayout, nullptr);
	vkDestroyPipeline(vkCtx.logicalDevice, ctx->skyboxPipeData.pipeline, nullptr);
	vkDestroyDescriptorSetLayout(vkCtx.logicalDevice, ctx->skyboxPipeData.descrSetLayout, nullptr);
	vkDestroyDescriptorPool(vkCtx.logicalDevice, ctx->skyboxPipeData.descrPool, nullptr);
	destroyBuffer(vkCtx.logicalDevice, &ctx->debugPipeData.tankBuffer);
	destroy_image_resource(vkCtx.logicalDevice, &ctx->skyboxPipeData.gpuCubeMap);
	vkDestroySampler(vkCtx.logicalDevice, ctx->skyboxPipeData.sampler, nullptr);
	destroyBuffer(vkCtx.logicalDevice, &ctx->skyboxPipeData.gpuVertexBuffer);
	destroyBuffer(vkCtx.logicalDevice, &ctx->skyboxPipeData.gpuIndexBuffer);

	destroySwapChain(vkCtx, &ctx->swapChain);
	destroyPlatformWindow(vkCtx, &ctx->windowInfo);
	destroyGlobalContext(&vkCtx);
}

static void allocate_command_buffers(FlockContext* ctx)
{
	ctx->commandPool = createCommandPool(ctx->vkCtx);

	VkCommandBufferAllocateInfo buffAllocInfo = {};
	buffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	buffAllocInfo.pNext = nullptr;
	buffAllocInfo.commandPool = ctx->commandPool;
	buffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	buffAllocInfo.commandBufferCount = ctx->swapChain.imageCount;
	vkAllocateCommandBuffers(ctx->vkCtx.logicalDevice, &buffAllocInfo, ctx->commandBuffers.data());
}

static void record_graphics_command_buffer(FlockContext* ctx, const FPSCamera& camera, int commandBufferIndex)
{

	auto& vkCtx = ctx->vkCtx;
	auto commandBuffer = ctx->commandBuffers[commandBufferIndex];
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	
	//set clear color for depth and color attachments with LOAD_OP_CLEAR values in it
	VkClearValue clearValues[2] = {};
	clearValues[0].color = {0.654f, 0.984f, 0.968f, 1.f};
	clearValues[1].depthStencil.depth = 1.f;
	clearValues[1].depthStencil.stencil = 0;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	if(vkCtx.queueFamIdx != vkCtx.computeQueueFamIdx)
	{
		//acquire barrier to transition ownersip of ssbo buffer to the graphics queue
		VkBufferMemoryBarrier acquireBarrier = {};
		acquireBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		acquireBarrier.pNext = nullptr;
		acquireBarrier.srcAccessMask = 0;
		acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		acquireBarrier.srcQueueFamilyIndex = vkCtx.computeQueueFamIdx;
		acquireBarrier.dstQueueFamilyIndex = vkCtx.queueFamIdx;
		acquireBarrier.buffer = ctx->computePipeData.instanceTransformsDeviceBuffer.buffer;
		acquireBarrier.offset = 0;
		acquireBarrier.size = ctx->computePipeData.instanceTransformsDeviceBuffer.bufferSize;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
			0,
			0, nullptr,
			1, &acquireBarrier,
			0, nullptr
		);
		VkBufferMemoryBarrier debugAcquireBarrier = {};
		debugAcquireBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		debugAcquireBarrier.srcAccessMask = 0;
		debugAcquireBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		debugAcquireBarrier.srcQueueFamilyIndex = vkCtx.computeQueueFamIdx;
		debugAcquireBarrier.dstQueueFamilyIndex = vkCtx.queueFamIdx;
		debugAcquireBarrier.buffer = ctx->computePipeData.debugBuffer.buffer;
		debugAcquireBarrier.offset = 0;
		debugAcquireBarrier.size = ctx->computePipeData.debugBuffer.bufferSize;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			0,
			0, nullptr,
			1, &debugAcquireBarrier,
			0, nullptr
		);
	}

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = ctx->renderPass;
	renderPassBeginInfo.framebuffer = ctx->swapChain.runtime.frameBuffers[commandBufferIndex];
	renderPassBeginInfo.renderArea.offset = {0, 0};
	renderPassBeginInfo.renderArea.extent = ctx->windowInfo.windowExtent;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->fishPipeData.fishPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->fishPipeData.pipeLayout, 0, 1, &ctx->fishPipeData.descrSet, 0, nullptr);
		vkCmdSetViewport(commandBuffer, 0, 1, &ctx->fishPipeData.viewport);
		vkCmdPushConstants(commandBuffer, ctx->fishPipeData.pipeLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec3), &camera.direction);
		
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &ctx->fishPipeData.vertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(commandBuffer, ctx->fishPipeData.indexBuffer.buffer, offset, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, ctx->fishPipeData.mesh.indexBuffer.size(), boidsGlobals.boidsCount, 0, 0, 0); 
		
		//debug commands
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->debugPipeData.pipeline);
		vkCmdSetLineWidth(commandBuffer, 5.f);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->debugPipeData.pipelineLayout, 0, 1, &ctx->debugPipeData.descrSet, 0, nullptr);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &ctx->computePipeData.debugBuffer.buffer, &offset);
		vkCmdDraw(commandBuffer, ctx->computePipeData.debugVertexCount, boidsGlobals.boidsCount, 0, 0);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &ctx->debugPipeData.tankBuffer.buffer, &offset);
		vkCmdDraw(commandBuffer, 24, 1, 0, 0);

		// skybox commands
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->skyboxPipeData.pipeline);
		// vkCmdSetViewport(commandBuffer, 0, 1, &pipeState.viewport);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->skyboxPipeData.pipeLayout, 0, 1, &ctx->skyboxPipeData.descrSet, 0, nullptr);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &ctx->skyboxPipeData.gpuVertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(commandBuffer, ctx->skyboxPipeData.gpuIndexBuffer.buffer, offset, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, ctx->skyboxPipeData.indices.size(), 1, 0, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	if(vkCtx.queueFamIdx != vkCtx.computeQueueFamIdx)
	{
		//release barrier to transition ownersip of ssbo buffer back to the compute queue
		VkBufferMemoryBarrier releaseBarrier = {};
		releaseBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		releaseBarrier.pNext = nullptr;
		releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		releaseBarrier.dstAccessMask = 0;
		releaseBarrier.srcQueueFamilyIndex = vkCtx.queueFamIdx; 
		releaseBarrier.dstQueueFamilyIndex = vkCtx.computeQueueFamIdx;
		releaseBarrier.buffer = ctx->computePipeData.instanceTransformsDeviceBuffer.buffer;
		releaseBarrier.offset = 0;
		releaseBarrier.size = ctx->computePipeData.instanceTransformsDeviceBuffer.bufferSize;
		
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			1, &releaseBarrier,
			0, nullptr
		);

		VkBufferMemoryBarrier releaseDebugBarrier = {};
		releaseDebugBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		releaseDebugBarrier.pNext = nullptr;
		releaseDebugBarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		releaseDebugBarrier.dstAccessMask = 0;
		releaseDebugBarrier.srcQueueFamilyIndex = vkCtx.queueFamIdx; 
		releaseDebugBarrier.dstQueueFamilyIndex = vkCtx.computeQueueFamIdx;
		releaseDebugBarrier.buffer = ctx->computePipeData.debugBuffer.buffer;
		releaseDebugBarrier.offset = 0;
		releaseDebugBarrier.size = ctx->computePipeData.debugBuffer.bufferSize;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			1, &releaseDebugBarrier,
			0, nullptr
		);
	}

	vkEndCommandBuffer(commandBuffer);
}

static void record_compute_command_buffer(FlockContext* ctx)
{
	auto& vkCtx = ctx->vkCtx;
	auto commandBuffer = ctx->computePipeData.commandBuffer;

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	if(vkCtx.queueFamIdx != vkCtx.computeQueueFamIdx)
	{
		VkBufferMemoryBarrier acquireBarrier = {};
		acquireBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		acquireBarrier.pNext = nullptr;
		acquireBarrier.srcAccessMask = 0;
		acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		acquireBarrier.srcQueueFamilyIndex = vkCtx.queueFamIdx;
		acquireBarrier.dstQueueFamilyIndex = vkCtx.computeQueueFamIdx;
		acquireBarrier.buffer = ctx->computePipeData.instanceTransformsDeviceBuffer.buffer;
		acquireBarrier.offset = 0;
		acquireBarrier.size = ctx->computePipeData.instanceTransformsDeviceBuffer.bufferSize;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			1, &acquireBarrier,
			0, nullptr
		);
		VkBufferMemoryBarrier acquireDebugBarrier = {};
		acquireDebugBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		acquireDebugBarrier.pNext = nullptr;
		acquireDebugBarrier.srcAccessMask = 0;
		acquireDebugBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		acquireDebugBarrier.srcQueueFamilyIndex = vkCtx.queueFamIdx;
		acquireDebugBarrier.dstQueueFamilyIndex = vkCtx.computeQueueFamIdx;
		acquireDebugBarrier.buffer = ctx->computePipeData.debugBuffer.buffer;
		acquireDebugBarrier.offset = 0;
		acquireDebugBarrier.size = ctx->computePipeData.debugBuffer.bufferSize;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			1, &acquireDebugBarrier,
			0, nullptr
		);
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->computePipeData.pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->computePipeData.pipelineLayout, 0, 1, &ctx->computePipeData.descriptorSet, 0, nullptr);
	vkCmdPushConstants(commandBuffer, ctx->computePipeData.pipelineLayout, 
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BoidsGlobals), &boidsGlobals);

	vkCmdDispatch(commandBuffer, boidsGlobals.boidsCount / ctx->computePipeData.workGroupSize + 1, 1, 1);
		
	if(vkCtx.queueFamIdx != vkCtx.computeQueueFamIdx)
	{
		VkBufferMemoryBarrier releaseBarrier = {};
		releaseBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		releaseBarrier.pNext = nullptr;
		releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		releaseBarrier.dstAccessMask = 0;
		releaseBarrier.srcQueueFamilyIndex = vkCtx.computeQueueFamIdx;
		releaseBarrier.dstQueueFamilyIndex = vkCtx.queueFamIdx;
		releaseBarrier.buffer = ctx->computePipeData.instanceTransformsDeviceBuffer.buffer;
		releaseBarrier.offset = 0;
		releaseBarrier.size = ctx->computePipeData.instanceTransformsDeviceBuffer.bufferSize;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
			0,
			0, nullptr,
			1, &releaseBarrier,
			0, nullptr
		);
		VkBufferMemoryBarrier releaseDebugBarrier = {};
		releaseDebugBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		releaseDebugBarrier.pNext = nullptr;
		releaseDebugBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		releaseDebugBarrier.dstAccessMask = 0;
		releaseDebugBarrier.srcQueueFamilyIndex = vkCtx.computeQueueFamIdx;
		releaseDebugBarrier.dstQueueFamilyIndex = vkCtx.queueFamIdx;
		releaseDebugBarrier.buffer = ctx->computePipeData.debugBuffer.buffer;
		releaseDebugBarrier.offset = 0;
		releaseDebugBarrier.size = ctx->computePipeData.debugBuffer.bufferSize;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			0,
			0, nullptr,
			1, &releaseDebugBarrier,
			0, nullptr
		);
	}

	vkEndCommandBuffer(commandBuffer);
}

int main(int argc, char** argv)
{
	
	FlockContext ctx = {};
	init_flock_context(&ctx);
	build_compute_pipeline(&ctx);
	build_fish_pipeline(&ctx);
	build_debug_pipeline(&ctx);
	build_skybox_pipeline(&ctx);
	create_frame_buffers(&ctx);
	allocate_command_buffers(&ctx);


	uint32_t syncIndex = 0;
	auto& imageFences = ctx.swapChain.runtime.workSubmittedFences;
	auto& imageAvailableSemaphores = ctx.swapChain.runtime.imageAvailableSemaphores;
	auto& imageMayPresentSemaphores = ctx.swapChain.runtime.imageMayPresentSemaphores;
	
	FPSCamera camera = {};
	camera.position = {0.f, 0.f, 2.f};

	HostTimer timer = {};
	timer.start();
	
	std::vector<mat4x4> jointMats = {};
	jointMats.resize(ctx.fishPipeData.animation.bindPose.size());
	int width = ctx.windowInfo.windowExtent.width;
	int height = ctx.windowInfo.windowExtent.height;

	mat4x4 perspective = perspectiveProjection(90.f, width/(float)height, 0.1f, 100.f);
	mat4x4 scale = loadScale(Vec3{0.1f, 0.1f, 0.1f});
	Quat quat = identityQuat();
	mat4x4 rotation = quatToRotationMat(quat);
	mat4x4 modelToWorldTransform = rotation * scale;
	
	struct Transform
	{
		mat4x4 model;
		mat4x4 viewProjection;
	};
	Transform mvp = {};
	mvp.model = modelToWorldTransform;

	auto vkCtx = ctx.vkCtx;
	VkFence computeFence = createFence(vkCtx.logicalDevice);
	VkSemaphore computeFinishedSemaphore = createSemaphore(vkCtx.logicalDevice);
	VkSemaphore computeMayStartSemaphore = createSemaphore(vkCtx.logicalDevice);
	//Signal the semaphore
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &computeMayStartSemaphore;
	VK_CALL(vkQueueSubmit(vkCtx.computeQueue, 1, &submitInfo, computeFence));
	vkWaitForFences(vkCtx.logicalDevice, 1, &computeFence, VK_TRUE, UINT64_MAX);
	
	VkFence computeFinishedFence = createFence(vkCtx.logicalDevice);

	while(!windowShouldClose(ctx.windowInfo.windowHandle))
	{
		updateMessageQueue();

		float deltaSec = timer.stopSec();
		timer.start();
		fpsCameraUpdate(ctx.windowInfo, deltaSec, &camera);
		boidsGlobals.deltaTime = deltaSec;
		mvp.viewProjection = camera.viewTransform * perspective;

		VkPipelineStageFlags computeWaitDstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		VkSubmitInfo computeQueueSumbitInfo = {};
		computeQueueSumbitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeQueueSumbitInfo.pNext = nullptr;
		computeQueueSumbitInfo.waitSemaphoreCount = 1;
		computeQueueSumbitInfo.pWaitSemaphores = &computeMayStartSemaphore;
		computeQueueSumbitInfo.pWaitDstStageMask = &computeWaitDstStageMask;
		computeQueueSumbitInfo.commandBufferCount = 1;
		computeQueueSumbitInfo.pCommandBuffers = &ctx.computePipeData.commandBuffer;
		computeQueueSumbitInfo.signalSemaphoreCount = 1;
		computeQueueSumbitInfo.pSignalSemaphores = &computeFinishedSemaphore;

		VK_CALL(vkQueueSubmit(vkCtx.computeQueue, 1, &computeQueueSumbitInfo, computeFinishedFence));

		//wait on host side before we may start using same image that has already been used before
		VK_CALL(vkWaitForFences(
			vkCtx.logicalDevice, 1,
			&imageFences[syncIndex],
			VK_TRUE, UINT_MAX
		));
		VK_CALL(vkResetFences(vkCtx.logicalDevice, 1, &imageFences[syncIndex]));
		
		uint32_t imageIndex = {};		
		VK_CALL(vkAcquireNextImageKHR(
			vkCtx.logicalDevice, ctx.swapChain.swapchain,
			UINT_MAX, imageAvailableSemaphores[syncIndex],
			VK_NULL_HANDLE, &imageIndex
		));

		VkDeviceSize uboMVPBufferOffset = imageIndex * sizeof(Transform);
		VK_CALL(copyDataToHostVisibleBuffer(ctx.vkCtx, uboMVPBufferOffset, &mvp, sizeof(Transform), &ctx.fishPipeData.ubo));

		updateAnimation(ctx.fishPipeData.animation, deltaSec, jointMats);
		
		VkDeviceSize uboJointBufferOffset = imageIndex * sizeof(mat4x4) * jointMats.size();
		VK_CALL(copyDataToHostVisibleBuffer(ctx.vkCtx, uboJointBufferOffset, jointMats.data(), sizeof(mat4x4) * jointMats.size(), &ctx.fishPipeData.jointMatrices));

		record_graphics_command_buffer(&ctx, camera, imageIndex);
		
		VkSemaphore graphicsWaitSemaphores[2] = {computeFinishedSemaphore, imageAvailableSemaphores[syncIndex]};
		VkSemaphore graphicsSignalSemaphores[2] = {computeMayStartSemaphore, imageMayPresentSemaphores[syncIndex]};
		VkPipelineStageFlags graphicsWaitDstStages[2] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 2;
		submitInfo.pWaitSemaphores = graphicsWaitSemaphores;
		submitInfo.pWaitDstStageMask = graphicsWaitDstStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &ctx.commandBuffers[imageIndex];
		submitInfo.signalSemaphoreCount = 2;
		submitInfo.pSignalSemaphores = graphicsSignalSemaphores;
		
		//VkQueueInf
		VK_CALL(vkQueueSubmit(vkCtx.graphicsQueue, 1, &submitInfo, imageFences[syncIndex]));

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &imageMayPresentSemaphores[syncIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &ctx.swapChain.swapchain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;
		
		vkWaitForFences(vkCtx.logicalDevice, 1, &computeFinishedFence, VK_TRUE, UINT_MAX);
		vkResetFences(vkCtx.logicalDevice, 1, &computeFinishedFence);
		
		record_compute_command_buffer(&ctx);

		VK_CALL(vkQueuePresentKHR(vkCtx.graphicsQueue, &presentInfo));
		syncIndex = (syncIndex + 1) % ctx.swapChain.imageCount;
	}

	vkDeviceWaitIdle(vkCtx.logicalDevice);

	vkDestroyFence(vkCtx.logicalDevice, computeFence, nullptr);
	vkDestroyFence(vkCtx.logicalDevice, computeFinishedFence, nullptr);
	vkDestroySemaphore(vkCtx.logicalDevice, computeMayStartSemaphore, nullptr);
	vkDestroySemaphore(vkCtx.logicalDevice, computeFinishedSemaphore, nullptr);
	
	destroy_flock_context(&ctx);
	
	return 0;
}