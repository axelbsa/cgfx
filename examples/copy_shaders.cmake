# Copy shader files to ./examples/shader/
file(GLOB shaders "${SOURCE_DIR}/*.wgsl")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
foreach(shader ${shaders})
    get_filename_component(name "${shader}" NAME)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${shader}" "${DEST_DIR}/shaders/${name}")
endforeach()

# Copy .txt files to ./examples
# /s/shader/model
file(GLOB shaders "${SOURCE_DIR}/*.txt")
foreach(shader ${shaders})
    get_filename_component(name "${shader}" NAME)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${shader}" "${DEST_DIR}/${name}")
endforeach()
