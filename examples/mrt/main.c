/**
 * @file main.c
 * @brief Multiple render targets (MRT) example.
 *
 * Demonstrates the color-target API:
 *   1. One render pass writes TWO offscreen textures at once — a pipeline with
 *      two color targets (CgfxPipelineDesc.color_targets) and a fragment shader
 *      with two @location outputs, driven by cgfx_frame_begin_render_pass_ex().
 *   2. A second pass samples both offscreen textures and shows target A on the
 *      left half of the window and target B on the right.
 *
 * Both offscreen targets use RGBA8Unorm (independent of the surface format),
 * which also exercises rendering to a non-surface format.
 */
#include <stdio.h>
#include "cgfx.h"

/* Fullscreen quad (6 verts) + uv. Fragment writes two color attachments. */
static const char *mrt_wgsl =
    "struct VsOut {\n"
    "    @builtin(position) pos : vec4f,\n"
    "    @location(0) uv : vec2f,\n"
    "}\n"
    "@vertex fn vs_main(@builtin(vertex_index) vi : u32) -> VsOut {\n"
    "    var p = array<vec2f, 6>(\n"
    "        vec2f(-1, -1), vec2f(1, -1), vec2f(-1, 1),\n"
    "        vec2f(-1,  1), vec2f(1, -1), vec2f( 1, 1));\n"
    "    var uv = array<vec2f, 6>(\n"
    "        vec2f(0, 1), vec2f(1, 1), vec2f(0, 0),\n"
    "        vec2f(0, 0), vec2f(1, 1), vec2f(1, 0));\n"
    "    var o : VsOut;\n"
    "    o.pos = vec4f(p[vi], 0.0, 1.0);\n"
    "    o.uv  = uv[vi];\n"
    "    return o;\n"
    "}\n"
    "struct FsOut {\n"
    "    @location(0) target0 : vec4f,\n"
    "    @location(1) target1 : vec4f,\n"
    "}\n"
    "@fragment fn fs_main(@location(0) uv : vec2f) -> FsOut {\n"
    "    var o : FsOut;\n"
    "    o.target0 = vec4f(uv.x, uv.y, 0.0, 1.0);\n"          /* red/green gradient */
    "    o.target1 = vec4f(0.0, 1.0 - uv.x, uv.y, 1.0);\n"   /* green/blue gradient */
    "    return o;\n"
    "}\n";

/* Samples both targets: left half = target A, right half = target B. */
static const char *present_wgsl =
    "struct VsOut {\n"
    "    @builtin(position) pos : vec4f,\n"
    "    @location(0) uv : vec2f,\n"
    "}\n"
    "@vertex fn vs_main(@builtin(vertex_index) vi : u32) -> VsOut {\n"
    "    var p = array<vec2f, 6>(\n"
    "        vec2f(-1, -1), vec2f(1, -1), vec2f(-1, 1),\n"
    "        vec2f(-1,  1), vec2f(1, -1), vec2f( 1, 1));\n"
    "    var uv = array<vec2f, 6>(\n"
    "        vec2f(0, 1), vec2f(1, 1), vec2f(0, 0),\n"
    "        vec2f(0, 0), vec2f(1, 1), vec2f(1, 0));\n"
    "    var o : VsOut;\n"
    "    o.pos = vec4f(p[vi], 0.0, 1.0);\n"
    "    o.uv  = uv[vi];\n"
    "    return o;\n"
    "}\n"
    "@group(0) @binding(0) var texA : texture_2d<f32>;\n"
    "@group(0) @binding(1) var texB : texture_2d<f32>;\n"
    "@group(0) @binding(2) var samp : sampler;\n"
    "@fragment fn fs_main(@location(0) uv : vec2f) -> @location(0) vec4f {\n"
    "    if (uv.x < 0.5) {\n"
    "        return textureSample(texA, samp, vec2f(uv.x * 2.0, uv.y));\n"
    "    }\n"
    "    return textureSample(texB, samp, vec2f((uv.x - 0.5) * 2.0, uv.y));\n"
    "}\n";

