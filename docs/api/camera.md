# Camera

Projection, view, and GPU uniform management for a 3D camera.

**Header:** `cgfx_camera.h`

Uses [cglm](https://github.com/recp/cglm) for matrix math.

---

## Structs

### CgfxCameraDesc

Configuration for creating a camera. Zero-initialize for sensible defaults.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `fovy` | `float` | `0` = 45 degrees | Vertical field of view in **degrees**. |
| `near_z` | `float` | `0` = 0.01 | Near clipping plane. |
| `far_z` | `float` | `0` = 100.0 | Far clipping plane. |
| `eye` | `vec3` | `{0, 0, 0}` | Camera position. |
| `center` | `vec3` | `{0, 0, 0}` | Look-at target. |
| `up` | `vec3` | `{0, 0, 0}` = `{0, 1, 0}` | Up direction. |

### CgfxCamera

Camera state with projection/view matrices and GPU resources.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `projection` | `mat4` | -- | Perspective projection matrix. |
| `view` | `mat4` | -- | View matrix (from look-at). |
| `buffer` | `CgfxBuffer` | -- | GPU uniform buffer (2 x `sizeof(mat4)` = 128 bytes). |
| `ok` | `bool` | -- | `true` if creation succeeded. Check before use. |

---

## Functions

### cgfx_camera_create

Create a camera with projection and view matrices and upload to GPU.

```c
CGFX_API CgfxCamera cgfx_camera_create(const CgfxCtx *ctx,
                               const CgfxCameraDesc *desc);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `desc` | `const CgfxCameraDesc *` | Camera configuration. Zero-initialized fields use defaults. |

**Returns:** A `CgfxCamera`. Call `cgfx_camera_destroy()` to release.

The aspect ratio is automatically computed from `ctx->width / ctx->height`.

---

### cgfx_camera_perspective

Recalculate the projection matrix.

```c
CGFX_API void cgfx_camera_perspective(CgfxCamera *cam, float fovy_deg,
                              float aspect, float near_z, float far_z);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `cam` | `CgfxCamera *` | Camera to update. |
| `fovy_deg` | `float` | Vertical field of view in **degrees**. |
| `aspect` | `float` | Aspect ratio (width / height). |
| `near_z` | `float` | Near clipping plane. |
| `far_z` | `float` | Far clipping plane. |

!!! tip "When to call"
    Call this when the window is resized or when you want to change the field of view. Follow with `cgfx_camera_write` to upload the new matrix.

---

### cgfx_camera_look_at

Recalculate the view matrix.

```c
CGFX_API void cgfx_camera_look_at(CgfxCamera *cam, vec3 eye, vec3 center, vec3 up);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `cam` | `CgfxCamera *` | Camera to update. |
| `eye` | `vec3` | Camera position. |
| `center` | `vec3` | Look-at target. |
| `up` | `vec3` | Up direction. |

---

### cgfx_camera_write

Upload both projection and view matrices to the GPU.

```c
CGFX_API void cgfx_camera_write(const CgfxCtx *ctx, const CgfxCamera *cam);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx *` | Initialized context. |
| `cam` | `const CgfxCamera *` | Camera to upload. |

Call this each frame, or after modifying the projection or view matrices via `cgfx_camera_perspective` or `cgfx_camera_look_at`.

---

### cgfx_camera_bind

Set a bind group on a render pass for the camera.

```c
CGFX_API void cgfx_camera_bind(WGPURenderPassEncoder pass,
                                WGPUBindGroup bind_group,
                                uint32_t group_index);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `pass` | `WGPURenderPassEncoder` | Active render pass encoder. |
| `bind_group` | `WGPUBindGroup` | Bind group containing the camera buffer. |
| `group_index` | `uint32_t` | The `@group(N)` index to bind to. |

Sets the given bind group at the specified `group_index` (e.g. `@group(0)`).

---

### cgfx_camera_destroy

Release the camera's GPU resources.

```c
CGFX_API void cgfx_camera_destroy(CgfxCamera *cam);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `cam` | `CgfxCamera *` | Camera to destroy. |

Releases the uniform buffer.

---

## Usage

### Matching WGSL shader

The shader must declare a uniform struct with two `mat4x4f` fields matching the GPU buffer layout:

```wgsl
struct Camera {
    projection: mat4x4f,
    view: mat4x4f,
};
@group(0) @binding(0) var<uniform> camera: Camera;
```

### Basic camera setup

```c
CgfxCamera cam = cgfx_camera_create(&ctx, &(CgfxCameraDesc){
    .fovy   = 45.0f,
    .eye    = {1.0f, 1.0f, -2.0f},
    .center = {0.0f, 0.0f,  0.0f},
});

// Create a bind group from the camera's buffer
WGPUBindGroup cam_bg = cgfx_bind_group_create_buffers(
    &ctx, &shader, 0, &cam.buffer, 1);

// In render loop:
while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();

    cgfx_camera_write(&ctx, &cam);

    CgfxFrame frame;
    if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.2, 1.0})) {
        wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
        cgfx_camera_bind(frame.render_pass, cam_bg, 0);
        cgfx_mesh_draw(frame.render_pass, &mesh);
        cgfx_frame_end(&ctx, &frame);
    }
}

cgfx_bind_group_destroy(cam_bg);
cgfx_camera_destroy(&cam);
```

### Animated camera

```c
float angle = 0.0f;

while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();
    angle += 0.01f;

    cgfx_camera_look_at(&cam,
        (vec3){cosf(angle) * 3.0f, 1.5f, sinf(angle) * 3.0f},
        (vec3){0.0f, 0.0f, 0.0f},
        (vec3){0.0f, 1.0f, 0.0f});
    cgfx_camera_write(&ctx, &cam);

    // ... render ...
}
```

!!! note "Coordinate system"
    cgfx uses a **left-handed** coordinate system with depth range **[0, 1]**, configured via `CGLM_FORCE_LEFT_HANDED` and `CGLM_FORCE_DEPTH_ZERO_TO_ONE` in CMake. This matches WebGPU's NDC conventions.

!!! note "fovy in degrees"
    The `fovy` parameter is in **degrees** for readability. It is converted to radians internally by cglm.
