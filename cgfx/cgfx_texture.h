/**
 * @file cgfx_texture.h
 * @brief GPU texture and sampler abstraction.
 *
 * CgfxTexture wraps a WGPUTexture and its default WGPUTextureView into a
 * single object. Supports sampled textures (for fragment shaders), storage
 * textures (for compute read/write), and render targets.
 *
 * Loading pixel data from disk is not cgfx's concern — the caller loads
 * pixels however they like, then uploads via cgfx_texture_write().
 *
 * Samplers are separate objects (reusable across textures), created with
 * cgfx_sampler_create() which returns a raw WGPUSampler handle.
 */
#ifndef CGFX_TEXTURE_H
#define CGFX_TEXTURE_H

#include <webgpu/webgpu.h>
#include <stdint.h>
#include "cgfx_export.h"

typedef struct CgfxCtx CgfxCtx;

/**
 * A GPU texture with its default view.
 *
 * Fields are public — access texture.view for bind groups,
 * texture.texture for raw WebGPU calls.
 */
typedef struct CgfxTexture {
    WGPUTexture        texture;     /**< GPU texture handle.                         */
    WGPUTextureView    view;        /**< Default view (whole texture, all mips).      */
    WGPUTextureFormat  format;      /**< Pixel format.                               */
    uint32_t           width;       /**< Width in pixels.                            */
    uint32_t           height;      /**< Height in pixels.                           */
    uint32_t           depth;       /**< Depth or array layers (1 for 2D).           */
    uint32_t           mip_levels;  /**< Number of mip levels.                       */
    bool               ok;          /**< True if creation succeeded — check before use. */
} CgfxTexture;

/**
 * Configuration for creating a texture.
 *
 * Zero-initialize for a standard sampled 2D RGBA8 texture:
 *   CgfxTextureDesc desc = { .width = 256, .height = 256 };
 *
 * For a compute storage texture:
 *   CgfxTextureDesc desc = {
 *       .width = 512, .height = 512,
 *       .format = WGPUTextureFormat_RGBA8Unorm,
 *       .usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding,
 *   };
 *
 * For a cube map:
 *   CgfxTextureDesc desc = {
 *       .width = 1024, .height = 1024,
 *       .depth = 6,
 *       .view_dimension = WGPUTextureViewDimension_Cube,
 *       .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
 *   };
 */
typedef struct CgfxTextureDesc {
    uint32_t              width;         /**< Texture width. Required.                 */
    uint32_t              height;        /**< Texture height. 0 = 1.                   */
    uint32_t              depth;         /**< Depth or array layers. 0 = 1.            */
    WGPUTextureFormat     format;        /**< Pixel format. 0 = RGBA8Unorm.            */
    WGPUTextureDimension  dimension;     /**< Texture dimension. 0 = 2D.               */
    uint32_t              mip_levels;    /**< Mip level count. 0 = 1.                  */
    uint32_t              sample_count;  /**< MSAA sample count. 0 = 1.                */
    WGPUTextureUsageFlags    usage;          /**< Usage flags. 0 = TextureBinding|CopyDst (or RenderAttachment for depth). */
    WGPUTextureViewDimension view_dimension; /**< View dimension. 0 = auto-detect from dimension + depth. */
} CgfxTextureDesc;

/**
 * Create a GPU texture and its default view. No data is uploaded.
 *
 * After creation, upload pixel data with cgfx_texture_write(), or leave
 * empty for render targets / storage textures.
 *
 * @param ctx   Initialized context.
 * @param desc  Texture configuration.
 * @return      A CgfxTexture. Call cgfx_texture_destroy() to release.
 */
CGFX_API CgfxTexture cgfx_texture_create(const CgfxCtx *ctx,
                                          const CgfxTextureDesc *desc);

/**
 * Upload pixel data to a texture (full mip 0, from origin 0,0,0).
 *
 * bytesPerRow and rowsPerImage are computed automatically from the texture's
 * format and dimensions. Supports common uncompressed formats (R8, RG8,
 * RGBA8, BGRA8, R16Float, RG16Float, RGBA16Float, R32Float, RG32Float,
 * RGBA32Float).
 *
 * For sub-region writes, mip-level writes, or compressed/exotic formats,
 * use raw wgpuQueueWriteTexture() with texture->texture directly.
 *
 * @param ctx        Initialized context.
 * @param texture    Texture to upload to. Must have CopyDst usage.
 * @param data       Pointer to pixel data.
 * @param data_size  Size of the pixel data in bytes.
 */
CGFX_API void cgfx_texture_write(const CgfxCtx *ctx,
                                  const CgfxTexture *texture,
                                  const void *data, uint64_t data_size);

/**
 * Upload pixel data to a single array layer or cube face.
 *
 * Identical to cgfx_texture_write() but targets a specific layer
 * (origin.z = layer, depthOrArrayLayers = 1). For cube maps, layers
 * 0–5 correspond to +X, −X, +Y, −Y, +Z, −Z.
 *
 * @param ctx        Initialized context.
 * @param texture    Texture to upload to. Must have CopyDst usage.
 * @param layer      Array layer or cube face index.
 * @param data       Pointer to pixel data for one layer.
 * @param data_size  Size of the pixel data in bytes.
 */
CGFX_API void cgfx_texture_write_layer(const CgfxCtx *ctx,
                                        const CgfxTexture *texture,
                                        uint32_t layer,
                                        const void *data, uint64_t data_size);

/**
 * Release a texture and its view.
 *
 * @param texture  Texture to destroy.
 */
CGFX_API void cgfx_texture_destroy(CgfxTexture *texture);

/**
 * Configuration for creating a sampler.
 *
 * Zero-initialize for a linear-filtering, clamp-to-edge sampler:
 *   WGPUSampler sampler = cgfx_sampler_create(&ctx, &(CgfxSamplerDesc){});
 *
 * Zero-init defaults override WebGPU enum zero values to the most
 * commonly useful settings (Linear filtering, ClampToEdge addressing).
 */
typedef struct CgfxSamplerDesc {
    WGPUFilterMode       mag_filter;     /**< Magnification filter. 0 = Linear.    */
    WGPUFilterMode       min_filter;     /**< Minification filter. 0 = Linear.     */
    WGPUMipmapFilterMode mipmap_filter;  /**< Mipmap filter. 0 = Linear.           */
    WGPUAddressMode      address_u;      /**< U-axis address mode. 0 = ClampToEdge.*/
    WGPUAddressMode      address_v;      /**< V-axis address mode. 0 = ClampToEdge.*/
    WGPUAddressMode      address_w;      /**< W-axis address mode. 0 = ClampToEdge.*/
    uint16_t             max_anisotropy; /**< Max anisotropy. 0 = 1.               */
    WGPUCompareFunction  compare;        /**< Comparison function. 0 = none.       */
} CgfxSamplerDesc;

/**
 * Create a sampler.
 *
 * Returns a raw WGPUSampler handle. The caller owns it and must release
 * it with wgpuSamplerRelease() when done.
 *
 * @param ctx   Initialized context.
 * @param desc  Sampler configuration. Pass zero-initialized for defaults.
 * @return      Sampler handle.
 */
CGFX_API WGPUSampler cgfx_sampler_create(const CgfxCtx *ctx,
                                          const CgfxSamplerDesc *desc);

#endif /* CGFX_TEXTURE_H */
