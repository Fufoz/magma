
#ifndef VK_DEBUG_H
#define VK_DEBUG_H

#include <volk.h>
#include "logging.h"

#include <cassert>

#define VK_FLAGS_NONE 0

#define VK_CALL(function)                                                           \
do{                                                                                 \
VkResult error = function;                                                          \
if(error != VK_SUCCESS)                                                             \
{                                                                                   \
	magma::log::error("Error calling {}. Reason: {}; file : {}; line {}",           \
		#function,vk_error_string(error), __FILE__, __LINE__);                           \
	assert(!"VK_ASSERT!");														    \
}																					\
}while(0)

#define VK_CALL_RETURN(function)                                                   \
do{                                                                                \
VkResult error = function;                                                         \
if(error != VK_SUCCESS)                                                            \
{                                                                                  \
	magma::log::error("Error calling {}. Reason: {}; file : {}; line {}",          \
		#function, vk_error_string(error), __FILE__, __LINE__);                         \
	return error;                                                                  \
}                                                                                  \
}while(0)

#define VK_CALL_RETURN_BOOL(function)                                              \
do{                                                                                \
VkResult error = function;                                                         \
if(error != VK_SUCCESS)                                                            \
{                                                                                  \
	magma::log::error("Error calling {}. Reason: {}; file : {}; line {}",          \
		#function, vk_error_string(error), __FILE__, __LINE__);                         \
	return false;                                                                  \
}                                                                                  \
}while(0)

#define VK_CHECK(function)                                                         \
do{                                                                                \
VkBool32 error = function;                                                         \
if(error != VK_TRUE)                                                               \
{                                                                                  \
	magma::log::error("Check failed calling {} in file :{}; line: {}",             \
		#function, __FILE__, __LINE__);                                            \
	assert(!"WTF");                                                                \
}                                                                                  \
}while(0)                                                                          \

const char* vk_error_string(VkResult error);

VkBool32 debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
	void*                                            pUserData);

#endif