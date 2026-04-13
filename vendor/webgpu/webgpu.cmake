# wgpu-native WebGPU dependency
#
# Downloads pre-compiled wgpu-native binaries from the official releases
# at https://github.com/gfx-rs/wgpu-native/releases
#
# Previous dependency used Elie Michel's WebGPU-distribution which
# supported both wgpu-native and Dawn backends. That distribution has
# gone unmaintained (7+ months, stuck at wgpu v0.19.4.1). This module
# fetches directly from the wgpu-native project instead.
#
# This limits the project to the wgpu-native backend only. The old
# Elie Michel dependency is preserved (commented out) below in case
# that distribution becomes active again.

include(FetchContent)

set(WGPU_VERSION "v29.0.0.0")
set(WGPU_COMMIT  "d2e3330ade4ae1bb238d76b485926f067e7ee64c")

if (NOT TARGET webgpu)

    # ── Detect platform and architecture ────────────────────────────

    if (EMSCRIPTEN)
        message(FATAL_ERROR
            "wgpu-native does not provide Emscripten binaries. "
            "For Emscripten, re-enable the Elie Michel distribution below.")
    endif()

    # Architecture
    set(SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
    if (SYSTEM_PROCESSOR STREQUAL "AMD64" OR SYSTEM_PROCESSOR STREQUAL "x86_64")
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(WGPU_ARCH "x86_64")
        elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(WGPU_ARCH "i686")
        endif()
    elseif (SYSTEM_PROCESSOR STREQUAL "arm64" OR SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(WGPU_ARCH "aarch64")
    endif()

    # Platform + archive name
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(WGPU_PLATFORM "linux")
        set(WGPU_ARCHIVE "wgpu-${WGPU_PLATFORM}-${WGPU_ARCH}-release.zip")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(WGPU_PLATFORM "macos")
        set(WGPU_ARCHIVE "wgpu-${WGPU_PLATFORM}-${WGPU_ARCH}-release.zip")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(WGPU_PLATFORM "windows")
        # Default to MSVC on Windows; set -DWGPU_WINDOWS_ABI=gnu to use MinGW
        if (NOT DEFINED WGPU_WINDOWS_ABI)
            set(WGPU_WINDOWS_ABI "msvc")
        endif()
        set(WGPU_ARCHIVE "wgpu-${WGPU_PLATFORM}-${WGPU_ARCH}-${WGPU_WINDOWS_ABI}-release.zip")
    else()
        message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif()

    set(WGPU_URL "https://github.com/gfx-rs/wgpu-native/releases/download/${WGPU_VERSION}/${WGPU_ARCHIVE}")
    message(STATUS "Fetching wgpu-native ${WGPU_VERSION} from ${WGPU_URL}")

    # ── Download and extract ────────────────────────────────────────

    FetchContent_Declare(
        wgpu-native
        URL      "${WGPU_URL}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Populate(wgpu-native)

    set(WGPU_ROOT "${wgpu-native_SOURCE_DIR}")
    message(STATUS "wgpu-native extracted to: ${WGPU_ROOT}")

    # ── Define the webgpu imported target ───────────────────────────

    add_library(webgpu SHARED IMPORTED GLOBAL)
    target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_WGPU)

    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(WGPU_RUNTIME_LIB "${WGPU_ROOT}/lib/wgpu_native.dll")
        set_target_properties(webgpu PROPERTIES
            IMPORTED_LOCATION "${WGPU_RUNTIME_LIB}"
            IMPORTED_IMPLIB   "${WGPU_ROOT}/lib/wgpu_native.dll.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${WGPU_ROOT}/include"
        )
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(WGPU_RUNTIME_LIB "${WGPU_ROOT}/lib/libwgpu_native.so")
        set_target_properties(webgpu PROPERTIES
            IMPORTED_LOCATION "${WGPU_RUNTIME_LIB}"
            IMPORTED_NO_SONAME TRUE
            INTERFACE_INCLUDE_DIRECTORIES "${WGPU_ROOT}/include"
        )
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(WGPU_RUNTIME_LIB "${WGPU_ROOT}/lib/libwgpu_native.dylib")
        set_target_properties(webgpu PROPERTIES
            IMPORTED_LOCATION "${WGPU_RUNTIME_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${WGPU_ROOT}/include"
        )
    endif()

    message(STATUS "Using WebGPU runtime from '${WGPU_RUNTIME_LIB}'")
    set(WGPU_RUNTIME_LIB ${WGPU_RUNTIME_LIB} PARENT_SCOPE)
    set(WGPU_RUNTIME_LIB ${WGPU_RUNTIME_LIB} CACHE INTERNAL "Path to the WebGPU library binary")

    # ── Runtime binary copy helper ──────────────────────────────────
    # Copies the shared library next to the executable so it can be found
    # at runtime. Called by examples/CMakeLists.txt for each target.

    function(target_copy_webgpu_binaries Target)
        add_custom_command(
            TARGET ${Target} POST_BUILD
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                "${WGPU_RUNTIME_LIB}"
                "$<TARGET_FILE_DIR:${Target}>"
            COMMENT
                "Copying '${WGPU_RUNTIME_LIB}' to '$<TARGET_FILE_DIR:${Target}>'..."
        )

        if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set_target_properties(${Target} PROPERTIES INSTALL_RPATH "./")
            add_custom_command(
                TARGET ${Target} POST_BUILD
                COMMAND
                    ${CMAKE_INSTALL_NAME_TOOL} "-change"
                    "@rpath/libwgpu_native.dylib"
                    "@executable_path/libwgpu_native.dylib"
                    "$<TARGET_FILE:${Target}>"
                VERBATIM
            )
        endif()
    endfunction()

endif()


# ════════════════════════════════════════════════════════════════════
# PREVIOUS DEPENDENCY: Elie Michel's WebGPU-distribution
#
# Supports WGPU, WGPU_STATIC, DAWN, and EMSCRIPTEN backends.
# Unmaintained as of 2025-09 (stuck at wgpu v0.19.4.1).
# Re-enable this block (and remove the wgpu-native block above)
# if the distribution becomes active again.
# Repository: https://github.com/eliemichel/WebGPU-distribution
# ════════════════════════════════════════════════════════════════════
#
# set(WEBGPU_BACKEND "WGPU" CACHE STRING "Backend implementation of WebGPU. Possible values are EMSCRIPTEN, WGPU, WGPU_STATIC and DAWN (it does not matter when using emcmake)")
# set_property(CACHE WEBGPU_BACKEND PROPERTY STRINGS EMSCRIPTEN WGPU WGPU_STATIC DAWN)
#
# # FetchContent's GIT_SHALLOW option is buggy and does not actually do a shallow
# # clone. This macro takes care of it.
# macro(FetchContent_DeclareShallowGit Name GIT_REPOSITORY GitRepository GIT_TAG GitTag)
#     FetchContent_Declare(
#         "${Name}"
#         DOWNLOAD_COMMAND
#             cd "${FETCHCONTENT_BASE_DIR}/${Name}-src" &&
#             git init &&
#             git fetch --depth=1 "${GitRepository}" "${GitTag}" &&
#             git reset --hard FETCH_HEAD
#     )
# endmacro()
#
# if (NOT TARGET webgpu)
#     string(TOUPPER ${WEBGPU_BACKEND} WEBGPU_BACKEND_U)
#
#     if (EMSCRIPTEN OR WEBGPU_BACKEND_U STREQUAL "EMSCRIPTEN")
#         FetchContent_DeclareShallowGit(
#             webgpu-backend-emscripten
#             GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
#             GIT_TAG        fa0b54d68841fb33188403b07959d403b24511de # emscripten-v3.1.61 + fix
#         )
#         FetchContent_MakeAvailable(webgpu-backend-emscripten)
#
#     elseif (WEBGPU_BACKEND_U STREQUAL "WGPU")
#         FetchContent_DeclareShallowGit(
#             webgpu-backend-wgpu
#             GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
#             GIT_TAG        54a60379a9d792848a2311856375ceef16db150e # wgpu-v0.19.4.1 + fix
#         )
#         FetchContent_MakeAvailable(webgpu-backend-wgpu)
#
#     elseif (WEBGPU_BACKEND_U STREQUAL "WGPU_STATIC")
#         FetchContent_DeclareShallowGit(
#             webgpu-backend-wgpu-static
#             GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
#             GIT_TAG        992fef64da25072ebe3844a73f7103105e7fd133 # wgpu-static-v0.19.4.1 + fix
#         )
#         FetchContent_MakeAvailable(webgpu-backend-wgpu-static)
#
#     elseif (WEBGPU_BACKEND_U STREQUAL "DAWN")
#         FetchContent_DeclareShallowGit(
#             webgpu-backend-dawn
#             GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
#             GIT_TAG        f49f0f3f6784a86a85944600d66f743e0c7eb4a9 # dawn-6536 + fix
#         )
#         FetchContent_MakeAvailable(webgpu-backend-dawn)
#
#     else()
#         message(FATAL_ERROR "Invalid value for WEBGPU_BACKEND: possible values are EMSCRIPTEN, WGPU, WGPU_STATIC and DAWN, but '${WEBGPU_BACKEND_U}' was provided.")
#     endif()
# endif()
