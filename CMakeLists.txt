cmake_minimum_required(VERSION 3.10)
add_executable(usrtix main.cc)
install(TARGETS usrtix RUNTIME DESTINATION ./)