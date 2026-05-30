#include "render_bridge.hpp"
#include "../render/cgfx_c.hpp"      // extern "C" { #include "cgfx.h" }
#include "../render/renderer.hpp"     // Renderer: owns CgfxCtx, exposes ctx()
#include "../assets/asset_gpu.hpp"    // MeshAsset / MaterialAsset / ShaderTemplate (cgfx-typed)
#include <cstring>
#include <algorithm>

// =============================================================================
// RenderBridge — drives cgfx directly (no retained renderer_* API).
// This is a SKETCH: the shapes and cgfx calls are real; exact flecs query syntax
// and limits queries vary by version. See RENDER_INTEGRATION sec 4/5/7, PHASE_04.
// =============================================================================

// One per-object uniform slot (@group(1)). Padded to the GPU's dynamic-offset
// alignment (256 on most adapters). Only `model` is used here; normal matrix etc.
// would live alongside it inside the same padded slot.
struct ObjectUniform { float model[16]; /* + normal matrix, etc. */ };

// cgfx-owning state for the bridge (hidden from the header).
struct RenderBridge::Gpu {
    CgfxCamera    camera{};          // owns the @group(0) camera buffer
    WGPUBindGroup group0 = nullptr;  // frame/camera bind group
    WGPUBindGroup group1 = nullptr;  // per-object bind group (has_dynamic_offset)
    CgfxBuffer    object_ubo{};      // one big buffer, N * stride
    uint32_t      stride   = 256;    // align_up(sizeof(ObjectUniform), minUBOAlignment)
    uint32_t      capacity = 0;      // how many object slots object_ubo holds
    std::vector<unsigned char> staging;  // CPU side, packed once per frame
};

RenderBridge::RenderBridge()  = default;
RenderBridge::~RenderBridge() = default;

void RenderBridge::init(Renderer* renderer, AssetManager* assets) {
    m_renderer = renderer;
    m_assets   = assets;
    if (!m_renderer) return;                 // headless: nothing GPU-side

    m_gpu = std::make_unique<Gpu>();
    CgfxCtx* ctx = m_renderer->ctx();
    // stride = align_up(sizeof(ObjectUniform), minUniformBufferOffsetAlignment).
    // Query the limit from cgfx; 256 is the safe default on virtually all adapters.
    m_gpu->stride = 256;
    // camera + group0 + group1 + object_ubo are created lazily / here once a
    // ShaderTemplate's group layouts exist (all templates share group 0/1 layouts).
    // m_gpu->camera = cgfx_camera_create(ctx, ...);  // see PHASE_03 3.4 / PHASE_04 4.4
    (void)ctx;
}

void RenderBridge::shutdown() {
    if (!m_gpu) return;
    if (m_gpu->group1)        cgfx_bind_group_destroy(m_gpu->group1);
    if (m_gpu->group0)        cgfx_bind_group_destroy(m_gpu->group0);
    if (m_gpu->object_ubo.ok) cgfx_buffer_destroy(&m_gpu->object_ubo);
    if (m_gpu->camera.ok)     cgfx_camera_destroy(&m_gpu->camera);
    m_gpu.reset();
}

void RenderBridge::submit_frame(flecs::world& world) {
    if (!m_renderer) return;                 // headless guard (PHASE_08 8.5)
    build_draw_list(world);
    record_and_submit();
}

