/**
 * @file cgfx_compute.c
 * @brief Implementation of compute pipeline creation and compute pass lifecycle.
 */
#include "cgfx_compute.h"

#include <stddef.h>
#include <string.h>

#include "cgfx_webgpu.h"


WGPUComputePipeline cgfx_compute_pipeline_create(const CgfxCtx *ctx,
                                                  const CgfxComputeDesc *desc) {
    const char *entry = desc->entry_point ? desc->entry_point : "cs_main";

    WGPUComputePipelineDescriptor pipeline_desc = {};
    pipeline_desc.nextInChain = nullptr;
    pipeline_desc.label = CGFX_STR("cgfx compute pipeline");
    pipeline_desc.layout = desc->shader->pipeline_layout;
    pipeline_desc.compute = (WGPUProgrammableStageDescriptor){
        .module = desc->shader->module,
        .entryPoint = CGFX_STR(entry),
    };

    return wgpuDeviceCreateComputePipeline(ctx->device, &pipeline_desc);
}


void cgfx_compute_pipeline_destroy(WGPUComputePipeline pipeline) {
    if (pipeline)
        wgpuComputePipelineRelease(pipeline);
}


bool cgfx_compute_begin(const CgfxCtx *ctx, CgfxComputePass *cp) {
    memset(cp, 0, sizeof(*cp));

    WGPUCommandEncoderDescriptor enc_desc = {};
    enc_desc.nextInChain = nullptr;
    enc_desc.label = CGFX_STR("cgfx compute encoder");
    cp->encoder = wgpuDeviceCreateCommandEncoder(ctx->device, &enc_desc);
    if (!cp->encoder)
        return false;

    WGPUComputePassDescriptor pass_desc = {};
    pass_desc.nextInChain = nullptr;
    pass_desc.label = CGFX_STR("cgfx compute pass");
    cp->pass = wgpuCommandEncoderBeginComputePass(cp->encoder, &pass_desc);
    cp->owns_encoder = true;

    return true;
}


void cgfx_compute_end(const CgfxCtx *ctx, CgfxComputePass *cp) {
    wgpuComputePassEncoderEnd(cp->pass);
    wgpuComputePassEncoderRelease(cp->pass);

    WGPUCommandBufferDescriptor cmd_desc = {};
    cmd_desc.nextInChain = nullptr;
    cmd_desc.label = CGFX_STR("cgfx compute commands");
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(cp->encoder, &cmd_desc);
    wgpuCommandEncoderRelease(cp->encoder);

    wgpuQueueSubmit(ctx->queue, 1, &commands);
    wgpuCommandBufferRelease(commands);

    cgfx__device_sync(ctx->instance, ctx->device, false);

    memset(cp, 0, sizeof(*cp));
}


bool cgfx_compute_pass_begin(WGPUCommandEncoder encoder, CgfxComputePass *cp) {
    memset(cp, 0, sizeof(*cp));

    cp->encoder = encoder;

    WGPUComputePassDescriptor pass_desc = {};
    pass_desc.nextInChain = nullptr;
    pass_desc.label = CGFX_STR("cgfx compute pass");
    cp->pass = wgpuCommandEncoderBeginComputePass(encoder, &pass_desc);
    cp->owns_encoder = false;

    return true;
}


void cgfx_compute_pass_end(CgfxComputePass *cp) {
    wgpuComputePassEncoderEnd(cp->pass);
    wgpuComputePassEncoderRelease(cp->pass);
    cp->pass = nullptr;
}


