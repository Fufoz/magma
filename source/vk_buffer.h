#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include <volk.h>

#include <cstddef>

struct Buffer
{
	VkBuffer buffer;
	VkDeviceMemory backupMemory;
};

Buffer createBuffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkBufferUsageFlags usageFlags,
	VkMemoryPropertyFlagBits memoryPropertyFlags, std::size_t size, uint32_t queueFamilyIdx, const void* copyFrom = nullptr);

void destroyBuffer(VkDevice logicalDevice, Buffer* buffer);

#endif