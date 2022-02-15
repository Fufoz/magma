cmake_minimum_required(VERSION 3.10)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

list(APPEND Sources 
	logging.cc
	camera.cc
	animation.cc
	mesh_loaders.cc
	vk_dbg.cc
	vk_loader.cc
	vk_boilerplate.cc
	vk_shader.cc
	vk_resource.cc
	vk_swapchain.cc
	vk_pipeline.cc
	vk_commands.cc
)

if(ANDROID OR IOS)
	message(FATAL_ERROR "Mobile platforms are not supported at the moment")
else()#assume desktop platforms here
	list(APPEND Sources
		platform/window_desktop.cc
		platform/input_desktop.cc
		${CMAKE_SOURCE_DIR}/extern/imgui/imgui.cpp
		${CMAKE_SOURCE_DIR}/extern/imgui/imgui_demo.cpp
		${CMAKE_SOURCE_DIR}/extern/imgui/imgui_draw.cpp
		${CMAKE_SOURCE_DIR}/extern/imgui/imgui_tables.cpp
		${CMAKE_SOURCE_DIR}/extern/imgui/imgui_widgets.cpp
		${CMAKE_SOURCE_DIR}/extern/imgui/imgui_impl_glfw.cpp
		${CMAKE_SOURCE_DIR}/extern/imgui/imgui_impl_vulkan.cpp
	)

endif()


add_library(magma STATIC ${Sources})

target_include_directories(magma PUBLIC 
	${Vulkan_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/extern/imgui
	./
)

target_link_libraries(magma PUBLIC fmt volk glfw meshopt tiny_gltf fast_obj)
