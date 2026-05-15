# Pipeline

Render pipeline creation with zero-init defaults.

**Header:** `cgfx_pipeline.h`

---

## Structs

### CgfxPipelineDesc

Configuration for creating a render pipeline. Zero-initialize for sensible defaults -- the only required field is `shader`.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `shader` | `const CgfxShader*` | *(required)* | Shader with module and layouts. |
| `vertex_entry` | `const char*` | `"vs_main"` | Vertex shader entry point name. |
| `fragment_entry` | `const char*` | `"fs_main"` | Fragment shader entry point name. |
| `topology` | `WGPUPrimitiveTopology` | `TriangleList` | Primitive topology. |
| `cull_mode` | `WGPUCullMode` | `None` | Face culling mode. |
| `front_face` | `WGPUFrontFace` | `CCW` | Front face winding order. |
| `depth_test` | `bool` | `false` | Enable depth/stencil testing. |
| `depth_format` | `WGPUTextureFormat` | `Depth24Plus` | Depth texture format (only used when `depth_test` is `true`). |
| `vertex_buffer_count` | `uint32_t` | `0` | Number of vertex buffer layouts. |
| `vertex_layouts` | `const WGPUVertexBufferLayout*` | `NULL` | Vertex buffer layout descriptors. |

---

## Functions

### cgfx_pipeline_create

Creates a render pipeline with sensible defaults filled in from the descriptor.

```c
CGFX_API WGPURenderPipeline cgfx_pipeline_create(const CgfxCtx *ctx,
                                                   const CgfxPipelineDesc *desc);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context (uses `ctx->device` and `ctx->surface_format`). |
| `desc` | `const CgfxPipelineDesc*` | Pipeline configuration. |

**Returns:** A `WGPURenderPipeline` handle, or `NULL` on failure.

**Pipeline configuration details:**

- **Layout:** Read from `shader->pipeline_layout`. When the shader has no bind groups, this is `NULL` and WebGPU uses automatic layout inference.
- **Alpha blending:** Always enabled with `srcAlpha` / `oneMinusSrcAlpha` for color channels, `zero` / `one` for alpha.
- **Color target:** Single target matching `ctx->surface_format`, all channels writable.
- **Multisample:** 1 sample per pixel, full mask, no alpha-to-coverage.
- **Depth testing:** Opt-in via `depth_test = true`. Uses less-than comparison with depth write enabled.

!!! note "Caller-owned pipeline"
    `cgfx_pipeline_create` returns a raw `WGPURenderPipeline`. Release it with `wgpuRenderPipelineRelease()` when done. There is no `cgfx_pipeline_destroy` function.

---

### Example: Minimal pipeline (no vertex buffers)

Vertices are generated procedurally in the shader using `@builtin(vertex_index)`.

```c
CgfxShader shader = cgfx_shader_create(&ctx, "triangle", wgsl, nullptr);

WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader = &shader,
});

// Render loop
while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();
    CgfxFrame frame;
    if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.2, 1.0})) {
        wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
        wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
        cgfx_frame_end(&ctx, &frame);
    }
}

// Cleanup
wgpuRenderPipelineRelease(pipeline);
cgfx_shader_destroy(&shader);
```

---

### Example: Pipeline with depth testing

Enable depth testing for 3D scenes with overlapping geometry. Requires the context to have a depth buffer (`depth_buffer = true` in `CgfxCtxDesc`).

```c
CgfxCtx ctx;
cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
    .width        = 1280,
    .height       = 720,
    .depth_buffer = true,
    .limits       = cgfx_default_limits(),
});

WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader     = &shader,
    .depth_test = true,
    .cull_mode  = WGPUCullMode_Back,
});

wgpuRenderPipelineRelease(pipeline);
```

!!! warning "Depth buffer must be enabled on the context"
    Setting `depth_test = true` on the pipeline without creating a depth buffer on the context (`CgfxCtxDesc.depth_buffer = true`) will result in a validation error from WebGPU.

---

### Example: Pipeline with vertex buffers

Use `cgfx_mesh_vertex_layout()` to get the layout matching `CgfxVertex`, or provide a custom layout for your own vertex format.

```c
WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();

WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader              = &shader,
    .vertex_buffer_count = 1,
    .vertex_layouts      = &layout,
    .depth_test          = true,
    .cull_mode           = WGPUCullMode_Back,
});

// Render a mesh
CgfxFrame frame;
if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.2, 1.0})) {
    wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
    cgfx_mesh_draw(frame.render_pass, &mesh);
    cgfx_frame_end(&ctx, &frame);
}

wgpuRenderPipelineRelease(pipeline);
```

!!! tip "Custom vertex formats"
    If you use a vertex format other than `CgfxVertex`, build your own `WGPUVertexBufferLayout` with the appropriate attributes and stride, and pass it via `vertex_layouts`.
