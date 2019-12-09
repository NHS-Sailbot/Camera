include(CTest)

link_libraries(${PROJECT_NAME})
include_directories(${camera_include_dir})
include_directories(${debug_include_dir})

add_executable(camera_master_test master_test.cpp)
add_test(camera_master_test camera_master_test)
