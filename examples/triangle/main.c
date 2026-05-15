/**
 * @file main.c
 * @brief Triangle example — demonstrates basic cgfx usage.
 *
 * Renders a purple triangle on a dark background using the cgfx library.
 * This is the simplest possible cgfx application: create a context,
 * a shader, a pipeline, and render in a loop.
 */
#include "cgfx.h"
#include <GLFW/glfw3.h>

static const char *shader_source =
    "@vertex                                                                \n"
    "fn vs_main(@builtin(vertex_index) in_vertex_index: u32)                \n"
    "    -> @builtin(position) vec4f {                                       \n"
    "    var p = vec2f(0.0, 0.0);                                           \n"
    "    if (in_vertex_index == 0u) {                                       \n"
    "        p = vec2f(-0.5, -0.5);                                         \n"
    "    } else if (in_vertex_index == 1u) {                                \n"
    "        p = vec2f(0.5, -0.5);                                          \n"
    "    } else {                                                           \n"
    "        p = vec2f(0.0, 0.5);                                           \n"
    "    }                                                                  \n"
    "    return vec4f(p, 0.0, 1.0);                                         \n"
    "}                                                                      \n"
    "                                                                       \n"
    "@fragment                                                              \n"
    "fn fs_main() -> @location(0) vec4f {                                   \n"
    "    return vec4f(0.8, 0.4, 1.0, 1.0);                                  \n"
    "}                                                                      \n";

static const char* shader_source2 =
    "@vertex fn vs_main(@builtin(vertex_index) idx : u32) -> @builtin(position) vec4f { \n"
    "    var pos = array<vec2f, 3>(                                                     \n"
    "        vec2f( 0.0,  0.5),                                                         \n"
    "        vec2f(-0.5, -0.5),                                                         \n"
    "        vec2f( 0.5, -0.5),                                                         \n"
    "    );                                                                             \n"
    "return vec4f(pos[idx], 0.0, 1.0);                                                  \n"
    "}                                                                                  \n"
    "                                                                                   \n"
    "@fragment fn fs_main() -> @location(0) vec4f {                                     \n"
    "    return vec4f(1.0, 0.0, 0.0, 1.0);                                              \n"
    "}                                                                                  \n";

static void on_resize(GLFWwindow *window, int width, int height) {
    CgfxCtx *ctx = glfwGetWindowUserPointer(window);
    cgfx_ctx_resize(ctx, (uint32_t)width, (uint32_t)height);
}

int main(void) {
    /* Initialize the rendering context: window, device, queue, surface */
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1920,
        .height = 1080,
        .title = "cgfx — triangle",
        .resizable = true,
        .limits = cgfx_default_limits()
    })) {
        return 1;
    }

    glfwSetWindowUserPointer(ctx.window, &ctx);
    glfwSetFramebufferSizeCallback(ctx.window, on_resize);

    /* Create shader module from WGSL source */
    CgfxShader shader = cgfx_shader_create(&ctx, "triangle shader", shader_source2,
        &(CgfxShaderDesc){});

    /* Create render pipeline with default settings (no vertex buffers,
     * triangle list topology, no culling, alpha blending) */
    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
    });

    if (!pipeline) {
        cgfx_ctx_destroy(&ctx);
        return 1;
    }
    double lastFpsTime = glfwGetTime();
    int frameCount = 0;
    glfwSwapInterval(0);

    /* Main render loop */
    while (cgfx_ctx_is_running(&ctx)) {
        glfwPollEvents();
        CgfxFrame frame;
        frameCount++;
        double now = glfwGetTime();
        if (now - lastFpsTime >= 1.0) {
            double fps = frameCount / (now - lastFpsTime);
            char title[64];
            snprintf(title, sizeof(title), "Dawn Hello Triangle — %.1f FPS", fps);
            glfwSetWindowTitle(ctx.window, title);
            frameCount = 0;
            lastFpsTime = now;
        }
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){ 0.1, 0.1, 0.2, 1.0 })) {
            /* Record draw commands directly on the render pass */
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
