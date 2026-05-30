/**
 * @file main.c
 * @brief Depth buffer example — renders a rotating 3D pyramid with depth
 *        testing so back faces are properly occluded.
 */
#include "cgfx.h"
#include "cgfx_loader.h"

typedef struct {
    float time;
    float _pad[3];
} Uniforms;


int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280,
        .height = 720,
        .title = "cgfx — depth texture",
        .depth_buffer = true,
        .limits = cgfx_default_limits(),
    })) {
        return 1;
    }

    CgfxShader shader = cgfx_shader_create_from_file(&ctx, "depth shader", "shaders/projection_matrices.wgsl",
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 1,
                .bindings = (CgfxBindingDesc[]){{
                    .binding = 0,
                }},
            }},
        });

    WGPUVertexBufferLayout layout = cgfx_mesh_vertex_layout();
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
        .depth_test = true,
        .vertex_buffer_count = 1,
        .vertex_layouts = &layout,
    });

    CgfxMesh mesh = cgfx_load_tutorial_mesh(&ctx, "pyramid.txt");

    Uniforms uniforms = {};
    CgfxUniform uniform = cgfx_uniform_create(&ctx, &shader, 0, &uniforms, sizeof(Uniforms));

    while (cgfx_ctx_is_running(&ctx)) {
        uniforms.time += 0.016f;
        cgfx_uniform_write(&ctx, &uniform);

        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.15, 1.0})) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            cgfx_shader_bind(frame.render_pass, &uniform.bind_group, 1);
            cgfx_mesh_draw(frame.render_pass, &mesh);
            cgfx_frame_end(&ctx, &frame);
        }
    }

    cgfx_uniform_destroy(&uniform);
    cgfx_mesh_destroy(&mesh);
    cgfx_pipeline_destroy(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
