cmake_minimum_required(VERSION 3.0.0)
project(camera VERSION 0.1.0)

if (NOT TARGET ${PROJECT_NAME})
	set(camera_include_dir "${CMAKE_CURRENT_SOURCE_DIR}/include")
	set(debug_include_dir "${CMAKE_CURRENT_SOURCE_DIR}/dep/debug/include")

	add_subdirectory(dep/debug)

	add_subdirectory(src)
	add_subdirectory(tests)
endif()
