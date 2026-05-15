# Shader & Bind Group Architecture

This guide explains how cgfx manages shader layouts and bind groups, and how to use them to pass uniform data to your shaders. The key idea is a clean ownership split: the **shader owns layouts**, the **caller owns bind groups**.

## The Ownership Model

When you create a `CgfxShader`, it compiles the WGSL source and builds a set of bind group layouts and a pipeline layout based on your descriptor. These layouts define the _shape_ of the data the shader expects. The actual data (bind groups, buffers) is created separately and owned by you.

### What the Shader Owns

- `WGPUShaderModule` -- the compiled WGSL code
- `WGPUBindGroupLayout[]` -- one layout per `@group(N)`, describing the binding slots
- `WGPUPipelineLayout` -- combines all group layouts into a pipeline-compatible layout

### What the Caller Owns

- `WGPUBindGroup` -- created via `cgfx_shader_create_bind_group()` or `cgfx_uniform_create()`
- `CgfxBuffer` / `WGPUBuffer` -- the GPU buffers holding actual data
- The CPU-side data that gets uploaded to those buffers

### Ownership Diagram

```
CgfxShader (library-managed)          Caller-managed
+---------------------------------+    +---------------------------+
| WGPUShaderModule                |    | CgfxUniform               |
| WGPUPipelineLayout              |    |   CgfxBuffer (GPU buffer) |
| WGPUBindGroupLayout[0] --------+----+-> WGPUBindGroup            |
| WGPUBindGroupLayout[1] --------+--. |   void *data (user-owned)  |
|   ...                           |  | +---------------------------+
+---------------------------------+  |
                                     | +---------------------------+
                                     | | CgfxUniform               |
                                     +-+-> WGPUBindGroup            |
                                       |   ...                     |
                                       +---------------------------+
```

Multiple bind groups can reference the same layout. This is the foundation for rendering multiple objects with the same shader but different data.

!!! note "Destruction"
    When you call `cgfx_shader_destroy()`, it releases the shader module, pipeline layout, and all bind group layouts. It does **not** release bind groups created from those layouts -- those are your responsibility. Destroy your bind groups (or `CgfxUniform` objects) before destroying the shader.

## Descriptor Hierarchy

cgfx uses a three-level descriptor hierarchy to describe what a shader expects. These descriptors map directly to WGSL `@group(N) @binding(M)` annotations.

### CgfxBindingDesc

Describes a single binding slot within a group:

```c
typedef struct CgfxBindingDesc {
    uint32_t               binding;           // @binding(N) index
    WGPUShaderStageFlags   visibility;        // Which shader stages can see it
    WGPUBufferBindingType  type;              // Uniform, Storage, ReadOnlyStorage
    uint64_t               min_binding_size;  // Minimum buffer size (0 = none)
} CgfxBindingDesc;
```

**Zero-init defaults:**

- `binding = 0` -- binds to `@binding(0)`
- `visibility = 0` -- defaults to `Vertex | Fragment` (visible in both stages)
- `type = 0` -- defaults to `Uniform`
- `min_binding_size = 0` -- no minimum enforced

### CgfxGroupDesc

Describes one bind group (`@group(N)`):

```c
typedef struct CgfxGroupDesc {
    uint32_t               binding_count;  // Number of bindings in this group
    const CgfxBindingDesc *bindings;       // Array of binding descriptions
} CgfxGroupDesc;
```

### CgfxShaderDesc

Top-level shader descriptor:

```c
typedef struct CgfxShaderDesc {
    uint32_t              group_count;  // Number of bind groups
    const CgfxGroupDesc  *groups;       // Array indexed by @group(N)
} CgfxShaderDesc;
```

### Mapping to WGSL

The descriptor hierarchy maps directly to WGSL declarations:

```
WGSL:                                    C descriptor:
@group(0) @binding(0)                    CgfxShaderDesc {
var<uniform> u: MyUniforms;                .group_count = 1,
                                           .groups = (CgfxGroupDesc[]){{
                                             .binding_count = 1,
                                             .bindings = (CgfxBindingDesc[]){{
                                               .binding = 0,
                                             }},
                                           }},
                                         }
```

