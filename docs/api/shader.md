# Shader

WGSL compilation, bind group layout management, and bind group helpers.

**Header:** `cgfx_shader.h`

---

## Structs

### CgfxBindingDesc

Describes a single binding slot within a bind group layout.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `binding` | `uint32_t` | `0` | The `@binding(N)` index in WGSL. |
| `visibility` | `WGPUShaderStageFlags` | `Vertex \| Fragment` | Shader stages that can access this binding. |
| `type` | `WGPUBufferBindingType` | `Uniform` | Buffer binding type. |
| `min_binding_size` | `uint64_t` | `0` (none) | Minimum buffer size in bytes. `0` means no minimum enforced. |

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

**Returns:** A `CgfxShader`. If `desc` is `NULL` or has no groups, only the shader module is created and `pipeline_layout` is `NULL`.

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

!!! note "Non-consecutive or non-buffer bindings"
    For bindings that are not consecutive buffer bindings (e.g., textures, samplers, or gaps in binding indices), use `shader->group_layouts[group_index]` with the raw WebGPU `wgpuDeviceCreateBindGroup` API directly.

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
