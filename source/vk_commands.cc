#include "vk_commands.h"

VkCommandPool create_command_pool(const VulkanGlobalContext& vkCtx, VkCommandPoolCreateFlags commandPoolFlags)
{
	//creating command buffer for transfer operation
	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.pNext = nullptr;
	cmdPoolCreateInfo.flags = commandPoolFlags;
	cmdPoolCreateInfo.queueFamilyIndex = vkCtx.queueFamIdx;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VK_CALL(vkCreateCommandPool(vkCtx.logicalDevice, &cmdPoolCreateInfo, nullptr, &commandPool));
	return commandPool;
}

void destroy_command_pool(VkDevice device, VkCommandPool cmdPool)
{
	vkDestroyCommandPool(device, cmdPool, nullptr);
}

void create_command_buffers(VkDevice logicalDevice, VkCommandPool commandPool, uint32_t count, std::vector<VkCommandBuffer>& out)
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


void create_command_buffer(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer* out)
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

VkCommandBuffer begin_tmp_commands(VulkanGlobalContext& ctx, VkCommandPool cmdPool)
{
	VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
	create_command_buffer(ctx.logicalDevice, cmdPool, &cmdBuffer);
	VkCommandBufferBeginInfo cmdBuffBeginInfo = {};
	cmdBuffBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBuffBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(cmdBuffer, &cmdBuffBeginInfo);
	return cmdBuffer;
}

void end_tmp_commands(VulkanGlobalContext& ctx, VkCommandPool cmdPool, VkCommandBuffer cmdBuffer)
{
	vkEndCommandBuffer(cmdBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(ctx.graphicsQueue);

	vkFreeCommandBuffers(ctx.logicalDevice, cmdPool, 1, &cmdBuffer);
	vkDestroyCommandPool(ctx.logicalDevice, cmdPool, nullptr);
}