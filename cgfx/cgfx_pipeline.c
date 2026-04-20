/**
 * @file cgfx_pipeline.c
 * @brief Implementation of render pipeline creation with defaults.
 */
#include "cgfx_pipeline.h"

#include <stddef.h>


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
    pipeline_desc.vertex.entryPoint = vs_entry;
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
    pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipeline_desc.primitive.frontFace = front_face;
    pipeline_desc.primitive.cullMode = desc->cull_mode; /* 0 = None */

    /*
     * Fragment state: configures the programmable fragment shader stage.
     * Specifies the shader, entry point, and color target(s) that the
     * fragment shader writes to.
     */
    WGPUFragmentState fragment_state = {};
    fragment_state.module = desc->shader->module;
    fragment_state.entryPoint = fs_entry;
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;

    /*
     * Blend state: standard alpha blending.
     * Color: output = src * srcAlpha + dst * (1 - srcAlpha)
     * Alpha: output = dst * 1 (preserve destination alpha)
     *
     * This is the most common blending mode for transparent/translucent
     * rendering. For opaque-only rendering, blend can be set to NULL
     * in the color target, but this default works for both cases.
     */
    WGPUBlendState blend_state = {};
    blend_state.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend_state.color.operation = WGPUBlendOperation_Add;
    blend_state.alpha.srcFactor = WGPUBlendFactor_Zero;
    blend_state.alpha.dstFactor = WGPUBlendFactor_One;
    blend_state.alpha.operation = WGPUBlendOperation_Add;

    /*
     * Color target state: describes the format and blending of each
     * render target. We have one target matching the window surface format.
     * writeMask = All means the shader can write to R, G, B, and A channels.
     */
    WGPUColorTargetState color_target = {};
    color_target.format = ctx->surface_format;
    color_target.blend = &blend_state;
    color_target.writeMask = WGPUColorWriteMask_All;

    fragment_state.targetCount = 1;
    fragment_state.targets = &color_target;
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
        depth_stencil.depthWriteEnabled = true;
        depth_stencil.depthCompare = WGPUCompareFunction_Less;
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
    pipeline_desc.multisample.count = 1;
    pipeline_desc.multisample.mask = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = false;

    pipeline_desc.layout = desc->shader->pipeline_layout;

    return wgpuDeviceCreateRenderPipeline(ctx->device, &pipeline_desc);
}
