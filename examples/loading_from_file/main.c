/**
 * @file main.c
 * @brief Buffer example -- demonstrates buffer copy and map-read.
 *
 * Creates a vertex buffer, copies it to a mappable buffer, then reads
 * back the data to verify the copy.
 */
#include "cgfx.h"

int main(void) {
    /* Initialize the rendering context: window, device, queue, surface */
    WGPURequiredLimits limits = cgfx_default_limits();
    // limits.limits.maxVertexAttributes = 1;
    // limits.limits.maxVertexBuffers = 1;
    // limits.limits.maxBufferSize = 128*1024;
    // limits.limits.maxVertexBufferArrayStride = 2 * sizeof(float);

    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280,
        .height = 720,
        .title = "cgfx — multiple attribute",
        .limits = limits,
    })) {
        return 1;
    }


    CgfxShader shader = cgfx_shader_create_from_file(&ctx,
                                                           "my shader",
                                                           "shaders/load_from_file.wgsl",
                                                           &(CgfxShaderDesc){});

    /* Create render pipeline with default settings */
    WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
        .vertex_buffer_count = 1,
        .vertex_layouts = &layout,
    });


    if (!pipeline) {
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    CgfxMesh mesh = cgfx_load_tutorial_mesh(&ctx, "webgpu.txt");

    /* Main render loop */
    while (cgfx_ctx_is_running(&ctx)) {
        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){ 0.1, 0.1, 0.2, 1.0 })) {

            /* Record draw commands directly on the render pass */
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            cgfx_mesh_draw(frame.render_pass, &mesh);

            cgfx_frame_end(&ctx, &frame);
        }
    }

    /* Cleanup */
    cgfx_mesh_destroy(&mesh);
    wgpuRenderPipelineRelease(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
