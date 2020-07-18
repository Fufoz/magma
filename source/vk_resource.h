#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include "vk_types.h"

#include <cstddef>

VkImage createImage(const VulkanGlobalContext& vkCtx, VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags usageFlags);

ImageResource createResourceImage(const VulkanGlobalContext& vkCtx, VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags usageFlags, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

Buffer createBuffer(const VulkanGlobalContext& vkCtx, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags requiredMemProperties, std::size_t size);

VkResult copyDataToHostVisibleBuffer(const VulkanGlobalContext& vkCtx, VkDeviceSize offset, const void* copyFrom, std::size_t copyByteSize, Buffer* buffer);

VkBool32 pushDataToDeviceLocalBuffer(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, Buffer* deviceLocalBuffer, VkQueue queue = VK_NULL_HANDLE);

VkBool32 pushTextureToDeviceLocalImage(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, VkExtent3D imageExtent, ImageResource* textureResource);

void destroyBuffer(VkDevice logicalDevice, Buffer* buffer);

bool findRequiredMemoryTypeIndex(VkPhysicalDevice physicalDevice, VkMemoryRequirements resourceMemoryRequirements, VkMemoryPropertyFlags desiredMemoryFlags, uint32_t* memoryTypeIndex);

#endif