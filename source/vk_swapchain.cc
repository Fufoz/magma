#include "vk_swapchain.h"
#include "vk_dbg.h"
#include "platform/platform.h"

static VkSurfaceFormatKHR querySurfaceFormat(VkSurfaceKHR windowSurface, VkPhysicalDevice physicalDevice)
{
	//query for available surface formats
	uint32_t surfaceFormatCount = {};
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &surfaceFormatCount, nullptr));
	std::vector<VkSurfaceFormatKHR> surfaceFormats = {};
	surfaceFormats.resize(surfaceFormatCount);
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &surfaceFormatCount, surfaceFormats.data()));
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

	return formatFound ? selectedFormat : surfaceFormats[0];
}

static VkPresentModeKHR queryPresentMode(VkSurfaceKHR windowSurface, VkPhysicalDevice physicalDevice, VkPresentModeKHR preferredPresentMode)
{
	//query for available surface presentation mode
	uint32_t presentModeCount = {};
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, nullptr));
	VkPresentModeKHR presentModes[VK_PRESENT_MODE_RANGE_SIZE_KHR];
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, presentModes));
	VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;//should exist anyway
	
	for(uint32_t i = 0; i < presentModeCount; i++)
	{
		if(presentModes[i] == preferredPresentMode)
		{
			selectedPresentMode = preferredPresentMode;
			break;
		}
	}
	return selectedPresentMode;
}

static VkBool32 queryPresentationSupport(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIdx, VkSurfaceKHR windowSurface)
{
	VkBool32 surfaceSupported = VK_FALSE;
	VK_CALL(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIdx, windowSurface, &surfaceSupported));
	return surfaceSupported;
}

static VkExtent2D pickImageExtent(VkExtent2D imageExtent, VkSurfaceCapabilitiesKHR& surfaceCaps)
{
	//determine current image extent:
	VkExtent2D selectedExtent = surfaceCaps.currentExtent;
	if(surfaceCaps.currentExtent.width == 0xffffffff
		|| surfaceCaps.currentExtent.height == 0xffffffff)
	{
		selectedExtent.width = std::min(std::max(surfaceCaps.minImageExtent.width, (uint32_t)imageExtent.width), surfaceCaps.maxImageExtent.width);
		selectedExtent.height = std::min(std::max(surfaceCaps.minImageExtent.height, (uint32_t)imageExtent.height), surfaceCaps.maxImageExtent.height);
	}
	return selectedExtent;
}

static VkSwapchainKHR createSwapChain(VkDevice logicalDevice, VkSurfaceKHR windowSurface, VkSurfaceCapabilitiesKHR surfaceCaps, VkSurfaceFormatKHR surfaceFormat, VkExtent2D imageExtent, VkPresentModeKHR presentationMode, uint32_t queueFamIdx, VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE)
{

	//create swapchain
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = nullptr;
	swapChainCreateInfo.flags = VK_FLAGS_NONE;
	swapChainCreateInfo.surface = windowSurface;
	swapChainCreateInfo.minImageCount = surfaceCaps.minImageCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = imageExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCreateInfo.queueFamilyIndexCount = 1;
	swapChainCreateInfo.pQueueFamilyIndices = &queueFamIdx;
	swapChainCreateInfo.presentMode = presentationMode;
	swapChainCreateInfo.preTransform = surfaceCaps.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = oldSwapchain;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	VK_CALL(vkCreateSwapchainKHR(logicalDevice, &swapChainCreateInfo, nullptr, &swapChain));
	return swapChain;
}

VkBool32 createSwapChain(const VulkanGlobalContext& vkCtx, WindowInfo& windowInfo, uint32_t preferredImageCount, SwapChain* swapChain)
{
	//query for available surface formats
	VkSurfaceFormatKHR surfaceFormat = querySurfaceFormat(windowInfo.surface, vkCtx.physicalDevice);	
	VkPresentModeKHR selectedPresentMode = queryPresentMode(windowInfo.surface, vkCtx.physicalDevice, VK_PRESENT_MODE_FIFO_KHR);
	VkExtent2D imageExtent = pickImageExtent(windowInfo.windowExtent, windowInfo.surfaceCaps);
	VK_CHECK(queryPresentationSupport(vkCtx.physicalDevice, vkCtx.queueFamIdx, windowInfo.surface));
	VkSwapchainKHR vkSwapChain = createSwapChain(vkCtx.logicalDevice, windowInfo.surface, windowInfo.surfaceCaps,
		surfaceFormat, imageExtent, selectedPresentMode, vkCtx.queueFamIdx, swapChain->swapchain);
	
	uint32_t swapChainImageCount = {};
	VK_CALL(vkGetSwapchainImagesKHR(vkCtx.logicalDevice, vkSwapChain, &swapChainImageCount, nullptr));
	auto& images = swapChain->runtime.images; 
	images.resize(swapChainImageCount);
	VK_CALL(vkGetSwapchainImagesKHR(vkCtx.logicalDevice, vkSwapChain, &swapChainImageCount, images.data()));
	
	auto& imageViews = swapChain->runtime.imageViews;
	imageViews.resize(swapChainImageCount);
	
	//populate imageViews vector
	for(uint32_t i = 0; i < imageViews.size(); i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.image = images[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = surfaceFormat.format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		VK_CALL(vkCreateImageView(vkCtx.logicalDevice, &imageViewCreateInfo, nullptr, &imageViews[i]));
	}

	auto& frameBuffers = swapChain->runtime.frameBuffers;
	frameBuffers.resize(swapChainImageCount);

	swapChain->imageCount = preferredImageCount > windowInfo.surfaceCaps.maxImageCount ? windowInfo.surfaceCaps.maxImageCount : preferredImageCount;
	swapChain->presentMode = selectedPresentMode;
	swapChain->swapchain = vkSwapChain;
	swapChain->imageFormat = surfaceFormat.format;
	
	return VK_TRUE;
}

VkResult destroySwapChain(VkDevice logicalDevice, SwapChain* swapChain)
{
	vkDestroySwapchainKHR(logicalDevice, swapChain->swapchain, nullptr);
	swapChain->swapchain = VK_NULL_HANDLE;
	return VK_SUCCESS;
}

VkResult recreateSwapChain(VulkanGlobalContext& vkCtx, WindowInfo& windowInfo, SwapChain* swapChain)
{
	//wait until gpu is done using any runtime objects
	vkDeviceWaitIdle(vkCtx.logicalDevice);

	for(auto& fb : swapChain->runtime.frameBuffers)
	{
		vkDestroyFramebuffer(vkCtx.logicalDevice, fb, nullptr);
	}

	for(auto& imageView : swapChain->runtime.imageViews)
	{
		vkDestroyImageView(vkCtx.logicalDevice, imageView, nullptr);
	}

	for(auto& image : swapChain->runtime.images)
	{
		vkDestroyImage(vkCtx.logicalDevice, image, nullptr);
	}

	windowInfo.windowExtent = getCurrentWindowExtent(windowInfo.windowHandle);

	createSwapChain(vkCtx, windowInfo, swapChain->imageCount, swapChain);

	//vkDestroyPipeline(vkCtx.logicalDevice, vkCtx.graphicsPipe, nullptr);
	//vkDestroyRenderPass(vkCtx.logicalDevice, vkCtx.renderPass, nullptr);

	return VK_SUCCESS;
}