# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
cmake . -B build
cmake --build build
./build/examples/triangle
```

## Architecture

This is a C23 rendering engine library (`cgfx`) wrapping WebGPU (wgpu-native v29), with GLFW for windowing. The library is a static lib (`libcgfx.a`) that abstracts WebGPU's verbose boilerplate into a minimal C API. All public symbols use the `cgfx_` prefix.

### Library modules (`cgfx/`)

| Module | Purpose |
|--------|---------|
| `cgfx_ctx` | Context: window + device + queue + surface init/destroy |
| `cgfx_shader` | Shader module creation from WGSL strings or SPIR-V binaries |
| `cgfx_pipeline` | Render pipeline with zero-init defaults, supports single or split shader modules |
| `cgfx_frame` | Per-frame begin/end cycle (acquire texture, encoder, pass, submit, present) |
| `cgfx_buffer` | GPU buffer creation (vertex, index, mapping, generic) |
| `cgfx_mesh` | CgfxVertex (pos+normal+uv) + CgfxMesh + vertex layout (**stubbed**) |
| `cgfx_primitives` | Plane, triangle, sphere, cube generators (**stubbed**) |
| `cgfx_internal.h` | Internal sync wrappers for async WebGPU requests |
| `cgfx.h` | Umbrella header — includes all modules |

### Design conventions

- **Transparent structs** — fields are public so users can access raw WebGPU handles (e.g., `ctx.device`)
- **Context passed by pointer** — no global state
- **Error handling** — functions return `bool`, errors go to stderr
- **Frame recording** — between `cgfx_frame_begin`/`cgfx_frame_end`, user records draw commands directly on `frame.render_pass` using raw WebGPU calls
- **Backend** — wgpu-native only. `WEBGPU_BACKEND_WGPU` is always defined.

### External dependencies (vendored in `vendor/`)

- `glfw` — windowing and input
- `webgpu` — wgpu-native v29.0.0.0, fetched as precompiled binaries via CMake FetchContent
- `glfw3webgpu` — bridges GLFW to WebGPU surface creation
- `slang` — Slang shader compiler (slangc), fetched as precompiled binaries via CMake FetchContent. Provides `cgfx_compile_slang()` to compile `.slang` files to WGSL at build time.

### CMake structure

- Root `CMakeLists.txt` — externals + `add_subdirectory(cgfx)` + `add_subdirectory(examples)`
- `cgfx/CMakeLists.txt` — static library with `PUBLIC` includes and links
- `examples/CMakeLists.txt` — `cgfx_add_example()` helper function, one-liner per example
- `vendor/webgpu/webgpu.cmake` — downloads platform-specific wgpu-native zip, defines `webgpu` imported target and `target_copy_webgpu_binaries()`. Old Elie Michel dependency is commented out at the bottom.
- `vendor/slang/slang.cmake` — downloads platform-specific Slang release, exposes `SLANGC` path and `cgfx_compile_slang()` function

## API Reference

### Context (`cgfx_ctx`)

```c
// Configuration — zero-init for defaults (1280x720, "cgfx", VSync)
typedef struct CgfxCtxDesc {
    uint32_t         width;         // 0 = 1280
    uint32_t         height;        // 0 = 720
    const char      *title;         // NULL = "cgfx"
    bool             resizable;     // default: true
    WGPUPresentMode  present_mode;  // 0 = Fifo (VSync)
    bool             spirv;         // deprecated, ignored (SPIR-V works without special flags)
} CgfxCtxDesc;

// The context — owns window, device, queue, surface
typedef struct CgfxCtx {
    GLFWwindow        *window;
    WGPUDevice         device;
    WGPUQueue          queue;
    WGPUSurface        surface;
    WGPUTextureFormat  surface_format;
    uint32_t           width, height;
} CgfxCtx;

bool cgfx_ctx_init(CgfxCtx *ctx, const CgfxCtxDesc *desc);
bool cgfx_ctx_is_running(const CgfxCtx *ctx);
void cgfx_ctx_destroy(CgfxCtx *ctx);
```

### Shaders (`cgfx_shader`)

```c
// Create shader module from WGSL source string
WGPUShaderModule cgfx_shader_create(const CgfxCtx *ctx,
                                     const char *label,
                                     const char *wgsl);

// Create shader module from pre-compiled SPIR-V binary (via naga SPIR-V frontend).
// Works on all backends. size_bytes must be a multiple of 4.
WGPUShaderModule cgfx_shader_create_spirv(const CgfxCtx *ctx,
                                           const char *label,
                                           const uint32_t *spirv,
                                           size_t size_bytes);
```

### Pipeline (`cgfx_pipeline`)

```c
typedef struct CgfxPipelineDesc {
    // Shader modules — two ways to provide them:
    WGPUShaderModule  shader;          // single module with both VS+FS entry points (WGSL style)
    WGPUShaderModule  vertex_shader;   // optional: separate vertex module (overrides shader)
    WGPUShaderModule  fragment_shader; // optional: separate fragment module (overrides shader)

    const char       *vertex_entry;    // NULL = "vs_main"
    const char       *fragment_entry;  // NULL = "fs_main"
    WGPUPrimitiveTopology topology;    // 0 = TriangleList
    WGPUCullMode      cull_mode;       // 0 = None
    WGPUFrontFace     front_face;      // 0 = CCW
    bool              depth_test;
    WGPUTextureFormat depth_format;    // 0 = Depth24Plus (only if depth_test)
    uint32_t                       vertex_buffer_count;
    const WGPUVertexBufferLayout  *vertex_buffers;
} CgfxPipelineDesc;

WGPURenderPipeline cgfx_pipeline_create(const CgfxCtx *ctx,
                                         const CgfxPipelineDesc *desc);
