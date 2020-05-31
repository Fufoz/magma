#include <magma.h>

#include <vector>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"

#include <meshoptimizer.h>

#include <memory>

struct Vertex
{
	Vec3 position;
	Vec3 normal;
};

struct Mesh
{
	std::vector<Vertex> vertexBuffer;
	std::vector<unsigned int> indexBuffer;
};

bool loadOBJ(const char* path, Mesh* geom)
{
	assert(path);
	assert(geom);
	auto meshUnloader = [](fastObjMesh* mesh)
	{
		fast_obj_destroy(mesh);
	};
	
	std::unique_ptr<fastObjMesh, decltype(meshUnloader)> mesh = {
		fast_obj_read(path),
		meshUnloader
	};

	if(!mesh.get())
	{
		magma::log::error("Failed to load mesh from %s", path);
		return false;
	}

	//assume triangle mesh here
	std::vector<Vertex> vertices = {};
	assert(mesh->face_vertices[0] == 3);
	const uint32_t vertexCount = mesh->face_vertices[0];
	vertices.resize(mesh->face_count * mesh->face_vertices[0]);

	mat4x4 translate = loadTranslation({-0.5f, -0.5f, 0.f});
	//loop through each face
	for(uint32_t i = 0; i < mesh->face_count; i++)
	{
		//loop through each vertex in a face
		for(uint32_t j = 0; j < vertexCount; j++)
		{
			const uint32_t vi = mesh->indices[3 * i + j].p;
			const uint32_t vn = mesh->indices[3 * i + j].n;
			const std::size_t out_index = i * vertexCount + j;

			vertices[out_index].position.x = mesh->positions[3 * vi + 0];
			vertices[out_index].position.y = mesh->positions[3 * vi + 1];
			vertices[out_index].position.z = mesh->positions[3 * vi + 2];
			auto& pos = vertices[out_index].position;

			vertices[out_index].position *= translate;
			
			vertices[out_index].normal.x = mesh->normals[3 * vn + 0];
			vertices[out_index].normal.y = mesh->normals[3 * vn + 1];
			vertices[out_index].normal.z = mesh->normals[3 * vn + 2];

			// magma::log::debug("Inserting x={} y={} z={} into vertex position",
			// 	vertices[i].position.x,
			// 	vertices[i].position.y,
			// 	vertices[i].position.z
			// );
		}
	}

	std::vector<unsigned int> remapTable = {};
	const std::size_t indexCount = 3 * mesh->face_count; 
	remapTable.resize(indexCount);//total nums of indices
	std::size_t newVertCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, indexCount, vertices.data(), vertices.size(), sizeof(Vertex));
	
	std::vector<Vertex> vertexBuffer = {};
	vertexBuffer.resize(newVertCount);
	meshopt_remapVertexBuffer(vertexBuffer.data(), vertices.data(), vertices.size(), sizeof(Vertex), remapTable.data());
	std::vector<unsigned int> indexBuffer = {};
	indexBuffer.resize(indexCount);
	meshopt_remapIndexBuffer(indexBuffer.data(), nullptr, indexCount, remapTable.data());
	for(auto vertex : vertexBuffer)
	{
		magma::log::info("Vx={} Vy={} Vz={}; Nx={} Ny={} Nz={}",
			vertex.position.x,vertex.position.y,vertex.position.z,
			vertex.normal.x, vertex.normal.y, vertex.normal.z);
	}
	
	for(auto index : indexBuffer)
	{
		magma::log::info("Index {}", index);
	}
	
	geom->indexBuffer = indexBuffer;
	geom->vertexBuffer = vertexBuffer;

	return true;
}

