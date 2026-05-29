# Compute

Compute pipeline creation and compute pass management.

**Header:** `cgfx_compute.h`

---

## Structs

### CgfxComputeDesc

Configuration for creating a compute pipeline. Zero-initialize for sensible defaults.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `shader` | `const CgfxShader*` | *(required)* | Shader with module and layouts. |
| `entry_point` | `const char*` | `"cs_main"` | Compute shader entry point name. |

---

### CgfxComputePass

Compute pass state. Created by `cgfx_compute_begin()` or `cgfx_compute_pass_begin()`. The `pass` field is the main interaction point -- use it to set pipelines, bind groups, and dispatch workgroups.

| Field | Type | Description |
|-------|------|-------------|
| `encoder` | `WGPUCommandEncoder` | Command encoder for this pass. |
| `pass` | `WGPUComputePassEncoder` | Active compute pass -- dispatch on this. |
| `owns_encoder` | `bool` | `true` for standalone passes (created by `cgfx_compute_begin`). |

---

## Functions

### cgfx_compute_pipeline_create

Create a compute pipeline from a shader.

```c
CGFX_API WGPUComputePipeline cgfx_compute_pipeline_create(
    const CgfxCtx *ctx, const CgfxComputeDesc *desc);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `desc` | `const CgfxComputeDesc*` | Compute pipeline configuration. `shader` is required. |

**Returns:** A `WGPUComputePipeline` handle, or `NULL` on failure. Caller must release with `wgpuComputePipelineRelease()`.

**Example:**

```c
WGPUComputePipeline pipeline = cgfx_compute_pipeline_create(&ctx,
    &(CgfxComputeDesc){ .shader = &shader });

// ... use pipeline ...
wgpuComputePipelineRelease(pipeline);
```

---

### cgfx_compute_begin

Begin a standalone compute pass with its own command encoder.

```c
CGFX_API bool cgfx_compute_begin(const CgfxCtx *ctx, CgfxComputePass *cp);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `cp` | `CgfxComputePass*` | Pointer to caller-allocated compute pass. |

**Returns:** `true` on success.

Creates a command encoder and begins a compute pass. Pair with `cgfx_compute_end()` which ends the pass, submits commands, and releases handles.

---

### cgfx_compute_end

End a standalone compute pass and submit commands.

```c
CGFX_API void cgfx_compute_end(const CgfxCtx *ctx, CgfxComputePass *cp);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `cp` | `CgfxComputePass*` | Compute pass started with `cgfx_compute_begin()`. |

Ends the compute pass, finishes the command encoder, submits the command buffer, releases all handles, and performs backend tick/poll.

---

### cgfx_compute_pass_begin

Begin a compute pass on an existing command encoder.

```c
CGFX_API bool cgfx_compute_pass_begin(WGPUCommandEncoder encoder,
                                       CgfxComputePass *cp);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `encoder` | `WGPUCommandEncoder` | Existing command encoder (e.g., `frame.encoder`). |
| `cp` | `CgfxComputePass*` | Pointer to caller-allocated compute pass. |

**Returns:** `true` on success.

Use for mixed render+compute workflows where the compute pass shares a command encoder with a render pass. Pair with `cgfx_compute_pass_end()` which only ends the pass (does NOT submit or release the encoder).

---

### cgfx_compute_pass_end

End a compute pass without submitting.

