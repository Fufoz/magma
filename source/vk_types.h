#ifndef MAGMA_VK_TYPES_H
#define MAGMA_VK_TYPES_H

#include <volk.h>
#include <vector>


struct VulkanGlobalContext
{
	VkInstance instance;
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties deviceProps;
	VkDebugUtilsMessengerEXT debugCallback;
	uint32_t queueFamIdx;
	uint32_t computeQueueFamIdx;
	VkQueue graphicsQueue;
	VkQueue computeQueue;
	bool hasDebugUtilsExtension = false;
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
	VkShaderStageFlagBits shaderType;
};

struct PipelineState
{
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;
	VkViewport viewport;
};

struct Buffer
{
	VkBuffer buffer;
	VkDeviceSize bufferSize;
	VkDeviceSize alignedSize;
	VkDeviceMemory backupMemory;
};

struct ImageResource
{
	VkImage image;
	VkImageView view;
	VkImageLayout layout;
	VkFormat format;
	VkDeviceSize imageSize;
	VkDeviceMemory backupMemory;
};

struct Texture
{
	ImageResource imageInfo;
	VkSampler textureSampler;
};

#endif