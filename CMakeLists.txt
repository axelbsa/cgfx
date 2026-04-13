cmake_minimum_required(VERSION 3.0...3.25)
project(
    ray-tracer
	VERSION 0.1.0
	LANGUAGES C
)

if (NOT EMSCRIPTEN)
	add_subdirectory(src/external/glfw)
else()
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(src/external/webgpu)
add_subdirectory(src/external/glfw3webgpu)

#add_executable(app
#    src/main.cpp
#    src/webgpu-utils.h
#    src/webgpu-utils.cpp
#)
add_executable(app
	src/main.c
)

target_link_libraries(app PRIVATE glfw webgpu glfw3webgpu)

target_copy_webgpu_binaries(app)

set_target_properties(app PROPERTIES CMAKE_C_STANDARD 23)

if (MSVC)
	target_compile_options(app PRIVATE /W4)
else()
	target_compile_options(app PRIVATE -Wall -Wextra -pedantic -std=c23)
endif()

if (XCODE)
	set_target_properties(app PROPERTIES
		XCODE_GENERATE_SCHEME ON
		XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
	)
endif()

if (EMSCRIPTEN)
	set_target_properties(app PROPERTIES SUFFIX ".html")
	target_link_options(app PRIVATE -sASYNCIFY)
endif()
