#include <volk.h>
#include <GLFW/glfw3.h>

#include "maths.h"
#include "logging.h"
#include "vk_dbg.h"
#include "vk_boilerplate.h"
#include "vk_shader.h"
#include "vk_buffer.h"

#include <vector>
#include <string>

std::vector<const char*> desiredExtensions = {
	"VK_EXT_debug_report"
};
std::vector<const char*> desiredLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

const char* glfwError()
{
	const char* err;
	int status = glfwGetError(&err);
	if(status != GLFW_NO_ERROR)
	{
		return err;
	}
	
	return "No error";
}

int main(int argc, char **argv)
{
	magma::log::setSeverityMask(magma::log::MASK_ALL);
	magma::log::warn("Loading Vulkan loader..");
	VK_CALL(volkInitialize());
	
	if(glfwInit() != GLFW_TRUE)
	{
		magma::log::error("GLFW: failed to init! {}", glfwError());
		return -1;
	}

	if(!glfwVulkanSupported())
	{
		magma::log::error("GLFW: vulkan is not supported! {}", glfwError());
		return -1;
	}

	uint32_t requiredExtCount = {};
	const char** requiredExtStrings = glfwGetRequiredInstanceExtensions(&requiredExtCount);

	for(std::size_t i = 0; i < requiredExtCount; i++)
	{
		desiredExtensions.push_back(requiredExtStrings[i]);
	}

	VK_CHECK(requestLayersAndExtensions(desiredExtensions, desiredLayers));
	
	VkInstance instance = createInstance();
	volkLoadInstance(instance);
	VkDebugReportCallbackEXT dbgCallback = registerDebugCallback(instance);
	
	uint32_t queueFamilyIdx = -1;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VK_CHECK(pickQueueIndexAndPhysicalDevice(
		instance,
		VK_QUEUE_GRAPHICS_BIT,
		VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
		&physicalDevice,
		&queueFamilyIdx)
	);
	assert(queueFamilyIdx >= 0);
	VkDevice logicalDevice = createLogicalDevice(physicalDevice, queueFamilyIdx);
	volkLoadDevice(logicalDevice);

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(logicalDevice, queueFamilyIdx, 0, &graphicsQueue);
	assert(graphicsQueue != VK_NULL_HANDLE);
	
	//check whether selected queue family index supports window image presentation
	if(glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, queueFamilyIdx) == GLFW_FALSE)
	{
		magma::log::error("Selected queue family index does not support image presentation!");
		return -1;
	}

	//disable glfw window context creation
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	int windowWidth = 640;
	int windowHeight = 480;
	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Magma", nullptr, nullptr);
	
	assert(window);
	//create OS specific window surface to render into
	VkSurfaceKHR windowSurface = VK_NULL_HANDLE;
	VK_CALL(glfwCreateWindowSurface(instance, window, nullptr, &windowSurface));

	//query created surface capabilities
	VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, windowSurface, &surfaceCapabilities));
	//query for available surface formats
	uint32_t surfaceFormatCount = {};
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &surfaceFormatCount, nullptr));
	std::vector<VkSurfaceFormatKHR> surfaceFormats = {};
	surfaceFormats.resize(surfaceFormatCount);
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &surfaceFormatCount, surfaceFormats.data()));
	VkFormat preferredFormat = VK_FORMAT_B8G8R8A8_UNORM;
	bool formatFound = false;
	VkSurfaceFormatKHR selectedFormat = {}; 
	for(auto& format : surfaceFormats)
	{
		if(format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM)
		{
			selectedFormat = format;
			formatFound = true;
			break;
		}
	}
	selectedFormat = formatFound ? selectedFormat : surfaceFormats[0];

	//query for available surface presentation mode
	uint32_t presentModeCount = {};
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, nullptr));
	VkPresentModeKHR presentModes[VK_PRESENT_MODE_RANGE_SIZE_KHR];
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, presentModes));
	VkPresentModeKHR preferredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;//should exist anyway
	for(uint32_t i = 0; i < presentModeCount; i++)
	{
		if(presentModes[i] == preferredPresentMode)
		{
			selectedPresentMode = preferredPresentMode;
			break;
		}
	}

	//determine current image extent:
	VkExtent2D selectedExtent = surfaceCapabilities.currentExtent;
	if(surfaceCapabilities.currentExtent.width == 0xffffffff
		|| surfaceCapabilities.currentExtent.height == 0xffffffff)
	{
		selectedExtent.width = std::min(std::max(surfaceCapabilities.minImageExtent.width, (uint32_t)windowWidth), surfaceCapabilities.maxImageExtent.width);
		selectedExtent.height = std::min(std::max(surfaceCapabilities.minImageExtent.width, (uint32_t)windowWidth), surfaceCapabilities.maxImageExtent.width);
	}

	//check for surface presentation support, to suppress validation layer warnings, even though 
	// we've check that already using glfw
	VkBool32 surfaceSupported = VK_FALSE;
	VK_CALL(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIdx, windowSurface, &surfaceSupported));
	assert(surfaceSupported == VK_TRUE);

	//create swapchain
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = nullptr;
	swapChainCreateInfo.flags = VK_FLAGS_NONE;
	swapChainCreateInfo.surface = windowSurface;
	swapChainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
	swapChainCreateInfo.imageFormat = selectedFormat.format;
	swapChainCreateInfo.imageColorSpace = selectedFormat.colorSpace;
	swapChainCreateInfo.imageExtent = selectedExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCreateInfo.queueFamilyIndexCount = 1;
	swapChainCreateInfo.pQueueFamilyIndices = &queueFamilyIdx;
	swapChainCreateInfo.presentMode = selectedPresentMode;
	swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	VK_CALL(vkCreateSwapchainKHR(logicalDevice, &swapChainCreateInfo, nullptr, &swapChain));

	//grab swapchain images
	uint32_t swapChainImageCount = {};
	VK_CALL(vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapChainImageCount, nullptr));
	std::vector<VkImage> swapChainImages = {};
	swapChainImages.resize(swapChainImageCount);
	VK_CALL(vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapChainImageCount, swapChainImages.data()));
	VkExtent2D currentImageExtent = selectedExtent;
	VkFormat swapchainImageFormat = selectedFormat.format;

	//images that we can actually "touch"
	std::vector<VkImageView> imageViews = {};
	imageViews.resize(swapChainImageCount);

	//populate imageViews vector
	for(uint32_t i = 0; i < imageViews.size(); i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.image = swapChainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = swapchainImageFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		VK_CALL(vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &imageViews[i]));
	}

	//loading spirv shaders
	VkShaderModule vertShaderModule = VK_NULL_HANDLE;
	VkShaderModule fragShaderModule = VK_NULL_HANDLE;
	VK_CHECK(loadShader(logicalDevice, "./shaders/triangleVert.spv", &vertShaderModule));
	VK_CHECK(loadShader(logicalDevice, "./shaders/triangleFrag.spv", &fragShaderModule));

	//pipeline configuration:

	//assign shaders to a specific pipeline stage
	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2] = {};
	shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfos[0].pNext = nullptr;
	shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageCreateInfos[0].module = vertShaderModule;
	shaderStageCreateInfos[0].pName = "main";
	shaderStageCreateInfos[0].pSpecializationInfo = nullptr;

	shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfos[1].pNext = nullptr;
	shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageCreateInfos[1].module = fragShaderModule;
	shaderStageCreateInfos[1].pName = "main";
	shaderStageCreateInfos[1].pSpecializationInfo = nullptr;
	
	struct VertexPC
	{
		Vec3 pos;
		Vec3 col;
	};

	VertexPC verticies[3] = {
		{{-0.5f, -0.5f, 0.f}, {0.6f, 0.9f, 1.f}},
		{{ 0.5f, -0.5f, 0.f}, {0.6f, 0.9f, 1.f}},
		{{ 0.0f,  0.5f, 0.f}, {1.f, 0.f, 0.f}}
	};

	Buffer stagingBuffer = createBuffer(logicalDevice, physicalDevice,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		sizeof(verticies), queueFamilyIdx
	);
	VK_CALL(copyDataToStagingBuffer(logicalDevice, 0, &verticies, &stagingBuffer));

	Buffer deviceLocalBuffer = createBuffer(logicalDevice, physicalDevice, 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		sizeof(verticies),
		queueFamilyIdx
	);

	//creating command buffer for transfer operation
	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.pNext = nullptr;
	cmdPoolCreateInfo.flags = VK_FLAGS_NONE;
	cmdPoolCreateInfo.queueFamilyIndex = queueFamilyIdx;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VK_CALL(vkCreateCommandPool(logicalDevice, &cmdPoolCreateInfo, nullptr, &commandPool));

	VkCommandBufferAllocateInfo cmdBuffAllocInfo = {};
	cmdBuffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBuffAllocInfo.pNext = nullptr;
	cmdBuffAllocInfo.commandPool = commandPool;
	cmdBuffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBuffAllocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CALL(vkAllocateCommandBuffers(logicalDevice, &cmdBuffAllocInfo, &commandBuffer));

	//begin recording transfer commands
	VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext = nullptr;
	cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;

	VK_CALL(vkBeginCommandBuffer(commandBuffer, &cmdBufferBeginInfo));

	VkBufferCopy bufferCopy = {};
	bufferCopy.size = sizeof(verticies);

	vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, deviceLocalBuffer.buffer, 1, &bufferCopy);
	
	VK_CALL(vkEndCommandBuffer(commandBuffer));
	
	VkSubmitInfo queueSubmitInfo = {};
	queueSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext = nullptr;
	queueSubmitInfo.waitSemaphoreCount = 0;
	queueSubmitInfo.pWaitSemaphores = nullptr;
	queueSubmitInfo.pWaitDstStageMask = nullptr;
	queueSubmitInfo.commandBufferCount = 1;
	queueSubmitInfo.pCommandBuffers = &commandBuffer;
	queueSubmitInfo.signalSemaphoreCount = 0;
	queueSubmitInfo.pSignalSemaphores = nullptr;

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = 0;

	VkFence fence = VK_NULL_HANDLE;
	VK_CALL(vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &fence));

	VK_CALL(vkQueueSubmit(graphicsQueue, 1, &queueSubmitInfo, fence));
	VK_CALL(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, 1000000000));

	//set fence back to unsignalled state
	VK_CALL(vkResetFences(logicalDevice, 1, &fence));

	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
	//destroy staging buffer since we don't need it anymore
	destroyBuffer(logicalDevice, &stagingBuffer);

	VkVertexInputBindingDescription vertexBindingDescription = {};
	vertexBindingDescription.binding = 0;
	vertexBindingDescription.stride = sizeof(VertexPC);
	vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertexAttribDescriptions[2] = {};
	vertexAttribDescriptions[0].location = 0;
	vertexAttribDescriptions[0].binding = 0;
	vertexAttribDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttribDescriptions[0].offset = 0;

	vertexAttribDescriptions[1].location = 1;
	vertexAttribDescriptions[1].binding = 0;
	vertexAttribDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttribDescriptions[1].offset = sizeof(Vec3);
	
	VkPipelineVertexInputStateCreateInfo pipeVertexInputStateCreateInfo = {};
	pipeVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipeVertexInputStateCreateInfo.pNext = nullptr;
	pipeVertexInputStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	pipeVertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;
	pipeVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 2;
	pipeVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttribDescriptions;

	VkPipelineInputAssemblyStateCreateInfo pipeInputAssemblyCreateInfo = {};
	pipeInputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipeInputAssemblyCreateInfo.pNext = nullptr;
	pipeInputAssemblyCreateInfo.flags = VK_FLAGS_NONE;
	pipeInputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipeInputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

