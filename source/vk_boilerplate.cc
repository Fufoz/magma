#include "vk_boilerplate.h"
#include "vk_loader.h"
#include "vk_dbg.h"
#include "logging.h"
#include <vector>
#include <platform/platform.h>

const char *desiredDeviceExtensions[] = {
	"VK_KHR_swapchain",
	"VK_KHR_maintenance1"//for negative viewport values
};
const uint32_t deviceExtSize = sizeof(desiredDeviceExtensions)/sizeof(desiredDeviceExtensions[0]);

static VkBool32 request_layers_and_extensions(const std::vector<const char*>& desiredExtensions, const std::vector<const char*>& desiredLayers)
{
	
	uint32_t vkApiVersion = {};
	VK_CALL(vkEnumerateInstanceVersion(&vkApiVersion));
	magma::log::debug("Vulkan api version {}.{}.{}", 
		VK_VERSION_MAJOR(vkApiVersion),
		VK_VERSION_MINOR(vkApiVersion),
		VK_VERSION_PATCH(vkApiVersion));
	
	uint32_t extCount = {};
	vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
	std::vector<VkExtensionProperties> extensions = {};
	extensions.resize(extCount); 
	vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extensions.data());
	
	magma::log::debug("Desired Extensions:");
	for(auto& extension : desiredExtensions)
	{
		magma::log::debug("\t Extension: {}", extension);
	}

	magma::log::debug("Extensions:");
	for(auto& extension : extensions)
	{
		magma::log::debug("\t Extension: {}", extension.extensionName);
	}

	//make sure that requested extensions are exposed by the vulkan loader
	uint32_t extFound = {};
	for(auto& desiredExt : desiredExtensions)
	{
		bool extFoundd = false;
		for(auto& extension : extensions)
		{
			if(!strcmp(desiredExt, extension.extensionName))
			{
				extFound++;
				extFoundd = true;
				break;
			}
		}
		if(!extFoundd)
			magma::log::error("Failed to find desired extension = {}", desiredExt);
	}

	uint32_t layerCount = {};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> layers = {};
	layers.resize(layerCount); 
	vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

	magma::log::debug("Desired Layers:");
	for(auto& desiredLayer : desiredLayers)
	{
		magma::log::debug("\t Layer: {}", desiredLayer);
	}

	magma::log::debug("Layers:");
	for(auto& layer : layers)
	{
		magma::log::debug("\t Layer: {}", layer.layerName);
		magma::log::debug("\t\t Description: {}", layer.description);
	}

	//make sure that requested layers are exposed by the vulkan loader
	uint32_t layersFound = {};
	for(auto& desiredLayer : desiredLayers)
	{
		bool layerFound = false;
		for(auto& layer : layers)
		{
			if(!strcmp(desiredLayer, layer.layerName))
			{
				layersFound++;
				layerFound = true;
				break;
			}
		}
		if(!layerFound)
			magma::log::error("Failed to find desired layer = {}",desiredLayer);
	}

	return (extFound == desiredExtensions.size()) && (layersFound == desiredLayers.size()) ? VK_TRUE : VK_FALSE; 
}

static VkDebugUtilsMessengerEXT register_debug_callback(VkInstance instance)
{
	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {};

	debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugUtilsMessengerCreateInfo.pNext = nullptr;
	debugUtilsMessengerCreateInfo.messageSeverity = 
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugUtilsMessengerCreateInfo.messageType = 
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugUtilsMessengerCreateInfo.pfnUserCallback = debug_callback;
	debugUtilsMessengerCreateInfo.pUserData = nullptr;

	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VK_CALL(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugMessenger));
	return debugMessenger;
}

static VkInstance create_instance(
	const std::vector<const char*>& desiredLayers,
	const std::vector<const char*>& desiredExtensions,
	bool hasDebugUtilsExt)
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "Magma";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = "Magma";
	//TODO: add vk_khr_maintenance1 extension if you want to support negative viewport values
	// in vulkan 1.0 api version
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);
	
	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {};
	debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugUtilsMessengerCreateInfo.pNext = nullptr;
	debugUtilsMessengerCreateInfo.messageSeverity = 
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT|
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugUtilsMessengerCreateInfo.messageType = 
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugUtilsMessengerCreateInfo.pfnUserCallback = debug_callback;
	debugUtilsMessengerCreateInfo.pUserData = nullptr;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	if(hasDebugUtilsExt)
	{
		instanceCreateInfo.pNext = &debugUtilsMessengerCreateInfo;
	}
	instanceCreateInfo.flags = 0;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledLayerCount = desiredLayers.size();
	instanceCreateInfo.ppEnabledLayerNames = (char**)desiredLayers.data();
	instanceCreateInfo.enabledExtensionCount = desiredExtensions.size();
	instanceCreateInfo.ppEnabledExtensionNames = (char**)desiredExtensions.data();
	VkInstance instance = VK_NULL_HANDLE;
	VK_CALL(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));
	return instance;
}

