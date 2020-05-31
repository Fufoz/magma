cmake_minimum_required(VERSION 3.10)

add_library(meshopt STATIC
	indexgenerator.cpp
)

target_include_directories(meshopt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})