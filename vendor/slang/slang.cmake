# Slang shader compiler — fetches pre-built binaries from GitHub releases
#
# Provides:
#   SLANGC              — path to the slangc compiler executable
#   cgfx_compile_slang  — CMake function to compile .slang files to SPIR-V
#
# https://github.com/shader-slang/slang

include(FetchContent)

set(SLANG_VERSION "2026.5.2")

if (EMSCRIPTEN)
    message(WARNING "Slang is not available for Emscripten builds")
    return()
endif()

# ── Detect platform and architecture ────────────────────────────────

set(SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
if (SYSTEM_PROCESSOR STREQUAL "AMD64" OR SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(SLANG_ARCH "x86_64")
elseif (SYSTEM_PROCESSOR STREQUAL "arm64" OR SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(SLANG_ARCH "aarch64")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SLANG_PLATFORM "linux")
    set(SLANG_EXT "tar.gz")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(SLANG_PLATFORM "macos")
    set(SLANG_EXT "tar.gz")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SLANG_PLATFORM "windows")
    set(SLANG_EXT "zip")
else()
    message(FATAL_ERROR "Unsupported platform for Slang: ${CMAKE_SYSTEM_NAME}")
endif()

set(SLANG_ARCHIVE "slang-${SLANG_VERSION}-${SLANG_PLATFORM}-${SLANG_ARCH}.${SLANG_EXT}")
set(SLANG_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${SLANG_ARCHIVE}")

message(STATUS "Fetching Slang v${SLANG_VERSION} from ${SLANG_URL}")

# ── Download and extract ────────────────────────────────────────────

FetchContent_Declare(
    slang-compiler
    URL      "${SLANG_URL}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Populate(slang-compiler)

set(SLANG_ROOT "${slang-compiler_SOURCE_DIR}")
message(STATUS "Slang extracted to: ${SLANG_ROOT}")

# ── Locate slangc ──────────────────────────────────────────────────

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SLANGC "${SLANG_ROOT}/bin/slangc.exe" CACHE FILEPATH "Path to slangc compiler")
else()
    set(SLANGC "${SLANG_ROOT}/bin/slangc" CACHE FILEPATH "Path to slangc compiler")
endif()

if (NOT EXISTS "${SLANGC}")
    message(FATAL_ERROR "slangc not found at ${SLANGC}")
endif()

message(STATUS "Using slangc: ${SLANGC}")

# ── Helper function: compile .slang → .wgsl ────────────────────────
#
# Usage:
#   cgfx_compile_slang(
#       SOURCE      path/to/shader.slang
#       ENTRIES     "vertexMain vertex" "fragmentMain fragment"
#       OUTPUT      ${CMAKE_CURRENT_BINARY_DIR}/shader.wgsl
#   )
#
# Each ENTRIES item is a pair: "<entry_point> <stage>".
# The OUTPUT file can then be loaded at runtime as WGSL.

function(cgfx_compile_slang)
    cmake_parse_arguments(ARG "" "SOURCE;OUTPUT" "ENTRIES" ${ARGN})

    if (NOT ARG_SOURCE)
        message(FATAL_ERROR "cgfx_compile_slang: SOURCE is required")
    endif()
    if (NOT ARG_ENTRIES)
        message(FATAL_ERROR "cgfx_compile_slang: ENTRIES is required (list of \"<entry> <stage>\" pairs)")
    endif()
    if (NOT ARG_OUTPUT)
        message(FATAL_ERROR "cgfx_compile_slang: OUTPUT is required")
    endif()

    # Build -entry/-stage arguments for each entry point
    set(ENTRY_ARGS)
    set(ENTRY_DESC)
    foreach(pair ${ARG_ENTRIES})
        separate_arguments(parts UNIX_COMMAND "${pair}")
        list(GET parts 0 entry_name)
        list(GET parts 1 entry_stage)
        list(APPEND ENTRY_ARGS -entry "${entry_name}" -stage "${entry_stage}")
        list(APPEND ENTRY_DESC "${entry_stage}:${entry_name}")
    endforeach()
    list(JOIN ENTRY_DESC ", " ENTRY_DESC_STR)

    add_custom_command(
        OUTPUT  "${ARG_OUTPUT}"
        COMMAND "${SLANGC}"
                "${ARG_SOURCE}"
                -target wgsl
                ${ENTRY_ARGS}
                -o "${ARG_OUTPUT}"
        DEPENDS "${ARG_SOURCE}"
        COMMENT "Slang: ${ARG_SOURCE} [${ENTRY_DESC_STR}] -> ${ARG_OUTPUT}"
        VERBATIM
    )
endfunction()
