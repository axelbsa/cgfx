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
#include "cgfx_ctx.h"
#include "cgfx_export.h"

/**
 * A GPU buffer with associated metadata.
 *
 * Wraps a WGPUBuffer handle along with its size in bytes and element count.
 * The element count is meaningful for index buffers (number of indices)
 * and can be used for vertex buffers (number of vertices) if desired.
 */
typedef struct CgfxBuffer {
    WGPUBuffer  buffer;  /**< The WebGPU buffer handle.                        */
    uint64_t    size;    /**< Total size of the buffer in bytes.               */
    uint32_t    count;   /**< Number of elements (vertices or indices).        */
    bool        ready;   /**< Set ready flag in callback                       */
    bool        ok;      /**< True if creation succeeded — check before use.   */
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
CGFX_API CgfxBuffer cgfx_buffer_create_vertex(const CgfxCtx *ctx,
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
CGFX_API CgfxBuffer cgfx_buffer_create_index(const CgfxCtx *ctx,
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
CGFX_API void cgfx_buffer_destroy(CgfxBuffer *buf);


/**
 * Create a GPU uniform buffer and optionally upload initial data.
 *
 * Creates a buffer with WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst.
 * If data is non-NULL, immediately uploads it via wgpuQueueWriteBuffer.
 *
 * Update data each frame with:
 *   wgpuQueueWriteBuffer(ctx->queue, buf.buffer, 0, &data, sizeof(data));
 *
 * @param ctx        Initialized context.
 * @param data       Initial data to upload, or NULL for uninitialized.
 * @param data_size  Size of the uniform buffer in bytes.
 * @return           A CgfxBuffer containing the GPU uniform buffer.
 */
CGFX_API CgfxBuffer cgfx_buffer_create_uniform(const CgfxCtx *ctx,
                                      const void *data,
                                      uint64_t data_size);

/* Mapping buffer */
CGFX_API CgfxBuffer cgfx_buffer_create_mapping(const CgfxCtx *ctx,
                                      const void *data,
                                      uint64_t data_size,
                                      uint32_t count);

/**
 * Create a GPU storage buffer and optionally upload initial data.
 *
 * Creates a buffer with WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
 * WGPUBufferUsage_CopySrc. CopySrc is included so the buffer can be copied
 * to a mapping buffer for read-back.
 *
 * @param ctx        Initialized context.
 * @param data       Initial data to upload, or NULL for uninitialized.
 * @param data_size  Size of the storage buffer in bytes.
 * @return           A CgfxBuffer containing the GPU storage buffer.
 */
CGFX_API CgfxBuffer cgfx_buffer_create_storage(const CgfxCtx *ctx,
                                                const void *data,
                                                uint64_t data_size);

/* Create a generic buffer function */
CGFX_API CgfxBuffer cgfx_buffer_create(const CgfxCtx *ctx,
                              WGPUBufferUsageFlags usage,
                              const void *data,    // NULL = don't upload
                              uint64_t data_size);

/**
 * Copy one buffer to another via an immediate command submission.
 *
 * Creates a temporary command encoder, records the copy, submits, and
 * releases. Useful for copying compute results to a mapping buffer,
 * duplicating vertex data, or any buffer-to-buffer transfer.
 *
 * @param ctx   Initialized context.
 * @param src   Source buffer. Must have CopySrc usage.
 * @param dst   Destination buffer. Must have CopyDst usage.
 * @param size  Number of bytes to copy. 0 = min(src.size, dst.size).
 */
CGFX_API void cgfx_buffer_copy(const CgfxCtx *ctx,
                                const CgfxBuffer *src,
                                const CgfxBuffer *dst,
                                uint64_t size);

#endif /* CGFX_BUFFER_H */
