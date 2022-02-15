
macro(build_example exec_name)
	add_executable(${exec_name} ${ARGN})
	target_link_libraries(${exec_name} PRIVATE magma)
	install(TARGETS ${exec_name} RUNTIME DESTINATION ./)
endmacro()


# compile_shader(fluid_ink_present fragment)

build_example(fluid_sim fluid_sim.cc)
