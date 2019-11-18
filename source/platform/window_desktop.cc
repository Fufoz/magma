#include "vk_types.h"

#include <vk_dbg.h>
#include <vk_loader.h>
#include <vk_boilerplate.h>
#include <logging.h>
#include <GLFW/glfw3.h>

const char* glfwError()
{
	const char* err;
	int status = glfwGetError(&err);
	if(status != GLFW_NO_ERROR)
	{
		return err;
	}
	
	return "No error";
}

const char** getRequiredSurfaceExtensions(uint32_t* surfaceExtCount)
{
	if(glfwInit() != GLFW_TRUE)
	{
		magma::log::error("GLFW: failed to init! {}", glfwError());
		return VK_FALSE;
	}
	return glfwGetRequiredInstanceExtensions(surfaceExtCount);
}

VkBool32 initPlatformWindow(const VulkanGlobalContext& globalInfo, uint32_t width, uint32_t height,
	const char* title, WindowInfo* surface)
{
	///////////////////////////////////////////////
	if(glfwInit() != GLFW_TRUE)
	{
		magma::log::error("GLFW: failed to init! {}", glfwError());
		return VK_FALSE;
	}

	if(!glfwVulkanSupported())
	{
		magma::log::error("GLFW: vulkan is not supported! {}", glfwError());
		return VK_FALSE;
	}	
	//check whether selected queue family index supports window image presentation
	if(glfwGetPhysicalDevicePresentationSupport(globalInfo.instance, globalInfo.physicalDevice, globalInfo.queueFamIdx) == GLFW_FALSE)
	{
		magma::log::error("Selected queue family index does not support image presentation!");
		return VK_FALSE;
	}

	//disable glfw window context creation
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(width, height, "Magma", nullptr, nullptr);
	if(!window)
	{
		magma::log::error("GLFW window was nullptr!");
		return VK_FALSE;
	}
	assert(window);
	//create OS specific window surface to render into
	VkSurfaceKHR windowSurface = VK_NULL_HANDLE;
	VK_CALL(glfwCreateWindowSurface(globalInfo.instance, window, nullptr, &windowSurface));
	
	//query created surface capabilities
	VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(globalInfo.physicalDevice, windowSurface, &surfaceCapabilities));

	surface->title = title;
	surface->windowHandle = (void*)window;
	surface->windowExtent = {width, height};
	surface->surface = windowSurface;
	surface->surfaceCaps = surfaceCapabilities;

	return VK_TRUE;
}

void destroyPlatformWindow(void* windowHandle)
{
	glfwDestroyWindow((GLFWwindow*)windowHandle);
}