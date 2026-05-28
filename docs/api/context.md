# Context

Window, WebGPU device, queue, and surface management.

**Header:** `cgfx_ctx.h`

---

## Structs

### CgfxCtxDesc

Configuration for creating a cgfx context. Zero-initialize for sensible defaults.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `width` | `uint32_t` | `1280` | Window width in pixels. |
| `height` | `uint32_t` | `720` | Window height in pixels. |
| `title` | `const char*` | `"cgfx"` | Window title string. |
| `resizable` | `bool` | `false` | Allow window resize. |
| `depth_buffer` | `bool` | `false` | Create a depth buffer at surface dimensions. |
| `present_mode` | `WGPUPresentMode` | `Fifo` (VSync) | Surface present mode. |
| `limits` | `WGPURequiredLimits` | see note | Device limits. Use `cgfx_default_limits()`. |

!!! tip "Always call `cgfx_default_limits()` for the limits field"
    Setting all limits to `0` requests minimum device limits, which may be too restrictive. `cgfx_default_limits()` sets every field to "undefined" (no preference), letting the device use its own defaults.

---

### CgfxCtx

The central rendering context. Owns the GLFW window and all core WebGPU objects.

| Field | Type | Description |
|-------|------|-------------|
| `window` | `GLFWwindow*` | The GLFW window handle. `NULL` for external contexts. |
| `device` | `WGPUDevice` | The logical GPU device. |
| `queue` | `WGPUQueue` | The default command queue. |
| `surface` | `WGPUSurface` | The window surface for presenting frames. |
| `surface_format` | `WGPUTextureFormat` | The preferred surface texture format. |
| `depth_texture` | `CgfxTexture` | Depth buffer. Zero-initialized if depth buffer is disabled. Access `depth_texture.view` for the view, `depth_texture.format` for the format. |
| `present_mode` | `WGPUPresentMode` | Active present mode (stored for use during resize). |
| `width` | `uint32_t` | Current window width in pixels. |
| `height` | `uint32_t` | Current window height in pixels. |

!!! note "Caller allocates CgfxCtx"
    The caller allocates `CgfxCtx` on the stack or heap -- cgfx does not allocate it. Typically declared as a local variable.

---

### CgfxCtxExternalDesc

Configuration for creating a context from an externally-owned window handle (e.g., Win32 HWND).

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `native_handle` | `void*` | *(required)* | Platform window handle (HWND on Windows). |
| `width` | `uint32_t` | *(required)* | Render surface width in pixels. |
| `height` | `uint32_t` | *(required)* | Render surface height in pixels. |
| `depth_buffer` | `bool` | `false` | Create a depth buffer at surface dimensions. |
| `present_mode` | `WGPUPresentMode` | `Fifo` (VSync) | Surface present mode. |
| `limits` | `WGPURequiredLimits` | see note | Device limits. Use `cgfx_default_limits()`. |

---

## Functions

### cgfx_default_limits

Returns a `WGPURequiredLimits` with every limit set to "undefined", meaning "use device defaults".

```c
CGFX_API WGPURequiredLimits cgfx_default_limits(void);
```

**Returns:** A `WGPURequiredLimits` struct safe to pass directly to `CgfxCtxDesc.limits`.

**Example:**

```c
WGPURequiredLimits limits = cgfx_default_limits();
limits.limits.maxVertexAttributes = 4;

CgfxCtxDesc desc = {
    .limits = limits,
};
```

---

### cgfx_ctx_init

Initializes a full cgfx context: GLFW window, WebGPU instance, adapter, device, queue, surface, and optional depth buffer.

```c
CGFX_API bool cgfx_ctx_init(CgfxCtx *ctx, const CgfxCtxDesc *desc);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `CgfxCtx*` | Pointer to a caller-allocated `CgfxCtx`. |
| `desc` | `const CgfxCtxDesc*` | Configuration. Zero-initialize for defaults. |

**Returns:** `true` on success, `false` on failure (errors printed to `stderr`).

**Initialization sequence:**

1. Initialize GLFW and create a window (`GLFW_NO_API` -- no OpenGL context)
2. Create a WebGPU instance
3. Create a platform-specific surface via `glfwGetWGPUSurface`
4. Request a GPU adapter (synchronous wrapper around async callback)
5. Request a logical device from the adapter
6. Register error and device-lost callbacks (print to `stderr`)
7. Get the default queue from the device
8. Query the preferred surface format and configure the surface
9. Create depth buffer if `desc->depth_buffer` is `true`

**Example:**

```c
CgfxCtx ctx;
if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
    .width  = 1920,
    .height = 1080,
    .title  = "My Application",
    .limits = cgfx_default_limits(),
})) {
    return 1;
}

