#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include "vk_types.h"

#include <cstddef>

ImageResource create_image_resource(const VulkanGlobalContext& vkCtx, VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags usageFlags, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

VkSampler create_default_sampler(VkDevice logicalDevice, bool* status = nullptr);

ImageResource create_cubemap_image(const VulkanGlobalContext& vkCtx, VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags usageFlags, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

Buffer create_buffer(const VulkanGlobalContext& vkCtx, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags requiredMemProperties, std::size_t size);

VkResult copy_data_to_host_visible_buffer(const VulkanGlobalContext& vkCtx, VkDeviceSize offset, const void* copyFrom, std::size_t copyByteSize, Buffer* buffer);

VkBool32 push_data_to_device_local_buffer(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, Buffer* deviceLocalBuffer, VkQueue queue = VK_NULL_HANDLE);

VkBool32 push_texture_to_device_local_image(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, VkExtent3D imageExtent, ImageResource* textureResource);

VkBool32 push_cubemap_to_device_local_image(VkCommandPool commandPool, const VulkanGlobalContext& vkCtx, const Buffer& stagingBuffer, VkExtent3D imageExtent, std::size_t planeStride, ImageResource* textureResource);

void destroy_buffer(VkDevice logicalDevice, Buffer* buffer);

void destroy_image_resource(VkDevice logicalDevice, ImageResource* image);

bool find_required_memtype_index(VkPhysicalDevice physicalDevice, VkMemoryRequirements resourceMemoryRequirements, VkMemoryPropertyFlags desiredMemoryFlags, uint32_t* memoryTypeIndex);

#endif