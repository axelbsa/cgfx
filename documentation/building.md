# Building

This page covers all the ways to build and integrate cgfx into your project.

## Static Library (Default)

The default build produces a static library (`libcgfx.a` on Linux/macOS, `cgfx.lib` on Windows):

```bash
cmake . -B build
cmake --build build
```

All cgfx symbols are linked directly into your executable. No runtime library dependencies beyond the system GPU drivers.

## Shared Library

To build cgfx as a shared library (`libcgfx.so` on Linux, `cgfx.dll` on Windows, `libcgfx.dylib` on macOS):

```bash
cmake . -B build -DCGFX_SHARED=ON
cmake --build build
```

### The CGFX_API Macro

When building as a shared library, the `CGFX_API` macro controls symbol visibility. All public cgfx functions are decorated with `CGFX_API`:

```c
CGFX_API bool cgfx_ctx_init(CgfxCtx *ctx, const CgfxCtxDesc *desc);
CGFX_API CgfxShader cgfx_shader_create(const CgfxCtx *ctx, ...);
```

The macro expands differently depending on the platform and build mode:

| Condition | CGFX_API expands to |
|-----------|---------------------|
| **Static build** (default) | *(empty)* -- no decoration needed |
| **Shared build, compiling cgfx itself** | `__declspec(dllexport)` (Windows) or `__attribute__((visibility("default")))` (Linux/macOS) |
| **Shared build, consuming cgfx** | `__declspec(dllimport)` (Windows) or *(empty)* (Linux/macOS) |

On Linux, cgfx compiles with `-fvisibility=hidden` when building shared, so only functions marked `CGFX_API` are exported. On Windows, `dllexport`/`dllimport` handles the same role.

!!! tip "C# P/Invoke"
    The shared library build is designed for embedding cgfx in other languages. For C# P/Invoke, build with `-DCGFX_SHARED=ON` and use `[DllImport("cgfx")]` on the C# side. All exported functions use the C calling convention.

### How It Works Internally

The CMake configuration manages the defines automatically:

- `-DCGFX_SHARED=ON` defines `CGFX_SHARED` as a **public** compile definition (visible to both cgfx and consumers)
- `CGFX_BUILD_SHARED` is defined as a **private** compile definition (only when compiling cgfx itself)

The `cgfx_export.h` header uses these defines:

```c
#ifdef CGFX_SHARED
#  ifdef CGFX_BUILD_SHARED        // Building the library
#    ifdef _WIN32
#      define CGFX_API __declspec(dllexport)
#    else
#      define CGFX_API __attribute__((visibility("default")))
#    endif
#  else                            // Consuming the library
#    ifdef _WIN32
#      define CGFX_API __declspec(dllimport)
#    else
#      define CGFX_API
#    endif
#  endif
#else                              // Static build
#  define CGFX_API
#endif
```

## CMake Integration

The recommended way to use cgfx in your project is as a CMake subdirectory.

### As a Subdirectory

Add cgfx to your project (e.g., as a Git submodule in `vendor/cgfx`), then in your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.0)
project(my_app LANGUAGES C)

# Add cgfx (brings in GLFW, WebGPU, and glfw3webgpu automatically)
add_subdirectory(vendor/cgfx)

# Your application
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE cgfx)
```

That is all you need. cgfx's CMake configuration uses `PUBLIC` includes and link dependencies, so linking against `cgfx` automatically gives your target:

- Include paths for cgfx headers, cglm, GLFW, and WebGPU
- Link dependencies for GLFW, the WebGPU backend, and glfw3webgpu
- Compile definitions for cglm (`CGLM_FORCE_DEPTH_ZERO_TO_ONE`, `CGLM_FORCE_LEFT_HANDED`)

### Shared Library as a Subdirectory

To build cgfx as a shared library within your project:

```cmake
set(CGFX_SHARED ON CACHE BOOL "Build cgfx as shared library" FORCE)
add_subdirectory(vendor/cgfx)
```

Or pass `-DCGFX_SHARED=ON` on the command line.

## Platform Notes

### Linux

GLFW requires windowing system development headers. Install them for your distribution:

**Debian / Ubuntu:**

```bash
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

**Wayland support (in addition to X11):**

```bash
sudo apt install libwayland-dev libxkbcommon-dev
```

**Fedora:**

```bash
sudo dnf install libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
```

The WebGPU backend (wgpu-native) uses Vulkan on Linux. Most modern GPU drivers include Vulkan support. Verify with:

```bash
vulkaninfo --summary
```

### Windows

MSVC 2022 is the recommended compiler. The CMake configuration automatically sets the C23 standard:

```cmake
if (MSVC)
    set(CMAKE_C_STANDARD 23)
    target_compile_options(cgfx PRIVATE /W4)
endif()
```

On Windows, cgfx also builds the `hwnd3webgpu` vendor library, which enables `cgfx_ctx_init_external()` for embedding the renderer in a native Win32 window (e.g., a WinForms Panel or an editor viewport).

!!! note "MSVC nullptr"
    MSVC's C23 support may not include `nullptr` in all configurations. cgfx provides a fallback define on Windows: `#define nullptr (void*)0`.

### macOS

macOS support is planned via the Metal backend. The WebGPU abstraction layer means no source code changes are needed -- only the backend selection at build time.

## Compiler Requirements

cgfx targets the C23 standard. The following C23 features are used throughout the codebase:

### Compound Literals with Designated Initializers

Used extensively for configuration descriptors:

```c
cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
    .width  = 1920,
    .height = 1080,
    .title  = "My App",
});
```

This pattern avoids temporary variables and makes initialization readable at the call site. Fields not mentioned are zero-initialized, providing sensible defaults.

### Zero-Initialization with `= {}`

Empty braces zero-initialize an entire struct:

```c
CgfxShader shader = cgfx_shader_create(&ctx, "shader", wgsl,
    &(CgfxShaderDesc){});
```

This is the C23 way to express "all defaults." Every cgfx descriptor interprets zero/NULL as a reasonable default value.

### `nullptr`

Used in place of `NULL` or `(void*)0` for null pointer constants, following C23 conventions.

### Compiler Flags

The build system configures compiler flags as follows:

| Compiler | Flags |
|----------|-------|
| **GCC / Clang** | `-std=c23 -Werror -Wall -Wextra -pedantic` |
| **MSVC** | `/std:c23 /W4` |

!!! warning "GCC Version"
    GCC 13 added initial C23 support (`-std=c2x`). GCC 14+ provides more complete C23 support under `-std=c23`. If you encounter issues with GCC 13, consider upgrading to GCC 14 or using Clang 16+.