```

### Frame (`cgfx_frame`)

```c
typedef struct CgfxFrame {
    WGPUTextureView        target_view;
    WGPUCommandEncoder     encoder;
    WGPURenderPassEncoder  render_pass;
} CgfxFrame;

bool cgfx_frame_begin(const CgfxCtx *ctx, CgfxFrame *frame, WGPUColor clear_color);
void cgfx_frame_end(const CgfxCtx *ctx, CgfxFrame *frame);
```

### Buffers (`cgfx_buffer`)

```c
typedef struct CgfxBuffer {
    WGPUBuffer buffer;
    uint64_t   size;
    uint32_t   count;
    bool       ready;   // for async map operations
} CgfxBuffer;

CgfxBuffer cgfx_buffer_create_vertex(const CgfxCtx *ctx, const void *data,
                                      uint64_t data_size, uint32_t count);
CgfxBuffer cgfx_buffer_create_index(const CgfxCtx *ctx, const uint32_t *indices,
                                     uint32_t count);
CgfxBuffer cgfx_buffer_create_mapping(const CgfxCtx *ctx, const void *data,
                                       uint64_t data_size, uint32_t count);
CgfxBuffer cgfx_buffer_create(const CgfxCtx *ctx, WGPUBufferUsage usage,
                               const void *data, uint64_t size);
void cgfx_buffer_destroy(CgfxBuffer *buf);
```

## Examples

### `examples/triangle/` — Minimal WGSL triangle

The simplest cgfx application. Inline WGSL shader, procedural vertices, render loop.

```c
#include "cgfx.h"

int main(void) {
    CgfxCtx ctx;
    cgfx_ctx_init(&ctx, &(CgfxCtxDesc){ .width = 1920, .height = 1080, .title = "triangle" });

    WGPUShaderModule shader = cgfx_shader_create(&ctx, "triangle", wgsl_source);
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = shader,
    });
    wgpuShaderModuleRelease(shader);

    while (cgfx_ctx_is_running(&ctx)) {
        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){ 0.1, 0.1, 0.2, 1.0 })) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
            cgfx_frame_end(&ctx, &frame);
        }
    }

    wgpuRenderPipelineRelease(pipeline);
    cgfx_ctx_destroy(&ctx);
}
```

### `examples/playing_with_buffers/` — Buffer copy + map-read

Creates a vertex buffer, copies it to a mappable buffer via command encoder, then reads the data back with `wgpuBufferMapAsync` to verify the GPU-side copy.

### `examples/compute/` — Minimal compute-less triangle

Same as the triangle example but structured for future compute pipeline work.

### `examples/vertex_attribute/` — Buffer operations

Same buffer copy/map pattern as `playing_with_buffers/`.

### `examples/slang_triangle/` — Slang triangle (Slang → WGSL → wgpu)

Demonstrates the Slang shader workflow. The shader (`triangle.slang`) is compiled to WGSL at build time by `slangc` via `cgfx_compile_slang()`, then loaded at runtime as a text file and passed to `cgfx_shader_create()`. Both vertex and fragment entry points compile into a single `.wgsl` file.

### Slang workflow

For using Slang instead of hand-written WGSL:

```c
#include "cgfx.h"

int main(void) {
    CgfxCtx ctx;
    cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1920, .height = 1080,
    });

    // Load .wgsl file generated from .slang at build time
    char *wgsl = load_text_file("shader.wgsl");
    WGPUShaderModule shader = cgfx_shader_create(&ctx, "slang shader", wgsl);
    free(wgsl);

    // Entry point names match the Slang source
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader         = shader,
        .vertex_entry   = "vertexMain",
        .fragment_entry = "fragmentMain",
    });

    wgpuShaderModuleRelease(shader);

    // ... render loop same as WGSL examples ...

    wgpuRenderPipelineRelease(pipeline);
    cgfx_ctx_destroy(&ctx);
}
```

## CMake Functions

### `cgfx_compile_slang()`

Compiles a `.slang` shader file to WGSL at build time using `slangc`. Supports multiple entry points in a single output file. Generates a custom command whose output can be added as a target source.

```cmake
cgfx_compile_slang(
    SOURCE   path/to/shader.slang                        # required
    ENTRIES  "vertexMain vertex" "fragmentMain fragment"  # required: list of "entry stage" pairs
    OUTPUT   ${CMAKE_CURRENT_BINARY_DIR}/shader.wgsl     # required
)
```

Example (from `examples/CMakeLists.txt`):

```cmake
cgfx_compile_slang(
    SOURCE  "${SLANG_SHADER}"
    ENTRIES "vertexMain vertex" "fragmentMain fragment"
    OUTPUT  "${WGSL_DIR}/triangle.wgsl"
)

cgfx_add_example(slang_triangle slang_triangle/main.c)
target_sources(slang_triangle PRIVATE "${WGSL_DIR}/triangle.wgsl")
```

## WebGPU API Version

This project uses **wgpu-native v29.0.0.0** which implements a significantly newer WebGPU C API than Elie Michel's tutorial (which targets wgpu v0.19.4.1). See `TRANSLATION.md` for a comprehensive mapping of all API differences when following the tutorial.

Key differences from the tutorial code:
- All `char*` labels/strings are now `WGPUStringView` (use `.data` + `.length = WGPU_STRLEN`)
- Async callbacks use `CallbackInfo` structs instead of separate callback+userdata args
- `wgpuSurfaceGetPreferredFormat()` replaced by `wgpuSurfaceGetCapabilities()`
- Error/device-lost callbacks set via `WGPUDeviceDescriptor` fields, not setter functions
- Surface creation structs renamed: `WGPUSurfaceDescriptorFrom*` → `WGPUSurfaceSource*`
- `WGPUShaderModuleWGSLDescriptor` → `WGPUShaderSourceWGSL`