```c
CGFX_API void cgfx_compute_pass_end(CgfxComputePass *cp);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `cp` | `CgfxComputePass*` | Compute pass started with `cgfx_compute_pass_begin()`. |

Only ends and releases the compute pass encoder. The command encoder lifetime is the caller's responsibility.

---

## Usage

!!! tip "Buffer copies"
    Use [`cgfx_buffer_copy`](buffer.md#cgfx_buffer_copy) from the [Buffer module](buffer.md) to copy compute results to a mapping buffer for read-back. Storage buffers created with `cgfx_buffer_create_storage` include `CopySrc` usage by default.

### Standalone compute

For compute-only work with no rendering:

```c
// 1. Create shader with storage buffer bindings
CgfxShader shader = cgfx_shader_create(&ctx, "compute", wgsl,
    &(CgfxShaderDesc){
        .group_count = 1,
        .groups = (CgfxGroupDesc[]){{
            .binding_count = 2,
            .bindings = (CgfxBindingDesc[]){
                { .binding = 0, .type = WGPUBufferBindingType_ReadOnlyStorage,
                  .visibility = WGPUShaderStage_Compute },
                { .binding = 1, .type = WGPUBufferBindingType_Storage,
                  .visibility = WGPUShaderStage_Compute },
            },
        }},
    });

// 2. Create pipeline
WGPUComputePipeline pipeline = cgfx_compute_pipeline_create(&ctx,
    &(CgfxComputeDesc){ .shader = &shader });

// 3. Create storage buffers
CgfxBuffer input  = cgfx_buffer_create_storage(&ctx, data, sizeof(data));
CgfxBuffer output = cgfx_buffer_create_storage(&ctx, NULL, sizeof(data));

// 4. Create bind group (reuse existing shader API)
WGPUBindGroup bg = cgfx_shader_create_bind_group(&ctx, &shader, 0,
    (CgfxBuffer[]){ input, output }, 2);

// 5. Dispatch
CgfxComputePass cp;
cgfx_compute_begin(&ctx, &cp);
wgpuComputePassEncoderSetPipeline(cp.pass, pipeline);
cgfx_shader_bind_compute(cp.pass, &bg, 1);
wgpuComputePassEncoderDispatchWorkgroups(cp.pass, 64, 1, 1);
cgfx_compute_end(&ctx, &cp);

// 6. Read back results
CgfxBuffer readback = cgfx_buffer_create_mapping(&ctx, output.size, 0);
cgfx_buffer_copy(&ctx, &output, &readback, 0);
float result[256];
cgfx_buffer_read(&ctx, &readback, result, sizeof(result));
```

!!! note "Visibility must be explicit"
    The default visibility for `CGFX_BINDING_BUFFER` is `Vertex|Fragment`. Compute shader bindings must explicitly set `.visibility = WGPUShaderStage_Compute`.

### Mixed render+compute

Use `cgfx_frame_begin_encoder()` to get a command encoder without starting a render pass, then run compute passes before rendering:

```c
CgfxFrame frame;
if (cgfx_frame_begin_encoder(&ctx, &frame)) {
    // Compute pass (before render)
    CgfxComputePass cp;
    cgfx_compute_pass_begin(frame.encoder, &cp);
    wgpuComputePassEncoderSetPipeline(cp.pass, compute_pipeline);
    cgfx_shader_bind_compute(cp.pass, &compute_bg, 1);
    wgpuComputePassEncoderDispatchWorkgroups(cp.pass, 64, 1, 1);
    cgfx_compute_pass_end(&cp);

    // Render pass (uses compute results)
    cgfx_frame_begin_render_pass(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.2, 1.0});
    wgpuRenderPassEncoderSetPipeline(frame.render_pass, render_pipeline);
    // ... draw commands ...
    cgfx_frame_end(&ctx, &frame);
}
```

!!! warning "Pass ordering"
    WebGPU requires that only one pass encoder is active on a command encoder at a time. Run compute passes before or after the render pass, not during it.

### Read-back pattern

After compute dispatch, copy results to a mapping buffer and read them on the CPU:

```c
// Copy storage buffer to mapping buffer, then read to CPU
CgfxBuffer readback = cgfx_buffer_create_mapping(&ctx, output.size, 0);
cgfx_buffer_copy(&ctx, &output, &readback, 0);

float result[256];
cgfx_buffer_read(&ctx, &readback, result, sizeof(result));
// ... use result ...
cgfx_buffer_destroy(&readback);
```
