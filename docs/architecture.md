# Architecture

This page describes the internal structure of cgfx: how the modules fit together, the initialization and rendering flows, and the design principles that guide the API.

## Module Overview

cgfx is organized into focused modules, each in its own header/source pair. The umbrella header `cgfx.h` includes all of them.

| Module | Header | Purpose |
|--------|--------|---------|
| **Export** | `cgfx_export.h` | `CGFX_API` macro for shared library export/import |
| **Context** | `cgfx_ctx.h` | Window, WebGPU instance, device, queue, surface |
| **Shader** | `cgfx_shader.h` | WGSL compilation, bind group layouts, pipeline layout |
| **Pipeline** | `cgfx_pipeline.h` | Render pipeline with zero-init defaults |
| **Frame** | `cgfx_frame.h` | Per-frame begin/end cycle (acquire, encode, submit, present) |
| **Buffer** | `cgfx_buffer.h` | GPU buffer creation (vertex, index, uniform, mapping) |
| **Uniform** | `cgfx_uniform.h` | Uniform buffer + bind group + data pointer bundle |
| **Mesh** | `cgfx_mesh.h` | Vertex format, mesh creation, vertex layout, draw helper |
| **Camera** | `cgfx_camera.h` | Projection/view matrices with GPU uniform management |
| **Primitives** | `cgfx_primitives.h` | Plane, triangle, sphere, cube generators |
| **Loader** | `cgfx_loader.h` | Load geometry from LearnWebGPU text format (temporary) |

## Module Dependency Diagram

```
cgfx.h  (umbrella — includes everything)
  |
  +-- cgfx_ctx.h            [WebGPU, GLFW]
  |     |
  +-- cgfx_buffer.h         [ctx]
  |     |
  +-- cgfx_shader.h         [ctx, buffer]
  |     |
  +-- cgfx_pipeline.h       [ctx, shader]
  |     |
  +-- cgfx_frame.h          [ctx]
  |     |
  +-- cgfx_uniform.h        [ctx, buffer, shader]
  |     |
  +-- cgfx_mesh.h           [ctx, buffer]
  |     |
  +-- cgfx_camera.h         [cglm, ctx, buffer, shader]
  |     |
  +-- cgfx_primitives.h     [ctx, mesh]
  |     |
  +-- cgfx_loader.h         [mesh]
```

All modules depend on `cgfx_export.h` for the `CGFX_API` macro, omitted above for clarity.

The context (`cgfx_ctx.h`) is the foundation -- every other module takes a `const CgfxCtx *` as its first parameter. The buffer module sits above the context, and the shader module depends on both context and buffer (because `cgfx_shader_create_bind_group` takes `CgfxBuffer` pointers). The pipeline depends on the shader for layout information.

## Initialization Flow

`cgfx_ctx_init` performs the complete WebGPU initialization sequence in a single call:

```
cgfx_ctx_init(ctx, &desc)
|
|   1. glfwInit() + glfwCreateWindow()
|      - GLFW_NO_API (no OpenGL context)
|      - Dimensions from desc (default 1280x720)
|
|   2. wgpuCreateInstance()
|      - WebGPU instance for backend discovery
|
|   3. glfwGetWGPUSurface() (via glfw3webgpu)
|      - Platform-specific surface creation
|      - Vulkan on Linux, D3D12/Vulkan on Windows
|
|   4. wgpuInstanceRequestAdapter() [sync wrapper]
|      - Selects the best available GPU
|      - Power preference, surface compatibility
|
|   5. wgpuAdapterRequestDevice() [sync wrapper]
|      - Creates logical device with requested limits
|      - Registers error and device-lost callbacks
|
|   6. wgpuDeviceGetQueue()
|      - Obtains the default command queue
|
|   7. wgpuSurfaceConfigure()
|      - Queries preferred surface format
|      - Sets present mode (VSync by default)
|
|   8. Depth texture (optional)
|      - Created if desc.depth_buffer == true
|      - Matches surface dimensions
v
CgfxCtx {
    .window          = GLFWwindow*
    .device          = WGPUDevice
    .queue           = WGPUQueue
    .surface         = WGPUSurface
    .surface_format  = WGPUTextureFormat
    .depth_texture   = WGPUTexture (or NULL)
    .depth_texture_view = WGPUTextureView (or NULL)
    .depth_format    = WGPUTextureFormat
    .width           = uint32_t
    .height          = uint32_t
}
```

