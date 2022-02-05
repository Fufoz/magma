#ifndef MAGMA_SWAPCHAIN_H
#define MAGMA_SWAPCHAIN_H

#include "vk_types.h"

bool createSwapChain(const VulkanGlobalContext& vkCtx, WindowInfo& windowInfo, uint32_t preferredImageCount, SwapChain* swapChain);

VkResult destroySwapChain(const VulkanGlobalContext& vkCtx, SwapChain* swapChain);

VkResult recreateSwapChain(VulkanGlobalContext& vkCtx, WindowInfo& windowInfo, SwapChain* swapChain);

//build framebuffers for later command buffer recording
void buildFrameBuffers(VkDevice logicaDevice, const PipelineState& pipelineState, VkExtent2D windowExtent, SwapChain* swapChain);

VkFramebuffer create_frame_buffer(
	VkDevice logicalDevice, VkImageView imageView,
	VkRenderPass renderpass, uint32_t width, uint32_t height);
#endif