#ifndef VK_BOILER_PLATE_H
#define VK_BOILER_PLATE_H

#include <volk.h>
#include <vector>
#include <string>

#include "vk_types.h"
#include <platform/platform.h>

extern std::vector<const char*> desiredExtensions;
extern std::vector<const char*> desiredLayers;

VkInstance createInstance();

VkBool32 requestLayersAndExtensions(
	const std::vector<const char*>& desiredExtensions,
	const std::vector<const char*>& desiredLayers
);

VkDebugReportCallbackEXT registerDebugCallback(VkInstance instance);

uint32_t findQueueFamilyIndex(VkPhysicalDevice physicalDevice, VkQueueFlags desiredFlags);

VkBool32 pickQueueIndexAndPhysicalDevice(VkInstance instance, VkQueueFlags queueFlags, VkPhysicalDeviceType preferredGPUType, VkPhysicalDevice* physicalDevice, uint32_t* queueFamIdx);

VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIdx);

VkBool32 initVulkanGlobalContext(
	std::vector<const char*>& desiredLayers,
	std::vector<const char*>& desiredExtensions,
	VulkanGlobalContext* generalInfo
);

//sync stuff
VkSemaphore createSemaphore(VkDevice logicalDevice);

VkFence createFence(VkDevice logicalDevice, bool signalled = false);

#endif