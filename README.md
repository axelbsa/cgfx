cgfx
====

A minimal C23 rendering engine library wrapping WebGPU. Abstracts away the verbose WebGPU boilerplate into a small, simple C API while keeping full access to raw WebGPU handles when needed.

Built on top of [wgpu-native](https://github.com/gfx-rs/wgpu-native) (with optional Dawn backend) and GLFW for windowing.

Building
--------

```
cmake . -B build
cmake --build build
./build/examples/triangle
```

Requires CMake 3.0+ and a C23-capable compiler.

Example
-------

```c
#include "cgfx.h"

int main(void) {
    CgfxCtx ctx;
    cgfx_ctx_init(&ctx, &(CgfxCtxDesc){ .width = 1920, .height = 1080, .title = "hello" });

    WGPUShaderModule shader = cgfx_shader_create(&ctx, "my shader", wgsl_source);
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){ .shader = shader });
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

Library flow
------------

```
                           INITIALIZATION
                           ==============

    cgfx_ctx_init()
    |
    |   GLFW window
    |   WebGPU instance
    |   Surface (platform-specific)
    |   Adapter (GPU selection)          All WebGPU boilerplate
    |   Device (logical GPU)             hidden in one call
    |   Queue
    |   Surface configuration
    v
    CgfxCtx { window, device, queue, surface, surface_format }


    cgfx_shader_create(ctx, label, wgsl_source)
    |
    |   Hides chained-struct descriptor         Returns raw
    |   pattern + backend-specific fields  ---> WGPUShaderModule
    v

    cgfx_pipeline_create(ctx, &desc)
    |
    |   Fills vertex/fragment/blend/            Returns raw
    |   primitive/multisample state with   ---> WGPURenderPipeline
    |   sensible defaults from zero-init
    v

                           RENDER LOOP
                           ===========

    while (cgfx_ctx_is_running(&ctx))
    |
    +-> cgfx_frame_begin(&ctx, &frame, clear_color)
    |   |
    |   |   Poll events
    |   |   Acquire surface texture view
    |   |   Create command encoder
    |   |   Begin render pass (clear color attachment)
    |   v
    |   CgfxFrame { encoder, render_pass, target_view }
    |
    |   === USER DRAW COMMANDS (raw WebGPU) =====
    |   |
    |   |   wgpuRenderPassEncoderSetPipeline(frame.render_pass, ...)
    |   |   wgpuRenderPassEncoderSetVertexBuffer(...)
    |   |   wgpuRenderPassEncoderSetIndexBuffer(...)
    |   |   wgpuRenderPassEncoderDrawIndexed(...)
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


                           GEOMETRY (stubbed)
                           ==================

    cgfx_buffer_create_vertex/index(ctx, data, size)
    |                                                   CgfxBuffer
    v                                                   { WGPUBuffer, size, count }

    cgfx_mesh_create(ctx, vertices, indices)
    |                                                   CgfxMesh
    |   Uses cgfx_buffer internally                     { vertex_buffer, index_buffer,
    v   Vertex format: pos(vec3) + normal(vec3) + uv(vec2) = 32 bytes    index_count }

    cgfx_primitives_plane/triangle/sphere/cube(ctx, ...)
    |
    |   Generates CPU vertex/index data                 Returns ready-to-render
    |   Uploads via cgfx_mesh_create()             ---> CgfxMesh
    v


                           CLEANUP
                           =======

    cgfx_mesh_destroy(&mesh)        (release vertex + index buffers)
    wgpuRenderPipelineRelease(...)  (user owns pipeline lifetime)
    cgfx_ctx_destroy(&ctx)          (release queue, surface, device, window, GLFW)
```

Modules
-------

### Implemented

| Module | Header | Description |
|--------|--------|-------------|
| Context | `cgfx_ctx.h` | Window creation, WebGPU device/queue/surface init and teardown. Wraps the entire initialization sequence into `cgfx_ctx_init()`. |
| Shader | `cgfx_shader.h` | Creates `WGPUShaderModule` from WGSL strings. Hides the chained-struct extension pattern and backend differences. |
| Pipeline | `cgfx_pipeline.h` | Creates `WGPURenderPipeline` with sensible zero-init defaults (triangle list, alpha blend, no culling, no depth, auto layout). |
| Frame | `cgfx_frame.h` | Per-frame begin/end cycle. Handles texture acquisition, command encoding, submission, presentation, and backend-specific tick/poll. |

### Stubbed (TODO)

| Module | Header | Description |
|--------|--------|-------------|
| Buffer | `cgfx_buffer.h` | GPU vertex and index buffer creation via `wgpuDeviceCreateBuffer` + `wgpuQueueWriteBuffer`. |
| Mesh | `cgfx_mesh.h` | `CgfxVertex` (position + normal + UV, 32 bytes), `CgfxMesh` (owns GPU buffers), and `cgfx_mesh_vertex_layout()` for pipeline creation. Vertex layout is implemented; create/destroy are stubbed. |
| Primitives | `cgfx_primitives.h` | Geometry generators: `cgfx_primitives_plane()`, `_triangle()`, `_sphere()`, `_cube()`. Each generates vertices/indices and returns a `CgfxMesh`. |

All stubbed functions have detailed TODO comments describing the exact implementation steps.

### Not yet planned

- Uniform/storage buffers and bind groups
- Texture loading and samplers
- Depth buffer management
- Camera / transform matrices
- Scene graph
- Compute pipelines

Design
------

- **Pure C23** -- no C++ required
- **Transparent structs** -- access raw WebGPU handles (e.g. `ctx.device`) for anything cgfx doesn't wrap
- **Zero-init defaults** -- `CgfxPipelineDesc desc = { .shader = s };` gives you working defaults
- **No global state** -- context is passed by pointer
- **Thin wrapper** -- cgfx manages boilerplate, you record draw commands with raw WebGPU calls

Project structure
-----------------

```
cgfx/                    Static library (libcgfx.a)
examples/triangle/       Triangle demo using the cgfx API
vendor/                  Vendored dependencies (glfw, webgpu, glfw3webgpu)
```
