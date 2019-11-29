#ifndef MAGMA_PLATFORM_H
#define MAGMA_PLATFORM_H

#include <vk_types.h>

VkBool32 initPlatformWindow(const VulkanGlobalContext& globalInfo, uint32_t width,
	uint32_t height, const char* title, WindowInfo* surface);

VkBool32 isPresentationSupported();

VkExtent2D getCurrentWindowExtent(void* windowHandle);

const char** getRequiredSurfaceExtensions(uint32_t* surfaceExtCount);

void destroyPlatformWindow(void* windowHandle);



#endif