/*
	VkPipelineTessellationStateCreateInfo pipeTesselationStateCreateInfo = {};
	pipeTesselationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	pipeTesselationStateCreateInfo.pNext = nullptr;
	pipeTesselationStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeTesselationStateCreateInfo.patchControlPoints = ;
*/

	VkViewport viewport = {};
	viewport.x = 0.f;
	viewport.y = windowHeight;
	viewport.width = windowWidth;
	viewport.height = -windowHeight;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	VkRect2D scissors = {};
	scissors.extent = currentImageExtent;

	VkPipelineViewportStateCreateInfo pipeViewPortStateCreateInfo = {};
	pipeViewPortStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipeViewPortStateCreateInfo.pNext = nullptr;
	pipeViewPortStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeViewPortStateCreateInfo.viewportCount = 1;
	pipeViewPortStateCreateInfo.pViewports = &viewport;
	pipeViewPortStateCreateInfo.scissorCount = 1;
	pipeViewPortStateCreateInfo.pScissors = &scissors;

	VkPipelineRasterizationStateCreateInfo pipeRastStateCreateInfo = {};
	pipeRastStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipeRastStateCreateInfo.pNext = nullptr;
	pipeRastStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeRastStateCreateInfo.depthClampEnable = VK_FALSE;
	pipeRastStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	pipeRastStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipeRastStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;//backface culling (i.e back triangles are discarded)
	pipeRastStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipeRastStateCreateInfo.depthBiasEnable = VK_FALSE;
	pipeRastStateCreateInfo.depthBiasConstantFactor = 0.f;
	pipeRastStateCreateInfo.depthBiasClamp = 0.f;
	pipeRastStateCreateInfo.depthBiasSlopeFactor = 0.f;
	pipeRastStateCreateInfo.lineWidth = 1.f;

	//disable msaa for now
	VkPipelineMultisampleStateCreateInfo pipeMultisampleCreateInfo = {};
	pipeMultisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipeMultisampleCreateInfo.pNext = nullptr;
	pipeMultisampleCreateInfo.flags = VK_FLAGS_NONE;
	pipeMultisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipeMultisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	pipeMultisampleCreateInfo.minSampleShading = 1.f;
	pipeMultisampleCreateInfo.pSampleMask = nullptr;
	pipeMultisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
	pipeMultisampleCreateInfo.alphaToOneEnable = VK_FALSE;


	//disable depth/stencile pipe
	VkPipelineDepthStencilStateCreateInfo pipeDepthStencilCreateInfo = {};
	pipeDepthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipeDepthStencilCreateInfo.pNext = nullptr;
	pipeDepthStencilCreateInfo.flags = VK_FLAGS_NONE;
	//pipeDepthStencilCreateInfo.depthTestEnable = VK_TRUE;
	//pipeDepthStencilCreateInfo.depthWriteEnable = VK_TRUE;
	//pipeDepthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	pipeDepthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	pipeDepthStencilCreateInfo.stencilTestEnable = VK_FALSE;
	//pipeDepthStencilCreateInfo.front = ;
	//pipeDepthStencilCreateInfo.back = ;
	pipeDepthStencilCreateInfo.minDepthBounds = 0.f;
	pipeDepthStencilCreateInfo.maxDepthBounds = 1.f;

	//color blending state:

	//1.specify per-target color blend settings per each color attachment
	//alpha-blend testing(blend fragment color based on it's opacity)
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	//colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	//colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	//colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	//colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	//colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	//colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|
		VK_COLOR_COMPONENT_G_BIT|
		VK_COLOR_COMPONENT_B_BIT|
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo pipeColorBlendCreateInfo = {};
	pipeColorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipeColorBlendCreateInfo.pNext = nullptr;
	pipeColorBlendCreateInfo.flags = VK_FLAGS_NONE;
	pipeColorBlendCreateInfo.logicOpEnable = VK_FALSE;
	pipeColorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	pipeColorBlendCreateInfo.attachmentCount = 1;
	pipeColorBlendCreateInfo.pAttachments = &colorBlendAttachmentState;
	pipeColorBlendCreateInfo.blendConstants[0] = 0.f;
	pipeColorBlendCreateInfo.blendConstants[1] = 0.f;
	pipeColorBlendCreateInfo.blendConstants[2] = 0.f;
	pipeColorBlendCreateInfo.blendConstants[3] = 0.f;

	//amount of settings that can be changed without recreating the whole pipeline
	VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};
	VkPipelineDynamicStateCreateInfo pipeDynamicStateCreateInfo = {};
	pipeDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipeDynamicStateCreateInfo.pNext = nullptr;
	pipeDynamicStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeDynamicStateCreateInfo.dynamicStateCount = 1;
	pipeDynamicStateCreateInfo.pDynamicStates = dynamicStates;

	VkPipelineLayoutCreateInfo pipeLayoutCreateInfo = {};
	pipeLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutCreateInfo.pNext =  nullptr;
	pipeLayoutCreateInfo.flags = VK_FLAGS_NONE;
	pipeLayoutCreateInfo.setLayoutCount = 0;
	pipeLayoutCreateInfo.pSetLayouts = nullptr;
	pipeLayoutCreateInfo.pushConstantRangeCount = 0;
	pipeLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreatePipelineLayout(logicalDevice, &pipeLayoutCreateInfo, nullptr, &pipelineLayout));

	VkAttachmentDescription attachmentDescr = {};
	//color attachment
	attachmentDescr.flags = {};
	attachmentDescr.format = swapchainImageFormat;
	attachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescr.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescr.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

