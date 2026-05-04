cgfx
====

A minimal C23 rendering engine library wrapping WebGPU. Abstracts away the verbose WebGPU boilerplate into a small, simple C API while keeping full access to raw WebGPU handles when needed.

Built on top of [wgpu-native](https://github.com/gfx-rs/wgpu-native) (with optional Dawn backend) and GLFW for windowing.

Building
--------

```
cmake . -B build
cmake --build build
./build/examples/depth_texture
```

For a shared library (`.dll`/`.so`/`.dylib`), e.g. for C# P/Invoke:

```
cmake . -B build -DCGFX_SHARED=ON
cmake --build build
```

Requires CMake 3.0+ and a C23-capable compiler.

Example
-------

```c
#include "cgfx.h"

int main(void) {
    CgfxCtx ctx;
    cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280, .height = 720,
        .title = "hello",
        .depth_buffer = true,
        .limits = cgfx_default_limits(),
    });

    CgfxShader shader = cgfx_shader_create(&ctx, "my shader", wgsl_source,
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 1,
                .bindings = (CgfxBindingDesc[]){{
                    .binding = 0,
                    .min_binding_size = sizeof(MyUniforms),
                }},
            }},
        });

    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
        .depth_test = true,
    });

    CgfxUniform uniform = cgfx_uniform_create(&ctx, &shader, 0,
                                               &my_uniforms, sizeof(MyUniforms));

    while (cgfx_ctx_is_running(&ctx)) {
        cgfx_uniform_write(&ctx, &uniform);

    CgfxShader shader = cgfx_shader_create(&ctx, "my shader", wgsl_source, nullptr);
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){ .shader = &shader });

    while (cgfx_ctx_is_running(&ctx)) {
        glfwPollEvents();
        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){ 0.1, 0.1, 0.2, 1.0 })) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            cgfx_shader_bind(frame.render_pass, &uniform.bind_group, 1);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
            cgfx_frame_end(&ctx, &frame);
        }
    }

    cgfx_uniform_destroy(&uniform);
    wgpuRenderPipelineRelease(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);
}
```

Library flow
------------

```
                           INITIALIZATION
                           ==============

    cgfx_ctx_init(ctx, &desc)
    |
    |   GLFW window
    |   WebGPU instance
    |   Surface (platform-specific)
    |   Adapter (GPU selection)          All WebGPU boilerplate
    |   Device (logical GPU)             hidden in one call
    |   Queue
    |   Surface configuration
    |   Depth texture (optional)
    v
    CgfxCtx { window, device, queue, surface, depth_texture, ... }


    cgfx_shader_create(ctx, label, wgsl, &desc)
    |
    |   Compiles WGSL                         CgfxShader
    |   Builds bind group layouts from  --->  { module, pipeline_layout,
    |   descriptor (or NULL for none)           group_layouts[], group_count }
    v

    cgfx_pipeline_create(ctx, &desc)
    |
    |   Fills vertex/fragment/blend/            Returns raw
    |   primitive/multisample/depth state  ---> WGPURenderPipeline
    |   with sensible defaults from zero-init
    |   Layout read from shader automatically
    v

    cgfx_uniform_create(ctx, &shader, group, data, size)
    |
    |   Creates GPU uniform buffer              CgfxUniform
    |   Creates bind group from shader    --->  { buffer, bind_group,
    |   layout, uploads initial data              data, size }
    v

                           RENDER LOOP
                           ===========

    while (cgfx_ctx_is_running(&ctx))
    |
    +-> glfwPollEvents()  (caller-owned — or platform equivalent)
    |
    +-> cgfx_frame_begin(&ctx, &frame, clear_color)
    |   |
    |   |   Acquire surface texture view
    |   |   Create command encoder
    |   |   Begin render pass (color + optional depth attachment)
    |   v
    |   CgfxFrame { encoder, render_pass, target_view }
    |
    |   === USER DRAW COMMANDS (raw WebGPU) =====
    |   |
    |   |   wgpuRenderPassEncoderSetPipeline(frame.render_pass, ...)
    |   |   cgfx_shader_bind(frame.render_pass, &bind_group, count)
    |   |   cgfx_mesh_draw(frame.render_pass, &mesh)
    |   |
    |   ===========================================
    |
    +-> cgfx_frame_end(&ctx, &frame)
        |
        |   End render pass
        |   Finish encoder -> command buffer
        |   Submit to queue
        |   Present surface
        |   Backend tick/poll


                           CLEANUP
                           =======

    cgfx_uniform_destroy(&uniform)      (release bind group + buffer)
    cgfx_mesh_destroy(&mesh)            (release vertex + index buffers)
    wgpuRenderPipelineRelease(...)      (user owns pipeline lifetime)
    cgfx_shader_destroy(&shader)        (release module + layouts + pipeline layout)
    cgfx_ctx_destroy(&ctx)              (release depth, surface, queue, device, window)
```

Modules
-------

| Module | Header | Description |
|--------|--------|-------------|
| Context | `cgfx_ctx.h` | Window/device/queue/surface init/teardown. `cgfx_ctx_init()` for GLFW, `cgfx_ctx_init_external()` for embedded windows (HWND). Optional depth buffer. |
| Shader | `cgfx_shader.h` | WGSL compilation + bind group layouts + pipeline layout. Shader owns layouts; bind groups are caller-owned. |
| Pipeline | `cgfx_pipeline.h` | Render pipeline with zero-init defaults. Reads layout from CgfxShader. Supports depth testing. |
| Frame | `cgfx_frame.h` | Per-frame begin/end cycle. Handles texture acquisition, command encoding, submission, presentation, and optional depth attachment. Caller polls events. |
| Buffer | `cgfx_buffer.h` | GPU buffer creation (vertex, index, uniform, mapping, generic). |
| Uniform | `cgfx_uniform.h` | Bundles a uniform buffer + bind group + data pointer for per-object uniform data. |
| Mesh | `cgfx_mesh.h` | `CgfxVertex` (position + normal + color + UV, 44 bytes), `CgfxMesh` (vertex + index GPU buffers), vertex layout descriptor, indexed draw. |
| Loader | `cgfx_loader.h` | Load geometry from LearnWebGPU tutorial text format. Temporary — will be replaced by glTF. |
| Primitives | `cgfx_primitives.h` | Geometry generators (plane, triangle, sphere, cube). **Stubbed.** |

### Not yet implemented

| Module | Header | Description |
|--------|--------|-------------|
| Mesh | `cgfx_mesh.h` | `CgfxVertex` (position + normal + UV, 32 bytes), `CgfxMesh` (owns GPU buffers), and `cgfx_mesh_vertex_layout()` for pipeline creation. Vertex layout is implemented; create/destroy are stubbed. |
| Primitives | `cgfx_primitives.h` | Geometry generators: `cgfx_primitives_plane()`, `_triangle()`, `_sphere()`, `_cube()`. Each generates vertices/indices and returns a `CgfxMesh`. |

All stubbed functions have detailed TODO comments describing the exact implementation steps.

### Not yet planned

- Texture loading and samplers
- Camera / transform matrices
- glTF model loading

Design
------

- **Pure C23** -- no C++ required
- **Transparent structs** -- access raw WebGPU handles (e.g. `ctx.device`, `shader.group_layouts[0]`) for anything cgfx doesn't wrap
- **Zero-init defaults** -- `CgfxPipelineDesc desc = { .shader = &s };` gives you working defaults
- **No global state** -- context is passed by pointer
- **Caller-owned event loop** -- cgfx never calls windowing event functions; the caller polls events before `cgfx_frame_begin`
- **Thin wrapper** -- cgfx manages boilerplate, you record draw commands with raw WebGPU calls
- **Shader owns layouts, caller owns bind groups** -- enables same shader with different per-object uniform data

Examples
--------

| Example | Description |
|---------|-------------|
| `triangle` | Minimal triangle, no vertex buffers (procedural vertices in shader) |
| `vertex_attribute` | Triangle with a vertex buffer and `@location(0)` attribute |
| `multiple_attributes` | Indexed quad with position, normal, and color attributes via CgfxMesh |
| `loading_from_file` | Load geometry from the tutorial text format using `cgfx_load_tutorial_mesh()` |
| `playing_with_buffers` | Buffer copy and map-read demonstration |
| `compute` | Buffer operations with a render pipeline |
| `multiple_uniforms` | Two objects sharing one shader with different uniform data via CgfxUniform |
| `depth_texture` | Rotating 3D pyramid with depth testing |
| `external_window` | Render into an externally-owned Win32 HWND (Windows-only, stub on other platforms) |
| `slang_triangle` | Slang shader compiled to WGSL at build time |

Project structure
-----------------

```
cgfx/                    Library (static or shared via -DCGFX_SHARED=ON)
examples/                Example applications (one directory each)
vendor/                  Vendored dependencies (glfw, webgpu, glfw3webgpu, hwnd3webgpu)
```
