cmake_minimum_required(VERSION 3.12)

project(Camera VERSION 1.0.0)

if (NOT TARGET Camera)
    set(Headers
        inc/Henry/Camera.hpp
    )
    set(Sources)
    set(Dependencies)

    if (WIN32)
        set(Sources ${Sources}
            src/plat/WindowsCamera.cpp
        )
    elseif (APPLE)
        set(Sources ${Sources})
    elseif (UNIX)
        set(Sources ${Sources}
            src/plat/LinuxCamera.cpp
        )
        set(Dependencies ${Dependencies}
            jpeg
        )
    endif()

    add_library(Camera ${Sources} ${Headers})

    target_link_libraries(Camera PUBLIC ${Dependencies})
    target_compile_features(Camera PUBLIC cxx_std_17)
    target_include_directories(Camera PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)
endif()
