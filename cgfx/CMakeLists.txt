add_library(cgfx STATIC
    cgfx_ctx.c
    cgfx_shader.c
    cgfx_pipeline.c
    cgfx_frame.c
    cgfx_buffer.c
    cgfx_uniform.c
    cgfx_mesh.c
    cgfx_primitives.c
    cgfx_loader.c
    cgfx_camera.c
)

target_include_directories(cgfx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/cglm/include
)
target_compile_definitions(cgfx PUBLIC
    CGLM_FORCE_DEPTH_ZERO_TO_ONE
    CGLM_FORCE_LEFT_HANDED
)
target_link_libraries(cgfx PUBLIC glfw webgpu glfw3webgpu)

if (MSVC)
    target_compile_options(cgfx PRIVATE /W4)
else()
    target_compile_options(cgfx PRIVATE -Werror -Wall -Wextra -pedantic -std=c23)
endif()
