#ifndef VK_LOADER_H
#define VK_LOADER_H

#include <volk.h>

VkResult locateAndInitVulkan();

void loadInstanceFunctionPointers(VkInstance instance);

void loadDeviceFunctionPointers(VkDevice logicalDevice);

#endif