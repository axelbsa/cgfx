# Mesh

Vertex data + index data + GPU buffers for renderable geometry.

**Header:** `cgfx_mesh.h`

---

## Structs

### CgfxVertex

Standard vertex format used by all cgfx meshes. 44 bytes per vertex, tightly packed.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `position` | `float[3]` | -- | XYZ position (offset 0, 12 bytes). |
| `normal` | `float[3]` | -- | Surface normal (offset 12, 12 bytes). |
| `color` | `float[3]` | -- | Vertex color RGB (offset 24, 12 bytes). |
| `uv` | `float[2]` | -- | Texture coordinates (offset 36, 8 bytes). |

### CgfxMesh

A renderable mesh with GPU-resident vertex and index buffers.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `vertex_buffer` | `CgfxBuffer` | -- | GPU vertex buffer (`CgfxVertex` array). |
| `index_buffer` | `CgfxBuffer` | -- | GPU index buffer (`uint32_t` array). |
| `index_count` | `uint32_t` | -- | Number of indices (= number of draw elements). |

---

## Functions

### cgfx_mesh_create

Create a mesh from vertex and index data.

```c
CgfxMesh cgfx_mesh_create(const CgfxCtx *ctx,
                           const CgfxVertex *vertices, uint32_t vertex_count,
                           const uint32_t *indices, uint32_t index_count);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `vertices` | `const CgfxVertex *` | Array of `CgfxVertex` structs. |
| `vertex_count` | `uint32_t` | Number of vertices in the array. |
| `indices` | `const uint32_t *` | Array of `uint32_t` triangle indices. |
| `index_count` | `uint32_t` | Number of indices (should be a multiple of 3 for triangles). |

**Returns:** A `CgfxMesh` with GPU buffers. Call `cgfx_mesh_destroy()` to free.

Uploads the provided vertex and index arrays to GPU buffers. The caller is responsible for freeing CPU-side vertex/index arrays if they were dynamically allocated -- the mesh only holds GPU copies.

---

### cgfx_mesh_destroy

Release a mesh's GPU buffers.

```c
void cgfx_mesh_destroy(CgfxMesh *mesh);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `mesh` | `CgfxMesh *` | Mesh to destroy. |

Releases both the vertex buffer and index buffer. After this call, the mesh must not be used.

---

### cgfx_mesh_draw

Record draw commands for a mesh on an active render pass.

```c
void cgfx_mesh_draw(WGPURenderPassEncoder pass, const CgfxMesh *mesh);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `pass` | `WGPURenderPassEncoder` | Active render pass encoder (from `cgfx_frame_begin()`). |
| `mesh` | `const CgfxMesh *` | Mesh to draw. Must have valid vertex and index buffers. |

Issues the three draw-call commands required to render an indexed mesh:

1. Binds the vertex buffer at slot 0.
2. Binds the index buffer with `WGPUIndexFormat_Uint32`.
3. Draws `mesh->index_count` indices as a single instance.

!!! note "Pipeline must be set first"
    The pipeline must already be set on the render pass before calling `cgfx_mesh_draw`. Pipelines and meshes are intentionally decoupled so the same mesh can be drawn with different pipelines (e.g. shadow pass and color pass).

!!! tip "Instanced or non-indexed draws"
    For instanced rendering or non-indexed draws, drop down to the raw WebGPU API using the buffer handles in `mesh->vertex_buffer.buffer` and `mesh->index_buffer.buffer`.

---

### cgfx_mesh_vertex_layout

Get the vertex buffer layout descriptor for `CgfxVertex`.

```c
WGPUVertexBufferLayout cgfx_mesh_vertex_layout(void);
```

**Returns:** A `WGPUVertexBufferLayout` matching `CgfxVertex`. The returned layout references static internal storage -- valid for the lifetime of the program.

The layout describes 4 attributes at shader locations 0--3:

| Location | Attribute | Format | Offset |
|----------|-----------|--------|--------|
| 0 | `position` | `Float32x3` | 0 |
| 1 | `normal` | `Float32x3` | 12 |
| 2 | `color` | `Float32x3` | 24 |
| 3 | `uv` | `Float32x2` | 36 |

Stride is `sizeof(CgfxVertex)` = 44 bytes. Step mode is `Vertex`.

---

## Usage

### Matching WGSL shader

Your WGSL vertex shader must declare inputs matching the `CgfxVertex` layout:

```wgsl
struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) color: vec3f,
    @location(3) uv: vec2f,
};
```

### Full mesh example

```c
CgfxVertex vertices[] = {
    { .position = {-0.5f, -0.5f, 0.0f}, .color = {1, 0, 0} },
    { .position = { 0.5f, -0.5f, 0.0f}, .color = {0, 1, 0} },
    { .position = { 0.0f,  0.5f, 0.0f}, .color = {0, 0, 1} },
};
uint32_t indices[] = { 0, 1, 2 };

CgfxMesh mesh = cgfx_mesh_create(&ctx, vertices, 3, indices, 3);

// Pipeline needs the vertex layout:
WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();
WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader = &shader,
    .vertex_buffer_count = 1,
    .vertex_layouts = &layout,
});

// In render loop:
CgfxFrame frame;
if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.2, 1.0})) {
    wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
    cgfx_mesh_draw(frame.render_pass, &mesh);
    cgfx_frame_end(&ctx, &frame);
}

// Cleanup:
cgfx_mesh_destroy(&mesh);
```

!!! note "Zero-initialized fields"
    When using designated initializers, unspecified fields in `CgfxVertex` are zero-initialized. In the example above, `normal` and `uv` default to `{0, 0, 0}` and `{0, 0}` respectively.
