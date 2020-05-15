add_executable(triangle triangle.cc)
target_link_libraries(triangle PRIVATE magma)
install(TARGETS triangle RUNTIME DESTINATION ./)