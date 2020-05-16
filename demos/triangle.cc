#include <magma.h>

#include <vector>
#include <string>


int main(int argc, char **argv)
{
	magma::log::setSeverityMask(magma::log::MASK_ALL);
	
	std::vector<const char*> desiredExtensions = {
		"VK_EXT_debug_report"
	};
	std::vector<const char*> desiredLayers = {
		"VK_LAYER_LUNARG_standard_validation"
	};
	
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
	VkCommandPool commandPool = createCommandPool(vkCtx);
	pushDataToDeviceLocalBuffer(commandPool, vkCtx, stagingBuffer, &deviceLocalBuffer);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//pipeline configuration:
	PipelineState pipelineState = {};

	//loading spirv shaders
	Shader vertexShader = {};
	Shader fragmentShader = {};
	VK_CHECK(loadShader(vkCtx.logicalDevice, "./shaders/triangleVert.spv", VK_SHADER_STAGE_VERTEX_BIT, &vertexShader));
	VK_CHECK(loadShader(vkCtx.logicalDevice, "./shaders/triangleFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, &fragmentShader));

	pipelineState.shaders[0] = vertexShader;
	pipelineState.shaders[1] = fragmentShader;

	//specify vertex attributes
	VertexBuffer vbuff = {};

	VkVertexInputBindingDescription bindDescr = {};
	bindDescr.binding = 0;
	bindDescr.stride = sizeof(VertexPC);
	bindDescr.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	const uint32_t attribCount = 2;
	VkVertexInputAttributeDescription attrDescr[attribCount] = {};
	attrDescr[0].location = 0;
	attrDescr[0].binding = 0;
	attrDescr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrDescr[0].offset = 0;

	attrDescr[1].location = 1;
	attrDescr[1].binding = 0;
	attrDescr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrDescr[1].offset = sizeof(Vec3);

	vbuff.bindingDescr = bindDescr;
	vbuff.attribCount = attribCount;
	vbuff.attrDescr[0] = attrDescr[0];
	vbuff.attrDescr[1] = attrDescr[1];

	configureGraphicsPipe(swapChain, vkCtx, vbuff, windowInfo.windowExtent, &pipelineState);
	buildFrameBuffers(vkCtx.logicalDevice, pipelineState, windowInfo.windowExtent, &swapChain);

	//build command buffer for each swapchain image
	std::vector<VkCommandBuffer> commandBuffers = {};
	createCommandBuffers(vkCtx.logicalDevice, commandPool, swapChain.imageCount, commandBuffers);
	buildTriangleCommandBuffer(swapChain, pipelineState, deviceLocalBuffer.buffer, windowInfo.windowExtent, commandBuffers);

	uint32_t syncIndex = 0;//index in semaphore array to use
	//aliases for per frame sync objects
	auto& imageFences = swapChain.runtime.workSubmittedFences;
	auto& imageAvailableSemaphores = swapChain.runtime.imageAvailableSemaphores;
	auto& imageMayPresentSemaphores = swapChain.runtime.imageMayPresentSemaphores;

	auto& resizeHandler = [&]()
	{
		recreateSwapChain(vkCtx, windowInfo, &swapChain);
		vkDestroyRenderPass(vkCtx.logicalDevice, pipelineState.renderPass, nullptr);
		pipelineState.renderPass = VK_NULL_HANDLE;
		vkDestroyPipeline(vkCtx.logicalDevice, pipelineState.pipeline, nullptr);
		pipelineState.pipeline = VK_NULL_HANDLE;
		configureGraphicsPipe(swapChain, vkCtx, vbuff, windowInfo.windowExtent, &pipelineState);
		buildFrameBuffers(vkCtx.logicalDevice, pipelineState, windowInfo.windowExtent, &swapChain);
		vkFreeCommandBuffers(vkCtx.logicalDevice, commandPool, commandBuffers.size(), commandBuffers.data());
		createCommandBuffers(vkCtx.logicalDevice, commandPool, swapChain.imageCount, commandBuffers);
		buildTriangleCommandBuffer(swapChain, pipelineState, deviceLocalBuffer.buffer, windowInfo.windowExtent, commandBuffers);
	};

	//render loop
	while(!windowShouldClose(windowInfo.windowHandle))
	{
		HostTimer t;
		t.start();
		
		//update OS message queue
		updateMessageQueue();

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
			resizeHandler();
		}

		//advance sync index pointing to the next semaphore/fence objects to use
		syncIndex = (syncIndex + 1) % swapChain.imageCount;

		magma::log::info("Frame Finished within {:.2f} ms", t.stop());
	}

	//flush command queue before cleanup
	vkDeviceWaitIdle(vkCtx.logicalDevice);
	
	destroyPipeline(vkCtx, &pipelineState);
	vkFreeCommandBuffers(vkCtx.logicalDevice, commandPool, commandBuffers.size(), commandBuffers.data());
	vkDestroyCommandPool(vkCtx.logicalDevice, commandPool, nullptr);
	destroyBuffer(vkCtx.logicalDevice, &stagingBuffer);
	destroyBuffer(vkCtx.logicalDevice, &deviceLocalBuffer);
	destroySwapChain(vkCtx, &swapChain);
	destroyPlatformWindow(vkCtx, &windowInfo);
	destroyGlobalContext(&vkCtx);

	return 0;
}