# Compute Pipeline & Render-to-Texture

A guide to using compute shaders in cgfx — standalone data-parallel work, mixed compute+render workflows, and writing to textures from the GPU.

## The core insight

**Compute and render are two independent pass types that share one command encoder.** A compute pass writes data (to buffers or textures), a render pass reads it and draws to the screen. Because they share the same encoder, WebGPU guarantees that all writes from the compute pass are visible to the render pass — no manual barriers, no synchronization primitives, no fences.

This is the key architectural decision: instead of submitting compute and render as separate command buffers (which would require explicit synchronization), cgfx's two-phase frame begin lets you record both passes on the same encoder. The GPU executes them in order, and the data just flows.

```
┌─────────────────────────────────────────────────────────────┐
│                  One Command Encoder                        │
│                                                             │
│   ┌─────────────┐         ┌──────────────┐                  │
│   │ Compute Pass│────────►│ Render Pass  │                  │
│   │             │  data   │              │                  │
│   │ write to    │  flows  │ sample from  │                  │
│   │ storage tex │  ────►  │ same texture │                  │
│   └─────────────┘         └──────────────┘                  │
│                                                             │
│   cgfx_frame_begin_encoder()        cgfx_frame_end()        │
│   ◄─── encoder created ───  ──── encoder submitted ───►     │
└─────────────────────────────────────────────────────────────┘
```

## Two usage patterns

cgfx supports two distinct compute patterns through the same `CgfxComputePass` struct. The `owns_encoder` flag tracks which pattern is active, and the begin/end functions handle the lifecycle accordingly.

### Standalone compute

For compute-only work — no rendering, no window needed (though cgfx still requires a context with `width=1, height=1`). The compute pass creates its own command encoder, submits on end.

```c
CgfxComputePass cp;
cgfx_compute_begin(&ctx, &cp);       // creates encoder, begins pass
// ... set pipeline, bind groups, dispatch ...
cgfx_compute_end(&ctx, &cp);         // ends pass, submits, releases
```

`cgfx_compute_end` does everything: end the pass, finish the encoder, submit the command buffer, release handles, and run the backend tick/poll. One function, done.

### Mixed compute+render

For the render-to-texture pattern — compute writes data, then render displays it. The compute pass borrows the frame's encoder. This is where the two-phase frame begin comes in.

```c
CgfxFrame frame;
if (cgfx_frame_begin_encoder(&ctx, &frame)) {      // phase 1: encoder only
    CgfxComputePass cp;
    cgfx_compute_pass_begin(frame.encoder, &cp);    // borrows encoder
    // ... dispatch ...
    cgfx_compute_pass_end(&cp);                     // ends pass only

    cgfx_frame_begin_render_pass(&ctx, &frame, bg); // phase 2: render pass
    // ... draw ...
    cgfx_frame_end(&ctx, &frame);                   // submits everything
}
```

`cgfx_compute_pass_end` only ends the compute pass — it does NOT submit or release the encoder. The encoder stays open for the render pass, and `cgfx_frame_end` submits both passes together as one command buffer.

### Why two patterns instead of one?

A unified API that always borrows an encoder would force standalone compute users to create and manage their own encoder — the exact boilerplate cgfx exists to eliminate. Conversely, a standalone-only API would prevent sharing an encoder with render, requiring explicit synchronization between separate submissions.

The `owns_encoder` flag resolves this: same struct, same dispatch code in between, different lifecycle at the boundaries.

## The two-phase frame begin

The standard `cgfx_frame_begin` acquires the surface texture, creates an encoder, AND begins the render pass — all in one call. That is convenient for pure rendering, but it means you cannot insert a compute pass before the render pass because a command encoder can only have one active pass at a time.

The two-phase split breaks this apart:

| Function | What it does | When to use |
|----------|-------------|-------------|
| `cgfx_frame_begin` | encoder + render pass | Pure rendering (unchanged, backward compatible) |
| `cgfx_frame_begin_encoder` | encoder only | Mixed compute+render (insert passes before rendering) |
| `cgfx_frame_begin_render_pass` | render pass on existing encoder | Called after your compute passes are done |

`cgfx_frame_begin` is now just a convenience wrapper that calls both phases internally. Existing code is unaffected.

