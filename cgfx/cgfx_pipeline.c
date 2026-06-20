/**
 * @file cgfx_pipeline.c
 * @brief Implementation of render pipeline creation with defaults.
 */
#include "cgfx_pipeline.h"
#include "cgfx_webgpu.h"

#include <stddef.h>
#include <stdio.h>


WGPURenderPipeline cgfx_pipeline_create(const CgfxCtx *ctx,
                                         const CgfxPipelineDesc *desc) {
    /* ── Apply defaults for zero-initialized fields ───────────── */
    const char *vs_entry = desc->vertex_entry  ? desc->vertex_entry  : "vs_main";
    const char *fs_entry = desc->fragment_entry ? desc->fragment_entry : "fs_main";

    WGPUPrimitiveTopology topology = desc->topology ? desc->topology
                                                    : WGPUPrimitiveTopology_TriangleList;
    WGPUFrontFace front_face = desc->front_face ? desc->front_face
                                                : WGPUFrontFace_CCW;

    /* ── Build the pipeline descriptor ────────────────────────── */
    WGPURenderPipelineDescriptor pipeline_desc = {};
    pipeline_desc.nextInChain = nullptr;

    /*
     * Vertex state: configures the programmable vertex shader stage.
     * The vertex buffers array describes how vertex data is laid out
     * in GPU memory — stride, step mode, and attribute formats/offsets.
     * When no vertex buffers are provided (count=0), the shader must
     * generate vertices procedurally (e.g., using vertex_index builtin).
     */
    pipeline_desc.vertex.module = desc->shader->module;
    pipeline_desc.vertex.entryPoint = CGFX_STR(vs_entry);
    pipeline_desc.vertex.constantCount = 0;
    pipeline_desc.vertex.constants = nullptr;
    pipeline_desc.vertex.bufferCount = desc->vertex_buffer_count;
    pipeline_desc.vertex.buffers = desc->vertex_layouts;

    /*
     * Primitive state: controls how vertices are assembled into primitives.
     * - topology: how vertices are grouped (triangles, lines, points)
     * - frontFace: which winding order is considered front-facing (for culling)
     * - cullMode: whether to discard front/back faces (optimization)
     * - stripIndexFormat: only relevant for strip topologies
     */
    pipeline_desc.primitive.topology = topology;

    bool is_strip = (topology == WGPUPrimitiveTopology_TriangleStrip ||
                     topology == WGPUPrimitiveTopology_LineStrip);
    pipeline_desc.primitive.stripIndexFormat = is_strip
        ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Undefined;
    pipeline_desc.primitive.frontFace = front_face;
    pipeline_desc.primitive.cullMode = desc->cull_mode; /* 0 = None */

    /*
     * Fragment state: configures the programmable fragment shader stage.
     * Specifies the shader, entry point, and color target(s) that the
     * fragment shader writes to.
     */
    WGPUFragmentState fragment_state = {};
    fragment_state.module = desc->shader->module;
    fragment_state.entryPoint = CGFX_STR(fs_entry);
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;

    /*
     * Color targets: one entry per fragment @location output. Defaults to a
     * single opaque target at the surface format. Callers can supply an array
     * for blending, offscreen formats, or multiple render targets (MRT).
     *
     * Opaque is the default — WebGPU performs no blend when target.blend is
     * NULL. Per-target blend is attached only when blend_enable is set.
     *
     * WGPUColorWriteMask_All == 0 would clash with the "0 = default" rule, so
     * a zero write_mask is interpreted as All (writing nothing is not the
     * sensible default; use raw WebGPU for that rare case).
     */
    #define CGFX_MAX_COLOR_TARGETS 8 /* WebGPU default maxColorAttachments */

    WGPUColorTargetState targets[CGFX_MAX_COLOR_TARGETS] = {};
    WGPUBlendState       blends[CGFX_MAX_COLOR_TARGETS]  = {};

    uint32_t target_count = desc->color_target_count ? desc->color_target_count : 1;
    if (target_count > CGFX_MAX_COLOR_TARGETS) {
        fprintf(stderr, "[cgfx_pipeline] color_target_count %u exceeds max %d; clamping\n",
                target_count, CGFX_MAX_COLOR_TARGETS);
        target_count = CGFX_MAX_COLOR_TARGETS;
    }

    for (uint32_t i = 0; i < target_count; i++) {
        const CgfxColorTarget *ct = desc->color_targets ? &desc->color_targets[i] : nullptr;

        targets[i].format = (ct && ct->format) ? ct->format : ctx->surface_format;
        targets[i].writeMask = (ct && ct->write_mask) ? ct->write_mask
                                                       : WGPUColorWriteMask_All;
        if (ct && ct->blend_enable) {
            blends[i] = ct->blend;
            targets[i].blend = &blends[i];
        } else {
            targets[i].blend = nullptr; /* opaque */
        }
    }

    fragment_state.targetCount = target_count;
    fragment_state.targets = targets;
    pipeline_desc.fragment = &fragment_state;

    /*
     * Depth/stencil state: disabled by default (nullptr).
     * When desc->depth_test is true, enables depth testing with
     * Less comparison and depth writes. This is needed for 3D scenes
     * where closer objects should occlude farther ones.
     */
    WGPUDepthStencilState depth_stencil = {};
    if (desc->depth_test) {
        WGPUStencilFaceState face_state = {};
        face_state.compare = WGPUCompareFunction_Always;
        face_state.failOp = WGPUStencilOperation_Keep;
        face_state.depthFailOp = WGPUStencilOperation_Keep;
        face_state.passOp = WGPUStencilOperation_Keep;

        WGPUTextureFormat depth_fmt = desc->depth_format ? desc->depth_format : WGPUTextureFormat_Depth24Plus;
        depth_stencil.format = depth_fmt;
        depth_stencil.depthWriteEnabled = !desc->depth_write_disabled;
        depth_stencil.depthCompare = desc->depth_compare
            ? desc->depth_compare : WGPUCompareFunction_Less;
        depth_stencil.stencilReadMask = 0xFFFFFFFF;
        depth_stencil.stencilWriteMask = 0xFFFFFFFF;
        depth_stencil.depthBias = 0;
        depth_stencil.depthBiasSlopeScale = 0;
        depth_stencil.depthBiasClamp = 0;
        depth_stencil.stencilFront = face_state;
        depth_stencil.stencilBack = face_state;
        /* Stencil defaults to no-op (all zeros from = {}) */
        pipeline_desc.depthStencil = &depth_stencil;
    } else {
        pipeline_desc.depthStencil = nullptr;
    }

    /*
     * Multisample state: 1 sample per pixel (no MSAA).
     * mask = ~0u means all sample bits are active.
     * alphaToCoverageEnabled = false (no coverage-based transparency).
     */
    pipeline_desc.multisample.count = desc->sample_count ? desc->sample_count : 1;
    pipeline_desc.multisample.mask = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = desc->alpha_to_coverage;

    pipeline_desc.layout = desc->shader->pipeline_layout;

    return wgpuDeviceCreateRenderPipeline(ctx->device, &pipeline_desc);
}


