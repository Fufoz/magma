#ifndef VK_LOADER_H
#define VK_LOADER_H

#include <volk.h>

VkResult locate_and_init_vulkan();

void load_instance_function_pointers(VkInstance instance);

void load_device_function_pointers(VkDevice logicalDevice);

#endif