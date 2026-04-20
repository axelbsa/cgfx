/**
 * @file main.c
 * @brief Multiple uniforms — two objects sharing one shader with different
 *        uniform data, demonstrating per-object CgfxUniform.
 */
#include <math.h>
#include "cgfx.h"

typedef struct {
    float color[4];   /* vec4f — rgba */
    float offset[4];  /* vec4f — xy = position, z = rotation angle, w = unused */
    float time;
} MyUniforms;


int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1280,
        .height = 720,
        .title = "cgfx — multiple uniforms",
        .limits = cgfx_default_limits(),
    })) {
        return 1;
    }

    /* Create shader with one bind group: @group(0) @binding(0) */
    CgfxShader shader = cgfx_shader_create_from_file(&ctx, "uniform shader", "shaders/shader.wgsl",
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 1,
                .bindings = (CgfxBindingDesc[]){{
                    .binding = 0,
                    .min_binding_size = sizeof(MyUniforms),
                }},
            }},
        });


    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
    });

    /* Two objects — same shader, different uniform data */
    MyUniforms uniforms_left = {
        .color  = {1.0f, 0.3f, 0.3f, 1.0f},
        .offset = {-0.4f, 0.0f, 0.0f, 0.0f},
    };
    MyUniforms uniforms_right = {
        .color  = {0.3f, 0.5f, 1.0f, 1.0f},
        .offset = { 0.4f, 0.0f, 0.0f, 0.0f},
    };

    CgfxUniform u_left  = cgfx_uniform_create(&ctx, &shader, 0, &uniforms_left,  sizeof(MyUniforms));
    CgfxUniform u_right = cgfx_uniform_create(&ctx, &shader, 0, &uniforms_right, sizeof(MyUniforms));

    float time = 0.0f;

    while (cgfx_ctx_is_running(&ctx)) {
        time += 0.016f;

        uniforms_left.offset[2]  =  time;
        uniforms_right.offset[2] = -time * 0.7f;

        cgfx_uniform_write(&ctx, &u_left);
        cgfx_uniform_write(&ctx, &u_right);

        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0.1, 0.1, 0.15, 1.0})) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);

            cgfx_shader_bind(frame.render_pass, &u_left.bind_group, 1);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);

            cgfx_shader_bind(frame.render_pass, &u_right.bind_group, 1);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);

            cgfx_frame_end(&ctx, &frame);
        }
    }

    cgfx_uniform_destroy(&u_left);
    cgfx_uniform_destroy(&u_right);
    wgpuRenderPipelineRelease(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
