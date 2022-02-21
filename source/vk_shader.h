#ifndef VK_SHADER_H
#define VK_SHADER_H

#include <vk_types.h>

VkBool32 load_shader(VkDevice logicalDevice, const char* path, VkShaderStageFlagBits shaderType, Shader* shader);

#endif