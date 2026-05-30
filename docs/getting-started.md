# Getting Started

This guide walks you through building cgfx from source and rendering your first triangle. By the end, you will have a window displaying a purple triangle powered by WebGPU.

## Prerequisites

Before you begin, make sure you have the following installed:

| Tool | Minimum Version | Notes |
|------|----------------|-------|
| **CMake** | 3.0+ | Build system generator |
| **C23 compiler** | GCC 13+, Clang 16+, or MSVC 2022 | Must support C23 features |
| **Git** | Any recent version | For cloning the repository |

!!! note "C23 Compiler Support"
    cgfx uses C23 features including compound literals with designated initializers, `nullptr`, and `= {}` zero-initialization. If your compiler does not support C23, the build will fail. On MSVC, the `/std:c23` flag is set automatically by the CMake configuration.

## Clone & Build

Clone the repository with its submodules and build:

```bash
git clone --recursive https://github.com/axelbsa/cgfx.git
cd cgfx
cmake . -B build
cmake --build build
```

Run the triangle example to verify everything works:

```bash
./build/examples/triangle
```

You should see a window with a purple triangle on a dark background.

!!! tip "Linux Users"
    On Linux, you may need X11 or Wayland development headers installed for GLFW. On Debian/Ubuntu: `sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libwayland-dev`.

## Your First Triangle

Let's walk through the triangle example step by step. This is the simplest possible cgfx application: create a context, a shader, a pipeline, and render in a loop.

### The Complete Program

```c
#include "cgfx.h"
#include <GLFW/glfw3.h>

static const char *wgsl =
    "@vertex fn vs_main(@builtin(vertex_index) idx : u32) -> @builtin(position) vec4f {\n"
    "    var pos = array<vec2f, 3>(vec2f(0.0, 0.5), vec2f(-0.5, -0.5), vec2f(0.5, -0.5));\n"
    "    return vec4f(pos[idx], 0.0, 1.0);\n"
    "}\n"
    "@fragment fn fs_main() -> @location(0) vec4f {\n"
    "    return vec4f(0.8, 0.4, 1.0, 1.0);\n"
    "}\n";

int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width  = 1920,
        .height = 1080,
        .title  = "cgfx — triangle",
        .limits = cgfx_default_limits(),
    })) {
        return 1;
    }

    CgfxShader shader = cgfx_shader_create(&ctx, "triangle shader", wgsl,
        &(CgfxShaderDesc){});

    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
    });

    while (cgfx_ctx_is_running(&ctx)) {
        glfwPollEvents();
        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.2, 1.0})) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
            cgfx_frame_end(&ctx, &frame);
        }
    }

    cgfx_pipeline_destroy(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
```

Now let's break it down piece by piece.

### Step 1: Initialize the Context

```c
CgfxCtx ctx;
if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
    .width  = 1920,
    .height = 1080,
    .title  = "cgfx — triangle",
    .limits = cgfx_default_limits(),
})) {
    return 1;
}
```

`CgfxCtx` is the central object in cgfx. It holds the GLFW window, WebGPU device, command queue, surface, and everything else needed for rendering.

`cgfx_ctx_init` performs the entire WebGPU initialization sequence behind the scenes:

1. Initializes GLFW and creates a window
2. Creates a WebGPU instance
3. Creates a platform-specific surface (Vulkan on Linux, D3D12/Vulkan on Windows, Metal on macOS)
4. Requests a GPU adapter (selects the best available GPU)
5. Creates a logical device with the requested limits
6. Obtains the command queue
7. Configures the surface for rendering

The configuration is passed as a compound literal `CgfxCtxDesc`. Any field you omit gets a sensible default -- for example, `width = 0` defaults to 1280, and `title = NULL` defaults to `"cgfx"`. The `cgfx_default_limits()` function returns a `WGPURequiredLimits` where every limit is set to "no preference," letting the driver choose.

!!! note "Transparent Structs"
    All cgfx structs are transparent -- their fields are public. After initialization, you can access raw WebGPU handles directly: `ctx.device`, `ctx.queue`, `ctx.surface`, etc. This lets you use the full WebGPU API alongside cgfx when needed.

### Step 2: Create a Shader

```c
CgfxShader shader = cgfx_shader_create(&ctx, "triangle shader", wgsl,
    &(CgfxShaderDesc){});
```

`cgfx_shader_create` compiles a WGSL source string into a `WGPUShaderModule`. The second argument is a debug label (useful for GPU error messages), and the third is the WGSL source code.

