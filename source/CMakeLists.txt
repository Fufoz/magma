cmake_minimum_required(VERSION 3.10)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_executable(magma 
	main.cc
	logging.cc
	vk_dbg.cc
	vk_boilerplate.cc
)

target_include_directories(magma PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(magma PRIVATE fmt volk glfw)

install(TARGETS magma RUNTIME DESTINATION ./)