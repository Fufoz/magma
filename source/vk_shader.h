#ifndef VK_SHADER_H
#define VK_SHADER_H

#include <volk.h>

VkBool32 loadShader(VkDevice logicalDevice, const char* path, VkShaderModule* shaderModule);

#endif