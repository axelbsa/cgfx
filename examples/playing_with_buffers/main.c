/**
 * @file main.c
 * @brief Triangle example — demonstrates basic cgfx usage.
 *
 * Renders a purple triangle on a dark background using the cgfx library.
 * This is the simplest possible cgfx application: create a context,
 * a shader, a pipeline, and render in a loop.
 */
#include <stdio.h>
#include <webgpu/wgpu.h>

#include "cgfx.h"

static const char *shader_source =
    "@vertex                                                                \n"
    "fn vs_main(@builtin(vertex_index) in_vertex_index: u32)                \n"
    "    -> @builtin(position) vec4f {                                       \n"
    "    var p = vec2f(0.0, 0.0);                                           \n"
    "    if (in_vertex_index == 0u) {                                       \n"
    "        p = vec2f(-0.5, -0.5);                                         \n"
    "    } else if (in_vertex_index == 1u) {                                \n"
    "        p = vec2f(0.5, -0.5);                                          \n"
    "    } else {                                                           \n"
    "        p = vec2f(0.0, 0.5);                                           \n"
    "    }                                                                  \n"
    "    return vec4f(p, 0.0, 1.0);                                         \n"
    "}                                                                      \n"
    "                                                                       \n"
    "@fragment                                                              \n"
    "fn fs_main() -> @location(0) vec4f {                                   \n"
    "    return vec4f(0.8, 0.4, 1.0, 1.0);                                  \n"
    "}                                                                      \n";


void onBuffer2Mapped(const WGPUBufferMapAsyncStatus status, void* pUserData ) {
    CgfxBuffer *buffer = (CgfxBuffer*)pUserData;
    if (status != WGPUBufferMapAsyncStatus_Success) return;
    buffer->ready = true;
    fprintf(stderr, "Buffer2 mapped with status %d\n", status);
}

// We define a function that hides implementation-specific variants of device polling:
void wgpuPollEvents([[maybe_unused]] WGPUDevice device, [[maybe_unused]] bool yieldToWebBrowser) {
#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(device, false, nullptr);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
    if (yieldToWebBrowser) {
        emscripten_sleep(100);
    }
#endif
}

int main(void) {
    /* Initialize the rendering context: window, device, queue, surface */
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1920,
        .height = 1080,
        .title = "cgfx — triangle",
        .limits = cgfx_default_limits()
    })) {
        return 1;
    }

    /* Create shader module from WGSL source */
    CgfxShader shader = cgfx_shader_create(&ctx, "triangle shader", shader_source,
        &(CgfxShaderDesc){});

    /* Create render pipeline with default settings (no vertex buffers,
     * triangle list topology, no culling, alpha blending) */
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
    });

    if (!pipeline) {
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    fprintf(stderr,"Sizeof uin64_t=%lu\n", sizeof(uint64_t));
    uint64_t foo[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    fprintf(stderr,"Sizeof foo=%lu\n", sizeof(foo));
    CgfxBuffer buffer1 = cgfx_buffer_create_vertex(&ctx, (void*)foo, sizeof(foo), 16);
    CgfxBuffer buffer2 = cgfx_buffer_create_mapping(&ctx, nullptr, sizeof(foo), 16);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(ctx.device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, buffer1.buffer, 0, buffer2.buffer, 0, sizeof(foo));
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(ctx.queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuBufferMapAsync(buffer2.buffer, WGPUMapMode_Read, 0, sizeof(foo), &onBuffer2Mapped, &buffer2);
    while (!buffer2.ready) {
        wgpuPollEvents(ctx.device, true /* yieldToBrowser */);
    }

    const uint64_t* bufferData = (uint64_t*)wgpuBufferGetConstMappedRange(buffer2.buffer,0, sizeof(foo));
    fprintf(stderr, "bufferData = [");
    for (int i = 0; i < 16; ++i) {
        if (i > 0) fprintf(stderr, ", ");
        fprintf(stderr,"%d",(int)bufferData[i]);
    }
    fprintf(stderr, "]\n");

    /* Main render loop */
    // while (cgfx_ctx_is_running(&ctx)) {
    //     CgfxFrame frame;
    //     if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){ 0.1, 0.1, 0.2, 1.0 })) {
    //         /* Record draw commands directly on the render pass */
    //         wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
    //         wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
    //         cgfx_frame_end(&ctx, &frame);
    //     }
    // }

    /* Cleanup */
    wgpuRenderPipelineRelease(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    uint32_t size = 2*sizeof(float);
    fprintf(stderr,"Sizeof 2*float = %d %x\n", size, size);

    return 0;
}
