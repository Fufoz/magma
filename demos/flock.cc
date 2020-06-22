#include <magma.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"

#include <meshoptimizer.h>

#include <memory>
#include <vector>

struct Vertex
{
	Vec3 position;
	Vec3 normal;
	Vec2 uv;
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
		magma::log::error("Failed to load mesh from {}", path);
		return false;
	}

	//assume triangle mesh here
	std::vector<Vertex> vertices = {};
	assert(mesh->face_vertices[0] == 3);
	const uint32_t vertexCount = mesh->face_vertices[0];
	vertices.resize(mesh->face_count * mesh->face_vertices[0]);

	//loop through each face
	for(uint32_t i = 0; i < mesh->face_count; i++)
	{
		//loop through each vertex in a face
		for(uint32_t j = 0; j < vertexCount; j++)
		{
			const uint32_t vi = mesh->indices[3 * i + j].p;
			const uint32_t vn = mesh->indices[3 * i + j].n;
			const uint32_t vt = mesh->indices[3 * i + j].t;

			const std::size_t out_index = i * vertexCount + j;

			vertices[out_index].position.x = mesh->positions[3 * vi + 0];
			vertices[out_index].position.y = mesh->positions[3 * vi + 1];
			vertices[out_index].position.z = mesh->positions[3 * vi + 2];
			
			vertices[out_index].normal.x = mesh->normals[3 * vn + 0];
			vertices[out_index].normal.y = mesh->normals[3 * vn + 1];
			vertices[out_index].normal.z = mesh->normals[3 * vn + 2];

			vertices[out_index].uv.u = mesh->texcoords[2 * vt + 0];
			vertices[out_index].uv.v = mesh->texcoords[2 * vt + 1];
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
		magma::log::debug("Pos {}; Normal {}; UV {}", vertex.position, vertex.normal, vertex.uv);
	}
	
	for(auto index : indexBuffer)
	{
		magma::log::debug("Index {}", index);
	}
	
	geom->indexBuffer = indexBuffer;
	geom->vertexBuffer = vertexBuffer;

	return true;
}

struct TextureInfo
{
	VkFormat format;
	uint32_t width;
	uint32_t height;
	uint8_t  numc;
	uint8_t* data;
};

