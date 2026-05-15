# Loader

Load geometry from the LearnWebGPU bespoke text format.

**Header:** `cgfx_loader.h`

!!! warning "Temporary"
    This module loads geometry from the [LearnWebGPU](https://eliemichel.github.io/LearnWebGPU/) bespoke text format. It will be replaced by glTF loading in a future version. Do not rely on this API for production use.

---

## Structs

### CgfxGeometry

Raw geometry data loaded from a text file. Owns the allocated memory -- free with `cgfx_free_geometry()`.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `point_data` | `float *` | -- | Flat array of point data (interleaved floats). |
| `point_count` | `uint32_t` | -- | Number of points. |
| `floats_per_point` | `uint32_t` | -- | Floats per point (5 or 6, auto-detected from file). |
| `index_data` | `uint16_t *` | -- | Index array. |
| `index_count` | `uint32_t` | -- | Number of indices. |

!!! note "5 vs 6 floats per point"
    The loader auto-detects the format by counting floats on the first data line. **6 floats:** `x y z r g b`. **5 floats:** `x y r g b` (z is set to 0 when converting to `CgfxVertex`).

---

## Functions

### cgfx_load_geometry

Load raw geometry from a text file.

```c
bool cgfx_load_geometry(const char *path, CgfxGeometry *out);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `const char *` | Path to the geometry text file. |
| `out` | `CgfxGeometry *` | Output struct to fill with loaded data. |

**Returns:** `true` on success, `false` on failure (errors printed to stderr).

**File format:**

```text
# Comments start with #
[points]
x y z r g b
x y z r g b
...
[indices]
i0 i1 i2
i0 i1 i2
...
```

- The `[points]` section contains one point per line with 5 or 6 space-separated floats.
- The `[indices]` section contains one triangle per line with 3 space-separated integers.
- Lines starting with `#` are comments.

---

### cgfx_free_geometry

Free loaded geometry data.

```c
void cgfx_free_geometry(CgfxGeometry *geo);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `geo` | `CgfxGeometry *` | Geometry to free. |

Frees the `point_data` and `index_data` arrays allocated by `cgfx_load_geometry`.

---

### cgfx_load_tutorial_mesh

Convenience function -- load a file and create a `CgfxMesh` in one call.

```c
CgfxMesh cgfx_load_tutorial_mesh(const CgfxCtx *ctx, const char *path);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `path` | `const char *` | Path to the geometry text file. |

**Returns:** A `CgfxMesh` with GPU buffers. Destroy with `cgfx_mesh_destroy()`.

Loads the file, converts the raw point data to a `CgfxVertex` array, and creates a `CgfxMesh`. The conversion handles both formats:

- **6-float points** (`x y z r g b`): position = `(x, y, z)`, color = `(r, g, b)`.
- **5-float points** (`x y r g b`): position = `(x, y, 0)`, color = `(r, g, b)`.

---

## Usage

### Quick mesh loading

```c
CgfxMesh mesh = cgfx_load_tutorial_mesh(&ctx, "pyramid.txt");

// In render loop:
wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
cgfx_mesh_draw(frame.render_pass, &mesh);

// Cleanup:
cgfx_mesh_destroy(&mesh);
```

### Manual loading (for inspection or custom conversion)

```c
CgfxGeometry geo;
if (cgfx_load_geometry("model.txt", &geo)) {
    // Inspect the data
    printf("Points: %u (%.0f floats each)\n", geo.point_count, (double)geo.floats_per_point);
    printf("Indices: %u\n", geo.index_count);

    // Convert manually, or use the raw data directly...

    cgfx_free_geometry(&geo);
}
```

!!! tip "Pipeline vertex layout"
    Meshes loaded with `cgfx_load_tutorial_mesh` use the standard `CgfxVertex` format. Use `cgfx_mesh_vertex_layout()` when creating the pipeline, just like any other mesh. See the [Mesh](mesh.md) documentation for details.
