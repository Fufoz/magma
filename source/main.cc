#include <volk.h>
#include <GLFW/glfw3.h>

#include "logging.h"
#include "vk_dbg.h"
#include "vk_boilerplate.h"

#include <vector>
#include <string>

std::vector<std::string> desiredExtensions = {
	"VK_EXT_debug_report"
};
std::vector<std::string> desiredLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

int main(int argc, char **argv)
{
	magma::log::setSeverityMask(magma::log::MASK_ALL);
	magma::log::warn("Loading Vulkan loader..");
	VK_CALL(volkInitialize());

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	if(!glfwVulkanSupported())
	{
		magma::log::error("GLFW: vulkan is not supported!");
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
	assert(graphicsQueue == VK_NULL_HANDLE);
	
	return 0;
}