//finds queue family index of the supplied physical device
static uint32_t find_queue_family_index(VkPhysicalDevice physicalDevice, VkQueueFlags desiredFlags, VkQueueFlags flagsToAvoid = 0)
{
	uint32_t queueFamPropsCount = 0;
	//Reports properties of the queues of the specified physical device
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamPropsCount, nullptr);
	VkQueueFamilyProperties queueFamProps[16] =  {};

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamPropsCount, queueFamProps);
	
	int queueFamilyIndex = -1;

	for(uint32_t i = 0 ; i < queueFamPropsCount; i++) 
	{
		if(queueFamProps[i].queueCount > 0 &&
			(queueFamProps[i].queueFlags & desiredFlags) &&
			!(queueFamProps[i].queueFlags & flagsToAvoid) )
		{
			return i;
		}
	}

	return VK_QUEUE_FAMILY_IGNORED;
}

static uint32_t get_dedicated_compute_queue(VkPhysicalDevice physicalDevice)
{
	return find_queue_family_index(physicalDevice, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT);
}

static VkBool32 pick_queue_index_and_physical_device(VkInstance instance, VkQueueFlags queueFlags, VkPhysicalDeviceType preferredGPUType, VkPhysicalDevice* physicalDevice, uint32_t* queueFamIdx)
{
	uint32_t physicalDeviceCount = 0;
	int32_t preferredIndex = -1;
	//Enumerates the physical devices accessible to a Vulkan instance
	VK_CALL(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));

	const uint32_t MAX_DEVICE_COUNT = 16;
	VkPhysicalDevice deviceList[MAX_DEVICE_COUNT] = {};
	VK_CALL(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, deviceList));

	for(uint32_t i = 0; i < physicalDeviceCount; i++) 
	{
		VkPhysicalDeviceProperties vkPhysicalDeviceProps = {};
		vkGetPhysicalDeviceProperties(deviceList[i], &vkPhysicalDeviceProps);

		magma::log::warn("Checking GPU - {}", vkPhysicalDeviceProps.deviceName);

		//checks whether physical device supports queue family with graphics flag set
		uint32_t familyIndex = find_queue_family_index(deviceList[i], queueFlags);
		
		if(familyIndex == VK_QUEUE_FAMILY_IGNORED)
			continue;

		if(vkPhysicalDeviceProps.deviceType == preferredGPUType) 
		{
			preferredIndex = i;
			break;
		}
	}

	if(preferredIndex >= 0) 
	{
		VkPhysicalDeviceProperties vkPhysicalDeviceProps = {};
		vkGetPhysicalDeviceProperties(deviceList[preferredIndex], &vkPhysicalDeviceProps);

		magma::log::warn("Selected GPU - {}", vkPhysicalDeviceProps.deviceName);
		*queueFamIdx = preferredIndex;
		*physicalDevice = deviceList[preferredIndex];
		return VK_TRUE;
	}

	magma::log::error("No compatible GPUS were found!");
	return VK_FALSE;
}

static VkDevice create_logical_device(VkPhysicalDevice physicalDevice, VkQueueFlags requestedQueueTypes)
{
	uint32_t deviceExtCount = {};
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtCount, nullptr);
	std::vector<VkExtensionProperties> deviceExt = {};
	deviceExt.resize(deviceExtCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtCount, deviceExt.data());
	
	magma::log::debug("Device extension properties:");
	for(auto dext : deviceExt)
	{
		magma::log::debug("\t{}",dext.extensionName);
	}

	uint32_t dextCountVerified = {};
	for(auto& desiredExt : desiredDeviceExtensions)
	{
		for(auto& availableExt : deviceExt)
		{
			if(!strcmp(desiredExt, availableExt.extensionName))
			{
				dextCountVerified++;
			}
		}
	}

	if(dextCountVerified != deviceExtSize)
	{
		magma::log::error("Some of the device level extensions weren't provided by"
			" selected physical device!");
		return VK_NULL_HANDLE;
	}

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
	const float queuePriority = 1.f;

	if(requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.pNext = nullptr;
		queueCreateInfo.flags = VK_FLAGS_NONE;
		queueCreateInfo.queueFamilyIndex = find_queue_family_index(physicalDevice, VK_QUEUE_GRAPHICS_BIT);
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	if(requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
	{
		uint32_t computeQueueFamIndex = get_dedicated_compute_queue(physicalDevice);
		//if there is no dedicated compute queue, just use the first one that has a compute flag set.
		if(computeQueueFamIndex == VK_QUEUE_FAMILY_IGNORED)
		{
			computeQueueFamIndex = find_queue_family_index(physicalDevice, VK_QUEUE_COMPUTE_BIT);
		}

		if(computeQueueFamIndex >= 0)
		{
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.pNext = nullptr;
			queueCreateInfo.flags = VK_FLAGS_NONE;
			queueCreateInfo.queueFamilyIndex = (uint32_t)computeQueueFamIndex;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}
	}
	
	VkPhysicalDeviceFeatures deviceFeatures = {};
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = nullptr;
	deviceCreateInfo.flags = VK_FLAGS_NONE;
	deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	deviceCreateInfo.enabledExtensionCount = deviceExtSize;
	deviceCreateInfo.ppEnabledExtensionNames = desiredDeviceExtensions;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	VkDevice logicalDevice = VK_NULL_HANDLE;
	VK_CALL(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice));

	return logicalDevice;
}