// use ctx...

cgfx_ctx_destroy(&ctx);
```

---

### cgfx_ctx_init_external

Initializes a cgfx context from an externally-owned window handle, without GLFW. Currently Windows-only (HWND).

```c
CGFX_API bool cgfx_ctx_init_external(CgfxCtx *ctx, const CgfxCtxExternalDesc *desc);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `CgfxCtx*` | Pointer to a caller-allocated `CgfxCtx`. |
| `desc` | `const CgfxCtxExternalDesc*` | Configuration with the native window handle. |

**Returns:** `true` on success, `false` on failure. Returns `false` on non-Windows platforms.

After this call:

- `ctx->window` is `NULL` (no GLFW window)
- `cgfx_ctx_is_running` always returns `true` (the caller decides lifetime)
- `cgfx_ctx_destroy` skips GLFW teardown

**Example (Win32):**

```c
// Embed cgfx rendering in a WinForms panel or editor viewport
CgfxCtx ctx;
cgfx_ctx_init_external(&ctx, &(CgfxCtxExternalDesc){
    .native_handle = (void*)hwnd,
    .width         = 800,
    .height        = 600,
    .limits        = cgfx_default_limits(),
});
```

---

### cgfx_ctx_is_running

Checks whether the context window is still open.

```c
CGFX_API bool cgfx_ctx_is_running(const CgfxCtx *ctx);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |

**Returns:** `false` when the window close has been requested (e.g., clicking X, pressing Alt+F4). Always returns `true` for external contexts.

**Example:**

```c
while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();
    // render frame...
}
```

---

### cgfx_ctx_resize

Reconfigures the WebGPU surface and recreates the depth buffer (if one exists) at new dimensions.

```c
CGFX_API bool cgfx_ctx_resize(CgfxCtx *ctx, uint32_t width, uint32_t height);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `CgfxCtx*` | Initialized context. |
| `width` | `uint32_t` | New framebuffer width in pixels. |
| `height` | `uint32_t` | New framebuffer height in pixels. |

**Returns:** `true` on success. `false` if width or height is zero (minimized window), in which case no changes are made.

Call this from your resize handler — cgfx does not register any resize callbacks internally. For GLFW windows, use `glfwSetFramebufferSizeCallback`. For external windows, call from your platform resize handler (e.g., `WM_SIZE` on Windows).

!!! note "Camera projection"
    `cgfx_ctx_resize` does **not** update camera projection matrices. If you have a `CgfxCamera`, call `cgfx_camera_perspective()` with the new aspect ratio after resizing.

**Example (GLFW):**

```c
static void on_resize(GLFWwindow *window, int width, int height) {
    CgfxCtx *ctx = glfwGetWindowUserPointer(window);
    cgfx_ctx_resize(ctx, (uint32_t)width, (uint32_t)height);
}

int main(void) {
    CgfxCtx ctx;
    cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280, .height = 720,
        .resizable = true,
        .limits = cgfx_default_limits(),
    });

    glfwSetWindowUserPointer(ctx.window, &ctx);
    glfwSetFramebufferSizeCallback(ctx.window, on_resize);

    // ... render loop ...
}
```

**Example (Win32 external window):**

```c
case WM_SIZE: {
    UINT w = LOWORD(lParam);
    UINT h = HIWORD(lParam);
    cgfx_ctx_resize(&ctx, w, h);
    break;
}
```

---

### cgfx_ctx_destroy

Releases all context resources in reverse creation order.

```c
CGFX_API void cgfx_ctx_destroy(CgfxCtx *ctx);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `CgfxCtx*` | Context to destroy. Must not be used after this call. |

**Teardown sequence:**

1. Destroy the depth texture (if enabled)
2. Release the command queue
3. Unconfigure and release the surface
4. Release the device
5. Destroy the GLFW window (skipped for external contexts)
6. Terminate GLFW (skipped for external contexts)

!!! warning "Destroy order matters"
    Destroy all pipelines, shaders, buffers, uniforms, meshes, and cameras **before** calling `cgfx_ctx_destroy`. The device is released during context destruction, and any outstanding GPU resources become invalid.
