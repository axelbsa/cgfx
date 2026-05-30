#pragma once
#include <cstdint>
#include <memory>

// =============================================================================
// AssetManager  (cgfx-FREE public-ish header)
//
// Loads meshes/materials/textures by path, deduplicates them, and hands back
// small uint32 HANDLES. The handle (a vector index) is the renderer-agnostic
// seam the ECS components (MeshHandle/MaterialHandle) use — nothing outside the
// render/assets layer needs to know what a CgfxMesh is.
//
// The GPU-backed asset structs hold cgfx objects by value, so they live in the
// internal, cgfx-aware asset_gpu.hpp (included only by asset_manager.cpp and
// render_bridge.cpp). cgfx therefore never leaks through THIS header
// (FOUNDATIONS sec 4). Storage lives behind a PIMPL.
//
// Implementation (cgltf meshes, stb_image textures, the ShaderTemplate +
// MaterialAsset split, dedup, the dynamic-offset object buffer) is spelled out in
// PHASE_04.txt and RENDER_INTEGRATION sec 4/8. Load synchronously, on the main
// thread, up front (no async/streaming/refcounting yet).
// =============================================================================

struct CgfxCtx;          // cgfx; used only by pointer here (forward decl, no include)
struct MeshAsset;        // defined in asset_gpu.hpp (cgfx-aware; .cpp only)
struct MaterialAsset;
struct ShaderTemplate;
struct TextureAsset;

class AssetManager {
public:
    AssetManager();
    ~AssetManager();                              // defined in .cpp (PIMPL needs complete Impl)
    AssetManager(const AssetManager&)            = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    void init(CgfxCtx* ctx);   // borrows the context from the Renderer (may be null: headless)
    void shutdown();           // cgfx_*_destroy everything it owns; clears caches

    // Load-or-get-cached. Returns a uint32 handle (vector index), path-deduplicated.
    uint32_t load_mesh(const char* path);
    uint32_t load_material(const char* path);
    uint32_t load_texture(const char* path);

    // Resolve a handle to the owned GPU asset. Called by RenderBridge, which is in
    // the cgfx-aware layer and includes asset_gpu.hpp. Returns nullptr on bad id.
    const MeshAsset*      get_mesh(uint32_t id)     const;
    const MaterialAsset*  get_material(uint32_t id) const;
    const ShaderTemplate* get_template(uint32_t id) const;
    const TextureAsset*   get_texture(uint32_t id)  const;

    void unload_all();

private:
    struct Impl;                       // owns the vectors + path caches (cgfx-typed)
    std::unique_ptr<Impl> m_impl;      // defined in asset_manager.cpp
};
