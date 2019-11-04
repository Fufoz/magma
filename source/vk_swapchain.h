#ifndef MAGMA_SWAPCHAIN_H
#define MAGMA_SWAPCHAIN_H

#include "vk_types.h"

VkBool32 createSwapChain(VulkanGlobalContext& vkCtx, WindowInfo& windowInfo, uint32_t preferredImageCount, SwapChain* swapChain);

VkResult destroySwapChain(SwapChain* swapChain);

VkResult recreateSwapChain(SwapChain* swapChain);

#endif