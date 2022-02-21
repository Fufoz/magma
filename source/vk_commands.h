#ifndef COMMANDS_H
#define COMMANDS_H

#include "vk_types.h"
#include "vk_dbg.h"

VkCommandPool create_command_pool(const VulkanGlobalContext& vkCtx, VkCommandPoolCreateFlags commandPoolFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

void create_command_buffers(VkDevice logicalDevice, VkCommandPool commandPool, uint32_t count, std::vector<VkCommandBuffer>& out);

void create_command_buffer(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer* out);

void destroy_command_pool(VkDevice device, VkCommandPool cmdPool);

VkCommandBuffer begin_tmp_commands(VulkanGlobalContext& ctx, VkCommandPool cmdPool);

void end_tmp_commands(VulkanGlobalContext& ctx, VkCommandPool cmdPool, VkCommandBuffer cmdBuffer);

#endif