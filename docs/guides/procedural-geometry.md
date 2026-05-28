# Procedural Geometry

A guide to generating your own meshes in code using `CgfxVertex` and
`cgfx_mesh_create`. Covers the vertex format, the creation pattern,
and complete implementations of four common primitives.

## The core insight

**cgfx provides the vertex format and the upload. Geometry generation
is your code.**

The library defines `CgfxVertex` (the shape of a vertex) and
`cgfx_mesh_create` (the upload to GPU). What you put in those vertices
— positions, normals, UVs — is entirely up to you. This separation is
deliberate: a low-level rendering API shouldn't ship with a built-in
cube generator. But it should make it trivial to build one yourself.

This guide walks through building four common primitives from scratch,
explaining the geometry math along the way.

## The CgfxVertex format

Every mesh in cgfx uses the same 96-byte vertex format:

```
┌─────────┬─────────┬─────────┬──────────┬──────────┬────────┬────────┬─────────┐
│ position│ normal  │ tangent │ texcoord0│ texcoord1│ color  │ joints │ weights │
│ float[3]│ float[3]│ float[4]│ float[2] │ float[2] │float[4]│ u16[4] │ float[4]│
│  12 B   │  12 B   │  16 B   │   8 B    │   8 B    │ 16 B   │  8 B   │  16 B   │
└─────────┴─────────┴─────────┴──────────┴──────────┴────────┴────────┴─────────┘
  offset 0    12        24         40          48       56      72        80
```

For procedural geometry, you typically fill in three fields:

| Field | What it does | Primitives use |
|-------|-------------|----------------|
| `position` | XYZ vertex position | Always |
| `normal` | Surface normal for lighting | Always |
| `texcoord0` | UV coordinates for texturing | Usually |

The remaining fields (`tangent`, `texcoord1`, `color`, `joints`,
`weights`) zero-initialize to safe defaults. You only fill them when
your use case requires them — tangent-space normal mapping, skeletal
animation, vertex colors, etc.

!!! tip "Zero-init is your friend"
    C's designated initializer syntax zero-fills omitted fields:
    ```c
    CgfxVertex v = {
        .position = { 1.0f, 0.0f, 0.0f },
        .normal   = { 0.0f, 1.0f, 0.0f },
    };
    // tangent, texcoord0, texcoord1, color, joints, weights = all zeros
    ```
    This means a simple triangle only needs to specify position and
    normal — 24 bytes of meaningful data out of 96. The unused fields
    are harmless padding that keeps the format uniform across all meshes.

## The mesh creation pattern

Every procedural mesh follows the same four-step pattern:

```
┌──────────────────────────────────────────────────────────────────┐
│ 1. ALLOCATE — CPU arrays for vertices and indices                │
│                                                                  │
│    CgfxVertex *vertices = malloc(vertex_count * sizeof(...));    │
│    uint32_t   *indices  = malloc(index_count * sizeof(...));     │
├──────────────────────────────────────────────────────────────────┤
│ 2. FILL — generate positions, normals, UVs, index connectivity   │
│                                                                  │
│    for each vertex:                                              │
│        compute position from parametric formula                  │
│        compute normal (face normal or normalized position)       │
│        compute UV from parameter space                           │
│    for each face:                                                │
│        emit triangle indices                                     │
├──────────────────────────────────────────────────────────────────┤
│ 3. UPLOAD — hand to cgfx_mesh_create                             │
│                                                                  │
│    CgfxMesh mesh = cgfx_mesh_create(ctx,                         │
│        vertices, vertex_count, indices, index_count);            │
│    // GPU now owns a copy — CPU arrays are no longer needed      │
├──────────────────────────────────────────────────────────────────┤
│ 4. FREE — release the CPU arrays                                 │
│                                                                  │
│    free(vertices);                                               │
│    free(indices);                                                │
│    return mesh;                                                  │
└──────────────────────────────────────────────────────────────────┘
```

