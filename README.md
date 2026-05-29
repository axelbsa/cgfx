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

Build Options
-------------

By default cgfx builds as a static library (`libcgfx.a` / `cgfx.lib`).

```bash
# Shared library (libcgfx.so / cgfx.dll / libcgfx.dylib), for C# P/Invoke
# or faster dev iteration:
cmake . -B build -DCGFX_SHARED=ON
cmake --build build
```

cgfx is designed to be consumed as a CMake subdirectory. `add_subdirectory(vendor/cgfx)` then `target_link_libraries(my_app PRIVATE cgfx)` brings in GLFW, WebGPU, and cglm automatically. See the [Building guide](https://axelbsa.github.io/cgfx/building/) for platform notes, the `CGFX_API` macro, and integration details.

Documentation
-------------

Full documentation lives at **https://axelbsa.github.io/cgfx/**, covering getting started, build options, architecture, guides, and the complete API reference. Runnable examples for every feature are in the [`examples/`](examples/) directory.

To build and serve the docs locally:

```bash
pipx install mkdocs-material --include-deps
mkdocs serve   # then open http://localhost:8000
```

`mkdocs gh-deploy` builds the site and pushes it to the `gh-pages` branch, which GitHub Pages serves automatically.

Usage
-----

A complete triangle: initialize a context, compile a shader, create a pipeline, and draw inside the per-frame begin/end cycle.

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

Between `cgfx_frame_begin` and `cgfx_frame_end` you record draw commands directly on `frame.render_pass` with raw WebGPU calls, and cgfx handles the boilerplate around it. For meshes, uniforms, textures, compute, and cameras, see the [Getting Started](https://axelbsa.github.io/cgfx/getting-started/) guide.

Design
------

- **Pure C23**, no C++ required
- **Transparent structs**, access raw WebGPU handles (e.g. `ctx.device`, `shader.group_layouts[0]`) for anything cgfx doesn't wrap
- **Zero-init defaults**, `CgfxPipelineDesc desc = { .shader = &s };` gives you working defaults
- **No global state**, context is passed by pointer
- **Caller-owned event loop**, cgfx never calls windowing event functions; the caller polls events before `cgfx_frame_begin`
- **Thin wrapper**, cgfx manages boilerplate, you record draw commands with raw WebGPU calls

Project structure
-----------------

```
cgfx/        Library (static, or shared via -DCGFX_SHARED=ON)
examples/    Example applications (one directory each)
docs/        Documentation source (MkDocs)
vendor/      Vendored dependencies (glfw, webgpu, glfw3webgpu, hwnd3webgpu, cglm)
```