VkSemaphore create_semaphore(VkDevice logicalDevice)
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = VK_FLAGS_NONE;

	VkSemaphore semaphore = VK_NULL_HANDLE;
	VK_CALL(vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &semaphore));
	return semaphore;
}

VkFence create_fence(VkDevice logicalDevice, bool signalled)
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : VK_FLAGS_NONE;
	
	VkFence fence = VK_NULL_HANDLE;
	VK_CALL(vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &fence));
	return fence;
}

bool init_vulkan_context(
	std::vector<const char*> desiredLayers,
	std::vector<const char*> desiredExtensions,
	VulkanGlobalContext* generalInfo)
{
	assert(generalInfo);
	
	VK_CALL_RETURN_BOOL(locate_and_init_vulkan());

	bool hasDebugUtilsExt = false;
	for(auto&& extension : desiredExtensions)
	{
		if(!strcmp(extension, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
		{
			hasDebugUtilsExt = true;
			break;
		}
	}

	uint32_t requiredExtCount = {};
	const char** requiredExtStrings = get_required_surface_exts(&requiredExtCount);

	for(std::size_t i = 0; i < requiredExtCount; i++)
	{
		desiredExtensions.push_back(requiredExtStrings[i]);
	}

	VK_CHECK(request_layers_and_extensions(desiredExtensions, desiredLayers));
	
	VkInstance instance = create_instance(desiredLayers, desiredExtensions, hasDebugUtilsExt);

	load_instance_function_pointers(instance);

	VkDebugUtilsMessengerEXT dbgCallback = VK_NULL_HANDLE;
	if(hasDebugUtilsExt)
	{
		dbgCallback = register_debug_callback(instance);
	}

	uint32_t graphicsQueueFamilyIdx = VK_QUEUE_FAMILY_IGNORED;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	//we use graphics queue by default
	VK_CHECK(pick_queue_index_and_physical_device(
		instance,
		VK_QUEUE_GRAPHICS_BIT,
		VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
		&physicalDevice,
		&graphicsQueueFamilyIdx)
	);
	assert(graphicsQueueFamilyIdx >= 0);

	VkDevice logicalDevice = create_logical_device(
		physicalDevice,
		VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT
	);

	load_device_function_pointers(logicalDevice);

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(logicalDevice, graphicsQueueFamilyIdx, 0, &graphicsQueue);
	assert(graphicsQueue != VK_NULL_HANDLE);
	
	uint32_t computeQueueFamilyIndex = get_dedicated_compute_queue(physicalDevice);
	VkQueue computeQueue = VK_NULL_HANDLE;
	if(computeQueueFamilyIndex >=0)
	{
		vkGetDeviceQueue(logicalDevice, computeQueueFamilyIndex, 0, &computeQueue);
		assert(computeQueue != VK_NULL_HANDLE);
	}
	else
	{
		computeQueue = graphicsQueue;
	}
	
	VkPhysicalDeviceProperties deviceProps = {};
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);

	generalInfo->instance = instance;
	generalInfo->logicalDevice = logicalDevice;
	generalInfo->physicalDevice = physicalDevice;
	generalInfo->debugCallback = dbgCallback;
	generalInfo->queueFamIdx = graphicsQueueFamilyIdx;
	generalInfo->computeQueueFamIdx = computeQueueFamilyIndex;
	generalInfo->graphicsQueue = graphicsQueue;
	generalInfo->computeQueue = computeQueue;
	generalInfo->deviceProps = deviceProps;
	generalInfo->hasDebugUtilsExtension = hasDebugUtilsExt;
	uint32_t pushConstantSize = deviceProps.limits.maxPushConstantsSize;
	return true;
}

void destroy_vulkan_context(VulkanGlobalContext* ctx)
{
	if(ctx->hasDebugUtilsExtension)
	{
		vkDestroyDebugUtilsMessengerEXT(ctx->instance, ctx->debugCallback, nullptr);
	}

	vkDestroyDevice(ctx->logicalDevice, nullptr);
	vkDestroyInstance(ctx->instance, nullptr);
}

VkBool32 get_supported_depth_format(VkPhysicalDevice physicalDevice, VkFormat* out)
{
	assert(out);

	VkFormat depthFmt = VK_FORMAT_UNDEFINED;
	const VkFormat allDepthFormats[5] = {
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D32_SFLOAT
	};

	for(const auto& format : allDepthFormats)
	{
		VkFormatProperties formatProps = {};
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
		if(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			*out = format;
			return VK_TRUE;
		}
	}

	return VK_FALSE;
}
