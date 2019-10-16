#ifndef VK_BOILER_PLATE_H
#define VK_BOILER_PLATE_H

#include <volk.h>

VkInstance createInstance();
VkBool32 requestLayersAndExtensions();
VkDebugReportCallbackEXT registerDebugCallback(VkInstance instance);
uint32_t findQueueFamilyIndex(VkPhysicalDevice physicalDevice, VkQueueFlags desiredFlags);
VkBool32 pickQueueIndexAndPhysicalDevice(VkInstance instance, VkQueueFlags queueFlags, VkPhysicalDeviceType preferredGPUType, VkPhysicalDevice* physicalDevice, uint32_t* queueFamIdx);
VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIdx);

#endif