`cgfx_mesh_create` copies the data into GPU buffers via
`wgpuQueueWriteBuffer`. After it returns, the CPU arrays are not
referenced again — the `CgfxMesh` struct holds only the GPU-side
`CgfxBuffer` handles and the index count.

For small primitives (triangle, cube) you can use stack arrays instead
of malloc. For parametric shapes (plane, sphere) where the vertex
count depends on a subdivision parameter, heap allocation is the way
to go.

## Primitive recipes

### Triangle

The simplest possible mesh — 3 vertices, 1 triangle. An equilateral
triangle centered at the origin in the XY plane, facing +Z.

**The geometry:**

```
         v0 (top)
         /\
        /  \
       /    \
      /      \
     /________\
   v1          v2
  (bottom-left) (bottom-right)
```

The height of an equilateral triangle with side length `size` is
`h = size * sqrt(3) / 2`. The centroid sits at `h/3` from the base
and `2h/3` from the top, so we offset the vertices to center the
triangle at the origin.

All three vertices share the same normal `(0, 0, 1)` — the triangle
is flat in the XY plane.

```c
static CgfxMesh create_triangle(const CgfxCtx *ctx, float size) {
    float h = size * sqrtf(3.0f) / 2.0f;

    CgfxVertex vertices[3] = {
        { .position = { 0.0f, h * 2.0f/3.0f, 0.0f },
          .normal = { 0.0f, 0.0f, 1.0f },
          .texcoord0 = { 0.5f, 1.0f } },
        { .position = { -size/2.0f, -h * 1.0f/3.0f, 0.0f },
          .normal = { 0.0f, 0.0f, 1.0f },
          .texcoord0 = { 0.0f, 0.0f } },
        { .position = { size/2.0f, -h * 1.0f/3.0f, 0.0f },
          .normal = { 0.0f, 0.0f, 1.0f },
          .texcoord0 = { 1.0f, 0.0f } },
    };
    uint32_t indices[3] = { 0, 1, 2 };

    return cgfx_mesh_create(ctx, vertices, 3, indices, 3);
}
```

**Counts:** 3 vertices, 3 indices, 1 triangle. No heap allocation
needed.

---

### Cube

A cube with 6 faces, centered at the origin. The key decision:
**vertices are not shared between faces.** Each face has its own 4
vertices with a unique face normal.

**Why 24 vertices instead of 8?**

A cube has 8 geometric corners, but each corner touches 3 faces with
3 different normals. If you shared vertices between faces, each vertex
would need a single normal — and you'd get smooth shading across
edges instead of the hard edges a cube should have. By duplicating
vertices (4 per face × 6 faces = 24), each vertex carries the correct
face normal.

```
    +Y
    │   ┌───────┐
    │  ╱       ╱│
    │ ╱  top  ╱ │
    │┌───────┐  │  ← each face has its own 4 vertices
    ││       │  │     with a unique axis-aligned normal
    ││ front │  ╱
    ││       │ ╱
    │└───────┘╱───── +X
    ╱
   +Z
```

**Index pattern:** Each face is a quad split into 2 triangles.
For face `f` with base vertex `f*4`, the pattern is
`(base+0, base+1, base+2)` and `(base+0, base+2, base+3)`.

