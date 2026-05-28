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
#include "cgfx_export.h"

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
 * The caller is responsible for polling events before calling this
 * function (e.g., glfwPollEvents() for GLFW, or the platform message
 * loop for native windows).
 *
 * Performs the per-frame setup:
 *   1. Acquire the next surface texture view from the swap chain
 *   2. Create a command encoder
 *   3. Begin a render pass with:
 *      - The surface texture as the single color attachment
 *      - Load operation: Clear with the provided clear_color
 *      - Store operation: Store (keep the rendered result)
 *      - Depth/stencil attachment (if ctx has a depth texture)
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
CGFX_API bool cgfx_frame_begin(const CgfxCtx *ctx, CgfxFrame *frame, WGPUColor clear_color);

/**
 * Begin a frame without starting a render pass.
 *
 * Acquires the next surface texture and creates a command encoder,
 * but does NOT begin a render pass. Use this for mixed compute+render
 * workflows where you need to run compute passes before the render pass.
 *
 * After compute passes, call cgfx_frame_begin_render_pass() to start
 * the render pass, then cgfx_frame_end() as normal.
 *
 * @param ctx    Initialized context.
 * @param frame  Pointer to caller-allocated CgfxFrame.
 * @return       true if the frame was started (surface texture available).
 */
CGFX_API bool cgfx_frame_begin_encoder(const CgfxCtx *ctx, CgfxFrame *frame);

/**
 * Begin the render pass on a frame started with cgfx_frame_begin_encoder().
 *
 * Sets up the render pass with the surface texture as color attachment
 * and optional depth/stencil attachment.
 *
 * @param ctx         Initialized context.
 * @param frame       Frame started with cgfx_frame_begin_encoder().
 * @param clear_color Background color to clear the frame with.
 */
CGFX_API void cgfx_frame_begin_render_pass(const CgfxCtx *ctx,
                                            CgfxFrame *frame,
                                            WGPUColor clear_color);

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
CGFX_API void cgfx_frame_end(const CgfxCtx *ctx, CgfxFrame *frame);

#endif /* CGFX_FRAME_H */
