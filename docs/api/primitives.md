# Primitives

Geometric primitive generators -- plane, triangle, sphere, cube.

**Header:** `cgfx_primitives.h`

!!! warning "Not Yet Implemented"
    All functions in this module are declared but currently return empty meshes. The API is stable and will be implemented in a future release.

---

## Functions

All primitive generators return a `CgfxMesh` with GPU-resident vertex and index buffers. Destroy with `cgfx_mesh_destroy()`. All primitives use the `CgfxVertex` format (position + normal + color + uv).

---

### cgfx_primitives_plane

Generate a flat plane in the XZ plane.

```c
CgfxMesh cgfx_primitives_plane(const CgfxCtx *ctx,
                                float width, float depth,
                                uint32_t subdivisions);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context (for GPU buffer upload). |
| `width` | `float` | Width of the plane along the X axis. |
| `depth` | `float` | Depth of the plane along the Z axis. |
| `subdivisions` | `uint32_t` | Number of subdivisions per axis (minimum 1). |

**Returns:** A `CgfxMesh`. Destroy with `cgfx_mesh_destroy()`.

**Geometry:**

- Center at origin `(0, 0, 0)`.
- Extends from `(-width/2, 0, -depth/2)` to `(+width/2, 0, +depth/2)`.
- Normal: `(0, 1, 0)` for all vertices.
- UVs: `(0, 0)` at `(-width/2, -depth/2)`, `(1, 1)` at `(+width/2, +depth/2)`.

**Tessellation:**

- `subdivisions=1` produces 4 vertices and 2 triangles (a single quad).
- `subdivisions=N` produces `(N+1)^2` vertices and `2*N^2` triangles.

```c
CgfxMesh floor = cgfx_primitives_plane(&ctx, 10.0f, 10.0f, 4);
// 25 vertices, 32 triangles
```

---

### cgfx_primitives_triangle

Generate an equilateral triangle in the XY plane.

```c
CgfxMesh cgfx_primitives_triangle(const CgfxCtx *ctx, float size);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `size` | `float` | Approximate radius (distance from center to vertex). |

**Returns:** A `CgfxMesh`. Destroy with `cgfx_mesh_destroy()`.

**Geometry:**

- 3 vertices, 1 triangle (3 indices).
- Center at origin `(0, 0, 0)`.
- Normal: `(0, 0, 1)` for all vertices.
- UVs mapped to cover `[0, 1]` range.

```c
CgfxMesh tri = cgfx_primitives_triangle(&ctx, 1.0f);
```

---

### cgfx_primitives_sphere

Generate a UV sphere.

```c
CgfxMesh cgfx_primitives_sphere(const CgfxCtx *ctx,
                                 float radius,
                                 uint32_t slices, uint32_t stacks);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `radius` | `float` | Sphere radius. |
| `slices` | `uint32_t` | Number of longitudinal divisions (minimum 3). |
| `stacks` | `uint32_t` | Number of latitudinal divisions (minimum 2). |

**Returns:** A `CgfxMesh`. Destroy with `cgfx_mesh_destroy()`.

**Geometry:**

- Center at origin `(0, 0, 0)`.
- Poles on the Y axis: north pole at `(0, +radius, 0)`, south pole at `(0, -radius, 0)`.
- Normals point radially outward (equal to normalized position).
- UVs: longitude maps to U `(0.0--1.0)`, latitude maps to V `(0.0--1.0)`.

**Tessellation:**

- Vertex count: `(slices + 1) * (stacks + 1)`.
- Triangle count: `2 * slices * stacks`.

```c
CgfxMesh sphere = cgfx_primitives_sphere(&ctx, 1.0f, 32, 16);
// 561 vertices, 1024 triangles
```

---

### cgfx_primitives_cube

Generate an axis-aligned cube.

```c
CgfxMesh cgfx_primitives_cube(const CgfxCtx *ctx, float size);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `size` | `float` | Side length of the cube. |

**Returns:** A `CgfxMesh`. Destroy with `cgfx_mesh_destroy()`.

**Geometry:**

- Center at origin `(0, 0, 0)`.
- Extends from `(-size/2, -size/2, -size/2)` to `(+size/2, +size/2, +size/2)`.
- 24 vertices (4 per face) -- not shared between faces, for correct normals.
- 36 indices (2 triangles per face, 6 faces).
- Normals: axis-aligned per face (`+X`, `-X`, `+Y`, `-Y`, `+Z`, `-Z`).
- UVs: each face maps `(0, 0)`--`(1, 1)` independently.

```c
CgfxMesh cube = cgfx_primitives_cube(&ctx, 2.0f);
// 24 vertices, 36 indices
```

---

## Usage

### Rendering a primitive

All primitives return a `CgfxMesh`, so they integrate with the standard mesh workflow:

```c
CgfxMesh cube = cgfx_primitives_cube(&ctx, 1.0f);

WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();
WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader = &shader,
    .vertex_buffer_count = 1,
    .vertex_layouts = &layout,
});

// In render loop:
wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
cgfx_mesh_draw(frame.render_pass, &cube);

// Cleanup:
cgfx_mesh_destroy(&cube);
```

!!! tip "Vertex format"
    All primitives use `CgfxVertex` (position + normal + color + uv). Your WGSL shader must declare the matching vertex input with `@location(0)` through `@location(3)`. See the [Mesh](mesh.md) documentation for the full vertex layout.
