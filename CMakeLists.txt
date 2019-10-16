cmake_minimum_required(VERSION 3.10)

if (WIN32)
   set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()

add_subdirectory(extern/volk)
add_subdirectory(extern/fmt)
add_subdirectory(source)