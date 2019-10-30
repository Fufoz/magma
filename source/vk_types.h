#ifndef MAGMA_VK_TYPES_H
#define MAGMA_VK_TYPES_H

#include <volk.h>
#include <GLFW/glfw3.h>

#include <vector>

struct VulkanGlobalInfo
{
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
	VkQueue graphicsQueue;
	VkPhysicalDeviceProperties deviceProps;
};

struct Canvas
{
	GLFWwindow* windowHandle;
	VkSurfaceKHR surface;
	const char* title;
	VkExtent2D windowExtent;
};

struct SwapChain
{
	uint32_t imageCount;//total amount of images owned by presentation engine
	VkExtent2D imageExtent;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	std::vector<VkFramebuffer> frameBuffers;
	std::vector<VkFence> workSubmittedFences;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> imageMayPresentSemaphores;
};

#endif