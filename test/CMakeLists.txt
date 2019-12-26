add_executable(camera_master_test master_test.cpp)
add_test(camera_master_test camera_master_test)

target_link_libraries(camera_master_test PRIVATE ${lib_name})
target_include_directories(camera_master_test PRIVATE ${lib_include_dir})

target_compile_options(camera_master_test PRIVATE -Werror)
target_compile_features(camera_master_test PRIVATE cxx_std_17)
