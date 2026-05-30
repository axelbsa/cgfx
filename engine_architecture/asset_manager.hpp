#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// =============================================================================
// AssetManager
//
// Owns all loaded GPU resources. Systems never load assets themselves —
// they ask the manager for a handle, and the manager returns a uint32_t
// that both ECS components and the renderer understand.
//
// Thread safety: NOT thread safe. Load assets on the main thread, or
// add a job-queue in front if you want async loading later.
// =============================================================================

struct MeshAsset {
    uint32_t    renderer_id;    // handle your C renderer understands
    std::string path;
};

struct MaterialAsset {
    uint32_t    renderer_id;
    std::string path;
};

class AssetManager {
public:
    // Lifecycle — called by Engine
    void init();
    void shutdown();

    // --- Mesh ---
    // Returns existing handle if already loaded (path-keyed cache).
    uint32_t load_mesh(const char* path);
    const MeshAsset* get_mesh(uint32_t id) const;

    // --- Material ---
    uint32_t load_material(const char* path);
    const MaterialAsset* get_material(uint32_t id) const;

    // --- Unload ---
    // Usually not called per-frame. Use for scene transitions.
    void unload_mesh(uint32_t id);
    void unload_material(uint32_t id);

    void unload_all();

private:
    std::vector<MeshAsset>                   m_meshes;
    std::vector<MaterialAsset>               m_materials;
    std::unordered_map<std::string, uint32_t> m_mesh_cache;
    std::unordered_map<std::string, uint32_t> m_material_cache;
};
