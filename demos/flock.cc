#include <magma.h>

#include <memory>
#include <vector>
#include <random>

//demo specific information
struct InstanceData
{
	mat4x4 isntanceTransform;
};
const std::size_t instanceCount = 1;

struct BoidTransform
{
	Quat orientation;
	Vec3 position;
	Vec3 direction;
};

struct BoidsGlobals
{
	float minDistance;//separation
	float flockRadius;
	float tankSize;
};

std::vector<BoidTransform> generateBoids(uint32_t numberOfBoids)
{
	const float tankSize = 5.f;
	std::default_random_engine generator;
	std::uniform_real_distribution<float> distribution(-tankSize, tankSize);
	auto randomValue = [&]()
	{
		return distribution(generator);
	};
	
	std::vector<BoidTransform> out = {};
	Vec3 defaultDirection = {0.f, 0.f, 1.f};

	for(uint32_t i = 0; i < numberOfBoids; i++)
	{
		BoidTransform transform = {};
		Vec3 newDirection = normaliseVec3({randomValue(), randomValue(), randomValue()});
		transform.direction = newDirection;
		transform.position = {randomValue(), randomValue(), randomValue()};
		transform.orientation = rotateFromTo(defaultDirection, newDirection);
		out.push_back(transform);
	}

	return out;
}

//generate random boids
//run compute to update direction && position
//rotate mesh into direction and move it // vertex shader
std::vector<mat4x4> 
updateBoidsTransform(std::vector<BoidTransform>& boids, const std::vector<Vec3>& newTranslations)
{
	std::vector<mat4x4> modelTransform = {};
	modelTransform.resize(boids.size());

	for(uint32_t i = 0; i < boids.size(); i++)
	{
		Vec3 newForwardVector = normaliseVec3(newTranslations[i] - boids[i].position);
		boids[i].orientation *= rotateFromTo(boids[i].direction, newForwardVector);
		boids[i].direction = newForwardVector;
		modelTransform[i] = quatToRotationMat(boids[i].orientation) * loadTranslation(boids[i].position) * loadTranslation(newTranslations[i]);
		boids[i].position = newTranslations[i];
	}

	return modelTransform;
}


static void buildComputePipeline(const VulkanGlobalContext& vkCtx)
{
	Shader computeShader = {};
	VK_CHECK(loadShader(vkCtx.logicalDevice, "shaders/boids.spv", VK_SHADER_STAGE_COMPUTE_BIT, &computeShader));

	//TODO: use specialisation constants to set workgroupsize dynamically
	
	// VkSpecializationInfo specInfo = {};
    // uint32_t                           mapEntryCount;
    // const VkSpecializationMapEntry*    pMapEntries;
    // size_t                             dataSize;
    // const void*                        pData;


	VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
	shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo.pNext = nullptr;
	shaderStageCreateInfo.flags = VK_FLAGS_NONE;
	shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shaderStageCreateInfo.module = computeShader.handle;
	shaderStageCreateInfo.pName = "main";
	shaderStageCreateInfo.pSpecializationInfo = nullptr;

	VkDescriptorSetLayoutBinding descrSetLayoutBinding = {};
    descrSetLayoutBinding.binding = 0;
    descrSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descrSetLayoutBinding.descriptorCount = 1;
    descrSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    descrSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descrSetLayoutCreateInfo = {};
    descrSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descrSetLayoutCreateInfo.pNext = nullptr;
    descrSetLayoutCreateInfo.flags = VK_FLAGS_NONE;
    descrSetLayoutCreateInfo.bindingCount = 1;
    descrSetLayoutCreateInfo.pBindings = &descrSetLayoutBinding;

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
}

int main(int argc, char** argv)
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

