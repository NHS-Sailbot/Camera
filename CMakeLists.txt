cmake_minimum_required(VERSION 3.5)
project(camera VERSION 1.0.0)

set(lib_name ${PROJECT_NAME}${PROJECT_VERSION})
set(lib_include_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(src)

include(CTest)
enable_testing()
add_subdirectory(test)
