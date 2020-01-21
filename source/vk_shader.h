#ifndef VK_SHADER_H
#define VK_SHADER_H

#include <vk_types.h>

VkBool32 loadShader(VkDevice logicalDevice, const char* path, VkShaderStageFlagBits shaderType, Shader* shader);

#endif