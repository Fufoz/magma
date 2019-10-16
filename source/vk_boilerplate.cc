#include "vk_boilerplate.h"
#include "vk_dbg.h"
#include "logging.h"

#include <vector>

const char *desiredLayers[] = {
	"VK_LAYER_LUNARG_standard_validation"
};
const uint32_t layersSize = sizeof(desiredLayers)/sizeof(desiredLayers[0]);

const char *desiredExtensions[] = {
	"VK_KHR_surface",
	"VK_KHR_win32_surface",
	"VK_EXT_debug_report"
};
const uint32_t extSize = sizeof(desiredExtensions)/sizeof(desiredExtensions[0]);

const char *desiredDeviceExtensions[] = {
	"VK_KHR_swapchain"
};
const uint32_t deviceExtSize = sizeof(desiredDeviceExtensions)/sizeof(desiredDeviceExtensions[0]);

VkBool32 requestLayersAndExtensions()
{
	uint32_t extCount = {};
	vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
	std::vector<VkExtensionProperties> extensions = {};
	extensions.resize(extCount); 
	vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extensions.data());

	magma::log::debug("Extensions:");
	for(auto& extension : extensions)
		magma::log::debug("\t Extension: {}", extension.extensionName);

	//make sure that requested extensions are exposed by the vulkan loader
	uint32_t extFound = {};
	for(auto& desiredExt : desiredExtensions)
	{
		for(auto& extension : extensions)
		{
			if(!strcmp(desiredExt, extension.extensionName))
			{
				extFound++;
			}
		}
	}

	uint32_t layerCount = {};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> layers = {};
	layers.resize(layerCount); 
	vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
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
		for(auto& layer : layers)
		{
			if(!strcmp(desiredLayer, layer.layerName))
			{
				layersFound++;
			}
		}
	}

	return (extFound == extSize) && (layersFound == layersSize) ? VK_TRUE : VK_FALSE; 
}

VkDebugReportCallbackEXT registerDebugCallback(VkInstance instance)
{
    //connect debug callback function
    VkDebugReportCallbackCreateInfoEXT callbackInfo = {};    
    
    callbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    callbackInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
        VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT |
        VK_DEBUG_REPORT_DEBUG_BIT_EXT;
    callbackInfo.pfnCallback = debugCallback;

    VkDebugReportCallbackEXT callback = 0;
    VK_CALL(vkCreateDebugReportCallbackEXT(instance, &callbackInfo, nullptr, &callback));
    return callback;
}

VkInstance createInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "Magma";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = "Magma";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);
	
	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.flags = 0;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledLayerCount = sizeof(desiredLayers)/sizeof(desiredLayers[0]);
	instanceCreateInfo.ppEnabledLayerNames = desiredLayers;
	instanceCreateInfo.enabledExtensionCount = sizeof(desiredExtensions)/sizeof(desiredExtensions[0]);
	instanceCreateInfo.ppEnabledExtensionNames = desiredExtensions;
	VkInstance instance = VK_NULL_HANDLE;
	VK_CALL(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));
	return instance;
}

//finds queue family index of the supplied physical device
uint32_t findQueueFamilyIndex(VkPhysicalDevice physicalDevice, VkQueueFlags desiredFlags)
{
    uint32_t queueFamPropsCount = 0;
    //Reports properties of the queues of the specified physical device
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamPropsCount, nullptr);
    VkQueueFamilyProperties queueFamProps[16] =  {};

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamPropsCount, queueFamProps);
    
    int queueFamilyIndex = -1;

    for(uint32_t i = 0 ; i < queueFamPropsCount; i++) 
	{
        if(queueFamProps[i].queueCount > 0 && queueFamProps[i].queueFlags & desiredFlags) 
		{
            return i;
        }
    }

    return VK_QUEUE_FAMILY_IGNORED;
}

VkBool32 pickQueueIndexAndPhysicalDevice(VkInstance instance, VkQueueFlags queueFlags, VkPhysicalDeviceType preferredGPUType, VkPhysicalDevice* physicalDevice, uint32_t* queueFamIdx)
{
    uint32_t physicalDeviceCount = 0;
    int32_t preferredIndex = -1;
    //Enumerates the physical devices accessible to a Vulkan instance
    VK_CALL(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));

    VkPhysicalDevice deviceList[16];

    VK_CALL(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, deviceList));

    for(uint32_t i = 0; i < physicalDeviceCount; i++) 
	{
        VkPhysicalDeviceProperties vkPhysicalDeviceProps = {};
        vkGetPhysicalDeviceProperties(deviceList[i], &vkPhysicalDeviceProps);

        magma::log::warn("Checking GPU - {}", vkPhysicalDeviceProps.deviceName);

        //checks whether physical device supports queue family with graphics flag set
        uint32_t familyIndex = findQueueFamilyIndex(deviceList[i], queueFlags);
        
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

VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIdx)
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
	
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	float queuePriority = 1.f;
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = nullptr;
    queueCreateInfo.flags = VK_FLAGS_NONE;
    queueCreateInfo.queueFamilyIndex = (uint32_t)queueFamilyIdx;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = nullptr;
	deviceCreateInfo.flags = VK_FLAGS_NONE;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	deviceCreateInfo.enabledExtensionCount = deviceExtSize;
	deviceCreateInfo.ppEnabledExtensionNames = desiredDeviceExtensions;
	deviceCreateInfo.pEnabledFeatures = nullptr;

	VkDevice logicalDevice = VK_NULL_HANDLE;
	VK_CALL(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice));

	return logicalDevice;
}
