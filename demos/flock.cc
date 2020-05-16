#include <magma.h>

#include <vector>

int main(int argc, char** argv)
{
	magma::log::setSeverityMask(magma::log::SeverityMask::MASK_ALL);
	magma::log::info("Loading mesh..");
	
	std::vector<const char*> desiredLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	std::vector<const char*> desiredExtensions = {
		"VK_EXT_debug_utils"
	};

	VulkanGlobalContext vkCtx = {};
	VK_CHECK(initVulkanGlobalContext(desiredLayers, desiredExtensions, &vkCtx));

	WindowInfo windowInfo = {};
	VK_CHECK(initPlatformWindow(vkCtx, 640, 480, "Magma", &windowInfo));	
	
	SwapChain swapChain = {};
	VK_CHECK(createSwapChain(vkCtx, windowInfo, 2, &swapChain));

	destroySwapChain(vkCtx, &swapChain);
	destroyPlatformWindow(vkCtx, &windowInfo);
	destroyGlobalContext(&vkCtx);
	
	return 0;
}