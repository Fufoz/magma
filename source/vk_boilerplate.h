#ifndef VK_BOILER_PLATE_H
#define VK_BOILER_PLATE_H

#include <volk.h>
#include <vector>
#include <string>

extern std::vector<std::string> desiredExtensions;
extern std::vector<std::string> desiredLayers;

VkInstance createInstance();

VkBool32 requestLayersAndExtensions(
	const std::vector<std::string>& desiredExtensions,
	const std::vector<std::string>& desiredLayers
);

VkDebugReportCallbackEXT registerDebugCallback(VkInstance instance);

uint32_t findQueueFamilyIndex(VkPhysicalDevice physicalDevice, VkQueueFlags desiredFlags);

VkBool32 pickQueueIndexAndPhysicalDevice(VkInstance instance, VkQueueFlags queueFlags, VkPhysicalDeviceType preferredGPUType, VkPhysicalDevice* physicalDevice, uint32_t* queueFamIdx);

VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIdx);

#endif