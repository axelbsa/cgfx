/**
 * @file main.c
 * @brief Compute example — GPU vector addition with read-back.
 *
 * Demonstrates standalone compute: create storage buffers, dispatch
 * a compute shader, copy results to a mapping buffer, read back to CPU.
 */
#include <stdio.h>
#include "cgfx.h"

static const char *compute_shader =
    "@group(0) @binding(0) var<storage, read>       input_a : array<f32>;\n"
    "@group(0) @binding(1) var<storage, read>       input_b : array<f32>;\n"
    "@group(0) @binding(2) var<storage, read_write> output  : array<f32>;\n"
    "\n"
    "@compute @workgroup_size(64)\n"
    "fn cs_main(@builtin(global_invocation_id) id : vec3u) {\n"
    "    let i = id.x;\n"
    "    if (i < arrayLength(&output)) {\n"
    "        output[i] = input_a[i] + input_b[i];\n"
    "    }\n"
    "}\n";

#define N 256

int main(void) {
    CgfxCtx ctx;
    if (!cgfx_ctx_init(&ctx, &(CgfxCtxDesc){
        .width = 1, .height = 1,
        .title = "cgfx compute",
        .limits = cgfx_default_limits(),
    })) return 1;

    /* Prepare input data: a[i] = i, b[i] = i * 2 */
    float a[N], b[N];
    for (int i = 0; i < N; i++) {
        a[i] = (float)i;
        b[i] = (float)(i * 2);
    }

    /* Create shader with 3 storage buffer bindings */
    CgfxShader shader = cgfx_shader_create(&ctx, "vector_add", compute_shader,
        &(CgfxShaderDesc){
            .group_count = 1,
            .groups = (CgfxGroupDesc[]){{
                .binding_count = 3,
                .bindings = (CgfxBindingDesc[]){
                    { .binding = 0, .type = WGPUBufferBindingType_ReadOnlyStorage,
                      .visibility = WGPUShaderStage_Compute },
                    { .binding = 1, .type = WGPUBufferBindingType_ReadOnlyStorage,
                      .visibility = WGPUShaderStage_Compute },
                    { .binding = 2, .type = WGPUBufferBindingType_Storage,
                      .visibility = WGPUShaderStage_Compute },
                },
            }},
        });

    /* Create compute pipeline */
    WGPUComputePipeline pipeline = cgfx_compute_pipeline_create(&ctx,
        &(CgfxComputeDesc){ .shader = &shader });
    if (!pipeline) {
        fprintf(stderr, "Failed to create compute pipeline\n");
        cgfx_shader_destroy(&shader);
        cgfx_ctx_destroy(&ctx);
        return 1;
    }

    /* Create storage buffers */
    CgfxBuffer buf_a   = cgfx_buffer_create_storage(&ctx, a, sizeof(a));
    CgfxBuffer buf_b   = cgfx_buffer_create_storage(&ctx, b, sizeof(b));
    CgfxBuffer buf_out = cgfx_buffer_create_storage(&ctx, nullptr, sizeof(a));

    /* Create bind group */
    WGPUBindGroup bg = cgfx_bind_group_create_buffers(&ctx, &shader, 0,
        (CgfxBuffer[]){ buf_a, buf_b, buf_out }, 3);

    /* Dispatch compute shader */
    CgfxComputePass cp;
    cgfx_compute_begin(&ctx, &cp);
    wgpuComputePassEncoderSetPipeline(cp.pass, pipeline);
    cgfx_shader_bind_compute(cp.pass, &bg, 1);
    wgpuComputePassEncoderDispatchWorkgroups(cp.pass, N / 64, 1, 1);
    cgfx_compute_end(&ctx, &cp);

    /* Read back results */
    CgfxBuffer readback = cgfx_buffer_create_mapping(&ctx, sizeof(a), N);
    cgfx_buffer_copy(&ctx, &buf_out, &readback, 0);

    float result[N];
    if (!cgfx_buffer_read(&ctx, &readback, result, sizeof(result))) {
        fprintf(stderr, "Failed to read back compute results\n");
        return 1;
    }

    printf("Vector addition: a[i] + b[i] where a[i]=i, b[i]=i*2\n");
    printf("First 8 results: ");
    for (int i = 0; i < 8; i++)
        printf("%.0f ", result[i]);
    printf("...\n");
    printf("Expected:         0 3 6 9 12 15 18 21 ...\n");

    bool correct = true;
    for (int i = 0; i < N; i++) {
        if (result[i] != (float)(i + i * 2)) {
            printf("MISMATCH at index %d: got %.0f, expected %.0f\n",
                   i, result[i], (float)(i + i * 2));
            correct = false;
            break;
        }
    }
    if (correct)
        printf("All %d results correct!\n", N);

    /* Cleanup */
    cgfx_bind_group_destroy(bg);
    cgfx_buffer_destroy(&readback);
    cgfx_buffer_destroy(&buf_out);
    cgfx_buffer_destroy(&buf_b);
    cgfx_buffer_destroy(&buf_a);
    cgfx_compute_pipeline_destroy(pipeline);
    cgfx_shader_destroy(&shader);
    cgfx_ctx_destroy(&ctx);

    return 0;
}
