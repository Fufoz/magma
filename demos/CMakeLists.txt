
macro(build_example exec_name)
	add_executable(${exec_name} ${ARGN})
	target_link_libraries(${exec_name} PRIVATE magma)
	install(TARGETS ${exec_name} RUNTIME DESTINATION ./)
endmacro()


#build_example(triangle triangle.cc)
# build_example(flock flock.cc)
build_example(fluid_sim fluid_sim.cc)
