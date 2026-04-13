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
endfunction()

# --- Examples ---
cgfx_add_example(triangle           triangle/main.c)
cgfx_add_example(playing_with_buffers playing_with_buffers/main.c)
cgfx_add_example(compute            compute/main.c)
