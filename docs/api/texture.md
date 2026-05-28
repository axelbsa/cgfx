# Texture

GPU texture and sampler abstraction.

**Header:** `cgfx_texture.h`

---

## Structs

### CgfxTexture

A GPU texture with its default view. All fields are public.

| Field | Type | Description |
|-------|------|-------------|
| `texture` | `WGPUTexture` | GPU texture handle. |
| `view` | `WGPUTextureView` | Default view (whole texture, all mips). |
| `format` | `WGPUTextureFormat` | Pixel format. |
| `width` | `uint32_t` | Width in pixels. |
| `height` | `uint32_t` | Height in pixels. |
| `depth` | `uint32_t` | Depth or array layers (1 for 2D). |
| `mip_levels` | `uint32_t` | Number of mip levels. |

---

### CgfxTextureDesc

Configuration for creating a texture. Zero-initialize for a standard sampled 2D RGBA8 texture.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `width` | `uint32_t` | *(required)* | Texture width. |
| `height` | `uint32_t` | `1` | Texture height. |
| `depth` | `uint32_t` | `1` | Depth or array layers. Set to `6` for cube maps. |
| `format` | `WGPUTextureFormat` | `RGBA8Unorm` | Pixel format. |
| `dimension` | `WGPUTextureDimension` | `2D` | Texture dimension. |
| `mip_levels` | `uint32_t` | `1` | Mip level count. |
| `sample_count` | `uint32_t` | `1` | MSAA sample count. |
| `usage` | `WGPUTextureUsageFlags` | `TextureBinding \| CopyDst` | Usage flags. Depth formats default to `RenderAttachment`. |
| `view_dimension` | `WGPUTextureViewDimension` | auto-detect | View dimension. `0` = auto: 2D for single layer, 2DArray for depth > 1, 3D for 3D dimension. Set to `Cube` for cube maps. |

---

### CgfxSamplerDesc

Configuration for creating a sampler. Zero-initialize for linear filtering with clamp-to-edge.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `mag_filter` | `WGPUFilterMode` | `Linear` | Magnification filter. |
| `min_filter` | `WGPUFilterMode` | `Linear` | Minification filter. |
| `mipmap_filter` | `WGPUMipmapFilterMode` | `Linear` | Mipmap filter. |
| `address_u` | `WGPUAddressMode` | `ClampToEdge` | U-axis address mode. |
| `address_v` | `WGPUAddressMode` | `ClampToEdge` | V-axis address mode. |
| `address_w` | `WGPUAddressMode` | `ClampToEdge` | W-axis address mode. |
| `max_anisotropy` | `uint16_t` | `1` | Maximum anisotropy. |
| `compare` | `WGPUCompareFunction` | none | Comparison function (for depth samplers). |

---

## Functions

### cgfx_texture_create

Create a GPU texture and its default view. No data is uploaded.

```c
CGFX_API CgfxTexture cgfx_texture_create(const CgfxCtx *ctx,
                                          const CgfxTextureDesc *desc);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `desc` | `const CgfxTextureDesc*` | Texture configuration. |

**Returns:** A `CgfxTexture`. Call `cgfx_texture_destroy()` to release.

**Example (sampled texture):**

```c
CgfxTexture tex = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
    .width = 256, .height = 256,
});
cgfx_texture_write(&ctx, &tex, pixel_data, data_size);
```

**Example (cube map):**

```c
CgfxTexture cubemap = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
    .width = 1024, .height = 1024,
    .depth = 6,
    .view_dimension = WGPUTextureViewDimension_Cube,
    .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
});
for (uint32_t face = 0; face < 6; face++)
    cgfx_texture_write_layer(&ctx, &cubemap, face, face_data[face], face_size);
```

**Example (depth texture):**

```c
CgfxTexture depth = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
    .width = 1920, .height = 1080,
    .format = WGPUTextureFormat_Depth24Plus,
});
// usage defaults to RenderAttachment for depth formats
// aspect is auto-detected as DepthOnly
```

---

### cgfx_texture_write

Upload pixel data to a texture (full mip 0, from origin 0,0,0).

```c
CGFX_API void cgfx_texture_write(const CgfxCtx *ctx,
                                  const CgfxTexture *texture,
                                  const void *data, uint64_t data_size);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `texture` | `const CgfxTexture*` | Texture to upload to. Must have `CopyDst` usage. |