int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 800, .height = 600,
        .title = "cgfx — multiple render targets",
        .limits = cgfx_default_limits(),
    })) return 1;

    /* Two offscreen color targets, RGBA8Unorm, used as render target + sampled. */
    CgfxTexture target_a = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
        .width = ctx.width, .height = ctx.height,
        .format = WGPUTextureFormat_RGBA8Unorm,
        .usage  = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
    });
    CgfxTexture target_b = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
        .width = ctx.width, .height = ctx.height,
        .format = WGPUTextureFormat_RGBA8Unorm,
        .usage  = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
    });
    if (!target_a.ok || !target_b.ok) {
        fprintf(stderr, "offscreen target creation failed\n");
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    WGPUSampler sampler = cgfx_sampler_create(&ctx, &(CgfxSamplerDesc){});

    /* MRT shader has no bind groups; present shader samples both targets. */
    CgfxShader mrt_shader = cgfx_shader_create(&ctx, "mrt", mrt_wgsl, nullptr);
    CgfxShader present_shader = cgfx_shader_create(&ctx, "present", present_wgsl,
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 3,
                .bindings = (CgfxBindingDesc[]){
                    { .binding = 0, .kind = CGFX_BINDING_TEXTURE },
                    { .binding = 1, .kind = CGFX_BINDING_TEXTURE },
                    { .binding = 2, .kind = CGFX_BINDING_SAMPLER },
                },
            }},
        });
    if (!mrt_shader.ok || !present_shader.ok) {
        fprintf(stderr, "shader creation failed\n");
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    /* Pipeline writing two RGBA8Unorm color targets (opaque). */
    WGPURenderPipeline mrt_pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &mrt_shader,
        .color_target_count = 2,
        .color_targets = (CgfxColorTarget[]){
            { .format = WGPUTextureFormat_RGBA8Unorm },
            { .format = WGPUTextureFormat_RGBA8Unorm },
        },
    });
    /* Present pipeline uses the default single surface-format target. */
    WGPURenderPipeline present_pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &present_shader,
    });
    if (!mrt_pipeline || !present_pipeline) {
        fprintf(stderr, "pipeline creation failed\n");
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    WGPUBindGroup present_bg = cgfx_bind_group_create(&ctx, &present_shader, 0,
        (CgfxBindGroupEntry[]){
            { .binding = 0, .texture = &target_a },
            { .binding = 1, .texture = &target_b },
            { .binding = 2, .sampler = sampler },
        }, 3);
    if (!present_bg) {
        fprintf(stderr, "bind group creation failed\n");
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    while (cgfx_ctx_is_running(&ctx)) {
        glfwPollEvents();

        CgfxFrame frame;
        if (cgfx_frame_begin_encoder(&ctx, &frame)) {
            /* Pass 1: write both offscreen targets in a single MRT pass. */
            cgfx_frame_begin_render_pass_ex(&ctx, &frame, &(CgfxRenderPassDesc){
                .color_count = 2,
                .color_views = (WGPUTextureView[]){ target_a.view, target_b.view },
                .clear_color = (WGPUColor){0, 0, 0, 1},
                .no_depth = true,
            });
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, mrt_pipeline);
            wgpuRenderPassEncoderDraw(frame.render_pass, 6, 1, 0, 0);
            cgfx_frame_end_render_pass(&frame);

            /* Pass 2: sample both targets onto the surface (split screen). */
            cgfx_frame_begin_render_pass(&ctx, &frame, (WGPUColor){0, 0, 0, 1});
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, present_pipeline);
            cgfx_shader_bind(frame.render_pass, &present_bg, 1);
            wgpuRenderPassEncoderDraw(frame.render_pass, 6, 1, 0, 0);
            cgfx_frame_end(&ctx, &frame);
        }
    }

    cgfx_bind_group_destroy(present_bg);
    cgfx_pipeline_destroy(present_pipeline);
    cgfx_pipeline_destroy(mrt_pipeline);
    cgfx_shader_destroy(&present_shader);
    cgfx_shader_destroy(&mrt_shader);
    cgfx_sampler_destroy(sampler);
    cgfx_texture_destroy(&target_b);
    cgfx_texture_destroy(&target_a);
    cgfx_ctx_destroy(&ctx);
    return 0;
}
