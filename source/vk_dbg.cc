
#include "vk_dbg.h"

VkBool32 debugCallback(
	VkDebugReportFlagsEXT      flags,
	VkDebugReportObjectTypeEXT objectType,
	uint64_t                   object,
	size_t                     location,
	int32_t                    messageCode,
	const char*                pLayerPrefix,
	const char*                pMessage,
	void*                      pUserData)
{
	// This silences warnings like "For optimal performance image layout should be VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL instead of GENERAL."
	// We'll assume other performance warnings are also not useful.
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		return VK_FALSE;

	switch(flags) {
		case VK_DEBUG_REPORT_ERROR_BIT_EXT : magma::log::error("{}", pMessage); break;
		case VK_DEBUG_REPORT_DEBUG_BIT_EXT : magma::log::debug("{}", pMessage); break;
		case VK_DEBUG_REPORT_WARNING_BIT_EXT : magma::log::warn("{}",pMessage); break;
		case VK_DEBUG_REPORT_INFORMATION_BIT_EXT : magma::log::info("{}",pMessage); break;
		case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT : magma::log::warn("{}", pMessage); break;
		default :;
	}

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		assert(!"Fatal Validation error encountered!");

	return VK_FALSE;
}

const char* vkStrError(VkResult error)
{
	switch(error)
	{
		case VK_SUCCESS :return "VK_SUCCESS";
		case VK_NOT_READY : return "VK_NOT_READY";
		case VK_TIMEOUT : return "VK_TIMEOUT";
		case VK_EVENT_SET : return "VK_EVENT_SET";
		case VK_EVENT_RESET : return "VK_EVENT_RESET";
		case VK_INCOMPLETE : return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY : return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY : return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED : return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST : return "VK_ERROR_DEVICE_LOST"; 
		case VK_ERROR_MEMORY_MAP_FAILED : return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT : return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT : return "VK_ERROR_EXTENSION_NOT_PRESENT"; 
		case VK_ERROR_FEATURE_NOT_PRESENT : return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER : return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS : return "VK_ERROR_TOO_MANY_OBJECTS"; 
		case VK_ERROR_FORMAT_NOT_SUPPORTED : return "VK_ERROR_FORMAT_NOT_SUPPORTED"; 
		case VK_ERROR_FRAGMENTED_POOL : return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_OUT_OF_POOL_MEMORY : return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE : return "VK_ERROR_INVALID_EXTERNAL_HANDLE"; 
		case VK_ERROR_SURFACE_LOST_KHR : return "VK_ERROR_SURFACE_LOST_KHR"; 
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR : return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; 
		case VK_SUBOPTIMAL_KHR : return "VK_SUBOPTIMAL_KHR"; 
		case VK_ERROR_OUT_OF_DATE_KHR : return "VK_ERROR_OUT_OF_DATE_KHR"; 
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR : return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT : return "VK_ERROR_VALIDATION_FAILED_EXT"; 
		case VK_ERROR_INVALID_SHADER_NV : return "VK_ERROR_INVALID_SHADER_NV";
		case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT : return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"; 
		case VK_ERROR_FRAGMENTATION_EXT : return "VK_ERROR_FRAGMENTATION_EXT";
		case VK_ERROR_NOT_PERMITTED_EXT : return "VK_ERROR_NOT_PERMITTED_EXT"; 
		default : return "VK_UNKNOWN_ERR";
	}
}