int main(int argc, char** argv)
{
	magma::log::setSeverityMask(magma::log::SeverityMask::MASK_ALL);

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
	VK_CHECK(initPlatformWindow(vkCtx, 640, 480, "Magma", &windowInfo));	

	SwapChain swapChain = {};
	VK_CHECK(createSwapChain(vkCtx, windowInfo, 2, &swapChain));

///////////////////////////pipeline creation//////////////////////////////////////////////////

	Mesh mesh = {};
	if(!loadOBJ("objs/cube.obj", &mesh))
	{
		return EXIT_FAILURE;
	}

	Shader vertexShader = {};
	VK_CHECK(loadShader(vkCtx.logicalDevice, "shaders/cubeVert.spv", VK_SHADER_STAGE_VERTEX_BIT, &vertexShader));

	Shader fragmentShader = {};
	VK_CHECK(loadShader(vkCtx.logicalDevice, "shaders/cubeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, &fragmentShader));

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
	
	VkVertexInputAttributeDescription attribDescriptions[2] = {};
	//positions
	attribDescriptions[0].location = 0;
	attribDescriptions[0].binding = 0;
	attribDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDescriptions[0].offset = 0;
	//normals
	attribDescriptions[1].location = 1;
	attribDescriptions[1].binding = 0;
	attribDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDescriptions[1].offset = sizeof(Vec3);

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.pNext = nullptr;
    vertexInputStateCreateInfo.flags = VK_FLAGS_NONE;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 2;
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
    rasterStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
	
	VkImageCreateInfo depthImageCreateInfo = {};
    depthImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageCreateInfo.pNext = nullptr;
    depthImageCreateInfo.flags = VK_FLAGS_NONE;
    depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.format = depthFormat;
    depthImageCreateInfo.extent = {width, height, 1};
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageCreateInfo.queueFamilyIndexCount = 1;
    depthImageCreateInfo.pQueueFamilyIndices = &vkCtx.queueFamIdx;
    depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage depthImage = VK_NULL_HANDLE;
	VK_CALL(vkCreateImage(vkCtx.logicalDevice, &depthImageCreateInfo, nullptr, &depthImage));
    
	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(vkCtx.logicalDevice, depthImage, &memRequirements);
	VkPhysicalDeviceMemoryProperties deviceMemProps = {};
	vkGetPhysicalDeviceMemoryProperties(vkCtx.physicalDevice, &deviceMemProps);
	//deviceMemProps.memoryTypes
	uint32_t imageMemTypeBits = memRequirements.memoryTypeBits;

	uint32_t preferredMemTypeIndex = -1;
	findRequiredMemoryTypeIndex(vkCtx.physicalDevice, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &preferredMemTypeIndex);

	VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext = nullptr;
    memAllocInfo.allocationSize = memRequirements.size;
    memAllocInfo.memoryTypeIndex = preferredMemTypeIndex;

	VkDeviceMemory depthImageMemory = {};
	VK_CALL(vkAllocateMemory(vkCtx.logicalDevice, &memAllocInfo, nullptr, &depthImageMemory));

	VK_CALL(vkBindImageMemory(vkCtx.logicalDevice, depthImage, depthImageMemory, 0));
	
	VkImageViewCreateInfo depthViewCreateInfo = {};
    depthViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewCreateInfo.pNext = nullptr;
    depthViewCreateInfo.flags = VK_FLAGS_NONE;
    depthViewCreateInfo.image = depthImage;
    depthViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewCreateInfo.format = depthFormat;
    depthViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewCreateInfo.subresourceRange.baseMipLevel = 0;
    depthViewCreateInfo.subresourceRange.levelCount = 1;
    depthViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    depthViewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView depthImageView = VK_NULL_HANDLE;
	VK_CALL(vkCreateImageView(vkCtx.logicalDevice, &depthViewCreateInfo, nullptr, &depthImageView));


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
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

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
	VkCommandPool cmdPool = createCommandPool(vkCtx);
	Buffer stagingVertexBuffer = createBuffer(vkCtx, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, mesh.vertexBuffer.size() * sizeof(Vertex));
	Buffer deviceLocalVertexBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		stagingVertexBuffer.bufferSize);
	VK_CALL(copyDataToStagingBuffer(vkCtx.logicalDevice, 0, mesh.vertexBuffer.data(), &stagingVertexBuffer));
	VK_CHECK(pushDataToDeviceLocalBuffer(cmdPool, vkCtx, stagingVertexBuffer, &deviceLocalVertexBuffer));
	
	//push indices to device local memory
	Buffer stagingIndexBuffer = createBuffer(vkCtx, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, mesh.indexBuffer.size() * sizeof(unsigned int));
	Buffer deviceLocalIndexBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		stagingIndexBuffer.bufferSize);
	VK_CALL(copyDataToStagingBuffer(vkCtx.logicalDevice, 0, mesh.indexBuffer.data(), &stagingIndexBuffer));
	VK_CHECK(pushDataToDeviceLocalBuffer(cmdPool, vkCtx, stagingIndexBuffer, &deviceLocalIndexBuffer));



	//build actual frame buffers for render pass commands
//	buildFrameBuffers(vkCtx.logicalDevice, pipeState, windowInfo.windowExtent, &swapChain);
	for(std::size_t i = 0; i < swapChain.imageCount; i++)
	{
		VkImageView imageViews[2] = {
			swapChain.runtime.imageViews[i],
			depthImageView
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

	std::vector<VkCommandBuffer> cmdBuffers = {};
	createCommandBuffers(vkCtx.logicalDevice, cmdPool, swapChain.imageCount, cmdBuffers);
	
	//set clear color for depth and color attachments with LOAD_OP_CLEAR values in it
	VkClearValue clearValues[2] = {};
	clearValues[0].color = {0.654f, 0.984f, 0.968f, 1.f};
	clearValues[1].depthStencil.depth = 1.f;
	clearValues[1].depthStencil.stencil = 0;

	//record commands into command buffers
	for(uint32_t i = 0; i < cmdBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo beginInfo = {};
	    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		vkBeginCommandBuffer(cmdBuffers[i], &beginInfo);

		VkRenderPassBeginInfo renderPassBeginInfo = {};
    	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    	renderPassBeginInfo.pNext = nullptr;
    	renderPassBeginInfo.renderPass = renderPass;
    	renderPassBeginInfo.framebuffer = swapChain.runtime.frameBuffers[i];
    	renderPassBeginInfo.renderArea.offset = {0, 0};
		renderPassBeginInfo.renderArea.extent = windowInfo.windowExtent;
    	renderPassBeginInfo.clearValueCount = 2;
    	renderPassBeginInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(cmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeState.pipeline);
			vkCmdSetViewport(cmdBuffers[i], 0, 1, &pipeState.viewport);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmdBuffers[i], 0, 1, &deviceLocalVertexBuffer.buffer, &offset);
			vkCmdBindIndexBuffer(cmdBuffers[i], deviceLocalIndexBuffer.buffer, offset, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdBuffers[i], mesh.indexBuffer.size(), 1, 0, 0, 0); 
		vkCmdEndRenderPass(cmdBuffers[i]);

		vkEndCommandBuffer(cmdBuffers[i]);
	}

	uint32_t syncIndex = 0;
	auto& imageFences = swapChain.runtime.workSubmittedFences;
	auto& imageAvailableSemaphores = swapChain.runtime.imageAvailableSemaphores;
	auto& imageMayPresentSemaphores = swapChain.runtime.imageMayPresentSemaphores;

	while(!windowShouldClose(windowInfo.windowHandle))
	{
		updateMessageQueue();
		uint32_t imageIndex = {};
		
		//wait on host side before we may start using same image that was used a frame before
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