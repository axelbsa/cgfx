# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
cmake . -B build
cmake --build build
./build/examples/triangle
```

## Architecture

This is a C23 rendering engine library (`cgfx`) wrapping WebGPU, with GLFW for windowing. The library is a static lib (`libcgfx.a`) that abstracts WebGPU's verbose boilerplate into a minimal C API. All public symbols use the `cgfx_` prefix.

### Library modules (`cgfx/`)

| Module | Purpose |
|--------|---------|
| `cgfx_ctx` | Context: window + device + queue + surface init/destroy |
| `cgfx_shader` | WGSL string → WGPUShaderModule (hides chained-struct pattern) |
| `cgfx_pipeline` | Render pipeline with zero-init defaults |
| `cgfx_frame` | Per-frame begin/end cycle (acquire texture, encoder, pass, submit, present) |
| `cgfx_buffer` | GPU vertex/index buffer creation (**stubbed — needs implementation**) |
| `cgfx_mesh` | CgfxVertex (pos+normal+uv) + CgfxMesh + vertex layout (**stubbed**) |
| `cgfx_primitives` | Plane, triangle, sphere, cube generators (**stubbed**) |
| `cgfx_internal.h` | Internal sync wrappers for async WebGPU requests |
| `cgfx.h` | Umbrella header — includes all modules |

Modules 1-4 (ctx, shader, pipeline, frame) are fully implemented with existing rendering code. Modules 5-7 (buffer, mesh, primitives) are stubbed with comprehensive TODO comments describing exact implementation steps.

### Design conventions

- **Transparent structs** — fields are public so users can access raw WebGPU handles (e.g., `ctx.device`)
- **Context passed by pointer** — no global state
- **Error handling** — functions return `bool`, errors go to stderr
- **Frame recording** — between `cgfx_frame_begin`/`cgfx_frame_end`, user records draw commands directly on `frame.render_pass` using raw WebGPU calls
- **Backend differences** — `#ifdef WEBGPU_BACKEND_WGPU`, `WEBGPU_BACKEND_DAWN`, `__EMSCRIPTEN__` are handled inside the library

### External dependencies (vendored in `vendor/`)

- `glfw` — windowing and input
- `webgpu` — WebGPU backend (wgpu-native by default, Dawn optional)
- `glfw3webgpu` — bridges GLFW to WebGPU surface creation

### CMake structure

- Root `CMakeLists.txt` — externals + `add_subdirectory(cgfx)` + `add_subdirectory(examples)`
- `cgfx/CMakeLists.txt` — static library with `PUBLIC` includes and links
- `examples/CMakeLists.txt` — `cgfx_add_example()` helper function, one-liner per example
