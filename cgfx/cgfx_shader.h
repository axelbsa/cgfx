/**
 * @file cgfx_shader.h
 * @brief Shader module creation, bind group layout management, and bind group helpers.
 *
 * CgfxShader wraps a WGPUShaderModule together with the bind group layouts
 * and pipeline layout derived from a user-provided CgfxShaderDesc. The
 * shader owns the layouts; bind groups are created separately and owned
 * by the caller.
 *
 * When no CgfxShaderDesc is provided (NULL), the shader has no bind group
 * layouts and the pipeline layout is NULL (automatic).
 */
#ifndef CGFX_SHADER_H
#define CGFX_SHADER_H

#include <webgpu/webgpu.h>
#include <stdint.h>
#include "cgfx_ctx.h"
#include "cgfx_buffer.h"

/**
 * Describes a single binding slot within a bind group layout.
 *
 * Zero-init defaults:
 *   - binding = 0  (@binding(0))
 *   - visibility = 0 → Vertex | Fragment
 *   - type = 0 → Uniform
 *   - min_binding_size = 0 (no minimum enforced)
 */
typedef struct CgfxBindingDesc {
    uint32_t               binding;           /**< @binding(N) index.                  */
    WGPUShaderStageFlags   visibility;        /**< Shader stage visibility. 0 = Vertex | Fragment. */
    WGPUBufferBindingType  type;              /**< Buffer binding type. 0 = Uniform.   */
    uint64_t               min_binding_size;  /**< Minimum buffer size. 0 = none.      */
} CgfxBindingDesc;

/**
 * Describes one bind group (@group(N)).
 */
typedef struct CgfxGroupDesc {
    uint32_t              binding_count;  /**< Number of bindings in this group. */
    const CgfxBindingDesc *bindings;      /**< Array of binding descriptions.    */
} CgfxGroupDesc;

/**
 * Shader creation descriptor. Describes the bind group layouts
 * the shader expects. Pass NULL to cgfx_shader_create for shaders
 * with no bindings.
 */
typedef struct CgfxShaderDesc {
    uint32_t              group_count;  /**< Number of bind groups. */
    const CgfxGroupDesc  *groups;       /**< Array indexed by @group(N). */
} CgfxShaderDesc;

/**
 * A compiled shader with its bind group layouts and pipeline layout.
 *
 * All fields are public — use shader.group_layouts[i] directly for
 * advanced bind group creation via raw WebGPU.
 */
typedef struct CgfxShader {
    WGPUShaderModule      module;          /**< The compiled WGSL shader module.    */
    WGPUPipelineLayout    pipeline_layout; /**< Pipeline layout (NULL = automatic). */
    WGPUBindGroupLayout  *group_layouts;   /**< Array of bind group layouts.        */
    uint32_t              group_count;     /**< Number of bind group layouts.       */
} CgfxShader;

/**
 * Create a shader from a WGSL source string.
 *
 * Compiles the WGSL code and optionally builds bind group layouts and a
 * pipeline layout from the provided descriptor. Pass NULL for desc when
 * the shader has no uniform/storage bindings.
 *
 * Example (no bindings):
 *   CgfxShader shader = cgfx_shader_create(&ctx, "simple", wgsl, nullptr);
 *
 * Example (with uniform):
 *   CgfxShader shader = cgfx_shader_create(&ctx, "lit", wgsl,
 *       &(CgfxShaderDesc){
 *           .group_count = 1,
 *           .groups = (CgfxGroupDesc[]){{
 *               .binding_count = 1,
 *               .bindings = (CgfxBindingDesc[]){{
 *                   .binding = 0,
 *                   .min_binding_size = sizeof(MyUniforms),
 *               }},
 *           }},
 *       });
 *
 * @param ctx    Initialized context.
 * @param label  Debug label for the shader module (may be NULL).
 * @param wgsl   Null-terminated WGSL source code.
 * @param desc   Bind group layout description, or NULL for no bindings.
 * @return       A CgfxShader. Call cgfx_shader_destroy() to release.
 */
CgfxShader cgfx_shader_create(const CgfxCtx *ctx,
                               const char *label,
                               const char *wgsl,
                               const CgfxShaderDesc *desc);

/**
 * Create a shader by loading WGSL from a file.
 *
 * Reads the file, passes the contents to cgfx_shader_create(), and frees
 * the file buffer. Otherwise identical to cgfx_shader_create().
 *
 * @param ctx    Initialized context.
 * @param label  Debug label (may be NULL).
 * @param path   Path to a WGSL source file.
 * @param desc   Bind group layout description, or NULL.
 * @return       A CgfxShader. Call cgfx_shader_destroy() to release.
 */
CgfxShader cgfx_shader_create_from_file(const CgfxCtx *ctx,
                                         const char *label,
                                         const char *path,
                                         const CgfxShaderDesc *desc);

/**
 * Create a bind group for a specific @group index using the shader's layout.
 *
 * Each buffer maps to consecutive binding indices: buffers[0] → @binding(0),
 * buffers[1] → @binding(1), etc. For non-consecutive bindings or non-buffer
 * resources, use shader->group_layouts[group_index] with raw WebGPU.
 *
 * @param ctx           Initialized context.
 * @param shader        Shader with bind group layouts.
 * @param group_index   The @group(N) index.
 * @param buffers       Array of CgfxBuffer (one per binding).
 * @param buffer_count  Number of buffers.
 * @return              Bind group handle. Caller releases with wgpuBindGroupRelease().
 */
WGPUBindGroup cgfx_shader_create_bind_group(const CgfxCtx *ctx,
                                            const CgfxShader *shader,
                                            uint32_t group_index,
                                            const CgfxBuffer *buffers,
                                            uint32_t buffer_count);

/**
 * Set bind groups on a render pass.
 *
 * Sets groups[0] at @group(0), groups[1] at @group(1), etc.
 * Convenience wrapper around wgpuRenderPassEncoderSetBindGroup.
 *
 * @param pass         Active render pass encoder.
 * @param groups       Array of bind group handles.
 * @param group_count  Number of bind groups to set.
 */
void cgfx_shader_bind(WGPURenderPassEncoder pass,
                       const WGPUBindGroup *groups,
                       uint32_t group_count);

/**
 * Destroy a shader and release all owned resources.
 *
 * Releases the shader module, pipeline layout, and all bind group layouts.
 * Does NOT release bind groups created via cgfx_shader_create_bind_group
 * — those are owned by the caller.
 *
 * @param shader  Shader to destroy.
 */
void cgfx_shader_destroy(CgfxShader *shader);

#endif /* CGFX_SHADER_H */
