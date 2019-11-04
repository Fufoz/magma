#ifndef MAGMA_PLATFORM_H
#define MAGMA_PLATFORM_H

#include <vk_types.h>

VkBool32 initPlatformWindow(const VulkanGlobalContext& globalInfo, uint32_t width, uint32_t height,
	const char* title, WindowInfo* surface);

const char** getRequiredSurfaceExtensions(uint32_t* surfaceExtCount);

void destroyPlatformWindow();

VkBool32 isPresentationSupported();


#endif
