/**
 * @file main.c
 * @brief Buffer example -- demonstrates buffer copy and map-read.
 *
 * Creates a vertex buffer, copies it to a mappable buffer, then reads
 * back the data to verify the copy.
 */
#include <stdio.h>
#include <webgpu/wgpu.h>

#include "cgfx.h"

static void onBuffer2Mapped(WGPUBufferMapAsyncStatus status,
                            void *userdata) {
    CgfxBuffer *buffer = (CgfxBuffer*)userdata;
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
    WGPURequiredLimits limits = cgfx_default_limits();
    limits.limits.maxVertexAttributes = 1;
    limits.limits.maxVertexBuffers = 1;
    limits.limits.maxBufferSize = 6 * 2 * sizeof(float);
    limits.limits.maxVertexBufferArrayStride = 2 * sizeof(float);

    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1920,
        .height = 1080,
        .title = "cgfx — vertex attribute",
        .limits = limits,
    })) {
        return 1;
    }

    /* Create shader module from WGSL source */
    static const char *shader_source =
        "@vertex\n"
        "fn vs_main(@location(0) in_vertex_position: vec2f) -> @builtin(position) vec4f { \n"
        "    return vec4f(p, 0.0, 1.0);\n"
        "}\n"
        "@fragment\n"
        "fn fs_main() -> @location(0) vec4f {\n"
        "    return vec4f(0.8, 0.4, 1.0, 1.0);\n"
        "}\n";

    WGPUShaderModule shader = cgfx_shader_create(&ctx, "triangle shader", shader_source);
    if (!shader) {
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    /* Create render pipeline with default settings */
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = shader,
    });
    wgpuShaderModuleRelease(shader);

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

    wgpuBufferMapAsync(buffer2.buffer, WGPUMapMode_Read, 0, sizeof(foo), onBuffer2Mapped, &buffer2);
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

    /* Cleanup */
    wgpuRenderPipelineRelease(pipeline);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
