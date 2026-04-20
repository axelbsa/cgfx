/**
 * @file cgfx_frame.c
 * @brief Implementation of per-frame rendering cycle.
 */
#include "cgfx_frame.h"

#include <stddef.h>

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif

#include <GLFW/glfw3.h>


/**
 * Acquire the next surface texture and create a view for rendering.
 *
 * Gets the current surface texture from the swap chain, checks its status,
 * and creates a 2D texture view configured for rendering.
 *
 * Backend difference: On non-wgpu-native backends, the surface texture is
 * released after creating the view (the view holds a reference). On
 * wgpu-native, surface textures must NOT be manually released.
 *
 * @param surface  The window surface to acquire from.
 * @return         A texture view for rendering, or NULL if unavailable.
 */
static WGPUTextureView cgfx__get_surface_texture_view(WGPUSurface surface) {
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(surface, &surface_texture);
    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        return nullptr;
    }

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.nextInChain = nullptr;
    view_desc.label = "cgfx surface texture view";
    view_desc.format = wgpuTextureGetFormat(surface_texture.texture);
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_All;

    WGPUTextureView view = wgpuTextureCreateView(surface_texture.texture, &view_desc);

#ifndef WEBGPU_BACKEND_WGPU
    /*
     * On Dawn and other backends, we release the texture after creating
     * the view — the view holds its own reference.
     * On wgpu-native, surface textures must NOT be manually released.
     */
    wgpuTextureRelease(surface_texture.texture);
#endif

    return view;
}


bool cgfx_frame_begin(const CgfxCtx *ctx, CgfxFrame *frame, WGPUColor clear_color) {
    /* Process window events (input, resize, close, etc.) */
    glfwPollEvents();

    /* Acquire the next surface texture to render into */
    frame->target_view = cgfx__get_surface_texture_view(ctx->surface);
    if (!frame->target_view)
        return false;

    /*
     * Create a command encoder. All GPU commands for this frame are
     * recorded into this encoder, then finalized into a command buffer
     * for submission.
     */
    WGPUCommandEncoderDescriptor encoder_desc = {};
    encoder_desc.nextInChain = nullptr;
    encoder_desc.label = "cgfx frame encoder";
    frame->encoder = wgpuDeviceCreateCommandEncoder(ctx->device, &encoder_desc);

    /*
     * Set up the render pass color attachment:
     * - view: the surface texture we're rendering to
     * - loadOp: Clear — fill with clear_color before rendering
     * - storeOp: Store — keep the rendered result for presentation
     * - depthSlice: required on non-wgpu-native backends
     */
    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = frame->target_view;
    color_attachment.resolveTarget = nullptr;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = clear_color;

#ifndef WEBGPU_BACKEND_WGPU
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif


    /* THIS NEEDS REFACTORING, probably a candidate for its own type */
    WGPUTextureDescriptor depthTextureDesc = {};
    WGPUTextureFormat depthTextureFormat = WGPUTextureFormat_Depth24Plus;
    depthTextureDesc.dimension = WGPUTextureDimension_2D;
    depthTextureDesc.format = depthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;
    depthTextureDesc.size = (WGPUExtent3D){1280, 720, 1};
    depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = &depthTextureFormat;
    WGPUTexture depthTexture = wgpuDeviceCreateTexture(ctx->device, &depthTextureDesc);

    // Create the view of the depth texture manipulated by the rasterizer
    WGPUTextureViewDescriptor depthTextureViewDesc = {};
    depthTextureViewDesc.aspect = WGPUTextureAspect_DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.baseMipLevel = 0;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = WGPUTextureViewDimension_2D;
    depthTextureViewDesc.format = depthTextureFormat;
    WGPUTextureView depthTextureView = wgpuTextureCreateView(depthTexture, &depthTextureViewDesc);

    WGPURenderPassDepthStencilAttachment depthStencilAttachment;
    // The view of the depth texture
    depthStencilAttachment.view = depthTextureView;

    // The initial value of the depth buffer, meaning "far"
    depthStencilAttachment.depthClearValue = 1.0f;
    // Operation settings comparable to the color attachment
    depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
    // we could turn off writing to the depth buffer globally here
    depthStencilAttachment.depthReadOnly = false;

    // Stencil setup, mandatory but unused
    depthStencilAttachment.stencilClearValue = 0;
    depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.stencilReadOnly = true;

    /*
     * Begin the render pass. The descriptor specifies:
     * - One color attachment (the surface texture)
     * - No depth/stencil attachment (for now)
     * - No timestamp queries
     *
     * The returned render pass encoder is exposed to the user for
     * recording draw commands.
     */
    WGPURenderPassDescriptor pass_desc = {};
    pass_desc.nextInChain = nullptr;
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_attachment;
    pass_desc.depthStencilAttachment = &depthStencilAttachment;
    pass_desc.timestampWrites = nullptr;

    frame->render_pass = wgpuCommandEncoderBeginRenderPass(frame->encoder, &pass_desc);

    return true;
}


void cgfx_frame_end(const CgfxCtx *ctx, CgfxFrame *frame) {
    /* Finalize the render pass — no more draw commands after this */
    wgpuRenderPassEncoderEnd(frame->render_pass);
    wgpuRenderPassEncoderRelease(frame->render_pass);

    /*
     * Finish the command encoder to produce a command buffer.
     * The command buffer contains all recorded GPU commands and
     * is ready for submission to the queue.
     */
    WGPUCommandBufferDescriptor cmd_desc = {};
    cmd_desc.nextInChain = nullptr;
    cmd_desc.label = "cgfx frame commands";
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(frame->encoder, &cmd_desc);
    wgpuCommandEncoderRelease(frame->encoder);

    /* Submit the command buffer to the GPU for execution */
    wgpuQueueSubmit(ctx->queue, 1, &commands);
    wgpuCommandBufferRelease(commands);

    /* Release the texture view — we're done rendering to it */
    wgpuTextureViewRelease(frame->target_view);

    /*
     * Present the rendered frame to the window.
     * On Emscripten (web), the browser handles presentation automatically
     * via requestAnimationFrame, so we skip this call.
     */
#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(ctx->surface);
#endif

    /*
     * Backend-specific synchronization:
     * - Dawn uses wgpuDeviceTick() to process pending async operations
     * - wgpu-native uses wgpuDevicePoll() for the same purpose
     * Without this, callbacks and resource cleanup may not happen promptly.
     */
#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(ctx->device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(ctx->device, false, nullptr);
#endif
}
