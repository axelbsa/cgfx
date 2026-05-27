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
#include "cgfx_export.h"

typedef struct CgfxTexture CgfxTexture;

/**
 * The kind of resource a binding slot describes.
 *
 * CGFX_BINDING_BUFFER is zero so existing code that doesn't set kind
 * continues to work unchanged.
 */
typedef enum CgfxBindingKind {
    CGFX_BINDING_BUFFER = 0,           /**< Buffer (uniform, storage, read-only storage). */
    CGFX_BINDING_TEXTURE,              /**< Sampled texture.                              */
    CGFX_BINDING_SAMPLER,              /**< Sampler.                                      */
    CGFX_BINDING_STORAGE_TEXTURE,      /**< Storage texture (compute read/write).         */
} CgfxBindingKind;

/**
 * Describes a single binding slot within a bind group layout.
 *
 * Zero-init defaults:
 *   - binding = 0  (@binding(0))
 *   - visibility = 0 → Vertex|Fragment (buffer), Fragment (texture/sampler), Compute (storage texture)
 *   - kind = 0 → Buffer (backward compatible)
 *   - type = 0 → Uniform (buffer kind only)
 *   - min_binding_size = 0 (no minimum enforced)
 *
 * For texture bindings:
 *   (CgfxBindingDesc){ .binding = 0, .kind = CGFX_BINDING_TEXTURE }
 *
 * For sampler bindings:
 *   (CgfxBindingDesc){ .binding = 1, .kind = CGFX_BINDING_SAMPLER }
 */
typedef struct CgfxBindingDesc {
    uint32_t               binding;           /**< @binding(N) index.                    */
    WGPUShaderStageFlags   visibility;        /**< Shader stage visibility. 0 = auto.    */
    CgfxBindingKind        kind;              /**< Binding kind. 0 = Buffer.             */

    WGPUBufferBindingType  type;              /**< Buffer binding type. 0 = Uniform.     */
    uint64_t               min_binding_size;  /**< Minimum buffer size. 0 = none.        */

    WGPUTextureSampleType    sample_type;     /**< Texture sample type. 0 = Float.       */
    WGPUTextureViewDimension view_dimension;  /**< Texture view dimension. 0 = 2D.       */

    WGPUStorageTextureAccess storage_access;  /**< Storage texture access. 0 = WriteOnly.*/
    WGPUTextureFormat        storage_format;  /**< Storage texture format. Required.      */
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
CGFX_API CgfxShader cgfx_shader_create(const CgfxCtx *ctx,
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
CGFX_API CgfxShader cgfx_shader_create_from_file(const CgfxCtx *ctx,
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
CGFX_API WGPUBindGroup cgfx_shader_create_bind_group(const CgfxCtx *ctx,
                                            const CgfxShader *shader,
                                            uint32_t group_index,
                                            const CgfxBuffer *buffers,
                                            uint32_t buffer_count);

/**
 * An entry in a general-purpose bind group, supporting buffers, textures,
 * and samplers. Set exactly one of buffer, texture, or sampler per entry.
 */
typedef struct CgfxBindGroupEntry {
    uint32_t           binding;   /**< @binding(N) index.                          */
    const CgfxBuffer  *buffer;    /**< Non-NULL for buffer bindings.               */
    const CgfxTexture *texture;   /**< Non-NULL for texture bindings (uses view).  */
    WGPUSampler        sampler;   /**< Non-NULL for sampler bindings.              */
} CgfxBindGroupEntry;

/**
 * Create a bind group with mixed buffer, texture, and sampler entries.
 *
 * Unlike cgfx_shader_create_bind_group() (buffer-only, positional),
 * this function uses explicit binding indices and supports all resource
 * types.
 *
 * Example (texture + sampler):
 *   WGPUBindGroup bg = cgfx_bind_group_create(&ctx, &shader, 0,
 *       (CgfxBindGroupEntry[]){
 *           { .binding = 0, .texture = &my_texture },
 *           { .binding = 1, .sampler = my_sampler },
 *       }, 2);
 *
 * @param ctx          Initialized context.
 * @param shader       Shader with bind group layouts.
 * @param group_index  The @group(N) index.
 * @param entries      Array of bind group entries.
 * @param entry_count  Number of entries.
 * @return             Bind group handle. Caller releases with wgpuBindGroupRelease().
 */
CGFX_API WGPUBindGroup cgfx_bind_group_create(const CgfxCtx *ctx,
                                               const CgfxShader *shader,
                                               uint32_t group_index,
                                               const CgfxBindGroupEntry *entries,
                                               uint32_t entry_count);

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
CGFX_API void cgfx_shader_bind(WGPURenderPassEncoder pass,
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
CGFX_API void cgfx_shader_destroy(CgfxShader *shader);

#endif /* CGFX_SHADER_H */
