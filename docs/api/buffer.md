# Buffer

GPU buffer creation and management for vertex, index, uniform, and generic data.

**Header:** `cgfx_buffer.h`

---

## Structs

### CgfxBuffer

A GPU buffer with associated metadata.

| Field | Type | Description |
|-------|------|-------------|
| `buffer` | `WGPUBuffer` | The WebGPU buffer handle. |
| `size` | `uint64_t` | Total size of the buffer in bytes. |
| `count` | `uint32_t` | Number of elements (vertices, indices, or other). |
| `ready` | `bool` | Callback completion flag (used internally for async mapping). |

---

## Functions

### cgfx_buffer_create_vertex

Creates a GPU vertex buffer and uploads data immediately.

```c
CGFX_API CgfxBuffer cgfx_buffer_create_vertex(const CgfxCtx *ctx,
                                               const void *data,
                                               uint64_t data_size,
                                               uint32_t count);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context (uses `device` and `queue`). |
| `data` | `const void*` | Pointer to vertex data to upload. |
| `data_size` | `uint64_t` | Size of the vertex data in bytes. |
| `count` | `uint32_t` | Number of vertices in the data. |

**Returns:** A `CgfxBuffer` with usage `Vertex | CopyDst`.

**Example:**

```c
float vertices[] = {
    // x,    y
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f,
};

CgfxBuffer vbuf = cgfx_buffer_create_vertex(
    &ctx, vertices, sizeof(vertices), 3
);

// Bind during rendering:
wgpuRenderPassEncoderSetVertexBuffer(
    frame.render_pass, 0, vbuf.buffer, 0, vbuf.size
);

// Cleanup:
cgfx_buffer_destroy(&vbuf);
```

---

### cgfx_buffer_create_index

Creates a GPU index buffer and uploads data immediately. Indices are `uint32_t` format.

```c
CGFX_API CgfxBuffer cgfx_buffer_create_index(const CgfxCtx *ctx,
                                              const uint32_t *indices,
                                              uint32_t count);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `indices` | `const uint32_t*` | Pointer to index array. |
| `count` | `uint32_t` | Number of indices. |

**Returns:** A `CgfxBuffer` with usage `Index | CopyDst`. Size is `count * sizeof(uint32_t)`.

**Example:**

```c
uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

CgfxBuffer ibuf = cgfx_buffer_create_index(&ctx, indices, 6);

// Bind during rendering:
wgpuRenderPassEncoderSetIndexBuffer(
    frame.render_pass, ibuf.buffer,
    WGPUIndexFormat_Uint32, 0, ibuf.size
);
wgpuRenderPassEncoderDrawIndexed(frame.render_pass, ibuf.count, 1, 0, 0, 0);

cgfx_buffer_destroy(&ibuf);
```

!!! note "Index format is always `uint32_t`"
    cgfx uses `WGPUIndexFormat_Uint32` for all index buffers. If you need 16-bit indices, use `cgfx_buffer_create` with `WGPUBufferUsage_Index` and manage the format yourself.

---

### cgfx_buffer_create_uniform

Creates a GPU uniform buffer with optional initial data upload.

```c
CGFX_API CgfxBuffer cgfx_buffer_create_uniform(const CgfxCtx *ctx,
                                                const void *data,
                                                uint64_t data_size);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `data` | `const void*` | Initial data to upload, or `NULL` for uninitialized. |
| `data_size` | `uint64_t` | Size of the uniform buffer in bytes. |

**Returns:** A `CgfxBuffer` with usage `Uniform | CopyDst`.

**Example:**

```c
typedef struct {
    float mvp[16];
} Uniforms;

Uniforms u = { /* ... */ };
CgfxBuffer ubuf = cgfx_buffer_create_uniform(&ctx, &u, sizeof(Uniforms));

// Update each frame:
wgpuQueueWriteBuffer(ctx.queue, ubuf.buffer, 0, &u, sizeof(u));

cgfx_buffer_destroy(&ubuf);
```

!!! tip "Prefer `CgfxUniform` for the common case"
    If you have a single uniform buffer bound to one group, `cgfx_uniform_create` bundles the buffer, bind group, and data pointer into one object. Use `cgfx_buffer_create_uniform` when you need more control (e.g., multiple buffers in one bind group).

---

### cgfx_buffer_create_mapping

Creates a mappable buffer for GPU-to-CPU readback.

```c
CGFX_API CgfxBuffer cgfx_buffer_create_mapping(const CgfxCtx *ctx,
                                                const void *data,
                                                uint64_t data_size,
                                                uint32_t count);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `data` | `const void*` | Initial data to upload, or `NULL`. |
| `data_size` | `uint64_t` | Size of the buffer in bytes. |
| `count` | `uint32_t` | Number of elements. |

**Returns:** A `CgfxBuffer` configured for mapping operations.

---

### cgfx_buffer_create

Generic buffer creation. The caller specifies the usage flags directly.

```c
CGFX_API CgfxBuffer cgfx_buffer_create(const CgfxCtx *ctx,
                                        WGPUBufferUsageFlags usage,
                                        const void *data,
                                        uint64_t data_size);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `usage` | `WGPUBufferUsageFlags` | WebGPU buffer usage flags (e.g., `WGPUBufferUsage_Storage \| WGPUBufferUsage_CopyDst`). |
| `data` | `const void*` | Initial data to upload, or `NULL` to skip upload. |
| `data_size` | `uint64_t` | Size of the buffer in bytes. |

**Returns:** A `CgfxBuffer` with the specified usage.

**Example:**

```c
// Create a storage buffer for compute
float compute_data[256] = {0};

CgfxBuffer storage = cgfx_buffer_create(
    &ctx,
    WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
    compute_data,
    sizeof(compute_data)
);

cgfx_buffer_destroy(&storage);
```

---

### cgfx_buffer_destroy

Releases the GPU buffer and zeros the struct.

```c
CGFX_API void cgfx_buffer_destroy(CgfxBuffer *buf);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `CgfxBuffer*` | Buffer to destroy. Must not be used after this call. |

!!! warning "Destroy before context"
    All buffers must be destroyed before calling `cgfx_ctx_destroy`. The device is released during context destruction.
