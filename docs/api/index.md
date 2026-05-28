# API Reference

cgfx is a minimal C23 rendering engine wrapping WebGPU. It exposes 13 modules through the umbrella header `cgfx.h`.

```c
#include "cgfx.h"  // includes every module below
```

## Modules

| Module | Header | Description |
|--------|--------|-------------|
| [Export](export.md) | `cgfx_export.h` | `CGFX_API` macro for shared library export/import |
| [Context](context.md) | `cgfx_ctx.h` | Window, WebGPU device, queue, and surface management |
| [Shader](shader.md) | `cgfx_shader.h` | WGSL compilation, bind group layouts, pipeline layout |
| [Pipeline](pipeline.md) | `cgfx_pipeline.h` | Render pipeline creation with zero-init defaults |
| [Frame](frame.md) | `cgfx_frame.h` | Per-frame begin/end rendering cycle |
| [Buffer](buffer.md) | `cgfx_buffer.h` | GPU buffer creation (vertex, index, uniform, storage, mapping, generic) |
| [Uniform](uniform.md) | `cgfx_uniform.h` | Uniform buffer + bind group + data pointer bundle |
| [Mesh](mesh.md) | `cgfx_mesh.h` | Vertex format, mesh creation, vertex layout, draw helpers |
| [Texture](texture.md) | `cgfx_texture.h` | GPU texture + view, sampler, depth, cube maps, per-layer writes |
| [Compute](compute.md) | `cgfx_compute.h` | Compute pipeline, compute pass, buffer copy |

| [Loader](loader.md) | `cgfx_loader.h` | Load geometry from LearnWebGPU text format (temporary) |
| [Camera](camera.md) | `cgfx_camera.h` | Projection/view matrices with GPU uniform management |

## Naming conventions

cgfx uses a consistent naming scheme across the entire API:

| Category | Prefix | Example |
|----------|--------|---------|
| Functions | `cgfx_` | `cgfx_ctx_init`, `cgfx_shader_create` |
| Types (structs) | `Cgfx` | `CgfxCtx`, `CgfxShader`, `CgfxPipelineDesc` |
| Macros | `CGFX_` | `CGFX_API`, `CGFX_SHARED` |

## Zero-init convention

All descriptor structs are designed so that a zero-initialized value maps to sensible defaults. Fields set to `0`, `NULL`, or `false` resolve to documented default values during creation.

```c
// Zero-init gives you a 1280x720 window titled "cgfx" with VSync
CgfxCtxDesc desc = {0};

// Override only what you need
CgfxCtxDesc desc = {
    .width  = 1920,
    .height = 1080,
    .title  = "My App",
    .limits = cgfx_default_limits(),
};
```

!!! tip "Limits are an exception"
    The `limits` field in `CgfxCtxDesc` should be initialized with `cgfx_default_limits()`, not left as zero. Setting all limits to `0` requests minimum device limits, which may be too restrictive for your use case.

## Error handling

Functions that can fail return `bool` -- `true` on success, `false` on failure. Error details are printed to `stderr`. There are no error codes or error message accessors.

```c
if (!cgfx_ctx_init(&ctx, &desc)) {
    // error details already printed to stderr
    return 1;
}
```

## Ownership conventions

cgfx follows a simple ownership model: the module that creates a resource provides a matching `_destroy` function for it.

| Resource | Created by | Destroyed by |
|----------|-----------|-------------|
| `CgfxCtx` | `cgfx_ctx_init` / `cgfx_ctx_init_external` | `cgfx_ctx_destroy` |
| `CgfxShader` | `cgfx_shader_create` / `cgfx_shader_create_from_file` | `cgfx_shader_destroy` |
| `CgfxCamera` | `cgfx_camera_create` | `cgfx_camera_destroy` |
| `CgfxUniform` | `cgfx_uniform_create` | `cgfx_uniform_destroy` |
| `CgfxMesh` | `cgfx_mesh_create` | `cgfx_mesh_destroy` |
| `CgfxTexture` | `cgfx_texture_create` | `cgfx_texture_destroy` |
| `CgfxBuffer` | `cgfx_buffer_create_*` | `cgfx_buffer_destroy` |
| `WGPUSampler` | `cgfx_sampler_create` | `wgpuSamplerRelease` (caller-owned) |
| `WGPURenderPipeline` | `cgfx_pipeline_create` | `wgpuRenderPipelineRelease` (caller-owned) |
| `WGPUComputePipeline` | `cgfx_compute_pipeline_create` | `wgpuComputePipelineRelease` (caller-owned) |
| `WGPUBindGroup` | `cgfx_shader_create_bind_group` | `wgpuBindGroupRelease` (caller-owned) |

!!! warning "Bind groups are caller-owned"
    Bind groups created via `cgfx_shader_create_bind_group` are **not** released by `cgfx_shader_destroy`. The caller must release them with `wgpuBindGroupRelease()`. When using `CgfxUniform` or `CgfxCamera`, their `_destroy` functions handle this automatically.

## Transparent structs

All cgfx structs have public fields. You can read and use the raw WebGPU handles directly for advanced usage that cgfx does not wrap:

```c
CgfxCtx ctx;
cgfx_ctx_init(&ctx, &desc);

// Access raw handles directly
WGPUDevice device = ctx.device;
WGPUQueue  queue  = ctx.queue;
```
