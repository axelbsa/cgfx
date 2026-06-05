# Helper function: add an example with common settings.
# Usage: cgfx_add_example(<name> <source_file>)
function(cgfx_add_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE cgfx)
    target_copy_webgpu_binaries(${name})

    if (MSVC)
        target_compile_options(${name} PRIVATE /W4)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -pedantic -std=c23)
    endif()

    if (XCODE)
        set_target_properties(${name} PROPERTIES
            XCODE_GENERATE_SCHEME ON
            XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
        )
    endif()

    if (EMSCRIPTEN)
        set_target_properties(${name} PROPERTIES SUFFIX ".html")
        target_link_options(${name} PRIVATE -sASYNCIFY)
    endif()

    # Copy .wgsl shaders from the example's source directory to the build output.
    # Runs at build time so new shaders are picked up without reconfiguring.
    get_filename_component(source_dir "${source}" DIRECTORY)
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}
            -DDEST_DIR=$<TARGET_FILE_DIR:${name}>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/copy_shaders.cmake
    )
endfunction()

# --- Slang triangle (Slang → WGSL → wgpu-native) ---
set(SLANG_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/slang_triangle/triangle.slang")
set(WGSL_DIR     "${CMAKE_CURRENT_BINARY_DIR}")

cgfx_compile_slang(
    SOURCE  "${SLANG_SHADER}"
    ENTRIES "vertexMain vertex" "fragmentMain fragment"
    OUTPUT  "${WGSL_DIR}/triangle.wgsl"
)

# --- Examples ---
cgfx_add_example(triangle               triangle/main.c)
cgfx_add_example(buffers                playing_with_buffers/main.c)
cgfx_add_example(compute                compute/main.c)
cgfx_add_example(compute_texture        compute_texture/main.c)
cgfx_add_example(primitives             primitives/main.c)
cgfx_add_example(vertex_attribute       vertex_attribute/main.c)
cgfx_add_example(multiple_attributes    multiple_attributes/main.c)
cgfx_add_example(loading_from_file      loading_from_file/main.c)
cgfx_add_example(multiple_uniforms      multiple_uniforms/main.c)
cgfx_add_example(depth_texture          depth_texture/main.c)
cgfx_add_example(projection_matrices   projection_matrices/main.c)
cgfx_add_example(mrt                     mrt/main.c)
cgfx_add_example(instanced_rendering    instanced_rendering/main.c)
cgfx_add_example(software_framebuffer   software_framebuffer/main.c)

# cgfx_add_example(external_window        external_window/main.c)

# --- Slang Examples ---
cgfx_add_example(slang_triangle         slang_triangle/main.c)
target_sources(slang_triangle PRIVATE "${WGSL_DIR}/triangle.wgsl")
