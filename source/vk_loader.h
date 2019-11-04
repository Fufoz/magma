#ifndef VK_LOADER_H
#define VK_LOADER_H

#include <volk.h>

inline VkResult locateAndInitVulkan()
{
	return volkInitialize();
}

inline void loadInstanceFunctionPointers(VkInstance instance)
{
	volkLoadInstance(instance);
}

inline void loadDeviceFunctionPointers(VkDevice logicalDevice)
{
	volkLoadDevice(logicalDevice);
}

#endif