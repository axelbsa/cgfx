# cgfx

A minimal C23 rendering engine library wrapping WebGPU. Abstracts away the verbose WebGPU boilerplate into a small, simple C API while keeping full access to raw WebGPU handles when needed.

Built on top of [wgpu-native](https://github.com/gfx-rs/wgpu-native) (with optional Dawn backend) and [GLFW](https://www.glfw.org/) for windowing.

---

## At a Glance

- **Pure C23**
- **Transparent structs** -- access raw WebGPU handles (`ctx.device`, `shader.group_layouts[0]`) for anything cgfx doesn't wrap
- **Zero-init defaults** -- `CgfxPipelineDesc desc = { .shader = &s };` gives you working defaults
- **No global state** -- context passed by pointer
- **Caller-owned event loop** -- cgfx never calls windowing event functions
- **Thin wrapper** -- cgfx manages boilerplate; you record draw commands with raw WebGPU calls

---

## Minimal Example

```c
#include "cgfx.h"

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
    cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280, .height = 720,
        .title = "hello cgfx",
        .limits = cgfx_default_limits(),
    });

    CgfxShader shader = cgfx_shader_create(&ctx, "tri", wgsl, &(CgfxShaderDesc){});
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
}
```

---

## Modules

| Module | Header | Description |
|--------|--------|-------------|
| [Context](api/context.md) | `cgfx_ctx.h` | Window, device, queue, surface init and teardown |
| [Shader](api/shader.md) | `cgfx_shader.h` | WGSL compilation, bind group layouts, pipeline layout |
| [Pipeline](api/pipeline.md) | `cgfx_pipeline.h` | Render pipeline with zero-init defaults |
| [Frame](api/frame.md) | `cgfx_frame.h` | Per-frame begin/end cycle |
| [Buffer](api/buffer.md) | `cgfx_buffer.h` | GPU buffer creation (vertex, index, uniform, generic) |
| [Uniform](api/uniform.md) | `cgfx_uniform.h` | Uniform buffer + bind group bundle |
| [Mesh](api/mesh.md) | `cgfx_mesh.h` | Standard vertex format, mesh creation, indexed draw |
| [Camera](api/camera.md) | `cgfx_camera.h` | Projection/view matrices with GPU uniform management |

| [Loader](api/loader.md) | `cgfx_loader.h` | Load geometry from tutorial text format |
| [Export](api/export.md) | `cgfx_export.h` | `CGFX_API` macro for shared library builds |

---

## Dependencies

| Library | Purpose | Vendored |
|---------|---------|----------|
| [GLFW](https://www.glfw.org/) | Windowing and input | Yes (`vendor/glfw`) |
| [wgpu-native](https://github.com/gfx-rs/wgpu-native) | WebGPU backend | Yes (`vendor/webgpu`) |
| [cglm](https://github.com/recp/cglm) | Math (matrices, vectors) | Yes (`vendor/cglm`) |
| [glfw3webgpu](https://github.com/AcademicNet/glfw3webgpu) | GLFW-to-WebGPU surface bridge | Yes (`vendor/glfw3webgpu`) |

---

## Quick Start

```bash
git clone https://github.com/axelbsa/cgfx.git
cd cgfx
cmake . -B build
cmake --build build
./build/examples/triangle
```

See the [Getting Started](getting-started.md) guide for a full walkthrough.
