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

int main(int argc, char **argv)
{
	magma::log::setSeverityMask(magma::log::MASK_ALL);
	magma::log::warn("Loading Vulkan loader..");
	VK_CALL(volkInitialize());

	if(glfwInit() != GLFW_TRUE)
	{
		const char* err;
		glfwGetError(&err);
		magma::log::error("GLFW: failed to init! {}", err);
		return -1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	if(!glfwVulkanSupported())
	{
		const char* err;
		glfwGetError(&err);
		magma::log::error("GLFW: vulkan is not supported! {}", err);
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
	
	return 0;
}