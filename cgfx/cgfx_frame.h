/**
 * @file cgfx_frame.h
 * @brief Frame management — begin/end frame rendering cycle.
 *
 * Wraps the per-frame boilerplate of WebGPU rendering:
 *   - Acquiring the next surface texture
 *   - Creating command encoders and render passes
 *   - Submitting command buffers
 *   - Presenting the surface
 *   - Backend-specific tick/poll
 *
 * Between cgfx_frame_begin() and cgfx_frame_end(), the user records
 * draw commands directly on the frame's render_pass using raw WebGPU calls.
 * This avoids wrapping every possible draw/bind operation while still
 * hiding the encoder/submit/present ceremony.
 *
 * Usage pattern:
 *   CgfxFrame frame;
 *   if (cgfx_frame_begin(&ctx, &frame, clear_color)) {
 *       wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
 *       wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
 *       cgfx_frame_end(&ctx, &frame);
 *   }
 */
#ifndef CGFX_FRAME_H
#define CGFX_FRAME_H

#include <webgpu/webgpu.h>
#include <stdbool.h>
#include "cgfx_ctx.h"

/**
 * Per-frame rendering state.
 *
 * Created by cgfx_frame_begin(), consumed by cgfx_frame_end().
 * The render_pass field is the main interaction point — use it to
 * record draw commands between begin and end.
 */
typedef struct CgfxFrame {
    WGPUCommandEncoder    encoder;     /**< Command encoder for this frame.           */
    WGPURenderPassEncoder render_pass; /**< Active render pass — record draws on this. */
    WGPUTextureView       target_view; /**< Surface texture view being rendered to.   */
} CgfxFrame;

/**
 * Begin a new frame.
 *
 * Performs the per-frame setup:
 *   1. glfwPollEvents() — process window/input events
 *   2. Acquire the next surface texture view from the swap chain
 *   3. Create a command encoder
 *   4. Begin a render pass with:
 *      - The surface texture as the single color attachment
 *      - Load operation: Clear with the provided clear_color
 *      - Store operation: Store (keep the rendered result)
 *      - No depth/stencil attachment
 *
 * If the surface texture is not available (e.g., window minimized,
 * surface lost), returns false and the frame should be skipped.
 *
 * After a successful begin, the caller should:
 *   1. Set pipeline(s) on frame->render_pass
 *   2. Bind vertex/index buffers
 *   3. Issue draw calls
 *   4. Call cgfx_frame_end()
 *
 * @param ctx         Initialized context.
 * @param frame       Pointer to caller-allocated CgfxFrame (typically on stack).
 * @param clear_color Background color to clear the frame with (RGBA, 0.0-1.0).
 * @return            true if the frame was started successfully.
 *                    false if the surface texture is unavailable (skip this frame).
 */
bool cgfx_frame_begin(const CgfxCtx *ctx, CgfxFrame *frame, WGPUColor clear_color);

/**
 * End and submit the current frame.
 *
 * Performs the per-frame teardown:
 *   1. End the render pass encoder
 *   2. Finish the command encoder, producing a command buffer
 *   3. Submit the command buffer to the device queue
 *   4. Release the render pass encoder, command encoder, and command buffer
 *   5. Release the surface texture view
 *   6. Present the surface (skipped on Emscripten — the browser handles it)
 *   7. Backend synchronization:
 *      - Dawn: wgpuDeviceTick() — processes pending work
 *      - wgpu-native: wgpuDevicePoll() — polls for completed operations
 *
 * Must be called after a successful cgfx_frame_begin().
 *
 * @param ctx   Initialized context.
 * @param frame Frame previously started with cgfx_frame_begin().
 */
void cgfx_frame_end(const CgfxCtx *ctx, CgfxFrame *frame);

#endif /* CGFX_FRAME_H */
