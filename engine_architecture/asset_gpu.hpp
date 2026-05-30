#pragma once
// =============================================================================
// asset_gpu.hpp — INTERNAL, cgfx-aware. The GPU-backed asset structs that hold
// cgfx objects BY VALUE.
//
// Per FOUNDATIONS sec 4, cgfx may appear only in the assets/ and render/
// TRANSLATION UNITS, never in a header visible to core/ systems/ physics/
// scene/ examples/. So this header is included ONLY by:
//     assets/asset_manager.cpp   (creates/owns these)
//     render/render_bridge.cpp   (reads them to record draws)
//
// Definitions and rationale: RENDER_INTEGRATION.txt sec 4. PHASE_04.txt builds it.
// =============================================================================
#include <cstdint>
#include <string>
#include "../render/cgfx_c.hpp"   // extern "C" { #include "cgfx.h" }

// SHARED across many materials so they reuse one pipeline (the big CPU win).
struct ShaderTemplate {
    CgfxShader         shader{};                    // module + layouts + pipeline layout
    WGPURenderPipeline pipeline = nullptr;          // main (opaque) pass
    WGPURenderPipeline pipeline_shadow = nullptr;   // optional depth-only variant
    uint32_t           param_size = 0;              // sizeof @group(2) MaterialParams UBO
    uint8_t            texture_slots = 0;
};

// One per mesh. Owns the CgfxMesh. Its vector index IS the MeshHandle.id.
struct MeshAsset {
    CgfxMesh    mesh{};
    std::string path;
};

// One per texture. Owns the CgfxTexture. Its vector index is the texture handle.
struct TextureAsset {
    CgfxTexture texture{};
    std::string path;
};

// One per material instance. Owns its @group(2) bind group + params UBO.
// Its vector index IS the MaterialHandle.id. NOTE: no "renderer_id" — the asset
// owns the cgfx objects directly (the old renderer_id model is gone).
struct MaterialAsset {
    uint32_t      template_id = 0;          // -> ShaderTemplate (pipeline sharing)
    CgfxBuffer    params_ubo{};             // @group(2) @binding(0)
    WGPUBindGroup group2 = nullptr;         // params + textures + sampler (built once)
    uint32_t      texture_ids[8] = {};      // -> TextureAsset indices (dedup/lifetime)
    WGPUSampler   sampler = nullptr;        // usually the shared default
    std::string   path;
};
