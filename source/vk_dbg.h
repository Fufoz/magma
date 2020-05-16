
#ifndef VK_DEBUG_H
#define VK_DEBUG_H

#include <volk.h>
#include "logging.h"

#include <cassert>

#define VK_FLAGS_NONE 0

#define VK_CALL(function)                                                           \
do{                                                                                 \
VkResult error = function;                                                          \
if(error != VK_SUCCESS){                                                            \
	magma::log::error("Error calling {}. Reason: {} ",#function,vkStrError(error)); \
	assert(!"VK_ASSERT!");														    \
}																					\
}while(0)

#define VK_CALL_RETURN(function)                                                   \
do{                                                                                \
VkResult error = function;                                                         \
if(error != VK_SUCCESS){                                                           \
	magma::log::error("Error calling %s. Reason: {}", #function, vkStrError(error));\
	return error;                                                                  \
}                                                                                  \
}while(0)


#define VK_CHECK(function)                                                         \
do{                                                                                \
VkBool32 error = function;                                                         \
if(error != VK_TRUE)                                                               \
	magma::log::error("Check failed calling {}", #function);                       \
}while(0)

const char* vkStrError(VkResult error);

VkBool32 instanceDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData);

VkBool32 runtimeDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData);

#endif