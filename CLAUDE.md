# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
cmake . -B build
cmake --build build
./build/examples/multiple_uniforms
```

## Architecture

This is a C23 rendering engine library (`cgfx`) wrapping WebGPU, with GLFW for windowing. The library is a static lib (`libcgfx.a`) that abstracts WebGPU's verbose boilerplate into a minimal C API. All public symbols use the `cgfx_` prefix.

### Library modules (`cgfx/`)

| Module | Purpose |
|--------|---------|
| `cgfx_ctx` | Context: window + device + queue + surface init/destroy |
| `cgfx_shader` | CgfxShader: WGSL compilation + bind group layouts + pipeline layout |
| `cgfx_pipeline` | Render pipeline with zero-init defaults, reads layout from CgfxShader |
| `cgfx_frame` | Per-frame begin/end cycle (acquire texture, encoder, pass, submit, present) |
| `cgfx_buffer` | GPU buffer creation (vertex, index, uniform, mapping, generic) |
| `cgfx_mesh` | CgfxVertex (pos+normal+color+uv) + CgfxMesh + vertex layout + draw |
| `cgfx_primitives` | Plane, triangle, sphere, cube generators (**stubbed**) |
| `cgfx_loader` | Load geometry from LearnWebGPU text format (temporary) |
| `cgfx_internal.h` | Internal sync wrappers for async WebGPU requests |
| `cgfx.h` | Umbrella header — includes all modules |

### Shader and bind group architecture

`CgfxShader` owns the `WGPUShaderModule`, bind group layouts (`WGPUBindGroupLayout[]`), and pipeline layout (`WGPUPipelineLayout`). Bind groups themselves are **not** stored on the shader — they are created via `cgfx_shader_create_bind_group()` and owned by the caller. This enables the "same shader, different uniforms per object" pattern.

The pipeline reads `shader->pipeline_layout` automatically. When a shader has no bind groups (desc is NULL), `pipeline_layout` is NULL and WebGPU uses automatic layout inference.

Key types: `CgfxBindingDesc` → `CgfxGroupDesc` → `CgfxShaderDesc` → `CgfxShader`.

### Design conventions

- **Transparent structs** — fields are public so users can access raw WebGPU handles (e.g., `ctx.device`, `shader.group_layouts[0]`)
- **Context passed by pointer** — no global state
- **Error handling** — functions return `bool`, errors go to stderr
- **Frame recording** — between `cgfx_frame_begin`/`cgfx_frame_end`, user records draw commands directly on `frame.render_pass` using raw WebGPU calls
- **Backend differences** — `#ifdef WEBGPU_BACKEND_WGPU`, `WEBGPU_BACKEND_DAWN`, `__EMSCRIPTEN__` are handled inside the library
- **Shader owns layouts, caller owns bind groups** — bind groups are created from the shader's layouts but returned to the user for per-object flexibility

### External dependencies (vendored in `vendor/`)

- `glfw` — windowing and input
- `webgpu` — WebGPU backend (wgpu-native by default, Dawn optional)
- `glfw3webgpu` — bridges GLFW to WebGPU surface creation

### CMake structure

- Root `CMakeLists.txt` — externals + `add_subdirectory(cgfx)` + `add_subdirectory(examples)`
- `cgfx/CMakeLists.txt` — static library with `PUBLIC` includes and links
- `examples/CMakeLists.txt` — `cgfx_add_example()` helper function, one-liner per example
