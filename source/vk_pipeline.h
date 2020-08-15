#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H
#include "vk_types.h"

VkBool32 configureGraphicsPipe(SwapChain& swapChain,
	VulkanGlobalContext& vkCtx,
	VertexBuffer& vertexInfo,
	VkExtent2D windowExtent,
	PipelineState* state);

void destroyPipeline(const VulkanGlobalContext& ctx, PipelineState* pipeline);


VkPipelineShaderStageCreateInfo fillShaderStageCreateInfo(
	const VkDevice 			logicalDevice,
 	const char* 			shaderSource,
	VkShaderStageFlagBits 	shaderStage);

VkPipelineVertexInputStateCreateInfo fillVertexInputStateCreateInfo(
	const VkVertexInputBindingDescription* 		bindingDescr,
	uint32_t 									bindingDescrCount,
	const VkVertexInputAttributeDescription* 	vertexAttribDescr,
	uint32_t 									attribDescrCount);

VkPipelineLayout createPipelineLayout(
	VkDevice						logicalDevice,
	const VkDescriptorSetLayout* 	descrSetLayouts,
	uint32_t 						descrSetLayoutCount,
	const VkPushConstantRange* 		pushConstantRanges,
	uint32_t 						pushConstantRangeCount);

VkPipelineRasterizationStateCreateInfo fillRasterizationStateCreateInfo(
	VkCullModeFlags cullMode,
	VkFrontFace 	frontFaceWindingOrder);

VkViewport createViewPort(VkExtent2D windowExtent);

VkPipelineViewportStateCreateInfo fillViewportStateCreateInfo(const VkViewport& viewport, const VkRect2D& scissors);

VkPipelineInputAssemblyStateCreateInfo fillInputAssemblyCreateInfo(VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

VkPipelineMultisampleStateCreateInfo fillMultisampleStateCreateInfo();

VkPipelineDepthStencilStateCreateInfo fillDepthStencilStateCreateInfo(VkCompareOp depthCompareOp);

VkPipelineColorBlendStateCreateInfo fillColorBlendStateCreateInfo(const VkPipelineColorBlendAttachmentState& blendAttachmentState);

VkPipelineDynamicStateCreateInfo fillDynamicStateCreateInfo(VkDynamicState* dynStates, uint32_t statesCount);


#endif