# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
cmake . -B build
cmake --build build
./build/examples/multiple_uniforms

# Shared library build (for C# P/Invoke or faster dev iteration):
cmake . -B build -DCGFX_SHARED=ON
```

## Architecture

This is a C23 rendering engine library (`cgfx`) wrapping WebGPU, with GLFW for windowing. Built as a static lib by default (`libcgfx.a`), or shared (`libcgfx.so`/`cgfx.dll`) with `-DCGFX_SHARED=ON`. Abstracts WebGPU's verbose boilerplate into a minimal C API. All public symbols use the `cgfx_` prefix and are decorated with `CGFX_API` for shared library export/import.

### Library modules (`cgfx/`)

| Module | Purpose |
|--------|---------|
| `cgfx_export` | `CGFX_API` macro for shared library export/import (`dllexport`/`visibility`) |
| `cgfx_ctx` | Context: `cgfx_ctx_init` (GLFW) or `cgfx_ctx_init_external` (HWND) + device + queue + surface. Optional depth buffer stored as `CgfxTexture`. |
| `cgfx_shader` | CgfxShader: WGSL compilation + bind group layouts + pipeline layout |
| `cgfx_pipeline` | Render pipeline with zero-init defaults, reads layout from CgfxShader |
| `cgfx_frame` | Per-frame begin/end cycle (acquire texture, encoder, pass, submit, present) |
| `cgfx_buffer` | GPU buffer creation (vertex, index, uniform, mapping, generic) |
| `cgfx_uniform` | CgfxUniform: buffer + bind group + data pointer bundle for per-object uniforms |
| `cgfx_mesh` | CgfxVertex (96 bytes: pos + normal + tangent + texcoord0 + texcoord1 + color + joints + weights) + CgfxMesh + vertex layout + draw |
| `cgfx_texture` | CgfxTexture (GPU texture + view) + sampler helper. Supports sampled, storage, render-target, and depth textures. Cube maps via `view_dimension` + `depth=6`. Per-layer writes with `cgfx_texture_write_layer()`. |
| `cgfx_primitives` | Plane, triangle, sphere, cube generators (**stubbed**) |
| `cgfx_camera` | CgfxCamera: projection + view matrices, perspective and look-at helpers (uses cglm, left-handed, depth [0,1]) |
| `cgfx_loader` | Load geometry from LearnWebGPU text format (temporary) |
| `cgfx_internal.h` | Internal sync wrappers for async WebGPU requests |
| `cgfx.h` | Umbrella header — includes all modules |

### Shader and bind group architecture

`CgfxShader` owns the `WGPUShaderModule`, bind group layouts (`WGPUBindGroupLayout[]`), and pipeline layout (`WGPUPipelineLayout`). Bind groups themselves are **not** stored on the shader — they are created via `cgfx_shader_create_bind_group()` and owned by the caller. This enables the "same shader, different uniforms per object" pattern.

The pipeline reads `shader->pipeline_layout` automatically. When a shader has no bind groups (desc is NULL), `pipeline_layout` is NULL and WebGPU uses automatic layout inference.

Key types: `CgfxBindingDesc` → `CgfxGroupDesc` → `CgfxShaderDesc` → `CgfxShader`.

`CgfxBindingDesc` supports four binding kinds via `CgfxBindingKind`: buffer (default, backward compatible), sampled texture, sampler, and storage texture. For mixed bind groups (buffers + textures + samplers), use `cgfx_bind_group_create()` with `CgfxBindGroupEntry`. The older `cgfx_shader_create_bind_group()` remains for buffer-only convenience.

### Uniform architecture

`CgfxUniform` bundles a GPU uniform buffer, its bind group, and a pointer to user-owned data. It wraps the create-buffer → create-bind-group → write-each-frame pattern into 3 calls: `cgfx_uniform_create()`, `cgfx_uniform_write()`, `cgfx_uniform_destroy()`. The user owns the data; `CgfxUniform` only stores a pointer. The lower-level `cgfx_buffer_create_uniform()` and `cgfx_shader_create_bind_group()` APIs remain available for advanced use.

### Design conventions

- **Transparent structs** — fields are public so users can access raw WebGPU handles (e.g., `ctx.device`, `shader.group_layouts[0]`)
- **Context passed by pointer** — no global state
- **Error handling** — functions return `bool`, errors go to stderr
- **Caller-owned event loop** — the caller polls events (e.g., `glfwPollEvents()`) before `cgfx_frame_begin`; cgfx does not call any windowing event functions
- **Frame recording** — between `cgfx_frame_begin`/`cgfx_frame_end`, user records draw commands directly on `frame.render_pass` using raw WebGPU calls
- **Backend differences** — `#ifdef WEBGPU_BACKEND_WGPU`, `WEBGPU_BACKEND_DAWN`, `__EMSCRIPTEN__` are handled inside the library
- **Shader owns layouts, caller owns bind groups** — bind groups are created from the shader's layouts but returned to the user for per-object flexibility

### External dependencies (vendored in `vendor/`)

- `glfw` — windowing and input
- `webgpu` — WebGPU backend (wgpu-native by default, Dawn optional)
- `glfw3webgpu` — bridges GLFW to WebGPU surface creation
- `hwnd3webgpu` — bridges a raw Win32 HWND to WebGPU surface creation (Windows-only, for editor embedding)

### CMake structure

- Root `CMakeLists.txt` — externals + `add_subdirectory(cgfx)` + `add_subdirectory(examples)`
- `cgfx/CMakeLists.txt` — library (static by default, shared with `-DCGFX_SHARED=ON`) with `PUBLIC` includes and links
- `examples/CMakeLists.txt` — `cgfx_add_example()` helper function, one-liner per example