// -----------------------------------------------------------------------------
// build_draw_list — pure ECS read. No GPU calls. Fills m_frame.
// One query for renderables, with CastsShadow as an OPTIONAL term (folding the
// old O(N*M) second shadow query into this one — RENDER_INTEGRATION 9).
// -----------------------------------------------------------------------------
void RenderBridge::build_draw_list(flecs::world& world) {
    m_frame.calls.clear();

    // Camera (the single IsCamera entity). CameraData is already up to date.
    world.each([&](flecs::entity, IsCamera, const CameraData& cam) {
        std::memcpy(m_frame.view,       cam.view,     sizeof(float) * 16);
        std::memcpy(m_frame.proj,       cam.proj,     sizeof(float) * 16);
        std::memcpy(m_frame.camera_pos, cam.position, sizeof(float) * 3);
    });

    // Renderables. (CastsShadow is read per-entity; with flecs use an optional
    // term, e.uses<CastsShadow>(), or a query with .optional() — version dependent.)
    uint32_t i = 0;
    world.each([&](flecs::entity e, const WorldTransform& wt,
                   const MeshHandle& mesh, const MaterialHandle& mat) {
        DrawCall dc{};
        dc.mesh_id        = mesh.id;
        dc.material_id    = mat.id;
        dc.uniform_offset = (i++) * (m_gpu ? m_gpu->stride : 256u);
        dc.casts_shadow   = e.has<CastsShadow>();
        std::memcpy(dc.world_matrix, wt.matrix, sizeof(float) * 16);
        m_frame.calls.push_back(dc);
    });

    // Sort by material template so we can set each pipeline once (batching).
    std::sort(m_frame.calls.begin(), m_frame.calls.end(),
        [&](const DrawCall& a, const DrawCall& b) {
            const MaterialAsset* ma = m_assets->get_material(a.material_id);
            const MaterialAsset* mb = m_assets->get_material(b.material_id);
            uint32_t ta = ma ? ma->template_id : 0, tb = mb ? mb->template_id : 0;
            return ta < tb;
        });
}

// -----------------------------------------------------------------------------
// record_and_submit — the only cgfx-recording function.
// -----------------------------------------------------------------------------
void RenderBridge::record_and_submit() {
    CgfxCtx* ctx = m_renderer->ctx();

    // Camera -> the owned CgfxCamera buffer (@group(0)).
    std::memcpy(m_gpu->camera.proj, m_frame.proj, sizeof(float) * 16);
    std::memcpy(m_gpu->camera.view, m_frame.view, sizeof(float) * 16);
    cgfx_camera_write(ctx, &m_gpu->camera);

    // Pack every object's model matrix into one staging buffer, one upload.
    // (Grow object_ubo / group1 when calls.size() exceeds capacity — omitted here.)
    m_gpu->staging.assign(m_frame.calls.size() * m_gpu->stride, 0);
    for (size_t k = 0; k < m_frame.calls.size(); ++k) {
        ObjectUniform u{};
        std::memcpy(u.model, m_frame.calls[k].world_matrix, sizeof(u.model));
        std::memcpy(m_gpu->staging.data() + k * m_gpu->stride, &u, sizeof(u));
    }
    wgpuQueueWriteBuffer(/*queue*/ nullptr /* ctx->queue */, m_gpu->object_ubo.buffer,
                         0, m_gpu->staging.data(), m_gpu->staging.size());

    CgfxFrame frame;
    WGPUColor clear = {0.05, 0.05, 0.08, 1.0};
    if (!cgfx_frame_begin(ctx, &frame, clear)) return;   // surface unavailable: skip

    cgfx_shader_bind(frame.render_pass, &m_gpu->group0, 1);   // @group(0) once per frame

    uint32_t cur_template = UINT32_MAX;
    for (const DrawCall& dc : m_frame.calls) {
        const MeshAsset*     me = m_assets->get_mesh(dc.mesh_id);
        const MaterialAsset* mt = m_assets->get_material(dc.material_id);
        if (!me || !mt) continue;

        if (mt->template_id != cur_template) {               // pipeline batching
            const ShaderTemplate* tp = m_assets->get_template(mt->template_id);
            wgpuRenderPassEncoderSetPipeline(frame.render_pass, tp->pipeline);
            cur_template = mt->template_id;
        }
        cgfx_shader_bind(frame.render_pass, &mt->group2, 1);              // @group(2)
        uint32_t off = dc.uniform_offset;
        cgfx_shader_bind_dynamic(frame.render_pass, 1, m_gpu->group1, &off, 1); // @group(1)
        cgfx_mesh_draw(frame.render_pass, &me->mesh);
    }

    cgfx_frame_end(ctx, &frame);
}
