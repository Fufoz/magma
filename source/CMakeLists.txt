cmake_minimum_required(VERSION 3.10)

add_executable(magma 
	main.cc
	logging.cc
	vk_dbg.cc
	vk_boilerplate.cc
)

target_include_directories(magma PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(magma PRIVATE fmt volk)

install(TARGETS magma RUNTIME DESTINATION ./)