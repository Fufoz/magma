#include "vk_types.h"

#include <vk_dbg.h>
#include <vk_loader.h>
#include <vk_boilerplate.h>
#include <logging.h>
#include <input.h>
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

bool windowShouldClose(void* windowHandle)
{
	return glfwWindowShouldClose((GLFWwindow*)windowHandle);
}

void updateMessageQueue()
{
	glfwPollEvents();
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

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	 int keyState = glfwGetKey(window, GLFW_KEY_W);

	if(key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(window, 1);
	
}

bool initPlatformWindow(const VulkanGlobalContext& globalInfo, uint32_t width, uint32_t height,
	const char* title, WindowInfo* surface)
{
	auto errorCallback = [](int errorCode, const char* descr)
	{
		magma::log::error("GLFW: error! Code = {}, description: {}", errorCode, descr);
		assert(!"WTF");
	};
	glfwSetErrorCallback(errorCallback);

	///////////////////////////////////////////////
	if(glfwInit() != GLFW_TRUE)
	{
		magma::log::error("GLFW: failed to init! {}", glfwError());
		return false;
	}

	if(!glfwVulkanSupported())
	{
		magma::log::error("GLFW: vulkan is not supported! {}", glfwError());
		return false;
	}
	//check whether selected queue family index supports window image presentation
	if(glfwGetPhysicalDevicePresentationSupport(globalInfo.instance, globalInfo.physicalDevice, globalInfo.queueFamIdx) == GLFW_FALSE)
	{
		magma::log::error("Selected queue family index does not support image presentation!");
		return false;
	}

	//disable glfw window context creation
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(width, height, "Magma", nullptr, nullptr);
	if(!window)
	{
		magma::log::error("GLFW window was nullptr!");
		return false;
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

	initInput(surface->windowHandle);
	
	glfwFocusWindow(window);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSetCursorPos(window, width/2.f, height/2.f);

	return true;
}

VkExtent2D getCurrentWindowExtent(void* windowHandle)
{
	int w = {};
	int h = {};
	glfwGetWindowSize((GLFWwindow*)windowHandle, &w, &h);
	return {(uint32_t)w, (uint32_t)h};
}

void updateWindowDimensions(VkPhysicalDevice physicalDevice, WindowInfo* out)
{
	//query created surface capabilities
	VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, out->surface, &surfaceCapabilities));

	out->windowExtent = getCurrentWindowExtent(out->windowHandle);
	out->surfaceCaps = surfaceCapabilities;
}

void destroyPlatformWindow(const VulkanGlobalContext& vkCtx, WindowInfo* info)
{
	vkDestroySurfaceKHR(vkCtx.instance, info->surface, nullptr);
	glfwDestroyWindow((GLFWwindow*)info->windowHandle);
	glfwTerminate();
}