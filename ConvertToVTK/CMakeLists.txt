cmake_minimum_required(VERSION 2.6)
project(ConvertToVTK)
 
find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin )

find_package(HDF5 REQUIRED)
ADD_SUBDIRECTORY(src/hash)

add_executable(ConvertToVTK src/main.cpp src/criaVTK.cpp src/criaVTK.h src/file_util.cpp src/file_util.h src/opts.h src/alphanum.hpp)

if(VTK_LIBRARIES)
    target_link_libraries(ConvertToVTK hashes ${VTK_LIBRARIES})
else()
    target_link_libraries(ConvertToVTK vtkHybrid hashes)
endif()
