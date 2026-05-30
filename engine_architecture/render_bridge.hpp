#pragma once
#include <vector>
#include <flecs.h>
#include "../core/components.hpp"
#include "../assets/asset_manager.hpp"

// =============================================================================
// RenderBridge
//
// The ONLY place ECS and your C renderer meet. Queries Flecs for renderable
// entities, builds a plain DrawList, and submits it.
//
// Your C renderer knows nothing about ECS or Flecs.
// Your ECS systems know nothing about GPU resources or draw calls.
//
// This bridge translates between the two worlds once per frame.
// =============================================================================

// Plain C-compatible struct — safe to pass to C renderer
struct DrawCall {
    uint32_t renderer_mesh_id;
    uint32_t renderer_material_id;
    float    world_matrix[16];   // column-major, matches cglm mat4 layout
    bool     casts_shadow;
};

struct FrameDrawList {
    std::vector<DrawCall> calls;
    float view[16];
    float proj[16];
    float camera_pos[3];
};

class RenderBridge {
public:
    // Lifecycle — passes window handle down to your C renderer
    void init(void* window_handle, const AssetManager& assets);
    void shutdown();

    // Called LAST in Engine::tick(), after world.progress().
    // Queries ECS, builds FrameDrawList, submits to C renderer.
    void submit_frame(flecs::world& world, float viewport_aspect);

private:
    void build_draw_list(flecs::world& world, float aspect);
    void submit_to_renderer();

    const AssetManager* m_assets = nullptr;
    FrameDrawList       m_frame;    // reused each frame, cleared at start
};