The fourth argument is the shader descriptor, which describes any bind group layouts the shader needs. For this simple triangle we have no uniform buffers, so we pass an empty `CgfxShaderDesc{}`. This results in `pipeline_layout = NULL`, which tells WebGPU to use automatic layout inference.

#### What is WGSL?

WGSL (WebGPU Shading Language) is the shader language for WebGPU. It replaces GLSL/HLSL/MSL with a single cross-platform language. Here is the shader used in this example:

```wgsl
@vertex fn vs_main(@builtin(vertex_index) idx : u32) -> @builtin(position) vec4f {
    var pos = array<vec2f, 3>(vec2f(0.0, 0.5), vec2f(-0.5, -0.5), vec2f(0.5, -0.5));
    return vec4f(pos[idx], 0.0, 1.0);
}

@fragment fn fs_main() -> @location(0) vec4f {
    return vec4f(0.8, 0.4, 1.0, 1.0);
}
```

- The **vertex shader** (`vs_main`) runs once per vertex. It uses the built-in `vertex_index` to look up a position from a hardcoded array -- no vertex buffer needed. The three positions define a triangle in clip space (coordinates from -1 to +1).
- The **fragment shader** (`fs_main`) runs once per pixel covered by the triangle. It returns a solid purple color (RGBA 0.8, 0.4, 1.0, 1.0).

### Step 3: Create a Pipeline

```c
WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
    .shader = &shader,
});
```

A render pipeline defines how vertices are processed and pixels are drawn. In raw WebGPU, creating a pipeline requires filling out a large descriptor with vertex state, fragment state, blend mode, topology, multisample settings, and more.

`cgfx_pipeline_create` provides sensible zero-init defaults for all of this:

- **Topology**: Triangle list
- **Culling**: None (both faces visible)
- **Front face**: Counter-clockwise
- **Blending**: Opaque (no blending) -- opt in via `CgfxColorTarget`
- **Entry points**: `"vs_main"` / `"fs_main"`
- **Layout**: Taken from `shader->pipeline_layout` (automatic for this example)

The only required field is `.shader`.

### Step 4: The Render Loop

```c
while (cgfx_ctx_is_running(&ctx)) {
    glfwPollEvents();
    CgfxFrame frame;
    if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.2, 1.0})) {
        wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
        wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
        cgfx_frame_end(&ctx, &frame);
    }
}
```

The render loop has four parts:

1. **`cgfx_ctx_is_running(&ctx)`** -- Returns `true` as long as the window is open. When the user closes the window (clicking X, pressing Alt+F4), this returns `false` and the loop exits.

2. **`glfwPollEvents()`** -- Processes window events (input, resize, close). cgfx never calls this for you -- the caller owns the event loop. This is a deliberate design choice that gives you full control over event timing.

3. **`cgfx_frame_begin(&ctx, &frame, clear_color)`** -- Acquires the next surface texture, creates a command encoder, and begins a render pass cleared to the specified color. Returns `false` if the surface is unavailable (e.g., window is minimized), in which case you skip the frame.

4. **Draw commands + `cgfx_frame_end`** -- Between begin and end, you record draw commands directly on `frame.render_pass` using raw WebGPU calls. cgfx does not wrap draw commands -- you use `wgpuRenderPassEncoderSetPipeline`, `wgpuRenderPassEncoderDraw`, etc. directly. When you call `cgfx_frame_end`, cgfx ends the render pass, submits the command buffer, presents the surface, and performs backend-specific synchronization.

!!! tip "Why raw WebGPU draw calls?"
    cgfx wraps the boilerplate (initialization, frame management, buffer creation) but leaves draw recording to you. This means you get the full power of WebGPU without cgfx becoming a bottleneck or limiting what you can do. Any WebGPU tutorial or reference applies directly to the code between `frame_begin` and `frame_end`.

### Step 5: Cleanup

```c
cgfx_pipeline_destroy(pipeline);
cgfx_shader_destroy(&shader);
cgfx_ctx_destroy(&ctx);
```

Resources are destroyed in reverse creation order. All cgfx-created resources use `cgfx_*_destroy()` for cleanup.

!!! warning "Destruction Order"
    Always destroy resources in reverse creation order. Destroying the context first would invalidate the device, making it impossible to properly release the pipeline and shader.

## Next Steps

Now that you have a triangle on screen, here are some directions to explore:

- **[Architecture](architecture.md)** -- Understand the module structure, initialization flow, and design principles behind cgfx.
- **[Shader & Bind Group Architecture](guides/shader-bind-groups.md)** -- Learn how to pass uniform data to shaders and render multiple objects with different parameters.
- **[Building](building.md)** -- Shared library builds, CMake integration, and platform-specific notes.
