if (WIN32)
set(sources ${sources}
    windows_camera.cpp
)
elseif (APPLE)
set(sources ${sources} 
    macos_camera.cpp
)
elseif (UNIX)
set(sources ${sources} 
    linux_camera.cpp
)
endif()

add_library(${lib_name} ${sources})

target_link_libraries(${lib_name} PUBLIC debug1.0.0)
target_include_directories(${lib_name} PRIVATE ${lib_include_dir})

target_compile_options(${lib_name} PRIVATE -Werror)
target_compile_features(${lib_name} PRIVATE cxx_std_17)

install(TARGETS ${lib_name} DESTINATION lib)
install(DIRECTORY ${lib_include_dir}/HENRY DESTINATION include)
