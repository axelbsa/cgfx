/**
 * @file cgfx_pipeline.h
 * @brief Render pipeline creation with sensible defaults.
 *
 * Wraps the verbose WebGPU render pipeline descriptor (vertex state,
 * fragment state, blend state, primitive topology, multisample, etc.)
 * behind a simple configuration struct with zero-init defaults.
 *
 * For advanced pipelines, you can always create WGPURenderPipeline
 * directly using ctx->device — this module is for the common case.
 */
#ifndef CGFX_PIPELINE_H
#define CGFX_PIPELINE_H

#include <webgpu/webgpu.h>
#include <stdbool.h>
#include <stdint.h>
#include "cgfx_ctx.h"
#include "cgfx_shader.h"
#include "cgfx_export.h"

/**
 * Description of a single color render target (one fragment @location output).
 *
 * Zero-initialize for the common case: a target at the surface format, opaque
 * (no blending), writing all channels.
 *
 *   - format = 0        → ctx->surface_format
 *   - blend_enable false → opaque (no blend state attached)
 *   - write_mask = 0     → WGPUColorWriteMask_All (note: 0 is treated as All,
 *                          not "write nothing")
 *
 * For blending, set blend_enable = true and fill `blend`, e.g.:
 *   (CgfxColorTarget){ .blend_enable = true, .blend = cgfx_blend_alpha() }
 */
typedef struct CgfxColorTarget {
    WGPUTextureFormat   format;       /**< Target texture format. 0 = ctx->surface_format. */
    bool                blend_enable; /**< false = opaque (no blend). */
    WGPUBlendState      blend;        /**< Blend state, used only when blend_enable is true. */
    WGPUColorWriteMask  write_mask;   /**< Channel write mask. 0 = All. */
} CgfxColorTarget;

/**
 * Configuration for creating a render pipeline.
 *
 * Zero-initialize for sensible defaults:
 *   - Triangle list topology
 *   - No backface culling
 *   - Counter-clockwise front face
 *   - No depth testing
 *   - One opaque color target at the surface format (no blending)
 *   - Pipeline layout from shader (automatic if shader has no bind groups)
 *   - Entry points: "vs_main" / "fs_main"
 *
 * The only required field is `shader`.
 *
 * Example:
 *   CgfxPipelineDesc desc = { .shader = &my_shader };
 *   WGPURenderPipeline pipeline = cgfx_pipeline_create(&ctx, &desc);
 */
typedef struct CgfxPipelineDesc {
    const CgfxShader        *shader;         /**< Required. Shader with module and layouts. */
    const char              *vertex_entry;    /**< Vertex shader entry point. NULL = "vs_main". */
    const char              *fragment_entry;  /**< Fragment shader entry point. NULL = "fs_main". */
    WGPUPrimitiveTopology   topology;    /**< Primitive topology. 0 = TriangleList.     */
    WGPUCullMode            cull_mode;       /**< Face culling mode. 0 = None.              */
    WGPUFrontFace           front_face;      /**< Front face winding. 0 = CCW.              */
    bool                    depth_test;          /**< Enable depth/stencil testing. Default: false. */
    WGPUTextureFormat       depth_format;        /**< Depth texture format (only if depth_test).
                                                      0 = Depth24Plus.                          */
    WGPUCompareFunction     depth_compare;       /**< Depth compare function. 0 = Less.         */
    bool                    depth_write_disabled; /**< true = depth test without writing depth.
                                                      false (default) = depth writes enabled.   */

    uint32_t                sample_count;        /**< MSAA sample count. 0 = 1 (no MSAA).       */
    bool                    alpha_to_coverage;   /**< Enable alpha-to-coverage. Default: false.  */

    /** Vertex buffer layouts (optional). Pass the result of
     *  cgfx_mesh_vertex_layout() here when rendering meshes. */
    uint32_t                         vertex_buffer_count;
    const WGPUVertexBufferLayout    *vertex_layouts;

    /** Color render targets (optional). When count is 0, the pipeline gets a
     *  single opaque target at ctx->surface_format. Provide an array for
     *  blending, offscreen formats, or multiple render targets (MRT). The
     *  count and formats must match the render pass the pipeline is used in. */
    uint32_t                  color_target_count;
    const CgfxColorTarget    *color_targets;
} CgfxPipelineDesc;

/**
 * Blend-state presets for CgfxColorTarget.blend (use with blend_enable = true).
 *
 *   - cgfx_blend_alpha:         straight alpha (SrcAlpha / OneMinusSrcAlpha)
 *   - cgfx_blend_additive:      additive (One / One)
 *   - cgfx_blend_premultiplied: premultiplied alpha (One / OneMinusSrcAlpha)
 */
CGFX_API WGPUBlendState cgfx_blend_alpha(void);
CGFX_API WGPUBlendState cgfx_blend_additive(void);
CGFX_API WGPUBlendState cgfx_blend_premultiplied(void);

/**
 * Create a render pipeline with sensible defaults.
 *
 * Fills in all the verbose WebGPU pipeline descriptor fields:
 *
 *   Vertex state:
 *   - Uses the provided shader module and vertex entry point
 *   - Attaches caller-provided vertex buffer layouts (or none for
 *     procedural vertex generation in the shader)
 *
 *   Fragment state:
 *   - Uses the provided shader module and fragment entry point
 *   - Color targets from desc->color_targets, or — when none are given —
 *     a single opaque target at ctx->surface_format
 *   - Opaque by default (no blending); set per-target blend via color_targets
 *   - All color channels writable by default
 *
 *   Primitive state:
 *   - Configurable topology (default: triangle list)
 *   - Configurable cull mode and front face
 *   - Strip index format auto-derived (Uint32 for strip topologies)
 *
 *   Depth/stencil:
 *   - Disabled by default. When enabled: Less comparison, depth writes on.
 *     Configurable via depth_compare and depth_write_disabled.
 *
 *   Multisample:
 *   - Configurable sample count (default: 1, no MSAA), full mask,
 *     optional alpha-to-coverage
 *
 *   Layout:
 *   - Uses shader->pipeline_layout (NULL = automatic, when shader has no bind groups)
 *
 * @param ctx   Initialized context (uses ctx->device and ctx->surface_format).
 * @param desc  Pipeline configuration. Zero-init for defaults (shader required).
 * @return      Pipeline handle, or NULL on failure.
 *              Caller must release with wgpuRenderPipelineRelease().
 */
CGFX_API WGPURenderPipeline cgfx_pipeline_create(const CgfxCtx *ctx,
                                         const CgfxPipelineDesc *desc);

#endif /* CGFX_PIPELINE_H */