```c
static CgfxMesh create_cube(const CgfxCtx *ctx, float size) {
    float s = size / 2.0f;

    CgfxVertex vertices[24] = {
        // Front face (+Z), normal (0, 0, +1)
        { .position = {-s, -s, +s}, .normal = {0,0,1}, .texcoord0 = {0,0} },
        { .position = {+s, -s, +s}, .normal = {0,0,1}, .texcoord0 = {1,0} },
        { .position = {+s, +s, +s}, .normal = {0,0,1}, .texcoord0 = {1,1} },
        { .position = {-s, +s, +s}, .normal = {0,0,1}, .texcoord0 = {0,1} },

        // Back face (-Z), normal (0, 0, -1)
        { .position = {+s, -s, -s}, .normal = {0,0,-1}, .texcoord0 = {0,0} },
        { .position = {-s, -s, -s}, .normal = {0,0,-1}, .texcoord0 = {1,0} },
        { .position = {-s, +s, -s}, .normal = {0,0,-1}, .texcoord0 = {1,1} },
        { .position = {+s, +s, -s}, .normal = {0,0,-1}, .texcoord0 = {0,1} },

        // Top face (+Y), normal (0, +1, 0)
        { .position = {-s, +s, +s}, .normal = {0,1,0}, .texcoord0 = {0,0} },
        { .position = {+s, +s, +s}, .normal = {0,1,0}, .texcoord0 = {1,0} },
        { .position = {+s, +s, -s}, .normal = {0,1,0}, .texcoord0 = {1,1} },
        { .position = {-s, +s, -s}, .normal = {0,1,0}, .texcoord0 = {0,1} },

        // Bottom face (-Y), normal (0, -1, 0)
        { .position = {-s, -s, -s}, .normal = {0,-1,0}, .texcoord0 = {0,0} },
        { .position = {+s, -s, -s}, .normal = {0,-1,0}, .texcoord0 = {1,0} },
        { .position = {+s, -s, +s}, .normal = {0,-1,0}, .texcoord0 = {1,1} },
        { .position = {-s, -s, +s}, .normal = {0,-1,0}, .texcoord0 = {0,1} },

        // Right face (+X), normal (+1, 0, 0)
        { .position = {+s, -s, +s}, .normal = {1,0,0}, .texcoord0 = {0,0} },
        { .position = {+s, -s, -s}, .normal = {1,0,0}, .texcoord0 = {1,0} },
        { .position = {+s, +s, -s}, .normal = {1,0,0}, .texcoord0 = {1,1} },
        { .position = {+s, +s, +s}, .normal = {1,0,0}, .texcoord0 = {0,1} },

        // Left face (-X), normal (-1, 0, 0)
        { .position = {-s, -s, -s}, .normal = {-1,0,0}, .texcoord0 = {0,0} },
        { .position = {-s, -s, +s}, .normal = {-1,0,0}, .texcoord0 = {1,0} },
        { .position = {-s, +s, +s}, .normal = {-1,0,0}, .texcoord0 = {1,1} },
        { .position = {-s, +s, -s}, .normal = {-1,0,0}, .texcoord0 = {0,1} },
    };

    uint32_t indices[36];
    for (uint32_t face = 0; face < 6; face++) {
        uint32_t base = face * 4;
        uint32_t i = face * 6;
        indices[i+0] = base + 0; indices[i+1] = base + 1; indices[i+2] = base + 2;
        indices[i+3] = base + 0; indices[i+4] = base + 2; indices[i+5] = base + 3;
    }

    return cgfx_mesh_create(ctx, vertices, 24, indices, 36);
}
```

**Counts:** 24 vertices, 36 indices, 12 triangles.

---

### Plane

A subdivided quad in the XZ plane, centered at the origin with the
normal pointing up (+Y). The `subdivisions` parameter controls
tessellation — useful for heightmap displacement or per-vertex effects.

**Grid structure:**

```
subdivisions = 3

    v0───v1───v2───v3          UV (0,0) ──────── UV (1,0)
    │ ╲  │ ╲  │ ╲  │             │                  │
    │  ╲ │  ╲ │  ╲ │             │    XZ plane      │
    v4───v5───v6───v7            │    normal = +Y   │
    │ ╲  │ ╲  │ ╲  │             │                  │
    │  ╲ │  ╲ │  ╲ │          UV (0,1) ──────── UV (1,1)
    v8───v9───v10──v11
    │ ╲  │ ╲  │ ╲  │
    │  ╲ │  ╲ │  ╲ │
    v12──v13──v14──v15

    (subdivisions+1)^2 = 16 vertices
    2 * subdivisions^2 = 18 triangles
```

