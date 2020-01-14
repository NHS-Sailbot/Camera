set(Headers
    Camera.hpp
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

add_library(${LibName} ${Sources} ${Headers})
target_link_libraries(${LibName} PUBLIC Debug jpeg)

target_compile_options(${LibName} PRIVATE -Werror)
target_compile_features(${LibName} PRIVATE cxx_std_17)

install(TARGETS ${LibName} DESTINATION lib)
install(FILES ${Headers} DESTINATION include/Henry)
