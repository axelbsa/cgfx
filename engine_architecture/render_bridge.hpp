#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <flecs.h>                  // internal layer header; never reaches consumers
#include "../core/components.hpp"

// =============================================================================
// RenderBridge — the ONLY place the ECS and cgfx meet.
//
// Each frame it reads renderable entities from the ECS into a plain DrawList
// (build_draw_list, no GPU calls), then records cgfx draws (record_and_submit).
// It drives cgfx DIRECTLY — there is no retained "renderer_*" C API
// (RENDER_INTEGRATION sec 1). The cgfx-owning state (the camera buffer, the
// @group(0)/@group(1) bind groups, the per-object dynamic uniform buffer) lives
// behind a PIMPL so cgfx never leaks through this header (FOUNDATIONS sec 4).
//
// Frame mapping, the dynamic-offset upload, shadow pass, and MSAA: RENDER_INTEGRATION
// sec 5/7. PHASE_04 wires the per-object buffer + pipeline batching.
// =============================================================================

class AssetManager;   // forward (no include needed in this header)
class Renderer;       // forward (render/renderer.hpp): owns the CgfxCtx

// Plain data; no cgfx types. Built from the ECS, consumed when recording.
struct DrawCall {
    uint32_t mesh_id;            // -> AssetManager mesh handle
    uint32_t material_id;        // -> AssetManager material handle
    float    world_matrix[16];   // column-major (cglm) model matrix
    uint32_t uniform_offset;     // byte offset into the per-object dynamic buffer
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
    RenderBridge();
    ~RenderBridge();                              // PIMPL: defined in .cpp
    RenderBridge(const RenderBridge&)            = delete;
    RenderBridge& operator=(const RenderBridge&) = delete;

    // renderer may be null (headless): submit_frame then no-ops.
    void init(Renderer* renderer, AssetManager* assets);
    void shutdown();

    // Called LAST in Engine::tick(). Aspect is NOT passed: CameraSystem already
    // baked the projection from the ViewportInfo singleton, so we just read CameraData.
    void submit_frame(flecs::world& world);

private:
    void build_draw_list(flecs::world& world);   // pure ECS read -> m_frame
    void record_and_submit();                    // cgfx recording (in .cpp)

    Renderer*     m_renderer = nullptr;          // borrowed
    AssetManager* m_assets   = nullptr;          // borrowed
    FrameDrawList m_frame;                        // reused each frame

    struct Gpu;                                   // cgfx state (camera, bind groups,
    std::unique_ptr<Gpu> m_gpu;                   // object UBO) — defined in the .cpp
};