!!! note "External Window Support"
    `cgfx_ctx_init_external` skips GLFW entirely and creates a surface from a raw window handle (e.g., Win32 HWND). The resulting `CgfxCtx` has `window = NULL`, and `cgfx_ctx_is_running` always returns `true` for external contexts. This is used for embedding cgfx in editor viewports or host applications.

## Render Loop Flow

A typical frame follows this pattern:

```
while (cgfx_ctx_is_running(&ctx))
|
|   glfwPollEvents()            <-- Caller-owned: cgfx never calls this
|
|   cgfx_frame_begin(&ctx, &frame, clear_color)
|   |
|   |   1. wgpuSurfaceGetCurrentTexture()
|   |      - Acquire next swapchain image
|   |      - Returns false if unavailable (minimized)
|   |
|   |   2. wgpuDeviceCreateCommandEncoder()
|   |
|   |   3. wgpuCommandEncoderBeginRenderPass()
|   |      - Color attachment: surface texture, clear to clear_color
|   |      - Depth attachment: depth_texture_view (if enabled)
|   |      - Load op: Clear, Store op: Store
|   |
|   v   frame.render_pass is now active
|
|   --- User records draw commands on frame.render_pass ---
|   |
|   |   wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline)
|   |   wgpuRenderPassEncoderSetVertexBuffer(...)
|   |   wgpuRenderPassEncoderSetBindGroup(...)
|   |   wgpuRenderPassEncoderDraw(...)
|   |
|   v
|
|   cgfx_frame_end(&ctx, &frame)
|   |
|   |   1. wgpuRenderPassEncoderEnd()
|   |   2. wgpuCommandEncoderFinish()  --> WGPUCommandBuffer
|   |   3. wgpuQueueSubmit(queue, buffer)
|   |   4. Release encoder, render pass, command buffer
|   |   5. Release surface texture view
|   |   6. wgpuSurfacePresent()  (skipped on Emscripten)
|   |   7. Backend sync:
|   |      - Dawn:       wgpuDeviceTick()
|   |      - wgpu-native: wgpuDevicePoll()
|   v
v
```

!!! tip "Frame Skipping"
    `cgfx_frame_begin` returns `false` when the surface texture is unavailable (typically when the window is minimized). Always check the return value and skip the frame -- do not call `cgfx_frame_end` without a successful begin.

## Cleanup Flow

Resources are destroyed in reverse creation order:

```
Application cleanup (reverse order of creation):

    1. cgfx_uniform_destroy(&uniform)     -- releases bind group + buffer
    2. wgpuRenderPipelineRelease(pipeline) -- raw WebGPU handle
    3. cgfx_shader_destroy(&shader)        -- releases module + layouts
    4. cgfx_ctx_destroy(&ctx)              -- releases everything below

cgfx_ctx_destroy(&ctx):
    |
    |   1. wgpuQueueRelease(queue)
    |   2. wgpuSurfaceUnconfigure(surface)
    |   3. wgpuSurfaceRelease(surface)
    |   4. wgpuDeviceRelease(device)
    |   5. glfwDestroyWindow(window)   (skipped for external contexts)
    |   6. glfwTerminate()             (skipped for external contexts)
    v
```

!!! warning "Destruction Order Matters"
    Always destroy resources before the context. The context owns the WebGPU device -- destroying it first would invalidate all GPU resources created from it, leading to undefined behavior.

## Design Principles

### Pure C23

cgfx is written in C23 with no C++ dependencies. It compiles with `-std=c23` (GCC/Clang) or `/std:c23` (MSVC). C23 features used include compound literals with designated initializers, `nullptr`, and `= {}` zero-initialization.

### Transparent Structs

All cgfx structs have public fields. There are no opaque handles or accessor functions -- you can read and write any field directly:

