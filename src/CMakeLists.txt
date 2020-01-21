set(Headers
    Camera.hpp
)
set(Sources
)

if (WIN32)
    set(Sources ${Sources})
elseif (APPLE)
    set(Sources ${Sources})
elseif (UNIX)
    set(Sources ${Sources}
        LinuxCamera.cpp
    )
endif()

add_library(Camera ${Sources} ${Headers})
target_link_libraries(Camera PUBLIC jpeg)
target_compile_features(Camera PUBLIC cxx_std_17)
target_include_directories(Camera PUBLIC ${CMAKE_CURRENT_LIST_DIR})
