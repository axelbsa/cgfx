/**
 * @file main.c
 * @brief Software framebuffer — plot pixels on the CPU, upload, blit.
 *
 * The modern equivalent of an OpenGL "draw to a PBO then blit" software renderer,
 * and a base for an old-school CPU raycaster:
 *
 *   1. Keep a CPU pixel buffer (one uint32 RGBA per pixel) at a small INTERNAL
 *      resolution (FB_W x FB_H).
 *   2. Each frame, write pixels into it (here: a cheap animated XOR pattern;
 *      replace plot() with your raycaster column loop).
 *   3. Upload it to a GPU texture with cgfx_texture_write() (-> wgpuQueueWriteTexture).
 *   4. Draw a fullscreen triangle that samples the texture with a NEAREST sampler,
 *      upscaling the small framebuffer to the window with crisp, chunky pixels.
 *
 * No compute shader needed: the CPU does the rasterization, the GPU just blits.
 * The upload of a small framebuffer per frame is negligible.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "cgfx.h"

/* Internal render resolution. The window is larger; nearest sampling upscales. */
#define FB_W 320
#define FB_H 200

/* Fullscreen triangle (3 verts, no vertex buffer). uv derived from clip position,
 * top-left origin so texel row 0 is the top of the screen. */
static const char *present_wgsl =
    "struct VsOut {\n"
    "    @builtin(position) pos : vec4f,\n"
    "    @location(0) uv : vec2f,\n"
    "}\n"
    "@vertex fn vs_main(@builtin(vertex_index) i : u32) -> VsOut {\n"
    "    var p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));\n"
    "    var o : VsOut;\n"
    "    o.pos = vec4f(p[i], 0.0, 1.0);\n"
    "    o.uv  = vec2f((p[i].x + 1.0) * 0.5, (1.0 - p[i].y) * 0.5);\n"
    "    return o;\n"
    "}\n"
    "@group(0) @binding(0) var fb_tex  : texture_2d<f32>;\n"
    "@group(0) @binding(1) var fb_samp : sampler;\n"
    "@fragment fn fs_main(@location(0) uv : vec2f) -> @location(0) vec4f {\n"
    "    return textureSample(fb_tex, fb_samp, uv);\n"
    "}\n";

/* Your "plotting" goes here. Replace with a raycaster: for each screen column,
 * cast a ray, compute wall height, write a vertical strip into `px`.
 * Pixel packing for RGBA8Unorm on little-endian: r | g<<8 | b<<16 | a<<24. */
static void plot(uint32_t *px, float t) {
    int scroll = (int)(t * 60.0f);
    for (int y = 0; y < FB_H; y++) {
        for (int x = 0; x < FB_W; x++) {
            uint8_t r = (uint8_t)((x ^ y) + scroll);
            uint8_t g = (uint8_t)(x);
            uint8_t b = (uint8_t)(y);
            px[y * FB_W + x] = (uint32_t)r | ((uint32_t)g << 8)
                             | ((uint32_t)b << 16) | (0xFFu << 24);
        }
    }
}

int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 960, .height = 600,            /* 3x the internal resolution */
        .title = "cgfx — software framebuffer",
        .limits = cgfx_default_limits(),
    })) return 1;

    /* CPU-side pixel buffer. */
    uint32_t *pixels = malloc((size_t)FB_W * FB_H * sizeof(uint32_t));
    if (!pixels) { cgfx_ctx_destroy(&ctx); return 1; }

    /* The GPU texture we upload into each frame and sample from. */
    CgfxTexture fb = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
        .width = FB_W, .height = FB_H,
        .format = WGPUTextureFormat_RGBA8Unorm,
        .usage  = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
    });
    if (!fb.ok) {
        fprintf(stderr, "framebuffer texture creation failed\n");
        free(pixels); cgfx_ctx_destroy(&ctx); return 1;
    }

    /* NEAREST sampler for crisp, blocky upscaling of the small framebuffer. */
    WGPUSampler sampler = cgfx_sampler_create(&ctx, &(CgfxSamplerDesc){
        .mag_filter    = CGFX_FILTER_NEAREST,
        .min_filter    = CGFX_FILTER_NEAREST,
        .mipmap_filter = CGFX_FILTER_NEAREST,
    });

    CgfxShader present = cgfx_shader_create(&ctx, "present", present_wgsl,
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
    if (!present.ok) {
        fprintf(stderr, "shader creation failed\n");
        cgfx_sampler_destroy(sampler); cgfx_texture_destroy(&fb);
        free(pixels); cgfx_ctx_destroy(&ctx); return 1;
    }

    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &present,
    });
    if (!pipeline) {
        fprintf(stderr, "pipeline creation failed\n");
        cgfx_shader_destroy(&present); cgfx_sampler_destroy(sampler);
        cgfx_texture_destroy(&fb); free(pixels); cgfx_ctx_destroy(&ctx); return 1;
    }

    WGPUBindGroup bg = cgfx_bind_group_create(&ctx, &present, 0,
        (CgfxBindGroupEntry[]){
            { .binding = 0, .texture = &fb },
            { .binding = 1, .sampler = sampler },
        }, 2);
    if (!bg) {
        fprintf(stderr, "bind group creation failed\n");
        cgfx_pipeline_destroy(pipeline); cgfx_shader_destroy(&present);
        cgfx_sampler_destroy(sampler); cgfx_texture_destroy(&fb);
        free(pixels); cgfx_ctx_destroy(&ctx); return 1;
    }

    float t = 0.0f;
    while (cgfx_ctx_is_running(&ctx)) {
        glfwPollEvents();

        t += 0.016f;
        plot(pixels, t);                                          /* CPU rasterize */
        cgfx_texture_write(&ctx, &fb, pixels,
                           (uint64_t)FB_W * FB_H * sizeof(uint32_t));  /* upload */

        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){0, 0, 0, 1})) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            cgfx_shader_bind(frame.render_pass, &bg, 1);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0); /* blit */
            cgfx_frame_end(&ctx, &frame);
        }
    }

    cgfx_bind_group_destroy(bg);
    cgfx_pipeline_destroy(pipeline);
    cgfx_shader_destroy(&present);
    cgfx_sampler_destroy(sampler);
    cgfx_texture_destroy(&fb);
    free(pixels);
    cgfx_ctx_destroy(&ctx);
    return 0;
}
