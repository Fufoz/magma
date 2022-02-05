
macro(build_example exec_name)
	add_executable(${exec_name} ${ARGN})
	target_link_libraries(${exec_name} PRIVATE magma)
	install(TARGETS ${exec_name} RUNTIME DESTINATION ./)
endmacro()


# function(compile_shader source shaderType)
# 	message(STATUS "compiling shader ${source}, type: ${shaderType}")
# 	add_custom_command(
# 		OUTPUT ${source}.spv
# 		COMMAND $ENV{VULKAN_SDK}/bin/glslc -fshader-stage=${shaderType} shaders/${source}.glsl -o shaders/${source}.spv
# 		DEPENDS ${CMAKE_SOURCE_DIR}/shaders/${source}.glsl
# 		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
# 	)
# endfunction()

# compile_shader(fluid_ink_present fragment)

build_example(fluid_sim fluid_sim.cc)
