# Shader

WGSL compilation, bind group layout management, and bind group helpers.

**Header:** `cgfx_shader.h`

---

## Structs

### CgfxBindingKind

The kind of resource a binding slot describes.

| Value | Description |
|-------|-------------|
| `CGFX_BINDING_BUFFER` (0) | Buffer (uniform, storage, read-only storage). Default — backward compatible. |
| `CGFX_BINDING_TEXTURE` | Sampled texture. |
| `CGFX_BINDING_SAMPLER` | Sampler. |
| `CGFX_BINDING_STORAGE_TEXTURE` | Storage texture (compute read/write). |

### CgfxBindingDesc

Describes a single binding slot within a bind group layout.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `binding` | `uint32_t` | `0` | The `@binding(N)` index in WGSL. |
| `visibility` | `WGPUShaderStageFlags` | auto | `0` = auto: Vertex\|Fragment for buffers, Fragment for textures/samplers, Compute for storage textures. |
| `kind` | `CgfxBindingKind` | `BUFFER` | The resource kind. |
| `type` | `WGPUBufferBindingType` | `Uniform` | Buffer binding type (only for `BUFFER` kind). |
| `min_binding_size` | `uint64_t` | `0` (none) | Minimum buffer size in bytes (only for `BUFFER` kind). |
| `has_dynamic_offset` | `bool` | `false` | Buffer uses dynamic offset at bind time (only for `BUFFER` kind). Use with `cgfx_shader_bind_dynamic`. |
| `sample_type` | `WGPUTextureSampleType` | `Float` | Texture sample type (only for `TEXTURE` kind). |
| `view_dimension` | `WGPUTextureViewDimension` | `2D` | Texture view dimension (for `TEXTURE` and `STORAGE_TEXTURE` kinds). |
| `sampler_type` | `WGPUSamplerBindingType` | `Filtering` | Sampler binding type (only for `SAMPLER` kind). Use `Comparison` for shadow mapping, `NonFiltering` for data textures. |
| `storage_access` | `WGPUStorageTextureAccess` | `WriteOnly` | Storage texture access (only for `STORAGE_TEXTURE` kind). |
| `storage_format` | `WGPUTextureFormat` | *(required)* | Storage texture format (only for `STORAGE_TEXTURE` kind). |

---

### CgfxGroupDesc

Describes one bind group (`@group(N)`).

| Field | Type | Description |
|-------|------|-------------|
| `binding_count` | `uint32_t` | Number of bindings in this group. |
| `bindings` | `const CgfxBindingDesc*` | Array of binding descriptions. |

---

### CgfxShaderDesc

Shader creation descriptor. Describes the bind group layouts the shader expects. Pass `NULL` to `cgfx_shader_create` for shaders with no bindings.

| Field | Type | Description |
|-------|------|-------------|
| `group_count` | `uint32_t` | Number of bind groups. |
| `groups` | `const CgfxGroupDesc*` | Array indexed by `@group(N)`. |

---

### CgfxShader

A compiled shader with its bind group layouts and pipeline layout. All fields are public.

| Field | Type | Description |
|-------|------|-------------|
| `module` | `WGPUShaderModule` | The compiled WGSL shader module. |
| `pipeline_layout` | `WGPUPipelineLayout` | Pipeline layout built from group layouts. `NULL` when no descriptor was provided (automatic layout). |
| `group_layouts` | `WGPUBindGroupLayout*` | Array of bind group layouts, one per `@group(N)`. |
| `group_count` | `uint32_t` | Number of bind group layouts. |
| `ok` | `bool` | `true` if creation succeeded. Check before use. A zeroed struct has `ok == false`. |

---

## Functions

### cgfx_shader_create

Compiles a WGSL source string and optionally builds bind group layouts from a descriptor.

