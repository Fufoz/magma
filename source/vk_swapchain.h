#ifndef MAGMA_SWAPCHAIN_H
#define MAGMA_SWAPCHAIN_H

#include <vk_types.h>

VkResult createSwapChain(SwapChain* swapChain);
VkResult destroySwapChain(SwapChain* swapChain);
VkResult recreateSwapChain(SwapChain* swapChain);

#endif