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
| `sample_type` | `WGPUTextureSampleType` | `Float` | Texture sample type (only for `TEXTURE` kind). |
| `view_dimension` | `WGPUTextureViewDimension` | `2D` | Texture view dimension (for `TEXTURE` and `STORAGE_TEXTURE` kinds). |
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

### cgfx_shader_create_bind_group

Creates a bind group for a specific `@group` index using the shader's layout. Each buffer maps to consecutive binding indices.

```c
CGFX_API WGPUBindGroup cgfx_shader_create_bind_group(const CgfxCtx *ctx,
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

**Returns:** A `WGPUBindGroup` handle, or `nullptr` if `group_index` is out of range. The caller owns this handle and must release it with `wgpuBindGroupRelease()`.

**Example:**

```c
CgfxBuffer ubuf = cgfx_buffer_create_uniform(&ctx, &my_data, sizeof(my_data));

WGPUBindGroup group = cgfx_shader_create_bind_group(
    &ctx, &shader, 0,        // @group(0)
    &ubuf, 1                  // one buffer at @binding(0)
);

// Use during rendering, then release:
wgpuBindGroupRelease(group);
cgfx_buffer_destroy(&ubuf);
```

!!! warning "Bind groups are caller-owned"
    `cgfx_shader_destroy` does **not** release bind groups created with this function. You must call `wgpuBindGroupRelease()` yourself, or use `CgfxUniform` / `CgfxCamera` which handle this automatically.

!!! note "Non-consecutive bindings"
    `cgfx_shader_create_bind_group` assumes consecutive buffer bindings. For non-consecutive bindings or mixed resource types (textures, samplers), use `cgfx_bind_group_create` below.

---

### CgfxBindGroupEntry

An entry in a general-purpose bind group, supporting buffers, textures, and samplers. Set exactly one of `buffer`, `texture`, or `sampler` per entry.

| Field | Type | Description |
|-------|------|-------------|
| `binding` | `uint32_t` | `@binding(N)` index. |
| `buffer` | `const CgfxBuffer*` | Non-NULL for buffer bindings. |
| `texture` | `const CgfxTexture*` | Non-NULL for texture bindings (uses `texture->view`). |
| `sampler` | `WGPUSampler` | Non-NULL for sampler bindings. |

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

**Returns:** A `WGPUBindGroup` handle. The caller owns this handle and must release it with `wgpuBindGroupRelease()`.

**Example (texture + sampler):**

```c
WGPUBindGroup bg = cgfx_bind_group_create(&ctx, &shader, 0,
    (CgfxBindGroupEntry[]){
        { .binding = 0, .texture = &my_texture },
        { .binding = 1, .sampler = my_sampler },
    }, 2);
```

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

### cgfx_shader_destroy

Releases the shader module, pipeline layout, and all bind group layouts.

```c
CGFX_API void cgfx_shader_destroy(CgfxShader *shader);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `shader` | `CgfxShader*` | Shader to destroy. Must not be used after this call. |

!!! warning "Does NOT release bind groups"
    Bind groups created via `cgfx_shader_create_bind_group` are owned by the caller and are not released here. Release them with `wgpuBindGroupRelease()` before destroying the shader. `CgfxUniform` and `CgfxCamera` handle their own bind group cleanup in their respective `_destroy` functions.

See the [Shader and Bind Group Ownership guide](../guides/shader-bind-groups.md) for a full explanation of the ownership model.
