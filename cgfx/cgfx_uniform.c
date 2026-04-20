/**
 * @file cgfx_uniform.c
 * @brief Implementation of CgfxUniform — uniform buffer + bind group bundle.
 */
#include "cgfx_uniform.h"

#include <string.h>


CgfxUniform cgfx_uniform_create(const CgfxCtx *ctx,
                                const CgfxShader *shader,
                                uint32_t group_index,
                                const void *data,
                                uint64_t size) {
    CgfxUniform uniform = {
        .data = data,
        .size = size,
    };

    uniform.buffer = cgfx_buffer_create_uniform(ctx, data, size);
    uniform.bind_group = cgfx_shader_create_bind_group(ctx, shader,
                                                       group_index,
                                                       &uniform.buffer, 1);

    return uniform;
}


void cgfx_uniform_write(const CgfxCtx *ctx, const CgfxUniform *uniform) {
    wgpuQueueWriteBuffer(ctx->queue, uniform->buffer.buffer,
                         0, uniform->data, uniform->size);
}


void cgfx_uniform_destroy(CgfxUniform *uniform) {
    if (uniform->bind_group)
        wgpuBindGroupRelease(uniform->bind_group);

    cgfx_buffer_destroy(&uniform->buffer);

    *uniform = (CgfxUniform){};
}
