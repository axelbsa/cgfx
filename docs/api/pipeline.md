# Pipeline

Render pipeline creation with zero-init defaults.

**Header:** `cgfx_pipeline.h`

---

## Structs

### CgfxColorTarget

Description of a single color render target (one fragment `@location` output). Zero-initialize for the common case: a target at the surface format, opaque (no blending), writing all channels.

```c
typedef struct CgfxColorTarget {
    WGPUTextureFormat   format;       // 0 = ctx->surface_format
    bool                blend_enable; // false = opaque
    WGPUBlendState      blend;        // used only when blend_enable is true
    WGPUColorWriteMask  write_mask;   // 0 = All
} CgfxColorTarget;
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `format` | `WGPUTextureFormat` | `ctx->surface_format` | Target texture format. `0` uses the context's surface format. |
| `blend_enable` | `bool` | `false` | `false` = opaque (no blend state attached). |
| `blend` | `WGPUBlendState` | *(unused)* | Blend state. Only used when `blend_enable` is `true`. |
| `write_mask` | `WGPUColorWriteMask` | `All` | Channel write mask. `0` is treated as `All`, not "write nothing". |

---

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
| `depth_compare` | `WGPUCompareFunction` | `Less` | Depth compare function (only used when `depth_test` is `true`). |
| `depth_write_disabled` | `bool` | `false` | `true` = depth test without writing depth. `false` = depth writes enabled. |
| `sample_count` | `uint32_t` | `1` | MSAA sample count. `0` is treated as `1`. |
| `alpha_to_coverage` | `bool` | `false` | Enable alpha-to-coverage. |
| `vertex_buffer_count` | `uint32_t` | `0` | Number of vertex buffer layouts. |
| `vertex_layouts` | `const WGPUVertexBufferLayout*` | `NULL` | Vertex buffer layout descriptors. |
| `color_target_count` | `uint32_t` | `0` | Number of color targets. `0` = single opaque target at `ctx->surface_format`. |
| `color_targets` | `const CgfxColorTarget*` | `NULL` | Color target array for per-target blend, offscreen formats, or MRT. |

---

## Functions

### cgfx_blend_alpha

Returns a `WGPUBlendState` for straight alpha blending (`SrcAlpha` / `OneMinusSrcAlpha`).

```c
CGFX_API WGPUBlendState cgfx_blend_alpha(void);
```

---

### cgfx_blend_additive

Returns a `WGPUBlendState` for additive blending (`One` / `One`).

```c
CGFX_API WGPUBlendState cgfx_blend_additive(void);
```

---

### cgfx_blend_premultiplied

Returns a `WGPUBlendState` for premultiplied alpha blending (`One` / `OneMinusSrcAlpha`).

```c
CGFX_API WGPUBlendState cgfx_blend_premultiplied(void);
```

!!! tip "Using blend presets"
    Pass these as the `blend` field of a `CgfxColorTarget` with `blend_enable = true`:

    ```c
    (CgfxColorTarget){ .blend_enable = true, .blend = cgfx_blend_alpha() }
    ```

---

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
- **Color targets:** Opaque by default (no blending). When `color_target_count` is `0`, the pipeline uses a single opaque target at `ctx->surface_format`. Supply a `CgfxColorTarget` array for per-target blend, offscreen formats, or multiple render targets (MRT).
- **Strip index format:** Auto-derived (`Uint32` for strip topologies, `Undefined` otherwise).
- **Multisample:** Configurable via `sample_count` (default `1`, no MSAA), full mask, optional `alpha_to_coverage`.
- **Depth testing:** Opt-in via `depth_test = true`. Uses `depth_compare` (default `Less`) with depth writes enabled. Set `depth_write_disabled = true` for depth testing without writing.

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

---

### Example: Pipeline with alpha blending

Enable straight alpha blending for transparent geometry. Use `CgfxColorTarget` with a blend preset.

```c
CgfxColorTarget target = {
    .blend_enable = true,
    .blend        = cgfx_blend_alpha(),
};

WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader             = &shader,
    .color_target_count = 1,
    .color_targets      = &target,
});

wgpuRenderPipelineRelease(pipeline);
```

!!! note "Other blend modes"
    Use `cgfx_blend_additive()` for additive blending (e.g., particles, glow effects) or `cgfx_blend_premultiplied()` for premultiplied alpha (e.g., compositing pre-blended textures).
