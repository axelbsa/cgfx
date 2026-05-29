/**
 * @file cgfx_uniform.h
 * @brief Uniform buffer + bind group bundle for per-object uniform data.
 *
 * CgfxUniform combines a GPU uniform buffer, its bind group, and a pointer
 * to user-owned data into a single object. This cuts the typical uniform
 * workflow from 5 operations (create buffer, create bind group, write,
 * release bind group, destroy buffer) down to 3 (create, write, destroy).
 *
 * The user owns the data — CgfxUniform only stores a pointer to it.
 * All fields are public (transparent struct).
 */
#ifndef CGFX_UNIFORM_H
#define CGFX_UNIFORM_H

#include <webgpu/webgpu.h>
#include <stdint.h>
#include "cgfx_ctx.h"
#include "cgfx_buffer.h"
#include "cgfx_shader.h"
#include "cgfx_export.h"

/**
 * A uniform buffer bundled with its bind group and a pointer to user data.
 *
 * Fields are public — access uniform.bind_group for cgfx_shader_bind(),
 * or uniform.buffer.buffer for raw WebGPU calls.
 */
typedef struct CgfxUniform {
    CgfxBuffer     buffer;      /**< GPU buffer (Uniform | CopyDst).             */
    WGPUBindGroup  bind_group;  /**< Bind group referencing this buffer.          */
    const void    *data;        /**< Pointer to user-owned data (never freed).    */
    uint64_t       size;        /**< Size of the uniform data in bytes.           */
    bool           ok;          /**< True if creation succeeded — check before use. */
} CgfxUniform;

/**
 * Create a uniform buffer and bind group in one call.
 *
 * Allocates a GPU uniform buffer, uploads the initial data, and creates
 * a bind group for the given @group index using the shader's layout.
 *
 * The data pointer is stored for use with cgfx_uniform_write(). The user
 * owns the data and must keep it valid for the lifetime of the uniform.
 *
 * @param ctx          Initialized context.
 * @param shader       Shader with bind group layouts.
 * @param group_index  The @group(N) index.
 * @param data         Pointer to user-owned uniform data.
 * @param size         Size of the uniform data in bytes.
 * @return             A CgfxUniform. Call cgfx_uniform_destroy() to release.
 */
CGFX_API CgfxUniform cgfx_uniform_create(const CgfxCtx *ctx,
                                const CgfxShader *shader,
                                uint32_t group_index,
                                const void *data,
                                uint64_t size);

/**
 * Upload the uniform's data to the GPU.
 *
 * Writes the data pointed to by uniform->data to the GPU buffer.
 * Call this each frame after modifying the user-owned data.
 *
 * @param ctx      Initialized context.
 * @param uniform  Uniform to upload.
 */
CGFX_API void cgfx_uniform_write(const CgfxCtx *ctx, const CgfxUniform *uniform);

/**
 * Destroy a uniform and release its GPU resources.
 *
 * Releases both the bind group and the underlying buffer.
 * Does NOT free the user-owned data pointer.
 *
 * @param uniform  Uniform to destroy.
 */
CGFX_API void cgfx_uniform_destroy(CgfxUniform *uniform);

#endif /* CGFX_UNIFORM_H */