bool loadTexture(const char* path, TextureInfo* out, bool flipImage = true)
{
	int twidth;
	int theight;
	int numChannels;
	assert(out);

	stbi_set_flip_vertically_on_load(flipImage);
	uint8_t* data = stbi_load(path, &twidth, &theight, &numChannels, 4);
	
	if(!data) 
	{
		magma::log::error("Failed to load texture from path {}", path);
		return false;
	}

	out->width = static_cast<uint32_t>(twidth); 
	out->height = static_cast<uint32_t>(theight);
	out->numc = 4; 
	out->data = data;
	out->format = VK_FORMAT_R8G8B8A8_UNORM;
	return true;
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
	if(!loadOBJ("objs/fish.obj", &mesh))
	{
		return -1;
	}

	TextureInfo fishTexture = {};
	if(!loadTexture("objs/fish.png", &fishTexture))
	{
		return -1;
	}

	//setup uniform  buffer and sampler object 
	//letting hardware to know upfront what descriptor type and binding count ubo will have
	VkDescriptorSetLayoutBinding layoutBindings[2] = {};
	//mvp
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutBindings[0].pImmutableSamplers = nullptr;
	
	//sampler
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[1].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.pNext = nullptr;
    descriptorSetLayoutInfo.bindingCount = 2;
    descriptorSetLayoutInfo.pBindings = layoutBindings;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreateDescriptorSetLayout(vkCtx.logicalDevice, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

	//for now store only camera direction vector
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Vec3);


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
	
	VkVertexInputAttributeDescription attribDescriptions[3] = {};
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
	//uvs
	attribDescriptions[2].location = 2;
	attribDescriptions[2].binding = 0;
	attribDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attribDescriptions[2].offset = 2 * sizeof(Vec3);

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = VK_FLAGS_NONE;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 3;
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

	//create texture image
	//1. create staging buffer to copy texture contents to gpu local memory
	Buffer stagingTextureBuffer = createBuffer(vkCtx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		fishTexture.numc * fishTexture.width * fishTexture.height
	);

	VK_CALL(copyDataToHostVisibleBuffer(vkCtx.logicalDevice, 0, fishTexture.data, &stagingTextureBuffer));

	//create image on device local memory to recieve texture data
	VkImageCreateInfo textureImageCreateInfo = {};
	textureImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	textureImageCreateInfo.pNext = nullptr;
	textureImageCreateInfo.flags = VK_FLAGS_NONE;
	textureImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	textureImageCreateInfo.format = fishTexture.format;
	textureImageCreateInfo.extent = {fishTexture.width, fishTexture.height, 1};
	textureImageCreateInfo.mipLevels = 1;
	textureImageCreateInfo.arrayLayers = 1;
	textureImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	textureImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	textureImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	textureImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureImageCreateInfo.queueFamilyIndexCount = 1;
	textureImageCreateInfo.pQueueFamilyIndices = &vkCtx.queueFamIdx;
	textureImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	VkImage textureImage = VK_NULL_HANDLE;
	vkCreateImage(vkCtx.logicalDevice, &textureImageCreateInfo, nullptr, &textureImage);

	VkMemoryRequirements texMemRequirements = {};
	vkGetImageMemoryRequirements(vkCtx.logicalDevice, textureImage, &texMemRequirements);
	deviceMemProps = {};
	vkGetPhysicalDeviceMemoryProperties(vkCtx.physicalDevice, &deviceMemProps);

	preferredMemTypeIndex = -1;
	findRequiredMemoryTypeIndex(vkCtx.physicalDevice, texMemRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &preferredMemTypeIndex);

	VkMemoryAllocateInfo texMemAllocInfo = {};
	texMemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	texMemAllocInfo.pNext = nullptr;
	texMemAllocInfo.allocationSize = texMemRequirements.size;
	texMemAllocInfo.memoryTypeIndex = preferredMemTypeIndex;

	VkDeviceMemory texImageMemory = {};
	VK_CALL(vkAllocateMemory(vkCtx.logicalDevice, &texMemAllocInfo, nullptr, &texImageMemory));

	VK_CALL(vkBindImageMemory(vkCtx.logicalDevice, textureImage, texImageMemory, 0));

	VkImageViewCreateInfo textureViewCreateInfo = {};
    textureViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    textureViewCreateInfo.pNext = nullptr;
    textureViewCreateInfo.flags = VK_FLAGS_NONE;
    textureViewCreateInfo.image = textureImage;
    textureViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    textureViewCreateInfo.format = fishTexture.format;
    textureViewCreateInfo.components = {
		VK_COMPONENT_SWIZZLE_R,
		VK_COMPONENT_SWIZZLE_G,
		VK_COMPONENT_SWIZZLE_B,
		VK_COMPONENT_SWIZZLE_ONE //maximum opacity
	};
    textureViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    textureViewCreateInfo.subresourceRange.baseMipLevel = 0;
    textureViewCreateInfo.subresourceRange.levelCount = 1;
    textureViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    textureViewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView textureView = VK_NULL_HANDLE;
	VK_CALL(vkCreateImageView(vkCtx.logicalDevice, &textureViewCreateInfo, nullptr, &textureView));

	//copy texture data to device local memory
	VkCommandPool cmdPool = createCommandPool(vkCtx);
	VkCommandBuffer cmdBuff = VK_NULL_HANDLE;
	//1.recording transfer commands into command buffer
	createCommandBuffer(vkCtx.logicalDevice, cmdPool, &cmdBuff);

	VkCommandBufferBeginInfo cmdBuffBegInfo = {};
    cmdBuffBegInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBuffBegInfo.pNext = nullptr;
    cmdBuffBegInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    cmdBuffBegInfo.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(cmdBuff, &cmdBuffBegInfo);

	VkBufferImageCopy copyRegion = {};
	copyRegion.bufferOffset = 0;
	// copyRegion.bufferRowLength = fishTexture.width;
	// copyRegion.bufferImageHeight = fishTexture.height;
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageOffset = {0, 0, 0};
	copyRegion.imageExtent = {fishTexture.width, fishTexture.height, 1};

	//issue a barrier to transition our newly created texture from undefined layout to DST_OPTIMAL
	VkImageMemoryBarrier imageMemBarrier = {};
	imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemBarrier.pNext = nullptr;
	imageMemBarrier.srcAccessMask = 0;
	imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemBarrier.srcQueueFamilyIndex = vkCtx.queueFamIdx;
	imageMemBarrier.dstQueueFamilyIndex = vkCtx.queueFamIdx;
	imageMemBarrier.image = textureImage;
    imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemBarrier.subresourceRange.baseMipLevel = 0;
    imageMemBarrier.subresourceRange.levelCount = 1;
    imageMemBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemBarrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(
		cmdBuff, 
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemBarrier
	);

	vkCmdCopyBufferToImage(cmdBuff, stagingTextureBuffer.buffer, textureImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	//issue another barrier
	//transfer texture layout to the shader layout so it can be sampled
	imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	vkCmdPipelineBarrier(
		cmdBuff, 
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemBarrier
	);
	
	vkEndCommandBuffer(cmdBuff);

	VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuff;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

	//submit transfer command into queue
	VkFence textureTransferredFence = createFence(vkCtx.logicalDevice);
	VK_CALL(vkQueueSubmit(vkCtx.graphicsQueue, 1, &submitInfo, textureTransferredFence));
	VK_CALL(vkWaitForFences(vkCtx.logicalDevice, 1, &textureTransferredFence, VK_TRUE, UINT64_MAX));
	
	//cleanup
	vkDestroyFence(vkCtx.logicalDevice, textureTransferredFence, nullptr);
	vkFreeCommandBuffers(vkCtx.logicalDevice, cmdPool, 1, &cmdBuff);
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
	texture.image = textureImage;
	texture.textureView = textureView;
	texture.textureSampler = textureSampler;
	texture.texturelayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	texture.textureSize = texMemAllocInfo.allocationSize;
	texture.backupMemory = texImageMemory;



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
	Buffer stagingVertexBuffer = createBuffer(vkCtx, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, mesh.vertexBuffer.size() * sizeof(Vertex));
	Buffer deviceLocalVertexBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		stagingVertexBuffer.bufferSize);
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx.logicalDevice, 0, mesh.vertexBuffer.data(), &stagingVertexBuffer));
	VK_CHECK(pushDataToDeviceLocalBuffer(cmdPool, vkCtx, stagingVertexBuffer, &deviceLocalVertexBuffer));
	
	//push indices to device local memory
	Buffer stagingIndexBuffer = createBuffer(vkCtx, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, mesh.indexBuffer.size() * sizeof(unsigned int));
	Buffer deviceLocalIndexBuffer = createBuffer(vkCtx, 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		stagingIndexBuffer.bufferSize);
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx.logicalDevice, 0, mesh.indexBuffer.data(), &stagingIndexBuffer));
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

	//creating descriptor pool to allocate descriptor sets from
	VkDescriptorPoolSize descriptorPoolSizes[2] = {};
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorPoolSizes[0].descriptorCount = 1;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = nullptr;
    descriptorPoolCreateInfo.maxSets = 2;
    descriptorPoolCreateInfo.poolSizeCount = 2;
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

	//start updating descriptor set with actual data
	mat4x4 perspective = perspectiveProjection(90.f, width/(float)height, 0.1f, 100.f);
	mat4x4 testScale = loadTranslation(Vec3{-0.5f, -0.5f, 0.f}) * loadScale(Vec3{0.5f, 0.5f, 0.5f});


	Buffer ubo = createBuffer(vkCtx, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, sizeof(mat4x4));
	VK_CALL(copyDataToHostVisibleBuffer(vkCtx.logicalDevice, 0, &testScale, &ubo));

	//write data to descriptor set
	VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = ubo.buffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = ubo.bufferSize;
	
	VkDescriptorImageInfo descriptorImageInfo = {};
    descriptorImageInfo.sampler = texture.textureSampler;
    descriptorImageInfo.imageView = texture.textureView;
    descriptorImageInfo.imageLayout = texture.texturelayout;


	VkWriteDescriptorSet writeDescrSets[2] = {};
    writeDescrSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescrSets[0].pNext = nullptr;
    writeDescrSets[0].dstSet = descriptorSet;
    writeDescrSets[0].dstBinding = 0;
    writeDescrSets[0].dstArrayElement = 0;
    writeDescrSets[0].descriptorCount = 1;
    writeDescrSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescrSets[0].pImageInfo = nullptr;
    writeDescrSets[0].pBufferInfo = &descriptorBufferInfo;
    writeDescrSets[0].pTexelBufferView = nullptr;

    writeDescrSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescrSets[1].pNext = nullptr;
    writeDescrSets[1].dstSet = descriptorSet;
    writeDescrSets[1].dstBinding = 1;
    writeDescrSets[1].dstArrayElement = 0;
    writeDescrSets[1].descriptorCount = 1;
    writeDescrSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescrSets[1].pImageInfo = &descriptorImageInfo;
    writeDescrSets[1].pBufferInfo = nullptr;
    writeDescrSets[1].pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(vkCtx.logicalDevice, 2, writeDescrSets, 0, nullptr);


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
			vkCmdBindDescriptorSets(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeState.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
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
			vkCmdDrawIndexed(cmdBuffers[index], mesh.indexBuffer.size(), 1, 0, 0, 0); 
		vkCmdEndRenderPass(cmdBuffers[index]);

		vkEndCommandBuffer(cmdBuffers[index]);
	};

	while(!windowShouldClose(windowInfo.windowHandle))
	{
		updateMessageQueue();
		float deltaSec = timer.stopSec();
		magma::log::info("Frame took {} sec", deltaSec);
		timer.start();
		fpsCameraUpdate(windowInfo, deltaSec, &camera);
		auto newTransform = testScale * camera.viewTransform * perspective;
		//TODO: synchronize access to uniform buffer per swapchain image
		VK_CALL(copyDataToHostVisibleBuffer(vkCtx.logicalDevice, 0, &newTransform, &ubo));

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