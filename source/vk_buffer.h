#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include "vk_types.h"

#include <cstddef>

Buffer createBuffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkBufferUsageFlags usageFlags,
	VkMemoryPropertyFlagBits requiredMemProperties, std::size_t size, uint32_t queueFamilyIdx);

VkResult copyDataToStagingBuffer(VkDevice logicalDevice, VkDeviceSize offset, const void* copyFrom, Buffer* buffer);

VkBool32 pushDataToDeviceLocalBuffer(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, Buffer* deviceLocalBuffer);

void destroyBuffer(VkDevice logicalDevice, Buffer* buffer);


#endif