!!! warning "One active pass at a time"
    WebGPU requires that only one pass encoder is active on a command encoder at any moment. You must end the compute pass before beginning the render pass (or vice versa). The passes execute in the order you record them.

## Storage textures: the bridge between compute and render

A storage texture is a GPU texture that compute shaders can write to directly via `textureStore()`. The same texture can also be sampled in a render pass via `textureSample()` — the trick is setting up the right usage flags at creation.

```c
CgfxTexture tex = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
    .width = 512, .height = 512,
    .format = WGPUTextureFormat_RGBA8Unorm,
    .usage = WGPUTextureUsage_StorageBinding    // compute can write
           | WGPUTextureUsage_TextureBinding,   // render can sample
});
```

This single texture appears in TWO bind groups with different roles:

```
┌─────────────────────────────────┐
│  CgfxTexture (one GPU texture)  │
│  usage: StorageBinding          │
│       | TextureBinding          │
└──────────┬──────────┬───────────┘
           │          │
    ┌──────▼──────┐  ┌▼──────────────┐
    │ Compute BG  │  │  Render BG    │
    │ binding 0:  │  │  binding 0:   │
    │  STORAGE_   │  │   TEXTURE     │
    │  TEXTURE    │  │  (sampled)    │
    │  (write)    │  │  binding 1:   │
    │ binding 1:  │  │   SAMPLER     │
    │  BUFFER     │  │               │
    │  (uniform)  │  │               │
    └─────────────┘  └───────────────┘
```

The compute shader sees it as `var output : texture_storage_2d<rgba8unorm, write>` and writes pixels with `textureStore`. The fragment shader sees it as `var tex : texture_2d<f32>` and reads pixels with `textureSample`. Same GPU memory, different access patterns, governed entirely by how the bind groups are set up.

### Why not use a buffer?

You could compute into a storage buffer and copy to a texture, but storage textures skip the copy entirely — the compute shader writes pixels in place, and the render shader samples them directly. For image-producing compute work (procedural textures, post-processing, simulation visualization), this is the natural choice.

Storage buffers remain the right tool for structured data (arrays of structs, counters, indirect draw arguments) where texel addressing doesn't apply.

## Bind groups for compute

Compute shader bindings use the same `CgfxShaderDesc` / `CgfxBindingDesc` / `CgfxBindGroupEntry` system as render shaders, with two differences you need to be aware of.

### Visibility must be explicit

The default visibility for `CGFX_BINDING_BUFFER` is `Vertex|Fragment`. The default for `CGFX_BINDING_TEXTURE` and `CGFX_BINDING_SAMPLER` is `Fragment`. For compute shader bindings, you must explicitly set `.visibility = WGPUShaderStage_Compute`:

```c
CgfxShader compute_shader = cgfx_shader_create(&ctx, "compute", wgsl,
    &(CgfxShaderDesc){
        .group_count = 1,
        .groups = (CgfxGroupDesc[]){{
            .binding_count = 2,
            .bindings = (CgfxBindingDesc[]){
                { .binding = 0, .kind = CGFX_BINDING_STORAGE_TEXTURE,
                  .storage_format = WGPUTextureFormat_RGBA8Unorm,
                  .visibility = WGPUShaderStage_Compute },   // <-- required
                { .binding = 1,
                  .min_binding_size = sizeof(Params),
                  .visibility = WGPUShaderStage_Compute },   // <-- required
            },
        }},
    });
```

!!! note "Why not auto-detect?"
    We could infer compute visibility from the binding kind or shader type, but that introduces fragile heuristics. A buffer might be visible to both compute and vertex stages. Explicit visibility is one extra field per binding and avoids a class of subtle bugs.

The one exception is `CGFX_BINDING_STORAGE_TEXTURE` — its default visibility is already `Compute`, since storage textures are almost exclusively a compute-shader feature. But being explicit doesn't hurt, and makes intent clear.

### Mixed bind groups with cgfx_bind_group_create

When a bind group contains different resource types (textures, buffers, samplers), use `cgfx_bind_group_create` with `CgfxBindGroupEntry` instead of the buffer-only `cgfx_shader_create_bind_group`:

