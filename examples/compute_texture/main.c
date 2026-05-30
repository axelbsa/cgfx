/**
 * @file main.c
 * @brief Compute-to-texture example — animated Julia set fractal.
 *
 * Demonstrates the mixed compute+render workflow:
 *   1. Compute shader writes a Julia set fractal to a storage texture
 *   2. Render shader samples the texture and draws a fullscreen quad
 *   3. Both passes share one command encoder via two-phase frame begin
 *
 * The fractal parameter c animates over time, producing a continuously
 * changing pattern.
 */
#include <stdio.h>
#include "cgfx.h"

typedef struct {
    float time;
    float _pad[3];
} Params;

static const char *compute_wgsl =
    "struct Params { time : f32 }\n"
    "@group(0) @binding(0) var output : texture_storage_2d<rgba8unorm, write>;\n"
    "@group(0) @binding(1) var<uniform> params : Params;\n"
    "\n"
    "fn hsv(h : f32, s : f32, v : f32) -> vec3f {\n"
    "    let k = vec3f(1.0, 2.0/3.0, 1.0/3.0);\n"
    "    let p = abs(fract(vec3f(h) + k) * 6.0 - 3.0);\n"
    "    return v * mix(vec3f(1.0), clamp(p - 1.0, vec3f(0.0), vec3f(1.0)), s);\n"
    "}\n"
    "\n"
    "@compute @workgroup_size(8, 8)\n"
    "fn cs_main(@builtin(global_invocation_id) id : vec3u) {\n"
    "    let dims = textureDimensions(output);\n"
    "    if (id.x >= dims.x || id.y >= dims.y) { return; }\n"
    "\n"
    "    let uv = (vec2f(id.xy) / vec2f(dims)) * 2.0 - 1.0;\n"
    "    var z = uv * 1.5;\n"
    "    let c = vec2f(\n"
    "        sin(params.time * 0.3) * 0.7885,\n"
    "        cos(params.time * 0.23) * 0.7885\n"
    "    );\n"
    "\n"
    "    var iter = 0u;\n"
    "    let max_iter = 128u;\n"
    "    for (var i = 0u; i < max_iter; i++) {\n"
    "        z = vec2f(z.x*z.x - z.y*z.y, 2.0*z.x*z.y) + c;\n"
    "        if (dot(z, z) > 4.0) { break; }\n"
    "        iter++;\n"
    "    }\n"
    "\n"
    "    var color : vec4f;\n"
    "    if (iter == max_iter) {\n"
    "        color = vec4f(0.0, 0.0, 0.0, 1.0);\n"
    "    } else {\n"
    "        let t = f32(iter) / f32(max_iter);\n"
    "        let rgb = hsv(t * 3.0 + params.time * 0.1, 0.8, 1.0 - t * 0.3);\n"
    "        color = vec4f(rgb, 1.0);\n"
    "    }\n"
    "    textureStore(output, id.xy, color);\n"
    "}\n";

static const char *render_wgsl =
    "struct VsOut {\n"
    "    @builtin(position) pos : vec4f,\n"
    "    @location(0) uv : vec2f,\n"
    "}\n"
    "\n"
    "@vertex fn vs_main(@builtin(vertex_index) vi : u32) -> VsOut {\n"
    "    var positions = array<vec2f, 6>(\n"
    "        vec2f(-1, -1), vec2f(1, -1), vec2f(-1, 1),\n"
    "        vec2f(-1,  1), vec2f(1, -1), vec2f( 1, 1),\n"
    "    );\n"
    "    var uvs = array<vec2f, 6>(\n"
    "        vec2f(0, 1), vec2f(1, 1), vec2f(0, 0),\n"
    "        vec2f(0, 0), vec2f(1, 1), vec2f(1, 0),\n"
    "    );\n"
    "    var out : VsOut;\n"
    "    out.pos = vec4f(positions[vi], 0.0, 1.0);\n"
    "    out.uv  = uvs[vi];\n"
    "    return out;\n"
    "}\n"
    "\n"
    "@group(0) @binding(0) var tex : texture_2d<f32>;\n"
    "@group(0) @binding(1) var tex_sampler : sampler;\n"
    "\n"
    "@fragment fn fs_main(@location(0) uv : vec2f) -> @location(0) vec4f {\n"
    "    return textureSample(tex, tex_sampler, uv);\n"
    "}\n";

