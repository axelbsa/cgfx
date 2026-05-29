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

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif


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
    result.ok = (result.buffer != nullptr);

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
    result.ok = (result.buffer != nullptr);

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
    result.ok = (result.buffer != nullptr);

    (void) data;

    return result;
}

CgfxBuffer cgfx_buffer_create_storage(const CgfxCtx *ctx,
                                       const void *data,
                                       const uint64_t data_size) {
    return cgfx_buffer_create(ctx,
                              WGPUBufferUsage_Storage |
                              WGPUBufferUsage_CopyDst |
                              WGPUBufferUsage_CopySrc,
                              data, data_size);
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
    result.ok = (result.buffer != nullptr);

    return result;
}


void cgfx_buffer_copy(const CgfxCtx *ctx,
                       const CgfxBuffer *src,
                       const CgfxBuffer *dst,
                       uint64_t size) {
    if (size == 0) {
        size = src->size < dst->size ? src->size : dst->size;
    }

    WGPUCommandEncoderDescriptor enc_desc = {};
    enc_desc.nextInChain = nullptr;
    enc_desc.label = "cgfx buffer copy encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(ctx->device, &enc_desc);

    wgpuCommandEncoderCopyBufferToBuffer(encoder, src->buffer, 0, dst->buffer, 0, size);

    WGPUCommandBufferDescriptor cmd_desc = {};
    cmd_desc.nextInChain = nullptr;
    cmd_desc.label = "cgfx buffer copy commands";
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, &cmd_desc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(ctx->queue, 1, &commands);
    wgpuCommandBufferRelease(commands);

#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(ctx->device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(ctx->device, false, nullptr);
#endif
}
