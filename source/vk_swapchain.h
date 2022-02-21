#ifndef MAGMA_SWAPCHAIN_H
#define MAGMA_SWAPCHAIN_H

#include "vk_types.h"

bool create_swapchain(const VulkanGlobalContext& vkCtx, WindowInfo& windowInfo, uint32_t preferredImageCount, SwapChain* swapChain);

VkResult destroy_swapchain(const VulkanGlobalContext& vkCtx, SwapChain* swapChain);

VkResult recreate_swapchain(VulkanGlobalContext& vkCtx, WindowInfo& windowInfo, SwapChain* swapChain);

VkFramebuffer create_frame_buffer(
	VkDevice logicalDevice, VkImageView imageView,
	VkRenderPass renderpass, uint32_t width, uint32_t height);
#endif