### Example: Single Uniform Buffer

A shader with one uniform buffer at `@group(0) @binding(0)`:

```c
CgfxShader shader = cgfx_shader_create(&ctx, "my_shader", wgsl,
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
```

!!! tip "min_binding_size"
    Setting `min_binding_size` to `sizeof(YourStruct)` enables validation: WebGPU will report an error if you try to bind a buffer smaller than this. It is optional but recommended for catching bugs early.

### Example: Multiple Groups

A shader with a camera at `@group(0)` and per-object data at `@group(1)`:

```c
CgfxShader shader = cgfx_shader_create(&ctx, "lit_shader", wgsl,
    &(CgfxShaderDesc){
        .group_count = 2,
        .groups = (CgfxGroupDesc[]){{
            // @group(0): camera matrices
            .binding_count = 1,
            .bindings = (CgfxBindingDesc[]){{
                .binding = 0,
                .min_binding_size = sizeof(CameraData),
            }},
        }, {
            // @group(1): per-object uniforms
            .binding_count = 1,
            .bindings = (CgfxBindingDesc[]){{
                .binding = 0,
                .min_binding_size = sizeof(ObjectData),
            }},
        }},
    });
```

## The "Same Shader, Different Uniforms" Pattern

This is the most important pattern in cgfx. One shader and one pipeline can render many objects, each with its own uniform data. This is achieved by creating multiple `CgfxUniform` objects from the same shader's layout.

### Complete Example

```c
#include "cgfx.h"

typedef struct {
    float color[4];
    float offset[4];
    float time;
} MyUniforms;

int main(void) {
    CgfxCtx ctx;
    cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280, .height = 720,
        .title = "cgfx — multiple uniforms",
        .limits = cgfx_default_limits(),
    });

    /* One shader, one pipeline */
    CgfxShader shader = cgfx_shader_create(&ctx, "shader", wgsl,
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 1,
                .bindings = (CgfxBindingDesc[]){{ .binding = 0 }},
            }},
        });

    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
    });

    /* Two objects with different data */
    MyUniforms data_a = { .color = {1, 0, 0, 1}, .offset = {-0.4f, 0, 0, 0} };
    MyUniforms data_b = { .color = {0, 0, 1, 1}, .offset = { 0.4f, 0, 0, 0} };

    CgfxUniform u_a = cgfx_uniform_create(&ctx, &shader, 0, &data_a, sizeof(MyUniforms));
    CgfxUniform u_b = cgfx_uniform_create(&ctx, &shader, 0, &data_b, sizeof(MyUniforms));

    while (cgfx_ctx_is_running(&ctx)) {
        glfwPollEvents();

        /* Upload CPU data to GPU each frame */
        cgfx_uniform_write(&ctx, &u_a);
        cgfx_uniform_write(&ctx, &u_b);

        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.15, 1.0})) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);

            /* Draw object A */
            cgfx_shader_bind(frame.render_pass, &u_a.bind_group, 1);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);

            /* Draw object B */
            cgfx_shader_bind(frame.render_pass, &u_b.bind_group, 1);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);

            cgfx_frame_end(&ctx, &frame);
        }
    }

    /* Cleanup */
    cgfx_uniform_destroy(&u_a);
    cgfx_uniform_destroy(&u_b);
    wgpuRenderPipelineRelease(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
```

### How It Works

1. **One shader** compiles the WGSL and creates a bind group layout for `@group(0)`.
2. **One pipeline** is created from that shader.
3. **Two `CgfxUniform` objects** (`u_a` and `u_b`) each create their own GPU buffer and bind group from the same layout, but pointing to different CPU data.
4. **Each frame**, `cgfx_uniform_write` uploads each object's CPU data to its GPU buffer.
5. **During rendering**, we set the pipeline once, then alternate between binding each uniform's bind group and issuing a draw call.

The key insight is that `cgfx_shader_bind` changes which bind group is active. Each subsequent draw call uses the most recently bound group. This is how one pipeline renders multiple objects with different colors, positions, or any other per-object data.