| `data` | `const void*` | Pointer to pixel data. |
| `data_size` | `uint64_t` | Size of the pixel data in bytes. |

`bytesPerRow` and `rowsPerImage` are computed automatically from the texture's format and dimensions. Supports common uncompressed formats.

For sub-region writes, mip-level writes, or compressed formats, use raw `wgpuQueueWriteTexture()` with `texture->texture`.

---

### cgfx_texture_write_layer

Upload pixel data to a single array layer or cube face.

```c
CGFX_API void cgfx_texture_write_layer(const CgfxCtx *ctx,
                                        const CgfxTexture *texture,
                                        uint32_t layer,
                                        const void *data, uint64_t data_size);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `texture` | `const CgfxTexture*` | Texture to upload to. Must have `CopyDst` usage. |
| `layer` | `uint32_t` | Array layer or cube face index. |
| `data` | `const void*` | Pointer to pixel data for one layer. |
| `data_size` | `uint64_t` | Size of the pixel data in bytes. |

For cube maps, layers 0--5 correspond to +X, -X, +Y, -Y, +Z, -Z.

---

### cgfx_texture_destroy

Release a texture and its view.

```c
CGFX_API void cgfx_texture_destroy(CgfxTexture *texture);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `texture` | `CgfxTexture*` | Texture to destroy. |

---

### cgfx_sampler_create

Create a sampler. Returns a raw `WGPUSampler` handle owned by the caller.

```c
CGFX_API WGPUSampler cgfx_sampler_create(const CgfxCtx *ctx,
                                          const CgfxSamplerDesc *desc);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `ctx` | `const CgfxCtx*` | Initialized context. |
| `desc` | `const CgfxSamplerDesc*` | Sampler configuration. Zero-initialize for defaults. |

**Returns:** A `WGPUSampler` handle. Release with `wgpuSamplerRelease()`.

**Example:**

```c
WGPUSampler sampler = cgfx_sampler_create(&ctx, &(CgfxSamplerDesc){});
// ... use in bind group ...
wgpuSamplerRelease(sampler);
```

---

## Usage

### Texture + sampler bind group

To use a texture in a shader, create a bind group with both the texture and a sampler:

```c
// Shader with texture + sampler bindings
CgfxShader shader = cgfx_shader_create(&ctx, "textured", wgsl,
    &(CgfxShaderDesc){
        .group_count = 1,
        .groups = (CgfxGroupDesc[]){{
            .binding_count = 2,
            .bindings = (CgfxBindingDesc[]){
                { .binding = 0, .kind = CGFX_BINDING_TEXTURE },
                { .binding = 1, .kind = CGFX_BINDING_SAMPLER },
            },
        }},
    });

// Create texture and sampler
CgfxTexture tex = cgfx_texture_create(&ctx, &(CgfxTextureDesc){
    .width = 256, .height = 256,
});
cgfx_texture_write(&ctx, &tex, pixels, pixel_size);

WGPUSampler sampler = cgfx_sampler_create(&ctx, &(CgfxSamplerDesc){});

// Create bind group
WGPUBindGroup bg = cgfx_bind_group_create(&ctx, &shader, 0,
    (CgfxBindGroupEntry[]){
        { .binding = 0, .texture = &tex },
        { .binding = 1, .sampler = sampler },
    }, 2);
```

### View dimension auto-detection

When `view_dimension` is `0` (default), it is inferred from the texture dimension and depth:

| Dimension | Depth | View Dimension |
|-----------|-------|----------------|
| 1D | any | `1D` |
| 2D | 1 | `2D` |
| 2D | > 1 | `2DArray` |
| 3D | any | `3D` |

Set `view_dimension` explicitly for cube maps (`Cube` with `depth = 6`) or cube map arrays (`CubeArray`).
