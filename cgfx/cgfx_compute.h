/**
 * @file cgfx_compute.h
 * @brief Compute pipeline and compute pass management.
 *
 * Wraps WebGPU compute pipeline creation and the compute pass lifecycle.
 * Two usage patterns are supported:
 *
 * **Standalone compute** — creates its own command encoder and submits:
 *   CgfxComputePass cp;
 *   cgfx_compute_begin(&ctx, &cp);
 *   wgpuComputePassEncoderSetPipeline(cp.pass, pipeline);
 *   cgfx_shader_bind_compute(cp.pass, &bind_group, 1);
 *   wgpuComputePassEncoderDispatchWorkgroups(cp.pass, 64, 1, 1);
 *   cgfx_compute_end(&ctx, &cp);
 *
 * **Mixed render+compute** — borrows an existing command encoder:
 *   CgfxFrame frame;
 *   if (cgfx_frame_begin_encoder(&ctx, &frame)) {
 *       CgfxComputePass cp;
 *       cgfx_compute_pass_begin(frame.encoder, &cp);
 *       // ... dispatch ...
 *       cgfx_compute_pass_end(&cp);
 *       cgfx_frame_begin_render_pass(&ctx, &frame, clear_color);
 *       // ... draw ...
 *       cgfx_frame_end(&ctx, &frame);
 *   }
 */
#ifndef CGFX_COMPUTE_H
#define CGFX_COMPUTE_H

#include <webgpu/webgpu.h>
#include <stdbool.h>
#include "cgfx_ctx.h"
#include "cgfx_shader.h"
#include "cgfx_buffer.h"
#include "cgfx_export.h"

/**
 * Compute pipeline descriptor.
 *
 * Zero-init defaults:
 *   - entry_point = NULL → "cs_main"
 *
 * The only required field is shader.
 */
typedef struct CgfxComputeDesc {
    const CgfxShader *shader;       /**< Required. Shader with module and layouts. */
    const char       *entry_point;  /**< Compute entry point. NULL = "cs_main".    */
} CgfxComputeDesc;

/**
 * Compute pass state.
 *
 * The pass field is the main interaction point — use it to set pipelines,
 * bind groups, and dispatch workgroups between begin and end.
 */
typedef struct CgfxComputePass {
    WGPUCommandEncoder     encoder;      /**< Command encoder for this pass.           */
    WGPUComputePassEncoder pass;         /**< Active compute pass — dispatch on this.  */
    bool                   owns_encoder; /**< True if this pass owns the encoder.      */
} CgfxComputePass;

/**
 * Create a compute pipeline.
 *
 * @param ctx   Initialized context.
 * @param desc  Compute pipeline configuration. shader is required.
 * @return      Pipeline handle, or NULL on failure.
 *              Caller must release with wgpuComputePipelineRelease().
 */
CGFX_API WGPUComputePipeline cgfx_compute_pipeline_create(
    const CgfxCtx *ctx,
    const CgfxComputeDesc *desc);

/**
 * Begin a standalone compute pass.
 *
 * Creates a command encoder and begins a compute pass. Pair with
 * cgfx_compute_end() which ends the pass, submits, and releases.
 *
 * @param ctx  Initialized context.
 * @param cp   Pointer to caller-allocated CgfxComputePass.
 * @return     true on success.
 */
CGFX_API bool cgfx_compute_begin(const CgfxCtx *ctx, CgfxComputePass *cp);

/**
 * End a standalone compute pass and submit.
 *
 * Ends the compute pass, finishes the command encoder, submits to the
 * queue, releases all handles, and performs backend tick/poll.
 *
 * @param ctx  Initialized context.
 * @param cp   Compute pass started with cgfx_compute_begin().
 */
CGFX_API void cgfx_compute_end(const CgfxCtx *ctx, CgfxComputePass *cp);

/**
 * Begin a compute pass on an existing command encoder.
 *
 * Use for mixed render+compute workflows where the compute pass shares
 * a command encoder with a render pass. Pair with cgfx_compute_pass_end()
 * which only ends the pass (does NOT submit or release the encoder).
 *
 * @param encoder  Command encoder (e.g., frame.encoder).
 * @param cp       Pointer to caller-allocated CgfxComputePass.
 * @return         true on success.
 */
CGFX_API bool cgfx_compute_pass_begin(WGPUCommandEncoder encoder,
                                       CgfxComputePass *cp);

/**
 * End a compute pass without submitting.
 *
 * Only ends and releases the compute pass encoder. The command encoder
 * lifetime is the caller's responsibility.
 *
 * @param cp  Compute pass started with cgfx_compute_pass_begin().
 */
CGFX_API void cgfx_compute_pass_end(CgfxComputePass *cp);

/**
 * Copy one buffer to another via an immediate command submission.
 *
 * Creates a temporary command encoder, records the copy, submits, and
 * releases. Useful for copying compute results to a mapping buffer.
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

#endif /* CGFX_COMPUTE_H */