!!! tip "cgfx_shader_bind"
    `cgfx_shader_bind(pass, &bind_group, count)` is a convenience wrapper around `wgpuRenderPassEncoderSetBindGroup`. It sets `count` consecutive bind groups starting at `@group(0)`. When you pass `count = 1`, it sets a single group at `@group(0)`.

## Low-Level vs High-Level APIs

cgfx provides two levels of abstraction for working with uniform buffers and bind groups.

### High-Level: CgfxUniform

The `CgfxUniform` API bundles a GPU buffer, bind group, and data pointer into a single object:

```c
/* Create: allocate GPU buffer + create bind group + store data pointer */
CgfxUniform uniform = cgfx_uniform_create(&ctx, &shader, 0, &my_data, sizeof(my_data));

/* Write: upload data to GPU (call each frame after modifying my_data) */
cgfx_uniform_write(&ctx, &uniform);

/* Destroy: release bind group + buffer */
cgfx_uniform_destroy(&uniform);
```

**Use when:**

- You have a struct of uniform data per object
- You want the simplest possible workflow
- The uniform maps to a single buffer at a single binding

### Low-Level: Buffer + Bind Group

For more control, use the building blocks directly:

```c
/* Create a uniform buffer */
CgfxBuffer buf = cgfx_buffer_create_uniform(&ctx, &my_data, sizeof(my_data));

/* Create a bind group from the shader's layout */
WGPUBindGroup group = cgfx_shader_create_bind_group(&ctx, &shader, 0, &buf, 1);

/* Upload data each frame */
wgpuQueueWriteBuffer(ctx.queue, buf.buffer, 0, &my_data, sizeof(my_data));

/* Bind during rendering */
wgpuRenderPassEncoderSetBindGroup(frame.render_pass, 0, group, 0, NULL);

/* Cleanup */
wgpuBindGroupRelease(group);
cgfx_buffer_destroy(&buf);
```

**Use when:**

- A bind group has multiple bindings (e.g., a uniform buffer and a storage buffer)
- You need non-consecutive binding indices
- You want to mix buffer types (uniform + storage) in one group
- You need to use non-buffer resources (textures, samplers) -- these require raw WebGPU bind group creation using `shader.group_layouts[N]`

!!! warning "Low-Level Bind Group Creation"
    `cgfx_shader_create_bind_group` assumes consecutive binding indices: `buffers[0]` maps to `@binding(0)`, `buffers[1]` to `@binding(1)`, etc. For non-consecutive bindings or non-buffer resources (textures, samplers), use `shader->group_layouts[group_index]` with the raw `wgpuDeviceCreateBindGroup` API.

### Comparison

| Feature | CgfxUniform | Low-Level |
|---------|-------------|-----------|
| Lines of code | 3 (create, write, destroy) | 5+ |
| Bindings per group | 1 | Any number |
| Resource types | Uniform buffer only | Any (buffers, textures, samplers) |
| Data upload | `cgfx_uniform_write` | `wgpuQueueWriteBuffer` |
| Owns data? | No (stores pointer) | No (caller manages) |

## Shader with No Bindings

For shaders that have no uniform or storage bindings (e.g., a procedural triangle), pass an empty descriptor or `nullptr`:

```c
/* Empty descriptor -- no bind groups, pipeline layout is automatic */
CgfxShader shader = cgfx_shader_create(&ctx, "simple", wgsl,
    &(CgfxShaderDesc){});

/* Or pass nullptr -- same effect */
CgfxShader shader = cgfx_shader_create(&ctx, "simple", wgsl, nullptr);
```

When the shader has no bind group layouts, `shader.pipeline_layout` is `NULL`. The pipeline creation function passes this `NULL` layout to WebGPU, which triggers automatic layout inference. This is the simplest case -- no layouts, no bind groups, no uniforms.

!!! note "When to use automatic layout"
    Automatic layout inference is convenient for simple shaders but has limitations: you cannot create bind groups from an automatically-inferred layout. If your shader has `@group`/`@binding` declarations in WGSL, you must describe them in `CgfxShaderDesc` even if you plan to create bind groups through raw WebGPU. The descriptor is the source of truth for layouts in cgfx.
