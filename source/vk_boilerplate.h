#ifndef VK_BOILER_PLATE_H
#define VK_BOILER_PLATE_H

#include <volk.h>
#include <vector>
#include <string>

#include "vk_types.h"
#include <platform/platform.h>


bool initVulkanGlobalContext(
	std::vector<const char*> desiredLayers,
	std::vector<const char*> desiredExtensions,
	VulkanGlobalContext* generalInfo
);

void destroyGlobalContext(VulkanGlobalContext* ctx);

//sync stuff
VkSemaphore createSemaphore(VkDevice logicalDevice);

VkFence createFence(VkDevice logicalDevice, bool signalled = false);

VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat* out);

#endif