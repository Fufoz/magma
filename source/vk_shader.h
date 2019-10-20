#ifndef VK_SHADER_H
#define VK_SHADER_H

#include <volk.h>
#include <vector>

VkBool32 loadShader(const char* path, std::vector<uint8_t>& out);
VkShaderModule createShaderModule(VkDevice logicalDevice, const std::vector<uint8_t>& source);

#endif