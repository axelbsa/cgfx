option(CGFX_SHARED "Build cgfx as a shared library (.dll/.so/.dylib)" OFF)

set(CGFX_SOURCES
    cgfx_ctx.c
    cgfx_shader.c
    cgfx_pipeline.c
    cgfx_frame.c
    cgfx_buffer.c
    cgfx_uniform.c
    cgfx_mesh.c
    cgfx_texture.c
    cgfx_primitives.c
    cgfx_loader.c
    cgfx_camera.c
    cgfx_compute.c
)

if (CGFX_SHARED)
    add_library(cgfx SHARED ${CGFX_SOURCES})
    target_compile_definitions(cgfx PRIVATE CGFX_BUILD_SHARED)
    target_compile_definitions(cgfx PUBLIC CGFX_SHARED)
    if (NOT WIN32)
        target_compile_options(cgfx PRIVATE -fvisibility=hidden)
    endif()
else()
    add_library(cgfx STATIC ${CGFX_SOURCES})
endif()

target_include_directories(cgfx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/cglm/include
)
target_compile_definitions(cgfx PUBLIC
    CGLM_FORCE_DEPTH_ZERO_TO_ONE
    CGLM_FORCE_LEFT_HANDED
)
target_link_libraries(cgfx PUBLIC glfw webgpu glfw3webgpu)
if (WIN32)
    target_link_libraries(cgfx PUBLIC hwnd3webgpu)
endif()

if (MSVC)
    set(CMAKE_C_STANDARD 23)
    target_compile_options(cgfx PRIVATE /W4)
else()
    target_compile_options(cgfx PRIVATE -Werror -Wall -Wextra -pedantic -std=c23)
endif()