```c
WGPUBindGroup compute_bg = cgfx_bind_group_create(&ctx, &compute_shader, 0,
    (CgfxBindGroupEntry[]){
        { .binding = 0, .texture = &tex },       // storage texture
        { .binding = 1, .buffer = &params_buf },  // uniform buffer
    }, 2);
```

Set exactly one of `.buffer`, `.texture`, or `.sampler` per entry. The binding index must match what you declared in `CgfxBindingDesc`.

### Binding to a compute pass

Use `cgfx_shader_bind_compute` instead of `cgfx_shader_bind`:

```c
// Render pass uses:
cgfx_shader_bind(frame.render_pass, &bg, 1);

// Compute pass uses:
cgfx_shader_bind_compute(cp.pass, &bg, 1);
```

Both are thin wrappers around `wgpuXxxPassEncoderSetBindGroup` — the only difference is the encoder type.

## Compute pipelines

A compute pipeline is simpler than a render pipeline — no vertex layouts, no blend state, no depth testing. Just a shader and an entry point:

```c
WGPUComputePipeline pipeline = cgfx_compute_pipeline_create(&ctx,
    &(CgfxComputeDesc){ .shader = &compute_shader });
```

The entry point defaults to `"cs_main"` (matching cgfx's `"vs_main"` / `"fs_main"` convention). Override with `.entry_point = "my_kernel"` if needed.

The returned `WGPUComputePipeline` is caller-owned — release with `wgpuComputePipelineRelease()`.

### Pipeline layout comes from the shader

Like render pipelines, the compute pipeline reads its layout from `shader->pipeline_layout`. This means the shader descriptor defines what bind groups the pipeline expects. If the descriptor is `NULL` (no bindings), WebGPU uses automatic layout inference.

## Complete example: compute-to-texture

This example computes an animated Julia set fractal on the GPU and displays it. It demonstrates every concept in this guide: storage textures, mixed compute+render, two-phase frame begin, and mixed bind groups.

### Setup

```c
#include "cgfx.h"

typedef struct {
    float time;
    float _pad[3];  // uniform buffers require 16-byte alignment
} Params;

int main(void) {
    CgfxCtx ctx;
    cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 800, .height = 600,
        .title = "Compute Texture",
        .limits = cgfx_default_limits(),
    });
```

### Create the shared texture

One texture, dual purpose — writable by compute, sampleable by render:

```c
    CgfxTexture tex = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
        .width = 512, .height = 512,
        .format = WGPUTextureFormat_RGBA8Unorm,
        .usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding,
    });
```

### Create the compute shader

The compute shader writes a Julia set fractal to the storage texture. Each thread handles one pixel. The `c` parameter animates over time:

```c
    static const char *compute_wgsl =
        "struct Params { time : f32 }\n"
        "@group(0) @binding(0) var output : texture_storage_2d<rgba8unorm, write>;\n"
        "@group(0) @binding(1) var<uniform> params : Params;\n"
        "\n"
        "@compute @workgroup_size(8, 8)\n"
        "fn cs_main(@builtin(global_invocation_id) id : vec3u) {\n"
        "    let dims = textureDimensions(output);\n"
        "    if (id.x >= dims.x || id.y >= dims.y) { return; }\n"
        "\n"
        "    let uv = (vec2f(id.xy) / vec2f(dims)) * 2.0 - 1.0;\n"
        "    var z = uv * 1.5;\n"
        "    let c = vec2f(\n"
        "        sin(params.time * 0.3) * 0.7885,\n"
        "        cos(params.time * 0.23) * 0.7885\n"
        "    );\n"
        "    // ... iterate z = z² + c, color by iteration count ...\n"
        "    textureStore(output, id.xy, color);\n"
        "}\n";
```

The descriptor declares a storage texture binding and a uniform buffer, both with compute visibility:

```c
    CgfxShader compute_shader = cgfx_shader_create(&ctx, "julia_compute", compute_wgsl,
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 2,
                .bindings = (CgfxBindingDesc[]){
                    { .binding = 0, .kind = CGFX_BINDING_STORAGE_TEXTURE,
                      .storage_format = WGPUTextureFormat_RGBA8Unorm,
                      .visibility = WGPUShaderStage_Compute },
                    { .binding = 1,
                      .min_binding_size = sizeof(Params),
                      .visibility = WGPUShaderStage_Compute },
                },
            }},
        });
```

### Create the render shader

The render shader draws a fullscreen quad and samples the computed texture. No vertex buffer needed — positions and UVs are generated procedurally from `vertex_index`:

```c
    static const char *render_wgsl =
        "struct VsOut {\n"
        "    @builtin(position) pos : vec4f,\n"
        "    @location(0) uv : vec2f,\n"
        "}\n"
        "@vertex fn vs_main(@builtin(vertex_index) vi : u32) -> VsOut {\n"
        "    // 6 vertices = 2 triangles covering [-1,1] in clip space\n"
        "    var positions = array<vec2f, 6>(\n"
        "        vec2f(-1, -1), vec2f(1, -1), vec2f(-1, 1),\n"
        "        vec2f(-1,  1), vec2f(1, -1), vec2f( 1, 1),\n"
        "    );\n"
        "    // ...\n"
        "}\n"
        "@group(0) @binding(0) var tex : texture_2d<f32>;\n"
        "@group(0) @binding(1) var tex_sampler : sampler;\n"
        "@fragment fn fs_main(@location(0) uv : vec2f) -> @location(0) vec4f {\n"
        "    return textureSample(tex, tex_sampler, uv);\n"
        "}\n";
```

The descriptor uses default visibility (Fragment) for the texture and sampler — the auto-detection works here because these are standard render-shader bindings:

```c
    CgfxShader render_shader = cgfx_shader_create(&ctx, "quad_render", render_wgsl,
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 2,
                .bindings = (CgfxBindingDesc[]){
                    { .binding = 0, .kind = CGFX_BINDING_TEXTURE },
                    { .binding = 1, .kind = CGFX_BINDING_SAMPLER },
                },
            }},
        });
```

### Create pipelines and bind groups

```c
    WGPUComputePipeline compute_pipeline = cgfx_compute_pipeline_create(&ctx,
        &(CgfxComputeDesc){ .shader = &compute_shader });

    WGPURenderPipeline render_pipeline = cgfx_pipeline_create(&ctx,
        &(CgfxPipelineDesc){ .shader = &render_shader });
    // vertex_buffer_count = 0 — procedural vertices in shader

    Params params = {};
    CgfxBuffer params_buf = cgfx_buffer_create_uniform(&ctx, &params, sizeof(Params));

    WGPUSampler sampler = cgfx_sampler_create(&ctx, &(CgfxSamplerDesc){});

    // Compute bind group: storage texture + uniform buffer
    WGPUBindGroup compute_bg = cgfx_bind_group_create(&ctx, &compute_shader, 0,
        (CgfxBindGroupEntry[]){
            { .binding = 0, .texture = &tex },
            { .binding = 1, .buffer = &params_buf },
        }, 2);

    // Render bind group: sampled texture + sampler
    WGPUBindGroup render_bg = cgfx_bind_group_create(&ctx, &render_shader, 0,
        (CgfxBindGroupEntry[]){
            { .binding = 0, .texture = &tex },
            { .binding = 1, .sampler = sampler },
        }, 2);
```

Notice how the same `tex` appears in both bind groups. The compute bind group references it as a storage texture (for writing), the render bind group references it as a sampled texture (for reading). This is determined by the bind group layout, not the texture itself.

### The render loop

This is where everything comes together — two-phase frame begin with compute before render:

```c
    while (cgfx_ctx_is_running(&ctx)) {
        params.time += 0.016f;
        wgpuQueueWriteBuffer(ctx.queue, params_buf.buffer, 0, &params, sizeof(params));

        CgfxFrame frame;
        if (cgfx_frame_begin_encoder(&ctx, &frame)) {
            // Phase 1: compute pass — write to storage texture
            CgfxComputePass cp;
            cgfx_compute_pass_begin(frame.encoder, &cp);
            wgpuComputePassEncoderSetPipeline(cp.pass, compute_pipeline);
            cgfx_shader_bind_compute(cp.pass, &compute_bg, 1);
            wgpuComputePassEncoderDispatchWorkgroups(cp.pass, 512/8, 512/8, 1);
            cgfx_compute_pass_end(&cp);

            // Phase 2: render pass — sample the texture, draw fullscreen quad
            cgfx_frame_begin_render_pass(&ctx, &frame, (WGPUColor){0, 0, 0, 1});
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, render_pipeline);
            cgfx_shader_bind(frame.render_pass, &render_bg, 1);
            wgpuRenderPassEncoderDraw(frame.render_pass, 6, 1, 0, 0);
            cgfx_frame_end(&ctx, &frame);
        }
    }
```

The dispatch call `(512/8, 512/8, 1)` launches 64×64 workgroups, each with 8×8 threads — totaling 512×512 threads, one per pixel. The workgroup size (8, 8) is declared in the WGSL `@workgroup_size` annotation and must match.

### Cleanup

Resources are destroyed in reverse order of creation. Bind groups before shaders (since bind groups reference shader layouts), pipelines before shaders, texture and context last:

```c
    wgpuBindGroupRelease(render_bg);
    wgpuBindGroupRelease(compute_bg);
    cgfx_buffer_destroy(&params_buf);
    wgpuSamplerRelease(sampler);
    wgpuRenderPipelineRelease(render_pipeline);
    wgpuComputePipelineRelease(compute_pipeline);
    cgfx_shader_destroy(&render_shader);
    cgfx_shader_destroy(&compute_shader);
    cgfx_texture_destroy(&tex);
    cgfx_ctx_destroy(&ctx);
```

## Standalone compute with read-back

Not all compute work produces images. For data-parallel computation (physics, sorting, reduction), you write to storage buffers and read results back to the CPU. The `examples/compute/` example demonstrates this with vector addition.

The read-back pattern requires three steps:

1. **Create storage buffers** with `cgfx_buffer_create_storage()` — includes `CopySrc` usage so the buffer can be copied.
2. **Copy to a mapping buffer** with `cgfx_buffer_copy()` — you cannot map a storage buffer directly; WebGPU requires a separate buffer with `MapRead` usage.
3. **Map and read** with `wgpuBufferMapAsync()` + `wgpuBufferGetConstMappedRange()`.

```c
// After compute dispatch:
CgfxBuffer readback = cgfx_buffer_create_mapping(&ctx, NULL, output.size, 0);
cgfx_buffer_copy(&ctx, &output, &readback, 0);  // GPU-to-GPU copy

wgpuBufferMapAsync(readback.buffer, WGPUMapMode_Read, 0, readback.size,
                    &on_mapped, &readback);
// ... poll until readback.ready ...
const float *result = wgpuBufferGetConstMappedRange(readback.buffer, 0, readback.size);
```

!!! tip "Why the intermediate copy?"
    WebGPU separates storage and mapping into different buffer usage flags for performance reasons. Storage buffers live in fast GPU memory optimized for shader access. Mapping buffers live in shared memory accessible to the CPU. `cgfx_buffer_copy` bridges the two with a one-shot command encoder submission.

## Design decisions

### Why a separate compute module?

The compute pipeline could have been folded into `cgfx_pipeline.h` (which handles render pipelines) or `cgfx_frame.h` (which manages the frame lifecycle). We chose a separate `cgfx_compute.h` because:

- **Render pipelines are complex** — vertex layouts, blend state, depth testing, cull modes. Compute pipelines have none of that. Mixing them would mean a descriptor where most fields are irrelevant depending on the pipeline type.
- **Compute passes are independent of surfaces** — they do not acquire textures or present frames. Putting them in `cgfx_frame.h` would create a false dependency on windowing.
- **The module is self-contained** — `CgfxComputeDesc`, `CgfxComputePass`, and five functions. Small modules are easier to understand.

### Why cgfx_buffer_copy lives in the buffer module

`cgfx_buffer_copy` is a general-purpose buffer-to-buffer copy. While it is most commonly used for compute read-back, it is equally useful for duplicating vertex data, copying uniform snapshots, or any GPU-to-GPU transfer. It lives in `cgfx_buffer.h` alongside the other buffer operations.

### Entry point convention

cgfx defaults to `"cs_main"` for compute entry points, matching the `"vs_main"` / `"fs_main"` convention for vertex and fragment shaders. This is a cgfx convention, not a WebGPU requirement — WGSL entry points can be named anything.