```c
// Access raw WebGPU handles directly
WGPUDevice dev = ctx.device;
WGPUQueue q = ctx.queue;

// Access shader internals
WGPUBindGroupLayout layout = shader.group_layouts[0];
WGPUPipelineLayout pl = shader.pipeline_layout;
```

This makes cgfx a thin layer rather than a walled garden. When cgfx does not wrap something you need, you can drop down to raw WebGPU using the handles stored in cgfx structs.

### Zero-Init Defaults

Every descriptor struct is designed so that `= {}` (all zeros) produces sensible defaults:

```c
// All defaults: 1280x720 window titled "cgfx", VSync, no depth buffer
cgfx_ctx_init(&ctx, &(CgfxCtxDesc){});

// All defaults: triangle list, no culling, alpha blending, vs_main/fs_main
cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){ .shader = &shader });
```

This convention means you only specify the fields you care about. Zero is always a valid, reasonable value -- never an error case.

### No Global State

All state is stored in `CgfxCtx` and passed by pointer. There are no global variables, singletons, or hidden initialization. You could theoretically create multiple contexts (though this is not a tested configuration with a single GLFW instance).

### Caller-Owned Event Loop

cgfx never calls `glfwPollEvents()`, `glfwWaitEvents()`, or any windowing event function. The caller is responsible for polling events before calling `cgfx_frame_begin`. This gives the application full control over event timing, input handling, and frame pacing.

```c
// The caller decides when and how to poll events
while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();   // <-- your responsibility
    // ... render ...
}
```

### Thin Wrapper

cgfx wraps the boilerplate (initialization, frame management, buffer creation, pipeline defaults) but does not wrap draw commands. Between `cgfx_frame_begin` and `cgfx_frame_end`, you use raw WebGPU calls directly:

```c
wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
wgpuRenderPassEncoderSetVertexBuffer(frame.render_pass, 0, vbuf, 0, size);
wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
```

This means any WebGPU tutorial, reference, or example code applies directly to the draw recording phase. cgfx gets out of the way where it matters most.

### Shader Owns Layouts, Caller Owns Bind Groups

`CgfxShader` owns the `WGPUBindGroupLayout` array and `WGPUPipelineLayout`. Bind groups are created from the shader's layouts but returned to the caller for per-object flexibility. This enables the "same shader, different uniforms per object" pattern. See the [Shader & Bind Group Architecture](guides/shader-bind-groups.md) guide for details.

## Backend Differences

cgfx supports multiple WebGPU backends. The differences are handled internally via preprocessor defines:

| Define | Backend | Notes |
|--------|---------|-------|
| `WEBGPU_BACKEND_WGPU` | wgpu-native (Rust) | Default backend. Uses `wgpuDevicePoll()` for synchronization. |
| `WEBGPU_BACKEND_DAWN` | Dawn (Google/Chrome) | Uses `wgpuDeviceTick()` for synchronization. |
| `__EMSCRIPTEN__` | Browser WebGPU | Skips `wgpuSurfacePresent()` (browser handles it). No device poll/tick needed. |

### What the backend affects

**Device synchronization** -- After submitting a command buffer, the CPU must wait for or poll GPU completion. wgpu-native uses `wgpuDevicePoll(device, false, NULL)`, while Dawn uses `wgpuDeviceTick(device)`. This is called automatically in `cgfx_frame_end`.

**Surface presentation** -- On native backends, `wgpuSurfacePresent(surface)` is called after submit. On Emscripten, the browser's requestAnimationFrame loop handles presentation, so this call is skipped.

**Async request wrappers** -- WebGPU adapter and device requests are asynchronous. cgfx wraps them in synchronous helpers (`cgfx_internal.h`) that block until the callback fires. The callback mechanism differs slightly between wgpu-native and Dawn, handled via `#ifdef` blocks.

**Surface texture handling** -- The method for acquiring and releasing surface textures varies slightly between backends, particularly around error handling for lost or outdated surfaces.

!!! note "Backend Selection"
    The backend is selected at CMake configure time based on the `webgpu` vendored dependency. By default, cgfx uses wgpu-native. To switch to Dawn, replace the `vendor/webgpu` directory with a Dawn distribution and ensure the CMake targets match.
