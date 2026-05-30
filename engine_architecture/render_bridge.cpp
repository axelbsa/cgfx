#include "render_bridge.hpp"

// Forward declarations from your C renderer
extern "C" {
    void renderer_begin_frame(void);
    void renderer_end_frame(void);
    void renderer_set_camera(const float* view, const float* proj,
                             const float* pos);
    void renderer_submit_mesh(uint32_t mesh_id, uint32_t material_id,
                              const float* world_matrix, int cast_shadow);
}

// =============================================================================
// RenderBridge — the only place ECS and the C renderer touch
// =============================================================================

void RenderBridge::init(void* window_handle, const AssetManager& assets) {
    m_assets = &assets;
    // Delegate to your C renderer's init
    // renderer_init(window_handle);  -- whatever your C API looks like
}

void RenderBridge::shutdown() {
    // renderer_shutdown();
}

void RenderBridge::submit_frame(flecs::world& world, float viewport_aspect) {
    build_draw_list(world, viewport_aspect);
    submit_to_renderer();
}

// =============================================================================
// build_draw_list — reads ECS, fills m_frame. No GPU calls here.
// =============================================================================

void RenderBridge::build_draw_list(flecs::world& world, float aspect) {
    m_frame.calls.clear();

    // --- Camera ---
    world.each([&](IsCamera, const CameraData& cam, const WorldTransform& wt) {
        memcpy(m_frame.view,       cam.view,     sizeof(float)*16);
        memcpy(m_frame.proj,       cam.proj,     sizeof(float)*16);
        memcpy(m_frame.camera_pos, cam.position, sizeof(float)*3);
    });

    // --- Renderables ---
    // Query: anything with a WorldTransform + MeshHandle + MaterialHandle
    world.each([&](const WorldTransform& wt,
                   const MeshHandle& mesh,
                   const MaterialHandle& mat) {
        // Resolve handles to renderer IDs via AssetManager
        const MeshAsset*     ma = m_assets->get_mesh(mesh.id);
        const MaterialAsset* mt = m_assets->get_material(mat.id);
        if (!ma || !mt) return;

        DrawCall dc;
        dc.renderer_mesh_id     = ma->renderer_id;
        dc.renderer_material_id = mt->renderer_id;
        dc.casts_shadow         = false;   // will be set by tag check below
        memcpy(dc.world_matrix, wt.matrix, sizeof(float)*16);

        m_frame.calls.push_back(dc);
    });

    // --- Shadow casters (second query, adds flag) ---
    // In practice you'd combine this into one query with optional components,
    // but separate queries are clearer here.
    world.each([&](const MeshHandle& mesh, CastsShadow) {
        for (auto& dc : m_frame.calls) {
            if (dc.renderer_mesh_id == m_assets->get_mesh(mesh.id)->renderer_id)
                dc.casts_shadow = true;
        }
    });
}

// =============================================================================
// submit_to_renderer — calls your C renderer API. No ECS knowledge here.
// =============================================================================

void RenderBridge::submit_to_renderer() {
    renderer_begin_frame();
    renderer_set_camera(m_frame.view, m_frame.proj, m_frame.camera_pos);

    for (const DrawCall& dc : m_frame.calls) {
        renderer_submit_mesh(dc.renderer_mesh_id,
                             dc.renderer_material_id,
                             dc.world_matrix,
                             dc.casts_shadow ? 1 : 0);
    }

    renderer_end_frame();
}
