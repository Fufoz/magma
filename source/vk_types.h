#ifndef MAGMA_VK_TYPES_H
#define MAGMA_VK_TYPES_H

#include <volk.h>
#include <GLFW/glfw3.h>

#include <vector>

struct VulkanGlobalContext
{
	VkInstance instance;
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;
	VkDebugReportCallbackEXT debugCallback;
	uint32_t queueFamIdx;
	VkQueue graphicsQueue;
	VkPipeline graphicsPipe;
};

struct WindowInfo
{
	const char* title;
	void* windowHandle;
	VkExtent2D windowExtent;
	VkSurfaceKHR surface;
	VkSurfaceCapabilitiesKHR surfaceCaps;
};

struct SwapChainRuntime
{
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	std::vector<VkFramebuffer> frameBuffers;
	std::vector<VkFence> workSubmittedFences;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> imageMayPresentSemaphores;
};

struct SwapChain
{
	uint32_t imageCount;//total amount of images owned by presentation engine
	VkSwapchainKHR swapchain;
	VkFormat imageFormat; 
	SwapChainRuntime runtime;
	VkPresentModeKHR presentMode;
};

struct Shader
{
	VkShaderModule handle;
	VkShaderStageFlagBits shaderStage;
};

struct Buffer
{
	VkBuffer buffer;
	VkDeviceSize bufferSize;
	VkDeviceMemory backupMemory;
};

#endif