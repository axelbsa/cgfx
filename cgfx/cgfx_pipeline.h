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
 * Configuration for creating a render pipeline.
 *
 * Zero-initialize for sensible defaults:
 *   - Triangle list topology
 *   - No backface culling
 *   - Counter-clockwise front face
 *   - No depth testing
 *   - Standard alpha blending
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
    bool                    depth_test;      /**< Enable depth/stencil testing. Default: false. */
    WGPUTextureFormat       depth_format;    /**< Depth texture format (only if depth_test).
                                            0 = Depth24Plus.                          */

    /** Vertex buffer layouts (optional). Pass the result of
     *  cgfx_mesh_vertex_layout() here when rendering meshes. */
    uint32_t                         vertex_buffer_count;
    const WGPUVertexBufferLayout    *vertex_layouts;
} CgfxPipelineDesc;

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
 *   - Single color target matching ctx->surface_format
 *   - Standard alpha blending: srcAlpha / oneMinusSrcAlpha for color,
 *     zero / one for alpha channel
 *   - All color channels writable
 *
 *   Primitive state:
 *   - Configurable topology (default: triangle list)
 *   - Configurable cull mode and front face
 *   - No strip index format (vertices processed sequentially)
 *
 *   Depth/stencil:
 *   - Disabled by default. When enabled, creates a depth-less comparison
 *     with write enabled.
 *
 *   Multisample:
 *   - 1 sample per pixel, full mask, no alpha-to-coverage
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
