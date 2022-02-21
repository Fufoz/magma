#include "vk_loader.h"

VkResult locate_and_init_vulkan()
{
	return volkInitialize();
}

void load_instance_function_pointers(VkInstance instance)
{
	volkLoadInstanceOnly(instance);
}

void load_device_function_pointers(VkDevice logicalDevice)
{
	volkLoadDevice(logicalDevice);
}
