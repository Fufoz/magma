#ifndef VK_BOILER_PLATE_H
#define VK_BOILER_PLATE_H

#include <volk.h>
#include <vector>
#include <string>

#include "vk_types.h"
#include <platform/platform.h>


bool init_vulkan_context(
	std::vector<const char*> desiredLayers,
	std::vector<const char*> desiredExtensions,
	VulkanGlobalContext* generalInfo
);

void destroy_vulkan_context(VulkanGlobalContext* ctx);

//sync stuff
VkSemaphore create_semaphore(VkDevice logicalDevice);

VkFence create_fence(VkDevice logicalDevice, bool signalled = false);

VkBool32 get_supported_depth_format(VkPhysicalDevice physicalDevice, VkFormat* out);

#endif