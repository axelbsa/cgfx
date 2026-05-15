add_library(hwnd3webgpu STATIC hwnd3webgpu.c)
target_include_directories(hwnd3webgpu PUBLIC .)
target_link_libraries(hwnd3webgpu PUBLIC webgpu)
