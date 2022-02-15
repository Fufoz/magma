#ifndef COMMANDS_H
#define COMMANDS_H

#include "vk_types.h"
#include "vk_dbg.h"

VkCommandPool createCommandPool(const VulkanGlobalContext& vkCtx, VkCommandPoolCreateFlags commandPoolFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

void createCommandBuffers(VkDevice logicalDevice, VkCommandPool commandPool, uint32_t count, std::vector<VkCommandBuffer>& out);

void createCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer* out);

void destroyCommandPool(VkDevice device, VkCommandPool cmdPool);

VkCommandBuffer begin_tmp_commands(VulkanGlobalContext& ctx, VkCommandPool cmdPool);

void end_tmp_commands(VulkanGlobalContext& ctx, VkCommandPool cmdPool, VkCommandBuffer cmdBuffer);

#endif