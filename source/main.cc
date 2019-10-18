#include <volk.h>
#include <GLFW/glfw3.h>

#include "logging.h"
#include "vk_dbg.h"
#include "vk_boilerplate.h"

#include <vector>
#include <string>

std::vector<const char*> desiredExtensions = {
	"VK_EXT_debug_report"
};
std::vector<const char*> desiredLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

const char* glfwError()
{
	const char* err;
	int status = glfwGetError(&err);
	if(status != GLFW_NO_ERROR)
	{
		return err;
	}
	
	return "No eror";
}

int main(int argc, char **argv)
{
	magma::log::setSeverityMask(magma::log::MASK_ALL);
	magma::log::warn("Loading Vulkan loader..");
	VK_CALL(volkInitialize());

	if(glfwInit() != GLFW_TRUE)
	{
		magma::log::error("GLFW: failed to init! {}", glfwError());
		return -1;
	}

	if(!glfwVulkanSupported())
	{
		magma::log::error("GLFW: vulkan is not supported! {}", glfwError());
		return -1;
	}

	uint32_t requiredExtCount = {};
	const char** requiredExtStrings = glfwGetRequiredInstanceExtensions(&requiredExtCount);

	for(std::size_t i = 0; i < requiredExtCount; i++)
	{
		desiredExtensions.push_back(requiredExtStrings[i]);
	}

	VK_CHECK(requestLayersAndExtensions(desiredExtensions, desiredLayers));
	
	VkInstance instance = createInstance();
	volkLoadInstance(instance);
	VkDebugReportCallbackEXT dbgCallback = registerDebugCallback(instance);
	
	uint32_t queueFamilyIdx = -1;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VK_CHECK(pickQueueIndexAndPhysicalDevice(
		instance,
		VK_QUEUE_GRAPHICS_BIT,
		VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
		&physicalDevice,
		&queueFamilyIdx)
	);
	assert(queueFamilyIdx >= 0);
	VkDevice logicalDevice = createLogicalDevice(physicalDevice, queueFamilyIdx);
	volkLoadDevice(logicalDevice);

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(logicalDevice, queueFamilyIdx, 0, &graphicsQueue);
	assert(graphicsQueue != VK_NULL_HANDLE);
	
	//check whether selected queue family index supports window image presentation
	if(glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, queueFamilyIdx) == GLFW_FALSE)
	{
		magma::log::error("Selected queue family index does not support image presentation!");
		return -1;
	}

	//disable glfw window context creation
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	uint32_t windowWidth = 640;
	uint32_t windowHeight = 480;
	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Magma", nullptr, nullptr);
	
	assert(window);
	//create OS specific window surface to render into
	VkSurfaceKHR windowSurface = VK_NULL_HANDLE;
	VK_CALL(glfwCreateWindowSurface(instance, window, nullptr, &windowSurface));

	//query created surface capabilities
	VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, windowSurface, &surfaceCapabilities));
	//query for available surface formats
	uint32_t surfaceFormatCount = {};
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &surfaceFormatCount, nullptr));
	std::vector<VkSurfaceFormatKHR> surfaceFormats = {};
	surfaceFormats.resize(surfaceFormatCount);
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &surfaceFormatCount, surfaceFormats.data()));
	VkFormat preferredFormat = VK_FORMAT_B8G8R8A8_UNORM;
	bool formatFound = false;
	VkSurfaceFormatKHR selectedFormat = {}; 
	for(auto& format : surfaceFormats)
	{
		if(format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM)
		{
			selectedFormat = format;
			formatFound = true;
			break;
		}
	}
	selectedFormat = formatFound ? selectedFormat : surfaceFormats[0];

	//query for available surface presentation mode
	uint32_t presentModeCount = {};
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, nullptr));
	VkPresentModeKHR presentModes[VK_PRESENT_MODE_RANGE_SIZE_KHR];
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, presentModes));
	VkPresentModeKHR preferredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;//should exist anyway
	for(uint32_t i = 0; i < presentModeCount; i++)
	{
		if(presentModes[i] == preferredPresentMode)
		{
			selectedPresentMode = preferredPresentMode;
			break;
		}
	}

	//determine current image extent:
	VkExtent2D selectedExtent = surfaceCapabilities.currentExtent;
	if(surfaceCapabilities.currentExtent.width == 0xffffffff
		|| surfaceCapabilities.currentExtent.height == 0xffffffff)
	{
		selectedExtent.width = std::min(std::max(surfaceCapabilities.minImageExtent.width, windowWidth), surfaceCapabilities.maxImageExtent.width);
		selectedExtent.height = std::min(std::max(surfaceCapabilities.minImageExtent.width, windowWidth), surfaceCapabilities.maxImageExtent.width);
	}

	//check for surface presentation support, to suppress validation layer warnings, even though 
	// we've check that already using glfw
	VkBool32 surfaceSupported = VK_FALSE;
	VK_CALL(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIdx, windowSurface, &surfaceSupported));
	assert(surfaceSupported == VK_TRUE);

	//create swapchain
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = nullptr;
	swapChainCreateInfo.flags = VK_FLAGS_NONE;
	swapChainCreateInfo.surface = windowSurface;
	swapChainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
	swapChainCreateInfo.imageFormat = selectedFormat.format;
	swapChainCreateInfo.imageColorSpace = selectedFormat.colorSpace;
	swapChainCreateInfo.imageExtent = selectedExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCreateInfo.queueFamilyIndexCount = 1;
	swapChainCreateInfo.pQueueFamilyIndices = &queueFamilyIdx;
	swapChainCreateInfo.presentMode = selectedPresentMode;
	swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	VK_CALL(vkCreateSwapchainKHR(logicalDevice, &swapChainCreateInfo, nullptr, &swapChain));

	//grab swapchain images
	uint32_t swapChainImageCount = {};
	VK_CALL(vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapChainImageCount, nullptr));
	std::vector<VkImage> images = {};
	images.resize(swapChainImageCount);
	VK_CALL(vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapChainImageCount, images.data()));
	VkExtent2D currentImageExtent = selectedExtent;
	VkFormat swapchainImageFormat = selectedFormat.format;

	//images that we can actually "touch"
	std::vector<VkImageView> imageViews = {};
	imageViews.resize(swapChainImageCount);

	//populate imageViews vector
	for(uint32_t i = 0; i < imageViews.size(); i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.image = images[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = swapchainImageFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		VK_CALL(vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &imageViews[i]));
	}
	
	return 0;
}