/*
	//depth attachment
	attachmentDescr[0].flags = {};
	attachmentDescr[0].format = swapchainImageFormat;
	attachmentDescr[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescr[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescr[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescr[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescr[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescr[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescr[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
*/
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;//index to a vkAttachmentDescription array
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//???? WHY

	VkSubpassDescription subpassDescr = {};
//    VkSubpassDescriptionFlags       flags;
	subpassDescr.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  //  uint32_t                        inputAttachmentCount;
  //  const VkAttachmentReference*    pInputAttachments;
	subpassDescr.colorAttachmentCount = 1;
	subpassDescr.pColorAttachments = &colorAttachmentRef;
	//const VkAttachmentReference*    pResolveAttachments;
	//const VkAttachmentReference*    pDepthStencilAttachment;
	//uint32_t                        preserveAttachmentCount;
	//const uint32_t*                 pPreserveAttachments;

	VkSubpassDependency subpassDep = {};
	subpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDep.dstSubpass = 0;
	subpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDep.srcAccessMask = 0;
	subpassDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDep.dependencyFlags = VK_FLAGS_NONE;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.flags = VK_FLAGS_NONE;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachmentDescr;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescr;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &subpassDep;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VK_CALL(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass));
//	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = nullptr;
	graphicsPipelineCreateInfo.flags = VK_FLAGS_NONE;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos;
	graphicsPipelineCreateInfo.pVertexInputState = &pipeVertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &pipeInputAssemblyCreateInfo;
