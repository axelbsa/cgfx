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

int main(void) {
    /* Initialize the rendering context: window, device, queue, surface */
    WGPURequiredLimits limits = cgfx_default_limits();
    // limits.limits.maxVertexAttributes = 1;
    // limits.limits.maxVertexBuffers = 1;
    // limits.limits.maxBufferSize = 128*1024;
    // limits.limits.maxVertexBufferArrayStride = 2 * sizeof(float);

    float vertexData[] = {
        // x0, y0
        -0.5, -0.5,

        // x1, y1
        +0.5, -0.5,

        // x2, y2
        +0.0, +0.5,

        // Add a second triangle:
        -0.55f, -0.5,
        -0.05f, +0.5,
        -0.55f, +0.5
    };

    fprintf(stderr, "VertexCount %d\n", (int)(sizeof(vertexData) / sizeof(vertexData[0])/2));
    uint32_t vertex_count = 6;

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
        "    return vec4f(in_vertex_position, 0.0, 1.0);\n"
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

    CgfxBuffer vertex_buffer = cgfx_buffer_create_vertex(&ctx, (void*)vertexData, sizeof(vertexData), vertex_count);

    WGPUVertexBufferLayout vertexBufferLayout = {0};
    WGPUVertexAttribute positionAttrib;

    positionAttrib.shaderLocation = 0;
    positionAttrib.format = WGPUVertexFormat_Float32x2;
    positionAttrib.offset = 0;

    vertexBufferLayout.attributeCount = 1;
    vertexBufferLayout.attributes = &positionAttrib;
    vertexBufferLayout.arrayStride = 2 * sizeof(float);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    /* Create render pipeline with default settings */
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = shader,
        .vertex_buffer_count = 1,
        .vertex_buffers = &vertexBufferLayout,
    });
    wgpuShaderModuleRelease(shader);


    if (!pipeline) {
        cgfx_ctx_destroy(&ctx);
        return 1;
    }


    /* Main render loop */
    while (cgfx_ctx_is_running(&ctx)) {
        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){ 0.1, 0.1, 0.2, 1.0 })) {

            /* Record draw commands directly on the render pass */
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            /* Set vertex buffer while encoding the render pass */
            wgpuRenderPassEncoderSetVertexBuffer(frame.render_pass, 0, vertex_buffer.buffer, 0, wgpuBufferGetSize(vertex_buffer.buffer));
            /* We use the `vertex_count` variable instead of hard-coding the vertex count */
            wgpuRenderPassEncoderDraw(frame.render_pass, vertex_count, 1, 0, 0);

            cgfx_frame_end(&ctx, &frame);
        }
    }

    /* Cleanup */
    wgpuRenderPipelineRelease(pipeline);
    cgfx_buffer_destroy(&vertex_buffer);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
