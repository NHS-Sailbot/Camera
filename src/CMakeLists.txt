set(sources core/add.cpp)

if (WIN32)
    set(sources ${sources} plat/windows/windows.cpp)
elseif (APPLE)
    set(sources ${sources} plat/macos/macos.cpp)
elseif (UNIX)
    set(sources ${sources} plat/linux/linux.cpp)
endif()

add_library(${PROJECT_NAME} ${sources})
