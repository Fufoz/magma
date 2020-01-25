#include "vk_commands.h"

VkCommandPool createCommandPool(VulkanGlobalContext& vkCtx, VkCommandPoolCreateFlags commandPoolFlags)
{
	//creating command buffer for transfer operation
	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.pNext = nullptr;
	cmdPoolCreateInfo.flags = VK_FLAGS_NONE;
	cmdPoolCreateInfo.queueFamilyIndex = vkCtx.queueFamIdx;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VK_CALL(vkCreateCommandPool(vkCtx.logicalDevice, &cmdPoolCreateInfo, nullptr, &commandPool));
	return commandPool;
}

void destroyCommandPool(VkDevice device, VkCommandPool cmdPool)
{
	vkDestroyCommandPool(device, cmdPool, nullptr);
}

void createCommandBuffers(VkDevice logicalDevice, VkCommandPool commandPool, uint32_t count, std::vector<VkCommandBuffer>& out)
{
	//build command buffer for each swapchain image
	VkCommandBufferAllocateInfo buffAllocInfo = {};
	buffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	buffAllocInfo.pNext = nullptr;
	buffAllocInfo.commandPool = commandPool;
	buffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	buffAllocInfo.commandBufferCount = count;
	out.resize(count);
	vkAllocateCommandBuffers(logicalDevice, &buffAllocInfo, out.data());
}


void createCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer* out)
{
	//build command buffer for each swapchain image
	VkCommandBufferAllocateInfo buffAllocInfo = {};
	buffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	buffAllocInfo.pNext = nullptr;
	buffAllocInfo.commandPool = commandPool;
	buffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	buffAllocInfo.commandBufferCount = 1;
	vkAllocateCommandBuffers(logicalDevice, &buffAllocInfo, out);
}

void buildTriangleCommandBuffer(const SwapChain& swapChain, const PipelineState& pipelineState,
	VkBuffer vBuffer, VkExtent2D windowExtent, const std::vector<VkCommandBuffer>& commandBuffers)
{
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
		renderPassBegInfo.renderPass = pipelineState.renderPass;
		renderPassBegInfo.framebuffer = swapChain.runtime.frameBuffers[i];
		renderPassBegInfo.renderArea.offset = {0, 0};
		renderPassBegInfo.renderArea.extent = windowExtent;
		renderPassBegInfo.clearValueCount = 1;
		renderPassBegInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBegInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineState.pipeline);
			vkCmdSetViewport(commandBuffers[i], 0, 1, &pipelineState.viewport);
			VkDeviceSize offsets = 0;
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &vBuffer, &offsets);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[i]);

		VK_CALL(vkEndCommandBuffer(commandBuffers[i]));
	}	
}