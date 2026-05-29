/**
 * @file main.c
 * @brief Buffer example -- demonstrates buffer copy and map-read.
 *
 * Creates a buffer with data, copies it to a mappable buffer via
 * the GPU command encoder, then reads back the data to verify the copy.
 */
#include <stdio.h>

#include "cgfx.h"

int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 640,
        .height = 480,
        .title = "cgfx — buffer copy",
        .limits = cgfx_default_limits()
    })) {
        return 1;
    }

    uint64_t data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    CgfxBuffer src = cgfx_buffer_create(&ctx,
        WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst,
        data, sizeof(data));

    CgfxBuffer dst = cgfx_buffer_create_mapping(&ctx, sizeof(data), 16);

    cgfx_buffer_copy(&ctx, &src, &dst, 0);

    uint64_t result[16];
    if (!cgfx_buffer_read(&ctx, &dst, result, sizeof(result))) {
        fprintf(stderr, "Failed to read back buffer data\n");
        return 1;
    }

    fprintf(stderr, "Read back: [");
    for (int i = 0; i < 16; ++i) {
        if (i > 0) fprintf(stderr, ", ");
        fprintf(stderr, "%d", (int)result[i]);
    }
    fprintf(stderr, "]\n");

    cgfx_buffer_destroy(&src);
    cgfx_buffer_destroy(&dst);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