#define TEX_SIZE 512

int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 800, .height = 600,
        .title = "cgfx — compute texture (Julia set)",
        .limits = cgfx_default_limits(),
    })) return 1;

    CgfxTexture tex = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
        .width = TEX_SIZE, .height = TEX_SIZE,
        .format = WGPUTextureFormat_RGBA8Unorm,
        .usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding,
    });

    WGPUSampler sampler = cgfx_sampler_create(&ctx, &(CgfxSamplerDesc){});

    CgfxShader compute_shader = cgfx_shader_create(&ctx, "julia_compute", compute_wgsl,
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 2,
                .bindings = (CgfxBindingDesc[]){
                    { .binding = 0, .kind = CGFX_BINDING_STORAGE_TEXTURE,
                      .storage_format = WGPUTextureFormat_RGBA8Unorm,
                      .visibility = WGPUShaderStage_Compute },
                    { .binding = 1,
                      .min_binding_size = sizeof(Params),
                      .visibility = WGPUShaderStage_Compute },
                },
            }},
        });

    CgfxShader render_shader = cgfx_shader_create(&ctx, "quad_render", render_wgsl,
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 2,
                .bindings = (CgfxBindingDesc[]){
                    { .binding = 0, .kind = CGFX_BINDING_TEXTURE },
                    { .binding = 1, .kind = CGFX_BINDING_SAMPLER },
                },
            }},
        });

    WGPUComputePipeline compute_pipeline = cgfx_compute_pipeline_create(&ctx,
        &(CgfxComputeDesc){ .shader = &compute_shader });
    if (!compute_pipeline) {
        fprintf(stderr, "Failed to create compute pipeline\n");
        return 1;
    }

    WGPURenderPipeline render_pipeline = cgfx_pipeline_create(&ctx,
        &(CgfxPipelineDesc){ .shader = &render_shader });
    if (!render_pipeline) {
        fprintf(stderr, "Failed to create render pipeline\n");
        return 1;
    }

    Params params = {};
    CgfxBuffer params_buf = cgfx_buffer_create_uniform(&ctx, &params, sizeof(Params));

    WGPUBindGroup compute_bg = cgfx_bind_group_create(&ctx, &compute_shader, 0,
        (CgfxBindGroupEntry[]){
            { .binding = 0, .texture = &tex },
            { .binding = 1, .buffer = &params_buf },
        }, 2);

    WGPUBindGroup render_bg = cgfx_bind_group_create(&ctx, &render_shader, 0,
        (CgfxBindGroupEntry[]){
            { .binding = 0, .texture = &tex },
            { .binding = 1, .sampler = sampler },
        }, 2);

    while (cgfx_ctx_is_running(&ctx)) {
        glfwPollEvents();
        params.time += 0.016f;
        wgpuQueueWriteBuffer(ctx.queue, params_buf.buffer, 0, &params, sizeof(params));

        CgfxFrame frame;
        if (cgfx_frame_begin_encoder(&ctx, &frame)) {
            CgfxComputePass cp;
            cgfx_compute_pass_begin(frame.encoder, &cp);
            wgpuComputePassEncoderSetPipeline(cp.pass, compute_pipeline);
            cgfx_shader_bind_compute(cp.pass, &compute_bg, 1);
            wgpuComputePassEncoderDispatchWorkgroups(cp.pass,
                TEX_SIZE / 8, TEX_SIZE / 8, 1);
            cgfx_compute_pass_end(&cp);

            cgfx_frame_begin_render_pass(&ctx, &frame, (WGPUColor){0, 0, 0, 1});
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, render_pipeline);
            cgfx_shader_bind(frame.render_pass, &render_bg, 1);
            wgpuRenderPassEncoderDraw(frame.render_pass, 6, 1, 0, 0);
            cgfx_frame_end(&ctx, &frame);
        }
    }

    cgfx_bind_group_destroy(render_bg);
    cgfx_bind_group_destroy(compute_bg);
    cgfx_buffer_destroy(&params_buf);
    cgfx_pipeline_destroy(render_pipeline);
    cgfx_compute_pipeline_destroy(compute_pipeline);
    cgfx_shader_destroy(&render_shader);
    cgfx_shader_destroy(&compute_shader);
    cgfx_sampler_destroy(sampler);
    cgfx_texture_destroy(&tex);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
