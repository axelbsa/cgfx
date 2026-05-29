# Uniform

Uniform buffer + bind group bundle for per-object uniform data.

**Header:** `cgfx_uniform.h`

---

## Structs

### CgfxUniform

Bundles a GPU uniform buffer, its bind group, and a pointer to user-owned data into a single object. The user owns the data; `CgfxUniform` only stores a pointer to it.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `buffer` | `CgfxBuffer` | -- | GPU uniform buffer (`Uniform \| CopyDst`). |
| `bind_group` | `WGPUBindGroup` | -- | Bind group referencing this buffer. |
| `data` | `const void *` | -- | Pointer to user-owned data (never freed by cgfx). |
| `size` | `uint64_t` | -- | Size of the uniform data in bytes. |
| `ok` | `bool` | -- | `true` if creation succeeded. Check before use. |

---

## Functions

### cgfx_uniform_create

Create a uniform buffer and bind group in one call.

```c
CGFX_API CgfxUniform cgfx_uniform_create(const CgfxCtx *ctx,
                                 const CgfxShader *shader,
                                 uint32_t group_index,
                                 const void *data,
                                 uint64_t size);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `shader` | `const CgfxShader *` | Shader with bind group layouts. |
| `group_index` | `uint32_t` | The `@group(N)` index to create the bind group for. |
| `data` | `const void *` | Pointer to user-owned uniform data. |
| `size` | `uint64_t` | Size of the uniform data in bytes. |

**Returns:** A `CgfxUniform`. Call `cgfx_uniform_destroy()` to release.

Allocates a GPU uniform buffer, uploads the initial data, and creates a bind group at the given `@group` index using the shader's layout. The `data` pointer is stored for use with `cgfx_uniform_write()`.

!!! warning "Lifetime"
    Keep your uniform data struct valid for the lifetime of `CgfxUniform`. The pointer is stored, not the data. If the data goes out of scope or is freed, `cgfx_uniform_write` will read garbage.

---

### cgfx_uniform_write

Upload the uniform's data to the GPU.

```c
CGFX_API void cgfx_uniform_write(const CgfxCtx *ctx, const CgfxUniform *uniform);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `uniform` | `const CgfxUniform *` | Uniform to upload. |

Writes the data pointed to by `uniform->data` to the GPU buffer via `wgpuQueueWriteBuffer`. Call this each frame after modifying the user-owned data.

---

### cgfx_uniform_destroy

Destroy a uniform and release its GPU resources.

```c
CGFX_API void cgfx_uniform_destroy(CgfxUniform *uniform);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `uniform` | `CgfxUniform *` | Uniform to destroy. |

Releases both the bind group and the underlying buffer. Does **not** free the user-owned data pointer.

---

## Usage

### The 3-call workflow

`CgfxUniform` reduces the typical uniform workflow from 5 operations down to 3:

1. **`cgfx_uniform_create`** -- allocate GPU buffer, create bind group.
2. **`cgfx_uniform_write`** -- upload data each frame.
3. **`cgfx_uniform_destroy`** -- release GPU resources.

### Time-based uniform example

```c
typedef struct {
    float color[4];
    float offset[4];
    float time;
    float _pad[3]; // align to 16 bytes if needed by your shader
} MyUniforms;

MyUniforms data = {
    .color = {1.0f, 0.3f, 0.3f, 1.0f},
};

CgfxUniform uniform = cgfx_uniform_create(&ctx, &shader, 0, &data, sizeof(data));

// In render loop:
while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();
    data.time += 0.016f;
    cgfx_uniform_write(&ctx, &uniform);

    CgfxFrame frame;
    if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.1, 1.0})) {
        wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
        cgfx_shader_bind(frame.render_pass,
                         (WGPUBindGroup[]){ uniform.bind_group }, 1);
        wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
        cgfx_frame_end(&ctx, &frame);
    }
}

cgfx_uniform_destroy(&uniform);
```

### Multiple uniforms (per-object data)

Because `CgfxShader` owns layouts but not bind groups, you can create multiple uniforms from the same shader for per-object variation:

```c
MyUniforms obj_a_data = { .color = {1, 0, 0, 1} };
MyUniforms obj_b_data = { .color = {0, 0, 1, 1} };

CgfxUniform obj_a = cgfx_uniform_create(&ctx, &shader, 0, &obj_a_data, sizeof(obj_a_data));
CgfxUniform obj_b = cgfx_uniform_create(&ctx, &shader, 0, &obj_b_data, sizeof(obj_b_data));

// Draw object A with its uniforms, then object B with different uniforms,
// using the same pipeline and shader.
```

!!! tip "Lower-level API"
    For advanced use cases, the lower-level `cgfx_buffer_create_uniform()` and `cgfx_shader_create_bind_group()` APIs remain available. `CgfxUniform` is a convenience wrapper on top of those.
