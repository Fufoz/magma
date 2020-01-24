#ifndef MAGMA_SWAPCHAIN_H
#define MAGMA_SWAPCHAIN_H

#include "vk_types.h"

VkBool32 createSwapChain(const VulkanGlobalContext& vkCtx, WindowInfo& windowInfo, uint32_t preferredImageCount, SwapChain* swapChain);

VkResult destroySwapChain(SwapChain* swapChain);

VkResult recreateSwapChain(VulkanGlobalContext& vkCtx, WindowInfo& windowInfo, PipelineState* pipe, SwapChain* swapChain);

#endif