//	graphicsPipelineCreateInfo.pTessellationState =;
	graphicsPipelineCreateInfo.pViewportState = &pipeViewPortStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &pipeRastStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &pipeMultisampleCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &pipeDepthStencilCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &pipeColorBlendCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &pipeDynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = renderPass;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	VkPipeline graphicsPipe = VK_NULL_HANDLE;
	VK_CALL(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipe));

	std::vector<VkFramebuffer> frameBuffers = {};
	frameBuffers.resize(imageViews.size());
	for(std::size_t i = 0; i < frameBuffers.size(); i++)
	{
		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = nullptr;
		frameBufferCreateInfo.flags = VK_FLAGS_NONE;
		frameBufferCreateInfo.renderPass = renderPass;
		frameBufferCreateInfo.attachmentCount = 1;
		frameBufferCreateInfo.pAttachments = &imageViews[i];
		frameBufferCreateInfo.width = currentImageExtent.width;
		frameBufferCreateInfo.height = currentImageExtent.height;
		frameBufferCreateInfo.layers = 1;
		VK_CALL(vkCreateFramebuffer(logicalDevice, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
	}

	//build command buffer for each swapchain image
	VkCommandBufferAllocateInfo buffAllocInfo = {};
	buffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	buffAllocInfo.pNext = nullptr;
	buffAllocInfo.commandPool = commandPool;
	buffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	buffAllocInfo.commandBufferCount = swapChainImages.size();
	std::vector<VkCommandBuffer> commandBuffers = {};
	commandBuffers.resize(swapChainImages.size());
	vkAllocateCommandBuffers(logicalDevice, &buffAllocInfo,commandBuffers.data());
	for(uint32_t i = 0; i < commandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo cmdBuffBegInfo = {};
    	cmdBuffBegInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    	cmdBuffBegInfo.pNext = nullptr;
    	cmdBuffBegInfo.flags = VK_FLAGS_NONE;
    	cmdBuffBegInfo.pInheritanceInfo = nullptr;

		VkClearValue clearColor = {};
		clearColor.color = {0.7f, 0.76f, 0.76f, 1.f};
		
		VK_CALL(vkBeginCommandBuffer(commandBuffers[i], &cmdBuffBegInfo));
		VkRenderPassBeginInfo renderPassBegInfo = {};
    	renderPassBegInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    	renderPassBegInfo.pNext = nullptr;
    	renderPassBegInfo.renderPass = renderPass;
    	renderPassBegInfo.framebuffer = frameBuffers[i];
    	renderPassBegInfo.renderArea.offset = {0, 0};
		renderPassBegInfo.renderArea.extent = currentImageExtent;
    	renderPassBegInfo.clearValueCount = 1;
    	renderPassBegInfo.pClearValues = &clearColor;
		
		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBegInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipe);
			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
			VkDeviceSize offsets = 0;
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &deviceLocalBuffer.buffer, &offsets);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[i]);

		VK_CALL(vkEndCommandBuffer(commandBuffers[i]));
	}

	VkSemaphore imageAvailable = {};
	VkSemaphore imageMayPresent = {};

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	VK_CALL(vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable));
	VK_CALL(vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &imageMayPresent));

	//render loop
	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		
		uint32_t imageId = {};
		vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageId);
		VkSubmitInfo queueSubmitInfo = {};
		queueSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		queueSubmitInfo.pNext = nullptr;
		queueSubmitInfo.waitSemaphoreCount = 1;
		queueSubmitInfo.pWaitSemaphores = &imageAvailable;
		VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		queueSubmitInfo.pWaitDstStageMask = &waitMask;
		queueSubmitInfo.commandBufferCount = 1;
		queueSubmitInfo.pCommandBuffers = &commandBuffers[imageId];
		queueSubmitInfo.signalSemaphoreCount = 1;
		queueSubmitInfo.pSignalSemaphores = &imageMayPresent;
		
		VK_CALL(vkQueueSubmit(graphicsQueue, 1, &queueSubmitInfo, VK_NULL_HANDLE));

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &imageMayPresent;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &imageId;
		presentInfo.pResults = nullptr;

		VK_CALL(vkQueuePresentKHR(graphicsQueue, &presentInfo));
		vkDeviceWaitIdle(logicalDevice);
	}
	return 0;
}