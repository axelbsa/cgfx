/**
 * @file cgfx_texture.c
 * @brief Implementation of GPU texture creation, upload, and sampler helpers.
 */
#include "cgfx_texture.h"
#include "cgfx_ctx.h"

#include <stdio.h>


static bool is_depth_only_format(WGPUTextureFormat format) {
    return format == WGPUTextureFormat_Depth16Unorm
        || format == WGPUTextureFormat_Depth24Plus
        || format == WGPUTextureFormat_Depth32Float;
}

static bool is_depth_format(WGPUTextureFormat format) {
    return is_depth_only_format(format)
        || format == WGPUTextureFormat_Depth24PlusStencil8
        || format == WGPUTextureFormat_Depth32FloatStencil8;
}


static uint32_t bytes_per_pixel(WGPUTextureFormat format) {
    switch (format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
        return 1;
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
        return 2;
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
        return 4;
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
        return 8;
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        return 16;
    default:
        return 0;
    }
}


CgfxTexture cgfx_texture_create(const CgfxCtx *ctx,
                                 const CgfxTextureDesc *desc) {
    CgfxTexture tex = {};

    uint32_t width      = desc->width;
    uint32_t height     = desc->height     ? desc->height     : 1;
    uint32_t depth      = desc->depth      ? desc->depth      : 1;
    uint32_t mip_levels = desc->mip_levels ? desc->mip_levels : 1;

    WGPUTextureFormat format = desc->format
        ? desc->format
        : WGPUTextureFormat_RGBA8Unorm;

    WGPUTextureDimension dimension = desc->dimension
        ? desc->dimension
        : WGPUTextureDimension_2D;

    uint32_t sample_count = desc->sample_count ? desc->sample_count : 1;

    WGPUTextureUsageFlags usage = desc->usage;
    if (!usage) {
        usage = is_depth_format(format)
            ? WGPUTextureUsage_RenderAttachment
            : (WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
    }

    WGPUTextureDescriptor texture_desc = {};
    texture_desc.dimension     = dimension;
    texture_desc.format        = format;
    texture_desc.size          = (WGPUExtent3D){ width, height, depth };
    texture_desc.mipLevelCount = mip_levels;
    texture_desc.sampleCount   = sample_count;
    texture_desc.usage         = usage;
    texture_desc.viewFormatCount = 1;
    texture_desc.viewFormats     = &format;

    tex.texture = wgpuDeviceCreateTexture(ctx->device, &texture_desc);
    if (!tex.texture) {
        fprintf(stderr, "[cgfx_texture] Failed to create texture (%ux%u)\n",
                width, height);
        return tex;
    }

    WGPUTextureViewDimension view_dim = desc->view_dimension;
    if (!view_dim) {
        if (dimension == WGPUTextureDimension_3D)
            view_dim = WGPUTextureViewDimension_3D;
        else if (dimension == WGPUTextureDimension_1D)
            view_dim = WGPUTextureViewDimension_1D;
        else if (depth > 1)
            view_dim = WGPUTextureViewDimension_2DArray;
        else
            view_dim = WGPUTextureViewDimension_2D;
    }

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format          = format;
    view_desc.dimension       = view_dim;
    view_desc.baseMipLevel    = 0;
    view_desc.mipLevelCount   = mip_levels;
    view_desc.baseArrayLayer  = 0;
    view_desc.arrayLayerCount = depth;
    view_desc.aspect          = is_depth_only_format(format)
                                    ? WGPUTextureAspect_DepthOnly
                                    : WGPUTextureAspect_All;

    tex.view = wgpuTextureCreateView(tex.texture, &view_desc);

    tex.format     = format;
    tex.width      = width;
    tex.height     = height;
    tex.depth      = depth;
    tex.mip_levels = mip_levels;
    tex.ok         = (tex.texture != nullptr && tex.view != nullptr);

    return tex;
}


void cgfx_texture_write(const CgfxCtx *ctx,
                         const CgfxTexture *texture,
                         const void *data, uint64_t data_size) {
    uint32_t bpp = bytes_per_pixel(texture->format);
    if (bpp == 0) {
        fprintf(stderr,
                "[cgfx_texture] Unsupported format for cgfx_texture_write. "
                "Use raw wgpuQueueWriteTexture instead.\n");
        return;
    }

    WGPUImageCopyTexture dest = {};
    dest.texture  = texture->texture;
    dest.mipLevel = 0;
    dest.origin   = (WGPUOrigin3D){ 0, 0, 0 };
    dest.aspect   = WGPUTextureAspect_All;

    WGPUTextureDataLayout layout = {};
    layout.offset       = 0;
    layout.bytesPerRow   = texture->width * bpp;
    layout.rowsPerImage  = texture->height;

    WGPUExtent3D size = {
        texture->width,
        texture->height,
        texture->depth,
    };

    wgpuQueueWriteTexture(ctx->queue, &dest, data, data_size, &layout, &size);
}


void cgfx_texture_write_layer(const CgfxCtx *ctx,
                               const CgfxTexture *texture,
                               uint32_t layer,
                               const void *data, uint64_t data_size) {
    uint32_t bpp = bytes_per_pixel(texture->format);
    if (bpp == 0) {
        fprintf(stderr,
                "[cgfx_texture] Unsupported format for cgfx_texture_write_layer. "
                "Use raw wgpuQueueWriteTexture instead.\n");
        return;
    }

    WGPUImageCopyTexture dest = {};
    dest.texture  = texture->texture;
    dest.mipLevel = 0;
    dest.origin   = (WGPUOrigin3D){ 0, 0, layer };
    dest.aspect   = WGPUTextureAspect_All;

    WGPUTextureDataLayout layout = {};
    layout.offset       = 0;
    layout.bytesPerRow   = texture->width * bpp;
    layout.rowsPerImage  = texture->height;

    WGPUExtent3D size = { texture->width, texture->height, 1 };

    wgpuQueueWriteTexture(ctx->queue, &dest, data, data_size, &layout, &size);
}


void cgfx_texture_destroy(CgfxTexture *texture) {
    if (texture->view)
        wgpuTextureViewRelease(texture->view);
    if (texture->texture) {
        wgpuTextureDestroy(texture->texture);
        wgpuTextureRelease(texture->texture);
    }
    *texture = (CgfxTexture){};
}


WGPUSampler cgfx_sampler_create(const CgfxCtx *ctx,
                                 const CgfxSamplerDesc *desc) {
    WGPUSamplerDescriptor sampler_desc = {};

    sampler_desc.magFilter = desc->mag_filter
        ? desc->mag_filter
        : WGPUFilterMode_Linear;

    sampler_desc.minFilter = desc->min_filter
        ? desc->min_filter
        : WGPUFilterMode_Linear;

    sampler_desc.mipmapFilter = desc->mipmap_filter
        ? desc->mipmap_filter
        : WGPUMipmapFilterMode_Linear;

    sampler_desc.addressModeU = desc->address_u
        ? desc->address_u
        : WGPUAddressMode_ClampToEdge;

    sampler_desc.addressModeV = desc->address_v
        ? desc->address_v
        : WGPUAddressMode_ClampToEdge;

    sampler_desc.addressModeW = desc->address_w
        ? desc->address_w
        : WGPUAddressMode_ClampToEdge;

    sampler_desc.maxAnisotropy = desc->max_anisotropy
        ? desc->max_anisotropy
        : 1;

    sampler_desc.compare    = desc->compare;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 32.0f;

    return wgpuDeviceCreateSampler(ctx->device, &sampler_desc);
}
