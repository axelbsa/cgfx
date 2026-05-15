cgfx
====

A minimal C23 rendering engine library wrapping WebGPU. Abstracts away the verbose WebGPU boilerplate into a small, simple C API while keeping full access to raw WebGPU handles when needed.

Built on top of [wgpu-native](https://github.com/gfx-rs/wgpu-native) (with optional Dawn backend) and GLFW for windowing.

Quick Start
-----------

```bash
git clone --recursive https://github.com/axelbsa/cgfx.git
cd cgfx
cmake . -B build
cmake --build build
./build/examples/triangle
```

Requires CMake 3.0+ and a C23-capable compiler (GCC 13+, Clang 16+, MSVC 2022).

Documentation
-------------

Full documentation is available at **https://axelbsa.github.io/cgfx/** — including build options, architecture overview, guides, and API reference.

To build and serve locally:

```bash
pipx install mkdocs-material --include-deps
mkdocs serve
```

Then open http://localhost:8000.

To deploy to GitHub Pages:

```bash
mkdocs gh-deploy
```

This builds the site and pushes it to the `gh-pages` branch. GitHub Pages serves from that branch automatically — no CI workflow required.

Example
-------

```c
#include "cgfx.h"

int main(void) {
    CgfxCtx ctx;
    cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280, .height = 720,
        .title = "hello cgfx",
        .limits = cgfx_default_limits(),
    });

    CgfxShader shader = cgfx_shader_create(&ctx, "tri", wgsl_source,
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

    wgpuRenderPipelineRelease(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);
}
```
