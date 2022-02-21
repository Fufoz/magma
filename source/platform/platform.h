#ifndef MAGMA_PLATFORM_H
#define MAGMA_PLATFORM_H

#include <vk_types.h>


bool init_platform_window(const VulkanGlobalContext& globalInfo, uint32_t width,
	uint32_t height, const char* title, WindowInfo* surface, bool fpsCameraMode = true);

VkExtent2D get_current_window_extent(void* windowHandle);

bool window_should_close(void* windowHandle);

void update_message_queue();

void update_window_dimensions(VkPhysicalDevice physicalDevice, WindowInfo* out);

const char** get_required_surface_exts(uint32_t* surfaceExtCount);

void destroy_platform_window(const VulkanGlobalContext& vkCtx, WindowInfo* info);


#endif
