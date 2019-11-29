#include <volk.h>

#include "maths.h"
#include "logging.h"
#include "vk_dbg.h"
#include "vk_boilerplate.h"
#include "vk_shader.h"
#include "vk_buffer.h"
#include "vk_swapchain.h"
#include "host_timer.h"
#include <vector>
#include <string>

std::vector<const char*> desiredExtensions = {
	"VK_EXT_debug_report"
};
std::vector<const char*> desiredLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

int main(int argc, char **argv)
{
	magma::log::setSeverityMask(magma::log::MASK_ALL);

	VulkanGlobalContext vkCtx = {};
	VK_CHECK(initVulkanGlobalContext(desiredLayers, desiredExtensions, &vkCtx));

	WindowInfo windowInfo = {};
	VK_CHECK(initPlatformWindow(vkCtx, 640, 480, "Magma", &windowInfo));
	
	SwapChain swapChain = {};
	VK_CHECK(createSwapChain(vkCtx, windowInfo, 2, &swapChain));

//////////////////////////////////////////////////////////////////////////
	struct VertexPC
	{
		Vec3 pos;
		Vec3 col;
	};

	VertexPC verticies[3] = {
		{{-0.5f, -0.5f, 0.f}, {0.6f, 0.9f, 1.f}},
		{{ 0.5f, -0.5f, 0.f}, {0.6f, 0.9f, 1.f}},
		{{ 0.0f,  0.5f, 0.f}, {1.0f, 0.0f, 0.f}}
	};

	Buffer stagingBuffer = createBuffer(vkCtx.logicalDevice, vkCtx.physicalDevice,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		sizeof(verticies), vkCtx.queueFamIdx
	);
	VK_CALL(copyDataToStagingBuffer(vkCtx.logicalDevice, 0, &verticies, &stagingBuffer));

	Buffer deviceLocalBuffer = createBuffer(vkCtx.logicalDevice, vkCtx.physicalDevice, 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		sizeof(verticies),
		vkCtx.queueFamIdx
	);

	//creating command buffer for transfer operation
	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.pNext = nullptr;
	cmdPoolCreateInfo.flags = VK_FLAGS_NONE;
	cmdPoolCreateInfo.queueFamilyIndex = vkCtx.queueFamIdx;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VK_CALL(vkCreateCommandPool(vkCtx.logicalDevice, &cmdPoolCreateInfo, nullptr, &commandPool));

	pushDataToDeviceLocalBuffer(commandPool, vkCtx, stagingBuffer, &deviceLocalBuffer);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//pipeline configuration:

	//loading spirv shaders
	VkShaderModule vertShaderModule = VK_NULL_HANDLE;
	VkShaderModule fragShaderModule = VK_NULL_HANDLE;
	VK_CHECK(loadShader(vkCtx.logicalDevice, "./shaders/triangleVert.spv", &vertShaderModule));
	VK_CHECK(loadShader(vkCtx.logicalDevice, "./shaders/triangleFrag.spv", &fragShaderModule));

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
	viewport.y = int(windowInfo.windowExtent.height);
	viewport.width = int(windowInfo.windowExtent.width);
	viewport.height = -int(windowInfo.windowExtent.height);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	VkRect2D scissors = {};
	scissors.extent = windowInfo.windowExtent;

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
	VkDynamicState dynamicStates = VK_DYNAMIC_STATE_VIEWPORT;

	VkPipelineDynamicStateCreateInfo pipeDynamicStateCreateInfo = {};
	pipeDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipeDynamicStateCreateInfo.pNext = nullptr;
	pipeDynamicStateCreateInfo.flags = VK_FLAGS_NONE;
	pipeDynamicStateCreateInfo.dynamicStateCount = 1;
	pipeDynamicStateCreateInfo.pDynamicStates = &dynamicStates;

	VkPipelineLayoutCreateInfo pipeLayoutCreateInfo = {};
	pipeLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutCreateInfo.pNext =  nullptr;
	pipeLayoutCreateInfo.flags = VK_FLAGS_NONE;
	pipeLayoutCreateInfo.setLayoutCount = 0;
	pipeLayoutCreateInfo.pSetLayouts = nullptr;
	pipeLayoutCreateInfo.pushConstantRangeCount = 0;
	pipeLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VK_CALL(vkCreatePipelineLayout(vkCtx.logicalDevice, &pipeLayoutCreateInfo, nullptr, &pipelineLayout));

	VkAttachmentDescription attachmentDescr = {};
	//color attachment
	attachmentDescr.flags = {};
	attachmentDescr.format = swapChain.imageFormat;
	attachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescr.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescr.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
	VK_CALL(vkCreateRenderPass(vkCtx.logicalDevice, &renderPassInfo, nullptr, &renderPass));

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
	VK_CALL(vkCreateGraphicsPipelines(vkCtx.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipe));

	std::vector<VkFramebuffer> frameBuffers = {};
	frameBuffers.resize(swapChain.imageCount);
	for(std::size_t i = 0; i < frameBuffers.size(); i++)
	{
		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = nullptr;
		frameBufferCreateInfo.flags = VK_FLAGS_NONE;
		frameBufferCreateInfo.renderPass = renderPass;
		frameBufferCreateInfo.attachmentCount = 1;
		frameBufferCreateInfo.pAttachments = &swapChain.runtime.imageViews[i];
		frameBufferCreateInfo.width = windowInfo.windowExtent.width;
		frameBufferCreateInfo.height = windowInfo.windowExtent.height;
		frameBufferCreateInfo.layers = 1;
		VK_CALL(vkCreateFramebuffer(vkCtx.logicalDevice, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
	}

	//build command buffer for each swapchain image
	VkCommandBufferAllocateInfo buffAllocInfo = {};
	buffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	buffAllocInfo.pNext = nullptr;
	buffAllocInfo.commandPool = commandPool;
	buffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	buffAllocInfo.commandBufferCount = swapChain.imageCount;
	std::vector<VkCommandBuffer> commandBuffers = {};
	commandBuffers.resize(swapChain.imageCount);
	vkAllocateCommandBuffers(vkCtx.logicalDevice, &buffAllocInfo, commandBuffers.data());
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
		renderPassBegInfo.renderArea.extent = windowInfo.windowExtent;
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

	uint32_t presentableFrames = swapChain.imageCount;
	std::vector<VkSemaphore> imageAvailableSemaphores = {};
	imageAvailableSemaphores.resize(presentableFrames);
	for(auto& semaphore : imageAvailableSemaphores)
	{
		semaphore = createSemaphore(vkCtx.logicalDevice);
	}

	std::vector<VkSemaphore> imageMayPresentSemaphores = {};
	imageMayPresentSemaphores.resize(presentableFrames);
	for(auto& semaphore : imageMayPresentSemaphores)
	{
		semaphore = createSemaphore(vkCtx.logicalDevice);
	}

	std::vector<VkFence> imageFences = {};
	imageFences.resize(presentableFrames);
	for(auto& fence : imageFences)
	{
		fence = createFence(vkCtx.logicalDevice, true);
	}

	uint32_t syncIndex = 0;//index in semaphore array to use

	//render loop
	while(!glfwWindowShouldClose((GLFWwindow*)windowInfo.windowHandle))
	{
		HostTimer t;
		t.start();
		glfwPollEvents();

		VK_CALL(vkWaitForFences(vkCtx.logicalDevice, 1, &imageFences[syncIndex], VK_TRUE, UINT64_MAX));
		VK_CALL(vkResetFences(vkCtx.logicalDevice, 1, &imageFences[syncIndex]));

		uint32_t imageId = {};

		VkResult acquireStatus = vkAcquireNextImageKHR(
			vkCtx.logicalDevice, swapChain.swapchain,
			UINT64_MAX, imageAvailableSemaphores[syncIndex],
			VK_NULL_HANDLE, &imageId
		);

		switch(acquireStatus)
		{
			case VK_SUCCESS : break;
			case VK_ERROR_OUT_OF_DATE_KHR : 
			{
				//recteate swapchain if its no longer matches surface properties
				magma::log::warn("Swapchain-Surface properties mismatch!");
				break;
			}
			default : 
			{
				magma::log::error("Image acquire returned {}", vkStrError(acquireStatus));
				assert(!"ACQUIRE IMG ASSERT");
				break;
			}
		}

		VkSubmitInfo queueSubmitInfo = {};
		queueSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		queueSubmitInfo.pNext = nullptr;
		queueSubmitInfo.waitSemaphoreCount = 1;
		queueSubmitInfo.pWaitSemaphores = &imageAvailableSemaphores[syncIndex];
		VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		queueSubmitInfo.pWaitDstStageMask = &waitMask;
		queueSubmitInfo.commandBufferCount = 1;
		queueSubmitInfo.pCommandBuffers = &commandBuffers[imageId];
		queueSubmitInfo.signalSemaphoreCount = 1;
		queueSubmitInfo.pSignalSemaphores = &imageMayPresentSemaphores[syncIndex];

		VK_CALL(vkQueueSubmit(vkCtx.graphicsQueue, 1, &queueSubmitInfo, imageFences[syncIndex]));

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &imageMayPresentSemaphores[syncIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain.swapchain;
		presentInfo.pImageIndices = &imageId;
		presentInfo.pResults = nullptr;

		VkResult presentStatus = vkQueuePresentKHR(vkCtx.graphicsQueue, &presentInfo);
		if(presentStatus == VK_ERROR_OUT_OF_DATE_KHR)
		{
			magma::log::warn("Swapchain-Surface properties mismatch!");
		}

		//advance sync index pointing to the next semaphore/fence objects to use
		syncIndex = (syncIndex + 1) % presentableFrames;

		magma::log::info("Frame Finished within {:.2f} ms", t.stop());
	}

	return 0;
}