///////////////////////////pipeline creation//////////////////////////////////////////////////

	Mesh mesh = {};
	Animation animation = {};
	animation.playbackRate = 4.f;
	if(!loadGLTF("objs/fish.gltf", &mesh, &animation))
	{
		return -1;
	}

	TextureInfo fishTexture = {};
	if(!loadTexture("objs/fish.png", &fishTexture, false))
	{
		return -1;
	}

	buildComputePipeline(vkCtx);

	//setup uniform  buffer and sampler object 
	//letting hardware to know upfront what descriptor type and binding count ubo will have
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


	Shader vertexShader = {};
	VK_CHECK(loadShader(vkCtx.logicalDevice, "shaders/fishVert.spv", VK_SHADER_STAGE_VERTEX_BIT, &vertexShader));

	Shader fragmentShader = {};
	VK_CHECK(loadShader(vkCtx.logicalDevice, "shaders/fishFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, &fragmentShader));

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2] = {};
	shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfos[0].pNext = nullptr;
	shaderStageCreateInfos[0].flags = VK_FLAGS_NONE;
	shaderStageCreateInfos[0].stage = vertexShader.shaderType;
	shaderStageCreateInfos[0].module = vertexShader.handle;
	shaderStageCreateInfos[0].pName = "main";
	shaderStageCreateInfos[0].pSpecializationInfo = nullptr;

	shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfos[1].pNext = nullptr;
	shaderStageCreateInfos[1].flags = VK_FLAGS_NONE;
	shaderStageCreateInfos[1].stage = fragmentShader.shaderType;
	shaderStageCreateInfos[1].module = fragmentShader.handle;
	shaderStageCreateInfos[1].pName = "main";
	shaderStageCreateInfos[1].pSpecializationInfo = nullptr;

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
	// depthStencilStateCreateInfo.front = ;
	// depthStencilStateCreateInfo.back = ;
//    depthStencilStateCreateInfo.minDepthBounds = 0.f;
//    depthStencilStateCreateInfo.maxDepthBounds = -10.f;

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

	//create sampler to sample out texture
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = VK_FLAGS_NONE;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 1.f;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.f;
	samplerCreateInfo.maxLod = 0.f;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	// samplerCreateInfo.unnormalizedCoordinates = ;

	VkSampler textureSampler = VK_NULL_HANDLE;
	VK_CALL(vkCreateSampler(vkCtx.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler));

	Texture texture = {};
	texture.imageInfo = textureImage;
	texture.textureSampler = textureSampler;
	texture.imageInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


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
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	

	VkSubpassDescription subpassDescr = {};
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	//NOTE: layout durring the subpass, not after!! hence we dont use VK_IMAGE_LAYOUT_PRESENT_SRC_KHR here
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

//    subpassDescr.flags;
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
	selfDependency.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	selfDependency.dstStageMask = 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	selfDependency.srcAccessMask = 0;
	selfDependency.dstAccessMask = 
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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
	// pipelineCreateInfo.basePipelineHandle = ;
	// pipelineCreateInfo.basePipelineIndex = ;

	VkPipeline pipeline;
	VK_CALL(vkCreateGraphicsPipelines(vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline));
	
	PipelineState pipeState = {};
	pipeState.shaders[0] = vertexShader;
	pipeState.shaders[1] = fragmentShader;
	pipeState.pipeline = pipeline;
	pipeState.pipelineLayout = pipeLayout;
	pipeState.renderPass = renderPass;
	pipeState.viewport = viewport;

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
	

	//build actual frame buffers for render pass commands
//	buildFrameBuffers(vkCtx.logicalDevice, pipeState, windowInfo.windowExtent, &swapChain);
	for(std::size_t i = 0; i < swapChain.imageCount; i++)
	{
		VkImageView imageViews[2] = {
			swapChain.runtime.imageViews[i],
			depthImage.view
		};

		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = nullptr;
		frameBufferCreateInfo.flags = VK_FLAGS_NONE;
		frameBufferCreateInfo.renderPass = renderPass;
		frameBufferCreateInfo.attachmentCount = 2;
		frameBufferCreateInfo.pAttachments = imageViews;
		frameBufferCreateInfo.width = windowInfo.windowExtent.width;
		frameBufferCreateInfo.height = windowInfo.windowExtent.height;
		frameBufferCreateInfo.layers = 1;
		VK_CALL(vkCreateFramebuffer(vkCtx.logicalDevice, &frameBufferCreateInfo, nullptr, &swapChain.runtime.frameBuffers[i]));
	}

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
	descrSetAllocInfo.pSetLayouts = &descriptorSetLayout;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VK_CALL(vkAllocateDescriptorSets(vkCtx.logicalDevice, &descrSetAllocInfo, &descriptorSet));
	
	//create ubos large enough to store data for each swapchain image to avoid ubo update synchronization
	Buffer ubo = createBuffer(vkCtx,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		swapChain.imageCount * sizeof(mat4x4) * 2
	);

	Buffer jointMatrices = createBuffer(vkCtx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		swapChain.imageCount * sizeof(mat4x4) * animation.bindPose.size()
	); 
	
	Buffer instanceMatrices = createBuffer(vkCtx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		swapChain.imageCount * sizeof(mat4x4) * instanceCount
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
	descriptorBufferInfoInstances.buffer = instanceMatrices.buffer;
	descriptorBufferInfoInstances.offset = 0;
	descriptorBufferInfoInstances.range = instanceMatrices.bufferSize;
	
	VkDescriptorImageInfo descriptorImageInfo = {};
	descriptorImageInfo.sampler = texture.textureSampler;
	descriptorImageInfo.imageView = texture.imageInfo.view;
	descriptorImageInfo.imageLayout = texture.imageInfo.layout;


	VkWriteDescriptorSet writeDescrSets[4] = {};
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

	vkUpdateDescriptorSets(vkCtx.logicalDevice, 4, writeDescrSets, 0, nullptr);


	std::vector<VkCommandBuffer> cmdBuffers = {};
	createCommandBuffers(vkCtx.logicalDevice, cmdPool, swapChain.imageCount, cmdBuffers);
	
	//set clear color for depth and color attachments with LOAD_OP_CLEAR values in it
	VkClearValue clearValues[2] = {};
	clearValues[0].color = {0.654f, 0.984f, 0.968f, 1.f};
	clearValues[1].depthStencil.depth = 1.f;
	clearValues[1].depthStencil.stencil = 0;

	uint32_t syncIndex = 0;
	auto& imageFences = swapChain.runtime.workSubmittedFences;
	auto& imageAvailableSemaphores = swapChain.runtime.imageAvailableSemaphores;
	auto& imageMayPresentSemaphores = swapChain.runtime.imageMayPresentSemaphores;
	
	FPSCamera camera = {};
	camera.position = {0.f, 0.f, 2.f};

	HostTimer timer = {};
	timer.start();

	auto recordCommandBufferAt = [&](uint32_t index)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		vkBeginCommandBuffer(cmdBuffers[index], &beginInfo);

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapChain.runtime.frameBuffers[index];
		renderPassBeginInfo.renderArea.offset = {0, 0};
		renderPassBeginInfo.renderArea.extent = windowInfo.windowExtent;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(cmdBuffers[index], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cmdBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeState.pipeline);
			vkCmdBindDescriptorSets(cmdBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeState.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdSetViewport(cmdBuffers[index], 0, 1, &pipeState.viewport);
			vkCmdPushConstants(cmdBuffers[index], pipeState.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec3), &camera.direction);
			
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmdBuffers[index], 0, 1, &deviceLocalVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(cmdBuffers[index], deviceLocalIndexBuffer.buffer, offset, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdBuffers[index], mesh.indexBuffer.size(), instanceCount, 0, 0, 0); 
		vkCmdEndRenderPass(cmdBuffers[index]);

		vkEndCommandBuffer(cmdBuffers[index]);
	};

	//record commands into command buffers
	for(uint32_t i = 0; i < cmdBuffers.size(); i++)
	{
		recordCommandBufferAt(i);
	}

	std::vector<mat4x4> jointMats = {};
	jointMats.resize(animation.bindPose.size());

	mat4x4 perspective = perspectiveProjection(90.f, width/(float)height, 0.1f, 100.f);
	mat4x4 scale = loadTranslation(Vec3{-0.5f, -0.5f, 0.f}) * loadScale(Vec3{0.5f, 0.5f, 0.5f});
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

	std::vector<BoidTransform> boidTransforms = generateBoids(instanceCount);
	//per isntance info
	std::vector<mat4x4> instanceTransforms = {instanceCount, loadIdentity()};
	
	
	for(uint32_t i = 0; i < instanceCount; i++)
	{
		instanceTransforms[i] *= quatToRotationMat(boidTransforms[i].orientation) * loadTranslation(boidTransforms[i].position);
	}

	Buffer boidsStateStagingBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		instanceCount * sizeof(BoidTransform));
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx, 0, boidTransforms.data(),
		instanceCount * sizeof(BoidTransform), &boidsStateStagingBuffer)
	);

	Buffer boidsStateDeviceBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		instanceCount * sizeof(BoidTransform));
	VK_CHECK(pushDataToDeviceLocalBuffer(cmdPool, vkCtx, boidsStateStagingBuffer, &boidsStateDeviceBuffer));

	// float angle = 0.f;
	// const float radius = 3.f;

	while(!windowShouldClose(windowInfo.windowHandle))
	{
		updateMessageQueue();
		float deltaSec = timer.stopSec();
		// magma::log::info("Frame took {} ms", deltaSec);
		timer.start();
		fpsCameraUpdate(windowInfo, deltaSec, &camera);
		mvp.viewProjection = camera.viewTransform * perspective;

		uint32_t imageIndex = {};

		//wait on host side before we may start using same image that has already been used before
		VK_CALL(vkWaitForFences(
			vkCtx.logicalDevice, 1,
			&imageFences[syncIndex],
			VK_TRUE, UINT_MAX
		));
		VK_CALL(vkResetFences(vkCtx.logicalDevice, 1, &imageFences[syncIndex]));
		
		VK_CALL(vkAcquireNextImageKHR(
			vkCtx.logicalDevice, swapChain.swapchain,
			UINT_MAX, imageAvailableSemaphores[syncIndex],
			VK_NULL_HANDLE, &imageIndex
		));

		VkDeviceSize uboMVPBufferOffset = imageIndex * sizeof(Transform);
		VK_CALL(copyDataToHostVisibleBuffer(vkCtx, uboMVPBufferOffset, &mvp, sizeof(Transform), &ubo));

		updateAnimation(animation, deltaSec, jointMats);
		
		VkDeviceSize uboJointBufferOffset = imageIndex * sizeof(mat4x4) * jointMats.size();
		VK_CALL(copyDataToHostVisibleBuffer(vkCtx, uboJointBufferOffset, jointMats.data(), sizeof(mat4x4) * jointMats.size(), &jointMatrices));

		//update instance data
		//move fish around circle
		// Vec3 translationVector = {};
		// translationVector.x = radius * cosf(toRad(angle));
		// translationVector.z = radius * sinf(toRad(angle));
		// auto& updatedTransforms = updateBoidsTransform(boidTransforms, {translationVector});
		// for(uint32_t i = 0; i < updatedTransforms.size(); i++)
		// {
			// instanceTransforms[i] = updatedTransforms[i];
			// magma::log::info("{}",instanceTransforms[i]);
		// }

		VkDeviceSize instanceOffset = imageIndex * sizeof(mat4x4) * instanceCount;
		VK_CALL(copyDataToHostVisibleBuffer(vkCtx, instanceOffset, instanceTransforms.data(), sizeof(mat4x4) * instanceCount, &instanceMatrices));
		// angle++;

		recordCommandBufferAt(imageIndex);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &imageAvailableSemaphores[syncIndex];
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitInfo.pWaitDstStageMask = &dstStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffers[imageIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &imageMayPresentSemaphores[syncIndex];
		
		//VkQueueInf
		VK_CALL(vkQueueSubmit(vkCtx.graphicsQueue, 1, &submitInfo, imageFences[syncIndex]));

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &imageMayPresentSemaphores[syncIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain.swapchain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		VK_CALL(vkQueuePresentKHR(vkCtx.graphicsQueue, &presentInfo));
		syncIndex = (syncIndex + 1) % swapChain.imageCount;
	}
	


	destroySwapChain(vkCtx, &swapChain);
	destroyPlatformWindow(vkCtx, &windowInfo);
	destroyGlobalContext(&vkCtx);
	
	return 0;
}