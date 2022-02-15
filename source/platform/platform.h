#ifndef MAGMA_PLATFORM_H
#define MAGMA_PLATFORM_H

#include <vk_types.h>

bool initPlatformWindow(const VulkanGlobalContext& globalInfo, uint32_t width,
	uint32_t height, const char* title, WindowInfo* surface);

VkExtent2D getCurrentWindowExtent(void* windowHandle);

bool windowShouldClose(void* windowHandle);

void updateMessageQueue();

void updateWindowDimensions(VkPhysicalDevice physicalDevice, WindowInfo* out);

const char** getRequiredSurfaceExtensions(uint32_t* surfaceExtCount);

void destroyPlatformWindow(const VulkanGlobalContext& vkCtx, WindowInfo* info);


#endif
