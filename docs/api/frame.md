# Frame

Per-frame rendering cycle -- acquire, record, submit, present.

**Header:** `cgfx_frame.h`

---

## Structs

### CgfxFrame

Per-frame rendering state. Created by `cgfx_frame_begin()`, consumed by `cgfx_frame_end()`. The `render_pass` field is the main interaction point -- use it to record draw commands between begin and end.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `encoder` | `WGPUCommandEncoder` | -- | Command encoder for this frame. |
| `render_pass` | `WGPURenderPassEncoder` | -- | Active render pass -- record draws on this. |
| `target_view` | `WGPUTextureView` | -- | Surface texture view being rendered to. |

---

## Functions

### cgfx_frame_begin

Begin a new frame by acquiring the surface texture and starting a render pass.

```c
bool cgfx_frame_begin(const CgfxCtx *ctx, CgfxFrame *frame, WGPUColor clear_color);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `frame` | `CgfxFrame *` | Pointer to caller-allocated `CgfxFrame` (typically on the stack). |
| `clear_color` | `WGPUColor` | Background color to clear the frame with (RGBA, 0.0--1.0). |

**Returns:** `true` if the frame was started successfully. `false` if the surface texture is unavailable (e.g. window minimized) -- skip the frame.

**What it does:**

1. Acquires the next surface texture view from the swap chain.
2. Creates a command encoder (labeled `"cgfx frame encoder"`).
3. Begins a render pass with:
    - The surface texture as the single color attachment.
    - Load operation: clear with the provided `clear_color`.
    - Store operation: store (keep the rendered result).
4. If the context has a depth texture (created via `CgfxCtxDesc.depth_buffer = true`), attaches `ctx->depth_texture.view` as the depth/stencil attachment with a clear value of `1.0`.

!!! warning "Caller-owned event loop"
    The caller **must** call `glfwPollEvents()` (or equivalent) **before** `cgfx_frame_begin`. cgfx never calls windowing event functions.

!!! tip "Handling false returns"
    When `cgfx_frame_begin` returns `false`, skip all rendering for that iteration. Do **not** call `cgfx_frame_end`.

---

### cgfx_frame_begin_encoder

Begin a frame without starting a render pass. Acquires the surface texture and creates a command encoder, but does NOT begin a render pass. Use for mixed compute+render workflows.

```c
CGFX_API bool cgfx_frame_begin_encoder(const CgfxCtx *ctx, CgfxFrame *frame);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `frame` | `CgfxFrame *` | Pointer to caller-allocated `CgfxFrame`. |

**Returns:** `true` if the frame was started (surface texture available). `frame->render_pass` is `NULL` until `cgfx_frame_begin_render_pass` is called.

After calling this, you can run compute passes on `frame->encoder` before starting the render pass.

---

### cgfx_frame_begin_render_pass

Begin the render pass on a frame started with `cgfx_frame_begin_encoder()`.

```c
CGFX_API void cgfx_frame_begin_render_pass(const CgfxCtx *ctx,
                                            CgfxFrame *frame,
                                            WGPUColor clear_color);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `frame` | `CgfxFrame *` | Frame started with `cgfx_frame_begin_encoder()`. |
| `clear_color` | `WGPUColor` | Background color to clear the frame with. |

Sets up the render pass with the surface texture as color attachment and optional depth/stencil attachment.

!!! note "Relationship to cgfx_frame_begin"
    `cgfx_frame_begin` is equivalent to calling `cgfx_frame_begin_encoder` followed by `cgfx_frame_begin_render_pass`. Use the split functions only when you need to insert compute passes before rendering.

---

### cgfx_frame_end

End the current frame, submit commands, and present the surface.

```c
void cgfx_frame_end(const CgfxCtx *ctx, CgfxFrame *frame);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `frame` | `CgfxFrame *` | Frame previously started with `cgfx_frame_begin()`. |

**What it does:**

1. Ends the render pass encoder.
2. Finishes the command encoder, producing a command buffer.
3. Submits the command buffer to the device queue.
4. Releases the render pass encoder, command encoder, and command buffer.
5. Releases the surface texture view.
6. Presents the surface (skipped on Emscripten -- the browser handles it).
7. Backend synchronization:
    - **Dawn:** `wgpuDeviceTick()` -- processes pending work.
    - **wgpu-native:** `wgpuDevicePoll()` -- polls for completed operations.

---

## Usage

### Standard frame loop

```c
while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();

    CgfxFrame frame;
    if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.2, 1.0})) {
        wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
        // bind buffers, uniforms, issue draw calls...
        wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);

        cgfx_frame_end(&ctx, &frame);
    }
}
```

### Frame loop with mesh and uniform

```c
while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();

    CgfxFrame frame;
    if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.05, 0.05, 0.05, 1.0})) {
        wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
        cgfx_shader_bind(frame.render_pass,
                         (WGPUBindGroup[]){ uniform.bind_group }, 1);
        cgfx_mesh_draw(frame.render_pass, &mesh);

        cgfx_frame_end(&ctx, &frame);
    }
}
```

!!! note "Recording draws"
    Between `cgfx_frame_begin` and `cgfx_frame_end`, you record draw commands directly on `frame.render_pass` using raw WebGPU calls. cgfx does not wrap every possible draw/bind operation -- it only hides the encoder/submit/present ceremony.

### Mixed compute + render

Use `cgfx_frame_begin_encoder` to run compute passes before the render pass:

```c
while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();

    CgfxFrame frame;
    if (cgfx_frame_begin_encoder(&ctx, &frame)) {
        // Compute pass first
        CgfxComputePass cp;
        cgfx_compute_pass_begin(frame.encoder, &cp);
        wgpuComputePassEncoderSetPipeline(cp.pass, compute_pipeline);
        cgfx_shader_bind_compute(cp.pass, &compute_bg, 1);
        wgpuComputePassEncoderDispatchWorkgroups(cp.pass, 64, 1, 1);
        cgfx_compute_pass_end(&cp);

        // Then render pass
        cgfx_frame_begin_render_pass(&ctx, &frame,
            (WGPUColor){0.1, 0.1, 0.2, 1.0});
        wgpuRenderPassEncoderSetPipeline(frame.render_pass, render_pipeline);
        wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
        cgfx_frame_end(&ctx, &frame);
    }
}
```

See the [Compute API reference](compute.md) for details on compute passes.
