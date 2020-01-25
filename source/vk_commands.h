#ifndef COMMANDS_H
#define COMMANDS_H

#include "vk_types.h"
#include "vk_dbg.h"

VkCommandPool createCommandPool(VulkanGlobalContext& vkCtx, VkCommandPoolCreateFlags commandPoolFlags = VK_FLAGS_NONE);

void createCommandBuffers(VkDevice logicalDevice, VkCommandPool commandPool, uint32_t count, std::vector<VkCommandBuffer>& out);

void createCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer* out);

void destroyCommandPool(VkDevice device, VkCommandPool cmdPool);

////
void buildTriangleCommandBuffer(const SwapChain& swapChain, const PipelineState& pipelineState,
	VkBuffer vBuffer, VkExtent2D windowExtent, const std::vector<VkCommandBuffer>& commandBuffers);

#endif