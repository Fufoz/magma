#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include <volk.h>

#include <cstddef>

struct Buffer
{
	VkBuffer buffer;
	VkDeviceSize bufferSize;
	VkDeviceMemory backupMemory;
};

Buffer createBuffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkBufferUsageFlags usageFlags,
	VkMemoryPropertyFlagBits requiredMemProperties, std::size_t size, uint32_t queueFamilyIdx);

VkResult copyDataToStagingBuffer(VkDevice logicalDevice, VkDeviceSize offset, const void* copyFrom, Buffer* buffer);

void destroyBuffer(VkDevice logicalDevice, Buffer* buffer);

#endif