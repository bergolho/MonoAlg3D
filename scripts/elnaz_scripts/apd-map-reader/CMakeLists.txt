cmake_minimum_required(VERSION 2.8)
 
PROJECT(ApdReader)
 
find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin )

add_executable(ApdReader main.cpp )
 
target_link_libraries(ApdReader ${VTK_LIBRARIES})