void cgfx_pipeline_destroy(WGPURenderPipeline pipeline) {
    if (pipeline)
        wgpuRenderPipelineRelease(pipeline);
}


WGPUBlendState cgfx_blend_alpha(void) {
    return (WGPUBlendState){
        .color = { .srcFactor = WGPUBlendFactor_SrcAlpha,
                   .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                   .operation = WGPUBlendOperation_Add },
        .alpha = { .srcFactor = WGPUBlendFactor_One,
                   .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                   .operation = WGPUBlendOperation_Add },
    };
}


WGPUBlendState cgfx_blend_additive(void) {
    return (WGPUBlendState){
        .color = { .srcFactor = WGPUBlendFactor_One,
                   .dstFactor = WGPUBlendFactor_One,
                   .operation = WGPUBlendOperation_Add },
        .alpha = { .srcFactor = WGPUBlendFactor_One,
                   .dstFactor = WGPUBlendFactor_One,
                   .operation = WGPUBlendOperation_Add },
    };
}


WGPUBlendState cgfx_blend_premultiplied(void) {
    return (WGPUBlendState){
        .color = { .srcFactor = WGPUBlendFactor_One,
                   .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                   .operation = WGPUBlendOperation_Add },
        .alpha = { .srcFactor = WGPUBlendFactor_One,
                   .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                   .operation = WGPUBlendOperation_Add },
    };
}
