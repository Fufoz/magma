cmake_minimum_required(VERSION 3.10)

if (WIN32)
	list(APPEND CMAKE_CXX_FLAGS "/MP")
	set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(LINUX)
#	set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
endif()

set(GLFW_INSTALL OFF)

add_subdirectory(extern/volk)
add_subdirectory(extern/glfw)
add_subdirectory(extern/fmt)
add_subdirectory(source)
add_subdirectory(demos)