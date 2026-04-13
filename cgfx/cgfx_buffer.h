/**
 * @file cgfx_buffer.h
 * @brief GPU buffer creation and management for vertex and index data.
 *
 * Wraps the WebGPU buffer creation and data upload pattern:
 *   wgpuDeviceCreateBuffer() + wgpuQueueWriteBuffer()
 *
 * Provides simple helpers for the two most common buffer types:
 * vertex buffers and index buffers. For other buffer types (uniform,
 * storage, etc.), use the raw WebGPU API via ctx->device directly.
 */
#ifndef CGFX_BUFFER_H
#define CGFX_BUFFER_H

#include <webgpu/webgpu.h>
#include <stdint.h>
#include <stddef.h>
#include "cgfx_ctx.h"

/**
 * A GPU buffer with associated metadata.
 *
 * Wraps a WGPUBuffer handle along with its size in bytes and element count.
 * The element count is meaningful for index buffers (number of indices)
 * and can be used for vertex buffers (number of vertices) if desired.
 */
typedef struct CgfxBuffer {
    WGPUBuffer  buffer;  /**< The WebGPU buffer handle.                       */
    uint64_t    size;    /**< Total size of the buffer in bytes.               */
    uint32_t    count;   /**< Number of elements (vertices or indices).        */
} CgfxBuffer;

/**
 * Create a GPU vertex buffer and upload data to it.
 *
 * Creates a buffer with WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
 * then immediately writes the provided data into it via the device queue.
 *
 * The buffer is sized exactly to fit the provided data. For dynamic
 * vertex data that changes each frame, you may want to use the raw
 * WebGPU API with mapped buffers instead.
 *
 * Implementation should:
 *   1. Create a WGPUBufferDescriptor with:
 *      - size = data_size
 *      - usage = Vertex | CopyDst
 *      - mappedAtCreation = false
 *   2. Call wgpuDeviceCreateBuffer(ctx->device, &desc)
 *   3. Call wgpuQueueWriteBuffer(ctx->queue, buffer, 0, data, data_size)
 *   4. Return a CgfxBuffer with the handle, size, and count
 *
 * @param ctx        Initialized context (uses device and queue).
 * @param data       Pointer to vertex data to upload.
 * @param data_size  Size of the vertex data in bytes.
 * @param count      Number of vertices in the data.
 * @return           A CgfxBuffer containing the GPU vertex buffer.
 */
CgfxBuffer cgfx_buffer_create_vertex(const CgfxCtx *ctx,
                                      const void *data,
                                      uint64_t data_size,
                                      uint32_t count);

/**
 * Create a GPU index buffer and upload data to it.
 *
 * Creates a buffer with WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst,
 * then immediately writes the provided index data into it.
 *
 * Indices are uint32_t (WGPUIndexFormat_Uint32). The buffer is sized
 * as count * sizeof(uint32_t).
 *
 * Implementation should:
 *   1. Calculate size = count * sizeof(uint32_t)
 *   2. Create a WGPUBufferDescriptor with:
 *      - size = calculated size
 *      - usage = Index | CopyDst
 *      - mappedAtCreation = false
 *   3. Call wgpuDeviceCreateBuffer(ctx->device, &desc)
 *   4. Call wgpuQueueWriteBuffer(ctx->queue, buffer, 0, indices, size)
 *   5. Return a CgfxBuffer with the handle, size, and count
 *
 * @param ctx      Initialized context (uses device and queue).
 * @param indices  Pointer to uint32_t index array.
 * @param count    Number of indices in the array.
 * @return         A CgfxBuffer containing the GPU index buffer.
 */
CgfxBuffer cgfx_buffer_create_index(const CgfxCtx *ctx,
                                     const uint32_t *indices,
                                     uint32_t count);

/**
 * Destroy a buffer and release its GPU resources.
 *
 * Calls wgpuBufferRelease() on the underlying handle and zeros out
 * the struct. After this call, the buffer must not be used.
 *
 * @param buf  Buffer to destroy.
 */
void cgfx_buffer_destroy(CgfxBuffer *buf);


/* More buffer types */

CgfxBuffer cgfx_buffer_create_mapping(const CgfxCtx *ctx,
                                      const void *data,
                                      uint64_t data_size,
                                      uint32_t count);

#endif /* CGFX_BUFFER_H */
