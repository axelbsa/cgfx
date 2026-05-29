/*
 * review/okcheck.c — headless verification for the T1.5 error model.
 *
 * Creates a WGPU adapter + device with NO window/surface, so it runs in
 * headless / X-forwarded environments where the example apps cannot. Verifies
 * that cgfx_shader_create reports success/failure via `.ok` and surfaces WGSL
 * compile errors.
 *
 * Build & run (from the repo root, after `cmake --build build`):
 *
 *   gcc -std=c23 -DWEBGPU_BACKEND_WGPU \
 *     -I cgfx -I vendor/cglm/include \
 *     -I build/_deps/webgpu-backend-wgpu-src/include \
 *     review/okcheck.c build/cgfx/libcgfx.a build/examples/libwgpu_native.so \
 *     -lm -o /tmp/okcheck
 *   LD_LIBRARY_PATH=build/examples /tmp/okcheck
 *
 * Expect: GOOD: ok=1 / BAD: ok=0 (with a WGSL error printed) / ZERO: ok=0 / RESULT: PASS
 * Exit code 0 = pass, 1 = fail, 77 = skipped (no adapter/device available).
 */
#include <stdio.h>
#include "cgfx.h"
#include "cgfx_internal.h"

int main(void) {
    WGPUInstanceDescriptor id = {};
    WGPUInstance inst = wgpuCreateInstance(&id);
    WGPURequestAdapterOptions ao = {};
    WGPUAdapter adapter = cgfx__request_adapter_sync(inst, &ao);
    if (!adapter) { fprintf(stderr, "TEST: no adapter (skip)\n"); return 77; }

    WGPUDeviceDescriptor dd = {};
    dd.label = "test device";
    WGPUDevice dev = cgfx__request_device_sync(adapter, &dd);
    wgpuInstanceRelease(inst);
    if (!dev) { fprintf(stderr, "TEST: no device (skip)\n"); return 77; }

    CgfxCtx ctx = {};
    ctx.device = dev;
    ctx.queue  = wgpuDeviceGetQueue(dev);

    const char *good =
        "@vertex fn vs_main() -> @builtin(position) vec4f {\n"
        "    return vec4f(0.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    /* 'positon' is an unresolved identifier -> WGSL compile error */
    const char *bad =
        "@vertex fn vs_main() -> @builtin(position) vec4f {\n"
        "    return vec4f(positon, 0.0, 1.0);\n"
        "}\n";

    fprintf(stderr, "--- creating GOOD shader ---\n");
    CgfxShader s_ok = cgfx_shader_create(&ctx, "good", good, nullptr);
    fprintf(stderr, "GOOD: ok=%d module=%p\n", s_ok.ok, (void *)s_ok.module);

    fprintf(stderr, "--- creating BAD shader ---\n");
    CgfxShader s_bad = cgfx_shader_create(&ctx, "bad", bad, nullptr);
    fprintf(stderr, "BAD:  ok=%d module=%p\n", s_bad.ok, (void *)s_bad.module);

    /* a zeroed struct must report ok == false */
    CgfxShader zeroed = {};
    fprintf(stderr, "ZERO: ok=%d\n", zeroed.ok);

    int pass = (s_ok.ok == true) && (s_bad.ok == false) && (zeroed.ok == false);
    fprintf(stderr, "RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
