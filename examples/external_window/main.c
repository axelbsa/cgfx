/**
 * @file main.c
 * @brief External window example — demonstrates cgfx_ctx_init_external().
 *
 * Shows how to initialize cgfx from an externally-owned window handle
 * (HWND on Windows). This is the pattern for embedding cgfx into a
 * C# WinForms editor or any other host that creates its own window.
 *
 * Windows-only. On other platforms, prints a message and exits.
 *
 * C# WinForms usage would look like:
 *
 *   [DllImport("cgfx_external.dll")]
 *   static extern bool cgfx_render_init(IntPtr hwnd, int width, int height);
 *
 *   // In your Form/Panel:
 *   cgfx_render_init(panel.Handle, panel.Width, panel.Height);
 */
#include <stdio.h>
#include "cgfx.h"

#ifdef _WIN32
#include <windows.h>

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
    "    return vec4f(0.2, 0.8, 0.4, 1.0);                                  \n"
    "}                                                                      \n";

static bool running = true;

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) {
        running = false;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int main(void) {
    /* ── Create a plain Win32 window (simulating what WinForms does) ── */
    const int width = 1280;
    const int height = 720;

    WNDCLASS wc = {
        .lpfnWndProc = wnd_proc,
        .hInstance = GetModuleHandle(NULL),
        .lpszClassName = "cgfx_external",
    };
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, wc.lpszClassName, "cgfx — external window",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, wc.hInstance, NULL);

    if (!hwnd) {
        fprintf(stderr, "Failed to create Win32 window\n");
        return 1;
    }

    /* ── Initialize cgfx from the HWND ─────────────────────────────── */
    CgfxCtx ctx;
    if (!cgfx_ctx_init_external(&ctx, &(CgfxCtxExternalDesc){
        .native_handle = hwnd,
        .width = width,
        .height = height,
        .limits = cgfx_default_limits(),
    })) {
        return 1;
    }

    CgfxShader shader = cgfx_shader_create(&ctx, "triangle shader", shader_source,
        &(CgfxShaderDesc){});

    WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &(CgfxPipelineDesc){
        .shader = &shader,
    });

    /* ── Render loop (Win32 message pump) ──────────────────────────── */
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        CgfxFrame frame;
        if (cgfx_frame_begin(&ctx, &frame, (WGPUColor){ 0.1, 0.1, 0.2, 1.0 })) {
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, pipeline);
            wgpuRenderPassEncoderDraw(frame.render_pass, 3, 1, 0, 0);
            cgfx_frame_end(&ctx, &frame);
        }
    }

    wgpuRenderPipelineRelease(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);
    DestroyWindow(hwnd);

    return 0;
}

#else

int main(void) {
    printf("This example requires Windows (Win32 HWND).\n");
    printf("It demonstrates cgfx_ctx_init_external() for embedding\n");
    printf("cgfx into an editor or host application.\n");
    return 0;
}

#endif
