/**
 * @file cgfx_buffer.c
 * @brief Implementation of GPU buffer creation and management.
 *
 * TODO: Fill in the implementations below. Each function documents
 * exactly what WebGPU calls are needed. The pattern is always:
 *   1. Fill a WGPUBufferDescriptor
 *   2. wgpuDeviceCreateBuffer()
 *   3. wgpuQueueWriteBuffer() to upload data
 */
#include "cgfx_buffer.h"

#include <string.h>


CgfxBuffer cgfx_buffer_create_vertex(const CgfxCtx *ctx,
                                      const void *data,
                                      const uint64_t data_size,
                                      const uint32_t count) {
    CgfxBuffer result = { .buffer = nullptr, .size = 0, .count = 0 };

    /*
     * TODO: Create a vertex buffer and upload data.
     *
     * 1. Create a WGPUBufferDescriptor:
     *    - .label = "cgfx vertex buffer"
     *    - .size = data_size
     *    - .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst
     *      (CopyDst is required because we upload via wgpuQueueWriteBuffer)
     *    - .mappedAtCreation = false
     *
     * 2. result.buffer = wgpuDeviceCreateBuffer(ctx->device, &desc);
     *
     * 3. wgpuQueueWriteBuffer(ctx->queue, result.buffer, 0, data, data_size);
     *    This copies the CPU-side vertex data to the GPU buffer.
     *
     * 4. Set result.size = data_size and result.count = count.
     */

    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = (WGPUStringView){ .data = "cgfx vertex buffer", .length = WGPU_STRLEN };
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    bufferDesc.size = data_size;
    bufferDesc.mappedAtCreation = false;

    result.buffer = wgpuDeviceCreateBuffer(ctx->device, &bufferDesc);
    wgpuQueueWriteBuffer(ctx->queue, result.buffer, 0, data, data_size);

    result.size = data_size;
    result.count = count;

    return result;
}


CgfxBuffer cgfx_buffer_create_index(const CgfxCtx *ctx,
                                     const uint32_t *indices,
                                     uint32_t count) {
    CgfxBuffer result = { .buffer = nullptr, .size = 0, .count = 0 };

    /*
     * TODO: Create an index buffer and upload data.
     *
     * 1. Calculate byte size: uint64_t size = (uint64_t)count * sizeof(uint32_t);
     *
     * 2. Create a WGPUBufferDescriptor:
     *    - .label = "cgfx index buffer"
     *    - .size = size
     *    - .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst
     *    - .mappedAtCreation = false
     *
     * 3. result.buffer = wgpuDeviceCreateBuffer(ctx->device, &desc);
     *
     * 4. wgpuQueueWriteBuffer(ctx->queue, result.buffer, 0, indices, size);
     *
     * 5. Set result.size = size and result.count = count.
     */

    (void)ctx;
    (void)indices;
    (void)count;

    return result;
}


void cgfx_buffer_destroy(CgfxBuffer *buf) {
     if (buf->buffer) wgpuBufferRelease(buf->buffer);
        memset(buf, 0, sizeof(*buf));  // or: *buf = (CgfxBuffer){0};
}


CgfxBuffer cgfx_buffer_create_mapping(const CgfxCtx *ctx,
                                      const void *data,
                                      uint64_t data_size,
                                      uint32_t count) {

    CgfxBuffer result = { .buffer = nullptr, .size = 0, .count = 0 };

    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = (WGPUStringView){ .data = "cgfx vertex buffer", .length = WGPU_STRLEN };
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    bufferDesc.size = data_size;
    bufferDesc.mappedAtCreation = false;

    result.buffer = wgpuDeviceCreateBuffer(ctx->device, &bufferDesc);

    if (data != nullptr && data_size > 0)
    {
        wgpuQueueWriteBuffer(ctx->queue, result.buffer, 0, data, data_size);
    }

    result.size = data_size;
    result.count = count;

    return result;
}


CgfxBuffer cgfx_buffer_create(const CgfxCtx *ctx,
                             const WGPUBufferUsage usage,
                             const void *data,
                             uint64_t data_size) {

    CgfxBuffer result = { .buffer = nullptr, .size = 0, .count = 0 };
    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = (WGPUStringView){ .data = "cgfx buffer", .length = WGPU_STRLEN };
    bufferDesc.usage = usage;
    bufferDesc.size = data_size;
    bufferDesc.mappedAtCreation = false;

    result.buffer = wgpuDeviceCreateBuffer(ctx->device, &bufferDesc);

    if (data != nullptr && data_size > 0)
    {
        wgpuQueueWriteBuffer(ctx->queue, result.buffer, 0, data, data_size);
    }

    result.size = data_size;

    return result;
}
