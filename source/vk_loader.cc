#include "vk_loader.h"

VkResult locateAndInitVulkan()
{
	return volkInitialize();
}

void loadInstanceFunctionPointers(VkInstance instance)
{
	volkLoadInstanceOnly(instance);
}

void loadDeviceFunctionPointers(VkDevice logicalDevice)
{
	volkLoadDevice(logicalDevice);
}
