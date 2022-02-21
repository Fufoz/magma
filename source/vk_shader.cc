#include "vk_shader.h"
#include "logging.h"
#include "vk_dbg.h"

#include <vector>

static VkShaderModule create_shader_module(VkDevice logicalDevice, const std::vector<uint8_t>& source)
{
	VkShaderModuleCreateInfo moduleCreateInfo = {};
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.pNext = nullptr;
	moduleCreateInfo.flags = VK_FLAGS_NONE;
	moduleCreateInfo.codeSize = source.size();
	moduleCreateInfo.pCode = (uint32_t*)source.data();

	VkShaderModule module = VK_NULL_HANDLE;
	VK_CALL(vkCreateShaderModule(logicalDevice, &moduleCreateInfo, nullptr, &module));
	return module;
}

VkBool32 load_shader(VkDevice logicalDevice, const char* path, VkShaderStageFlagBits shaderType, Shader* shader)
{

	FILE* shaderFile = fopen(path, "rb");
	if(!shaderFile)
	{
		magma::log::error("Failed to load shader by path: {}", path);
		return VK_FALSE;
	}

	fseek(shaderFile, 0, SEEK_END);
	std::size_t fileSize = ftell(shaderFile);
	rewind(shaderFile);
	
	std::vector<uint8_t> buffer;
	buffer.resize(fileSize);
	
	std::size_t totalBytesRead = {};
	uint8_t* data = buffer.data();
	while(totalBytesRead < fileSize)
	{
		std::size_t bytesRead = fread(data, sizeof(uint8_t), fileSize, shaderFile);
		totalBytesRead += bytesRead;
		data += bytesRead;
	}

	assert(totalBytesRead == fileSize);
	fclose(shaderFile);
	
	shader->handle = create_shader_module(logicalDevice, buffer);
	shader->shaderType = shaderType;
	return shader->handle != VK_NULL_HANDLE ? VK_TRUE : VK_FALSE;
}
