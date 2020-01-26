#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H
#include "vk_types.h"

VkBool32 configureGraphicsPipe(SwapChain& swapChain,
	VulkanGlobalContext& vkCtx,
	VertexBuffer& vertexInfo,
	VkExtent2D windowExtent,
	PipelineState* state);

void destroyPipeline(const VulkanGlobalContext& ctx, PipelineState* pipeline);
#endif