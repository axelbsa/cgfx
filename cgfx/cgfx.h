/**
 * @file cgfx.h
 * @brief cgfx — Minimal C rendering engine wrapping WebGPU.
 *
 * Include this single header to get the full cgfx API.
 * All public functions are prefixed with cgfx_.
 * All public types are prefixed with Cgfx.
 *
 * Modules:
 *   cgfx_ctx        — Context: window + device + queue + surface
 *   cgfx_shader     — Shader module creation from WGSL
 *   cgfx_pipeline   — Render pipeline with sensible defaults
 *   cgfx_frame      — Per-frame begin/end rendering cycle
 *   cgfx_buffer     — GPU buffer creation (vertex/index)
 *   cgfx_uniform    — Uniform buffer + bind group bundle
 *   cgfx_mesh       — Mesh: vertex + index buffers + layout
 *   cgfx_primitives — Primitive geometry generators
 */
#ifndef CGFX_H
#define CGFX_H

#include "cgfx_ctx.h"
#include "cgfx_shader.h"
#include "cgfx_pipeline.h"
#include "cgfx_frame.h"
#include "cgfx_buffer.h"
#include "cgfx_uniform.h"
#include "cgfx_mesh.h"
#include "cgfx_primitives.h"
#include "cgfx_loader.h"
#include "cgfx_camera.h"

#endif /* CGFX_H */
