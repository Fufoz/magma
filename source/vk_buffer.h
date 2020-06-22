#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include "vk_types.h"

#include <cstddef>

Buffer createBuffer(const VulkanGlobalContext& vkCtx, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlagBits requiredMemProperties, std::size_t size);

VkResult copyDataToHostVisibleBuffer(VkDevice logicalDevice, VkDeviceSize offset, const void* copyFrom, Buffer* buffer);

VkBool32 pushDataToDeviceLocalBuffer(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, Buffer* deviceLocalBuffer);

void destroyBuffer(VkDevice logicalDevice, Buffer* buffer);

bool findRequiredMemoryTypeIndex(VkPhysicalDevice physicalDevice, VkMemoryRequirements resourceMemoryRequirements, VkMemoryPropertyFlags desiredMemoryFlags, uint32_t* memoryTypeIndex);

#endif