```c
CGFX_API CgfxShader cgfx_shader_create(const CgfxCtx *ctx,
                                        const char *label,
                                        const char *wgsl,
                                        const CgfxShaderDesc *desc);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `label` | `const char*` | Debug label for the shader module. May be `NULL`. |
| `wgsl` | `const char*` | Null-terminated WGSL source code. |
| `desc` | `const CgfxShaderDesc*` | Bind group layout description, or `NULL` for no bindings. |

**Returns:** A `CgfxShader`. Check `.ok` before use -- it is `false` if the shader module could not be created (e.g., file not found for `_from_file`, or invalid WGSL). If `desc` is `NULL` or has no groups, only the shader module is created and `pipeline_layout` is `NULL`.

**Example (no bindings):**

```c
CgfxShader shader = cgfx_shader_create(&ctx, "simple", wgsl_source, nullptr);
```

**Example (with one uniform at @group(0) @binding(0)):**

```c
CgfxShader shader = cgfx_shader_create(&ctx, "lit", wgsl_source,
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

---

### cgfx_shader_create_from_file

Reads a WGSL file from disk and compiles it. Otherwise identical to `cgfx_shader_create`.

```c
CGFX_API CgfxShader cgfx_shader_create_from_file(const CgfxCtx *ctx,
                                                   const char *label,
                                                   const char *path,
                                                   const CgfxShaderDesc *desc);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `label` | `const char*` | Debug label. May be `NULL`. |
| `path` | `const char*` | Path to a `.wgsl` source file. |
| `desc` | `const CgfxShaderDesc*` | Bind group layout description, or `NULL`. |

**Returns:** A `CgfxShader`. The file buffer is freed internally after compilation.

**Example:**

```c
CgfxShader shader = cgfx_shader_create_from_file(&ctx, "mesh shader",
    "shaders/mesh.wgsl",
    &(CgfxShaderDesc){
        .group_count = 1,
        .groups = (CgfxGroupDesc[]){{
            .binding_count = 1,
            .bindings = (CgfxBindingDesc[]){{
                .binding = 0,
                .min_binding_size = sizeof(SceneUniforms),
            }},
        }},
    });
```

---

### cgfx_bind_group_create_buffers

Creates a bind group for a specific `@group` index using the shader's layout. Each buffer maps to consecutive binding indices.

```c
CGFX_API WGPUBindGroup cgfx_bind_group_create_buffers(const CgfxCtx *ctx,
                                                       const CgfxShader *shader,
                                                       uint32_t group_index,
                                                       const CgfxBuffer *buffers,
                                                       uint32_t buffer_count);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `shader` | `const CgfxShader*` | Shader with bind group layouts. |
| `group_index` | `uint32_t` | The `@group(N)` index. |
| `buffers` | `const CgfxBuffer*` | Array of `CgfxBuffer`, one per binding. `buffers[0]` maps to `@binding(0)`, `buffers[1]` to `@binding(1)`, etc. |
| `buffer_count` | `uint32_t` | Number of buffers in the array. |

**Returns:** A `WGPUBindGroup` handle, or `nullptr` if `group_index` is out of range. The caller owns this handle and must release it with `cgfx_bind_group_destroy()`.

**Example:**

```c
CgfxBuffer ubuf = cgfx_buffer_create_uniform(&ctx, &my_data, sizeof(my_data));

WGPUBindGroup group = cgfx_bind_group_create_buffers(
    &ctx, &shader, 0,        // @group(0)
    &ubuf, 1                  // one buffer at @binding(0)
);

// Use during rendering, then release:
cgfx_bind_group_destroy(group);
cgfx_buffer_destroy(&ubuf);
```

!!! warning "Bind groups are caller-owned"
    `cgfx_shader_destroy` does **not** release bind groups created with this function. You must call `cgfx_bind_group_destroy()` yourself, or use `CgfxUniform` which handles this automatically.

!!! note "Non-consecutive bindings"
    `cgfx_bind_group_create_buffers` assumes consecutive buffer bindings. For non-consecutive bindings or mixed resource types (textures, samplers), use `cgfx_bind_group_create` below.

---

### CgfxBindGroupEntry

An entry in a general-purpose bind group, supporting buffers, textures, and samplers. Set exactly one of `buffer`, `texture`, or `sampler` per entry.

| Field | Type | Description |
|-------|------|-------------|
| `binding` | `uint32_t` | `@binding(N)` index. |
| `buffer` | `const CgfxBuffer*` | Non-NULL for buffer bindings. |
| `texture` | `const CgfxTexture*` | Non-NULL for texture bindings (uses `texture->view`). |
| `sampler` | `WGPUSampler` | Non-NULL for sampler bindings. |
| `offset` | `uint64_t` | Buffer sub-range offset in bytes. 0 = start. |
| `size` | `uint64_t` | Buffer sub-range size in bytes. 0 = entire buffer. |

---

### cgfx_bind_group_create

Creates a bind group with mixed buffer, texture, and sampler entries using explicit binding indices.

```c
CGFX_API WGPUBindGroup cgfx_bind_group_create(const CgfxCtx *ctx,
                                               const CgfxShader *shader,
                                               uint32_t group_index,
                                               const CgfxBindGroupEntry *entries,
                                               uint32_t entry_count);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `shader` | `const CgfxShader*` | Shader with bind group layouts. |
| `group_index` | `uint32_t` | The `@group(N)` index. |
| `entries` | `const CgfxBindGroupEntry*` | Array of bind group entries. |
| `entry_count` | `uint32_t` | Number of entries. |

**Returns:** A `WGPUBindGroup` handle. The caller owns this handle and must release it with `cgfx_bind_group_destroy()`.

**Example (texture + sampler):**

```c
WGPUBindGroup bg = cgfx_bind_group_create(&ctx, &shader, 0,
    (CgfxBindGroupEntry[]){
        { .binding = 0, .texture = &my_texture },
        { .binding = 1, .sampler = my_sampler },
    }, 2);
```

---

### cgfx_bind_group_destroy

Release a bind group.

```c
CGFX_API void cgfx_bind_group_destroy(WGPUBindGroup group);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `group` | `WGPUBindGroup` | Bind group to release. |

---

### cgfx_shader_bind

Sets bind groups on an active render pass. `groups[0]` is set at `@group(0)`, `groups[1]` at `@group(1)`, and so on.

```c
CGFX_API void cgfx_shader_bind(WGPURenderPassEncoder pass,
                                const WGPUBindGroup *groups,
                                uint32_t group_count);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `pass` | `WGPURenderPassEncoder` | Active render pass encoder. |
| `groups` | `const WGPUBindGroup*` | Array of bind group handles. |
| `group_count` | `uint32_t` | Number of bind groups to set. |

**Example:**

```c
// Bind a single group at @group(0)
cgfx_shader_bind(frame.render_pass, &my_bind_group, 1);

// Bind multiple groups: @group(0) = camera, @group(1) = material
WGPUBindGroup groups[] = { camera_group, material_group };
cgfx_shader_bind(frame.render_pass, groups, 2);
```

---

### cgfx_shader_bind_compute

Sets bind groups on an active compute pass. `groups[0]` is set at `@group(0)`, `groups[1]` at `@group(1)`, and so on.

```c
CGFX_API void cgfx_shader_bind_compute(WGPUComputePassEncoder pass,
                                        const WGPUBindGroup *groups,
                                        uint32_t group_count);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `pass` | `WGPUComputePassEncoder` | Active compute pass encoder. |
| `groups` | `const WGPUBindGroup*` | Array of bind group handles. |
| `group_count` | `uint32_t` | Number of bind groups to set. |

**Example:**

```c
// Bind a single group at @group(0) on a compute pass
cgfx_shader_bind_compute(cp.pass, &my_bind_group, 1);
```

Mirror of `cgfx_shader_bind` for compute passes instead of render passes.

---

### cgfx_shader_bind_dynamic

Set a single bind group on a render pass with dynamic offsets. Use with bind groups whose layout entries have `has_dynamic_offset = true`.

```c
CGFX_API void cgfx_shader_bind_dynamic(WGPURenderPassEncoder pass,
                                        uint32_t group_index,
                                        WGPUBindGroup group,
                                        const uint32_t *offsets,
                                        uint32_t offset_count);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `pass` | `WGPURenderPassEncoder` | Active render pass encoder. |
| `group_index` | `uint32_t` | The `@group(N)` index to bind to. |
| `group` | `WGPUBindGroup` | Bind group handle. |
| `offsets` | `const uint32_t*` | Array of byte offsets into the dynamic buffers. |
| `offset_count` | `uint32_t` | Number of offsets (must match the number of dynamic-offset bindings in the layout). |

**Example (one uniform buffer with dynamic offset):**

```c
// Layout: one dynamic uniform
CgfxShader shader = cgfx_shader_create(&ctx, "dyn", wgsl,
    &(CgfxShaderDesc){
        .group_count = 1,
        .groups = (CgfxGroupDesc[]){{
            .binding_count = 1,
            .bindings = (CgfxBindingDesc[]){{
                .binding = 0,
                .min_binding_size = sizeof(ObjectData),
                .has_dynamic_offset = true,
            }},
        }},
    });

// One big buffer, one bind group
CgfxBuffer big_buf = cgfx_buffer_create_uniform(&ctx, all_objects,
    object_count * aligned_size);
WGPUBindGroup bg = cgfx_bind_group_create(&ctx, &shader, 0,
    (CgfxBindGroupEntry[]){{
        .binding = 0, .buffer = &big_buf,
        .size = sizeof(ObjectData),
    }}, 1);

// Per-draw: change only the offset
for (uint32_t i = 0; i < object_count; i++) {
    uint32_t offset = i * aligned_size;
    cgfx_shader_bind_dynamic(frame.render_pass, 0, bg, &offset, 1);
    cgfx_mesh_draw(frame.render_pass, &mesh);
}
```

---

### cgfx_shader_bind_compute_dynamic

Set a single bind group on a compute pass with dynamic offsets. Mirror of `cgfx_shader_bind_dynamic` for compute passes.

```c
CGFX_API void cgfx_shader_bind_compute_dynamic(WGPUComputePassEncoder pass,
                                                uint32_t group_index,
                                                WGPUBindGroup group,
                                                const uint32_t *offsets,
                                                uint32_t offset_count);
```

---

### cgfx_shader_destroy

Releases the shader module, pipeline layout, and all bind group layouts.

```c
CGFX_API void cgfx_shader_destroy(CgfxShader *shader);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `shader` | `CgfxShader*` | Shader to destroy. Must not be used after this call. |

!!! warning "Does NOT release bind groups"
    Bind groups created via `cgfx_bind_group_create_buffers` are owned by the caller and are not released here. Release them with `cgfx_bind_group_destroy()` before destroying the shader. `CgfxUniform` handles its own bind group cleanup in its `_destroy` function.

See the [Shader and Bind Group Ownership guide](../guides/shader-bind-groups.md) for a full explanation of the ownership model.
