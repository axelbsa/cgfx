/**
 * @file cgfx_buffer.c
 * @brief Implementation of GPU buffer creation and management.
 *
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
    CgfxBuffer result = {.buffer = nullptr, .size = 0, .count = 0};

    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = "cgfx vertex buffer";
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
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
    CgfxBuffer result = {.buffer = nullptr, .size = 0, .count = 0};
    const uint64_t size = (uint64_t) count * sizeof(uint32_t);

    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = "cgfx vertex buffer";
    bufferDesc.usage =  WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
    bufferDesc.size = size;
    bufferDesc.mappedAtCreation = false;

    result.buffer = wgpuDeviceCreateBuffer(ctx->device, &bufferDesc);
    wgpuQueueWriteBuffer(ctx->queue, result.buffer, 0, indices, size);

    result.size = size;
    result.count = count;

    return result;
}


void cgfx_buffer_destroy(CgfxBuffer *buf) {
    if (buf->buffer)
        wgpuBufferRelease(buf->buffer);
    memset(buf, 0, sizeof(*buf)); // or: *buf = (CgfxBuffer){0};
}


CgfxBuffer cgfx_buffer_create_uniform(const CgfxCtx *ctx,
                                      const void *data,
                                      const uint64_t data_size) {
    return cgfx_buffer_create(ctx,
                              WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
                              data, data_size);
}


CgfxBuffer cgfx_buffer_create_mapping(const CgfxCtx *ctx,
                                      const void *data,
                                      const uint64_t data_size,
                                      const uint32_t count) {
    CgfxBuffer result = {.buffer = nullptr, .size = 0, .count = 0};

    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = "cgfx mapping buffer";
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    bufferDesc.size = data_size;
    bufferDesc.mappedAtCreation = false;

    result.buffer = wgpuDeviceCreateBuffer(ctx->device, &bufferDesc);
    result.size = data_size;
    result.count = count;

    (void) data;

    return result;
}

CgfxBuffer cgfx_buffer_create(const CgfxCtx *ctx,
                              const WGPUBufferUsageFlags usage,
                              const void *data,
                              const uint64_t data_size) {
    CgfxBuffer result = {.buffer = nullptr, .size = 0, .count = 0};
    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = "cgfx generic buffer";
    bufferDesc.usage = usage;
    bufferDesc.size = data_size;
    bufferDesc.mappedAtCreation = false;

    result.buffer = wgpuDeviceCreateBuffer(ctx->device, &bufferDesc);

    if (data != nullptr && data_size > 0) {
        wgpuQueueWriteBuffer(ctx->queue, result.buffer, 0, data, data_size);
    }

    result.size = data_size;

    return result;
}
