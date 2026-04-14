cmake_minimum_required(VERSION 3.0...3.25)
project(
    cgfx
	VERSION 0.1.0
	LANGUAGES C
)

# --- External dependencies ---
if (NOT EMSCRIPTEN)
	add_subdirectory(vendor/glfw)
else()
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(vendor/webgpu)
add_subdirectory(vendor/glfw3webgpu)
add_subdirectory(vendor/slang)

# --- cgfx static library ---
add_subdirectory(cgfx)

# --- Examples ---
add_subdirectory(examples)
