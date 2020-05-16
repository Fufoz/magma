#include "vk_loader.h"

VkResult locateAndInitVulkan()
{
	return volkInitialize();
}

void loadInstanceFunctionPointers(VkInstance instance)
{
	volkLoadInstance(instance);
}

void loadDeviceFunctionPointers(VkDevice logicalDevice)
{
	volkLoadDevice(logicalDevice);
}
