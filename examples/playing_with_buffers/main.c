/**
 * @file main.c
 * @brief Buffer example -- demonstrates buffer copy and map-read.
 *
 * Creates a buffer with data, copies it to a mappable buffer via
 * the GPU command encoder, then reads back the data to verify the copy.
 */
#include <stdio.h>
#include <webgpu/wgpu.h>

#include "cgfx.h"

static void on_buffer_mapped(const WGPUBufferMapAsyncStatus status, void *user_data) {
    CgfxBuffer *buffer = (CgfxBuffer *)user_data;
    if (status != WGPUBufferMapAsyncStatus_Success) return;
    buffer->ready = true;
}

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

    CgfxBuffer dst = cgfx_buffer_create_mapping(&ctx, nullptr, sizeof(data), 16);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(ctx.device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, src.buffer, 0, dst.buffer, 0, sizeof(data));
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(ctx.queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuBufferMapAsync(dst.buffer, WGPUMapMode_Read, 0, sizeof(data), &on_buffer_mapped, &dst);
    while (!dst.ready) {
        wgpuDevicePoll(ctx.device, false, nullptr);
    }

    const uint64_t *result = (const uint64_t *)wgpuBufferGetConstMappedRange(dst.buffer, 0, sizeof(data));
    fprintf(stderr, "Read back: [");
    for (int i = 0; i < 16; ++i) {
        if (i > 0) fprintf(stderr, ", ");
        fprintf(stderr, "%d", (int)result[i]);
    }
    fprintf(stderr, "]\n");

    wgpuBufferUnmap(dst.buffer);

    cgfx_buffer_destroy(&src);
    cgfx_buffer_destroy(&dst);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
