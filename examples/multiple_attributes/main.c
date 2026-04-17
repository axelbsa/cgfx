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

    /*
     *   CgfxVertex vertices[3] = {
     *       // Top vertex
     *       { .position = { 0.0f, h * 2.0f/3.0f, 0.0f },
     *         .normal = { 0.0f, 0.0f, 1.0f },
     *         .uv = { 0.5f, 1.0f } },
     *       // Bottom-left vertex
     *       { .position = { -size/2.0f, -h * 1.0f/3.0f, 0.0f },
     *         .normal = { 0.0f, 0.0f, 1.0f },
     *         .uv = { 0.0f, 0.0f } },
     *       // Bottom-right vertex
     *       { .position = { size/2.0f, -h * 1.0f/3.0f, 0.0f },
     *         .normal = { 0.0f, 0.0f, 1.0f },
     *         .uv = { 1.0f, 0.0f } },
     *   };
     *
     *
     *       -0.5, -0.5, // Point #0 (A)
             +0.5, -0.5, // Point #1
             +0.5, +0.5, // Point #2 (C)
             -0.5, +0.5, // Point #3
     * */

    CgfxVertex vertex[4] = {
            {.position = {-0.5f, -0.5f, -1.0f},
                .normal = {0.0f, 0.0f, 0.1f},
                .uv={0}},
            {.position = {0.5f, -0.5f, -1.0f},
             .normal = {0.0f, 0.0f, 0.1f},
             .uv={0}},
            {.position = {0.5f, 0.5f, -1.0f},
             .normal = {0.0f, 0.0f, 0.1f},
             .uv={0}},
            {.position = {-0.5f, 0.5f, -1.0f},
             .normal = {0.0f, 0.0f, 0.1f},
             .uv={0}}
    };

    uint32_t indexData[] = {
            0, 1, 2, // Triangle #0 connects points #0, #1 and #2
            0, 2, 3  // Triangle #1 connects points #0, #2 and #3
    };

    float vertexData[] = {
        // x0, y0, z0
        -0.5, -0.5, 0,

        // x1, y1
        +0.5, -0.5, 0,

        // x2, y2
        +0.0, +0.5, 0,

        // Add a second triangle:
        -0.55f, -0.5, 0,
        -0.05f, +0.5, 0,
        -0.55f, +0.5, 0
    };

    fprintf(stderr, "VertexCount %d\n", (int)(sizeof(vertexData) / sizeof(vertexData[0])/2));
    uint32_t vertex_count = (sizeof(vertexData) / sizeof(vertexData[0]) / 3);


    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280,
        .height = 720,
        .title = "cgfx — multiple attribute",
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

    CgfxMesh mesh = cgfx_mesh_create(&ctx, vertex, 4, indexData, 6);

    /* Create render pipeline with default settings */
    WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = shader,
        .vertex_buffer_count = 1,
        .vertex_layouts = &layout,
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
            cgfx_mesh_draw(frame.render_pass, &mesh);
            /* Set vertex buffer while encoding the render pass */
            //wgpuRenderPassEncoderSetVertexBuffer(frame.render_pass, 0, vertex_buffer.buffer, 0, wgpuBufferGetSize(vertex_buffer.buffer));
            /* We use the `vertex_count` variable instead of hard-coding the vertex count */
            //wgpuRenderPassEncoderDraw(frame.render_pass, vertex_count, 1, 0, 0);

            cgfx_frame_end(&ctx, &frame);
        }
    }

    /* Cleanup */
    cgfx_mesh_destroy(&mesh);
    wgpuRenderPipelineRelease(pipeline);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