Each grid cell is split into 2 triangles using vertices at:
top-left (tl), top-right (tr), bottom-left (bl), bottom-right (br).
Triangle 1: `(tl, bl, tr)`. Triangle 2: `(tr, bl, br)`.

```c
static CgfxMesh create_plane(const CgfxCtx *ctx,
                              float width, float depth,
                              uint32_t subdivisions) {
    uint32_t cols = subdivisions;
    uint32_t rows = subdivisions;
    uint32_t vertex_count = (cols + 1) * (rows + 1);
    uint32_t index_count  = 6 * cols * rows;

    CgfxVertex *vertices = malloc(vertex_count * sizeof(CgfxVertex));
    uint32_t   *indices  = malloc(index_count * sizeof(uint32_t));

    for (uint32_t i = 0; i <= rows; i++) {
        for (uint32_t j = 0; j <= cols; j++) {
            float u = (float)j / (float)cols;
            float v = (float)i / (float)rows;
            vertices[i * (cols + 1) + j] = (CgfxVertex){
                .position = { -width/2 + u * width, 0.0f, -depth/2 + v * depth },
                .normal   = { 0.0f, 1.0f, 0.0f },
                .texcoord0 = { u, v },
            };
        }
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < rows; i++) {
        for (uint32_t j = 0; j < cols; j++) {
            uint32_t tl = i * (cols + 1) + j;
            uint32_t tr = tl + 1;
            uint32_t bl = (i + 1) * (cols + 1) + j;
            uint32_t br = bl + 1;
            indices[idx++] = tl; indices[idx++] = bl; indices[idx++] = tr;
            indices[idx++] = tr; indices[idx++] = bl; indices[idx++] = br;
        }
    }

    CgfxMesh mesh = cgfx_mesh_create(ctx, vertices, vertex_count,
                                      indices, index_count);
    free(vertices);
    free(indices);
    return mesh;
}
```

**Counts for `subdivisions = N`:**

| Subdivisions | Vertices | Triangles | Indices |
|:---:|:---:|:---:|:---:|
| 1 | 4 | 2 | 6 |
| 4 | 25 | 32 | 96 |
| 16 | 289 | 512 | 1536 |

---

### Sphere

A UV sphere using latitude/longitude parameterization. Poles are on
the Y axis. Each vertex's normal equals its normalized position —
the surface of a sphere is its own normal.

**Parameterization:**

```
             North pole (0, +r, 0)
                  │
     theta = 0 → ●──────────────── stack 0
                ╱ │ ╲
              ╱   │   ╲
            ╱     │     ╲
          ╱       │       ╲
 theta → ●───────●───────● ← stack
          ╲       │       ╱
            ╲     │     ╱
              ╲   │   ╱
                ╲ │ ╱
    theta = π → ●──────────────── stack = stacks
                  │
             South pole (0, -r, 0)

    phi goes around the equator: 0 → 2π
    theta goes from north pole to south: 0 → π
```

For each grid point `(stack, slice)`:

- `theta = stack * PI / stacks` — polar angle from north pole
- `phi = slice * 2*PI / slices` — azimuthal angle around Y axis
- Position: `r * (sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi))`
- Normal: same direction, unit length (just `(x, y, z)` before scaling by `r`)
- UV: `(slice/slices, stack/stacks)` — longitude maps to U, latitude to V

**Why `(slices+1) * (stacks+1)` vertices?**

The first and last slice in each stack have the same position (phi=0
and phi=2*PI are the same point) but different UV coordinates (U=0
and U=1). Without this duplication, a texture would have a visible
seam where U wraps around. The same applies to the pole rows.

