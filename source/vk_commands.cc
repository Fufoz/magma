#include "vk_commands.h"

VkCommandPool createCommandPool(const VulkanGlobalContext& vkCtx, VkCommandPoolCreateFlags commandPoolFlags)
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
