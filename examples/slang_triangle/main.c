/**
 * @file main.c
 * @brief Slang triangle — demonstrates the Slang → WGSL → wgpu pipeline.
 *
 * Same triangle as the WGSL example, but the shader is written in Slang,
 * compiled to WGSL at build time by slangc, and loaded at runtime as a
 * text file via cgfx_shader_create().
 *
 * Both vertex and fragment entry points live in one .slang file and are
 * compiled into a single .wgsl output, matching the standard cgfx
 * single-module shader workflow.
 */
#include <stdio.h>
#include <stdlib.h>

#include "cgfx.h"


/**
 * Load a text file into a malloc'd null-terminated string.
 * Returns NULL on failure. Caller must free() the returned pointer.
 */
static char *load_text_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[slang_triangle] Failed to open %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fprintf(stderr, "[slang_triangle] Empty or invalid file: %s\n", path);
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "[slang_triangle] Failed to read %s\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }

    buf[len] = '\0';
    fclose(f);
    return buf;
}


int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width  = 1920,
        .height = 1080,
        .title  = "cgfx — Slang triangle",
        .limits = cgfx_default_limits()
    })) {
        return 1;
    }

    /* Load the WGSL compiled from Slang at build time.
     * The .wgsl file is placed next to the executable by CMake. */
    char *wgsl = load_text_file("triangle.wgsl");
    if (!wgsl) {
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    CgfxShader shader = cgfx_shader_create(&ctx, "slang triangle", wgsl,
        &(CgfxShaderDesc){});
    free(wgsl);

    /* Slang generates entry point names matching the Slang source */
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader         = &shader,
        .vertex_entry   = "vertexMain",
        .fragment_entry = "fragmentMain",
    });

    if (!pipeline) {
        fprintf(stderr, "[slang_triangle] Failed to create pipeline\n");
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    /* Main render loop */
    while (cgfx_ctx_is_running(&ctx)) {
        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){ 0.1, 0.1, 0.2, 1.0 })) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
            cgfx_frame_end(&ctx, &frame);
        }
    }

    /* Cleanup */
    wgpuRenderPipelineRelease(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