```c
static CgfxMesh create_sphere(const CgfxCtx *ctx,
                               float radius,
                               uint32_t slices, uint32_t stacks) {
    uint32_t vertex_count = (slices + 1) * (stacks + 1);
    uint32_t index_count  = 6 * slices * stacks;

    CgfxVertex *vertices = malloc(vertex_count * sizeof(CgfxVertex));
    uint32_t   *indices  = malloc(index_count * sizeof(uint32_t));

    for (uint32_t stack = 0; stack <= stacks; stack++) {
        float theta = (float)stack * M_PI / (float)stacks;
        float sin_theta = sinf(theta);
        float cos_theta = cosf(theta);

        for (uint32_t slice = 0; slice <= slices; slice++) {
            float phi = (float)slice * 2.0f * M_PI / (float)slices;
            float x = sin_theta * cosf(phi);
            float y = cos_theta;
            float z = sin_theta * sinf(phi);

            uint32_t vi = stack * (slices + 1) + slice;
            vertices[vi] = (CgfxVertex){
                .position = { radius * x, radius * y, radius * z },
                .normal   = { x, y, z },
                .texcoord0 = { (float)slice / (float)slices,
                               (float)stack / (float)stacks },
            };
        }
    }

    uint32_t idx = 0;
    for (uint32_t stack = 0; stack < stacks; stack++) {
        for (uint32_t slice = 0; slice < slices; slice++) {
            uint32_t tl = stack * (slices + 1) + slice;
            uint32_t tr = tl + 1;
            uint32_t bl = (stack + 1) * (slices + 1) + slice;
            uint32_t br = bl + 1;
            indices[idx++] = tl; indices[idx++] = bl; indices[idx++] = tr;
            indices[idx++] = tr; indices[idx++] = bl; indices[idx++] = br;
        }
    }

    CgfxMesh mesh = cgfx_mesh_create(ctx, vertices, vertex_count,
                                      indices, index_count);
    free(vertices);
    free(indices);
    return mesh;
}
```

**Counts for `slices = S, stacks = T`:**

| Slices | Stacks | Vertices | Triangles |
|:---:|:---:|:---:|:---:|
| 8 | 4 | 45 | 64 |
| 16 | 8 | 153 | 256 |
| 32 | 16 | 561 | 1024 |

!!! note "Degenerate triangles at the poles"
    At the north and south poles, the top row of triangles degenerates
    into thin slivers where two of the three vertices share the same
    position. The GPU handles this fine — degenerate triangles produce
    zero pixels. An alternative approach uses dedicated pole vertices
    with triangle fans, but the simpler grid approach works well in
    practice.

## Rendering procedural meshes

Once you have a `CgfxMesh`, it integrates with the standard cgfx
rendering workflow. The complete `examples/primitives/` example
renders all four shapes in a 3D scene:

```c
// Create meshes
CgfxMesh triangle = create_triangle(&ctx, 1.5f);
CgfxMesh cube     = create_cube(&ctx, 1.5f);
CgfxMesh plane    = create_plane(&ctx, 2.0f, 2.0f, 4);
CgfxMesh sphere   = create_sphere(&ctx, 0.8f, 24, 16);

// Pipeline uses the standard CgfxVertex layout
WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();
WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader = &shader,
    .depth_test = true,
    .vertex_buffer_count = 1,
    .vertex_layouts = &layout,
});

// Draw in render loop — same as any other CgfxMesh
cgfx_mesh_draw(frame.render_pass, &triangle);
cgfx_mesh_draw(frame.render_pass, &cube);
cgfx_mesh_draw(frame.render_pass, &plane);
cgfx_mesh_draw(frame.render_pass, &sphere);

// Cleanup
cgfx_mesh_destroy(&triangle);
cgfx_mesh_destroy(&cube);
cgfx_mesh_destroy(&plane);
cgfx_mesh_destroy(&sphere);
```

Because all procedural meshes use `CgfxVertex`, they work with
`cgfx_mesh_vertex_layout()` and `cgfx_mesh_draw` — the same pipeline
renders any mesh regardless of how it was generated. See the
[Buffers, Layouts & Pipeline](buffers-layouts-pipeline.md) guide for
more on how vertex layouts and pipelines connect.
