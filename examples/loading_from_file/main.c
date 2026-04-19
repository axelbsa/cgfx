/**
 * @file main.c
 * @brief Buffer example -- demonstrates buffer copy and map-read.
 *
 * Creates a vertex buffer, copies it to a mappable buffer, then reads
 * back the data to verify the copy.
 */
#include <stdio.h>
#include <stdlib.h>
#include <webgpu/wgpu.h>

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

    /* Create shader module from WGSL source */
    static const char *shader_source =
        "struct VertexInput { \n"
        "   @location(0) position: vec2f, \n"
        "   @location(1) normal: vec3f, \n"
        "   @location(2) color: vec3f, \n"
        "}; \n"
        "struct VertexOutput { \n"
        "   @builtin(position) position: vec4f, \n"
        "   @location(0) color: vec3f, \n"
        "}; \n"
        "@vertex\n"
        "fn vs_main(in: VertexInput) -> VertexOutput { \n"
        "    var out: VertexOutput; \n"
        "    out.position = vec4f(in.position, 0.0, 1.0); \n"
        "    out.color = in.color; \n"
        "    return out;\n"
        "}\n"
        "@fragment\n"
        "fn fs_main(@location(0) color: vec3f) -> @location(0) vec4f {\n"
        "    return vec4f(color.x, color.y, color.z, 1.0);\n"
        "}\n";

    WGPUShaderModule shader = cgfx_shader_create(&ctx, "triangle shader", shader_source);
    if (!shader) {
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

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

    CgfxGeometry geo;
    cgfx_load_geometry("webgpu.txt", &geo);

    const uint32_t vertex_count = geo.point_count / 5;
    CgfxVertex* vertices = malloc(vertex_count * sizeof(CgfxVertex));
    for (uint32_t i = 0; i < vertex_count; i++)
    {
        const float* p = &geo.point_data[i * 5];
        vertices[i] = (CgfxVertex){
            .position = {p[0], p[1], 0.0f},
            .normal = {0.0f, 0.0f, 1.0f},
            .color = {p[2], p[3], p[4]},
            .uv = {0.0f, 0.0f},
        };
    }


    uint32_t* indices = malloc(geo.index_count * sizeof(uint32_t));
    for (uint32_t i = 0; i < geo.index_count; i++)
        indices[i] = geo.index_data[i];

    CgfxMesh mesh = cgfx_mesh_create(&ctx, vertices, vertex_count, indices, geo.index_count);

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
    cgfx_ctx_destroy(&ctx);

    free(vertices);

    return 0;
}
