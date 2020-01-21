#ifndef MAGMA_VK_TYPES_H
#define MAGMA_VK_TYPES_H

#include <volk.h>
#include <vector>
#include <GLFW/glfw3.h>

struct VulkanGlobalContext
{
	VkInstance instance;
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;
	VkDebugReportCallbackEXT debugCallback;
	uint32_t queueFamIdx;
	VkQueue graphicsQueue;
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
	Shader shaders[2];
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;
};

struct Buffer
{
	VkBuffer buffer;
	VkDeviceSize bufferSize;
	VkDeviceMemory backupMemory;
};

const uint32_t MAX_VERTEX_ATTRIB_DESCR = 10;
struct VertexBuffer
{
	VkVertexInputAttributeDescription attrDescr[MAX_VERTEX_ATTRIB_DESCR];
	Buffer gpuBuffer;
};

#endif