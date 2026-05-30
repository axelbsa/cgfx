#include "engine.hpp"
#include "../systems/transform_system.hpp"
#include "../systems/camera_system.hpp"

// =============================================================================
// Engine::init — wire everything up, register ECS systems
// =============================================================================

void Engine::init(const EngineConfig& cfg) {
    // 1. Assets first — other systems need to load resources
    m_assets.init();

    // 2. Input
    m_input.init();

    // 3. Physics
    if (cfg.enable_physics) {
        PhysicsConfig pcfg;
        pcfg.gravity_y    = cfg.gravity_y;
        pcfg.num_threads  = cfg.physics_threads;
        m_physics.init(pcfg);
    }

    // 4. Renderer
    m_render.init(cfg.window_handle, m_assets);

    // 5. Register ECS systems (defines their tick order within world.progress)
    //    Flecs runs them in declaration order unless .after() overrides it.
    TransformSystem::register_systems(m_world);   // pass 1 (roots), pass 2 (children)
    CameraSystem::register_systems(m_world);       // registered .after(transform pass 2)

    // 6. Create a default player entity that InputSystem writes onto
    m_player_entity = m_world.entity("player")
        .set<Position>({})
        .set<Rotation>({})
        .set<Scale>({})
        .set<WorldTransform>({})
        .set<InputState>({});
}

// =============================================================================
// Engine::tick — THE FRAME ORDER. This is the contract.
//
//  1. poll_and_write   — platform events → InputState component
//  2. step_and_writeback — Jolt sim → Position + Rotation components
//  3. world.progress   — runs Flecs systems in declared order:
//       a. TransformSystem pass 1 (root entities)
//       b. TransformSystem pass 2 (children, after pass 1)
//       c. CameraSystem (after pass 2)
//  4. submit_frame     — query ECS → build draw list → call C renderer
// =============================================================================

void Engine::tick(float dt, float viewport_aspect) {
    // Step 1 — Input
    m_input.poll_and_write(m_world, m_player_entity);

    // Step 2 — Physics (reads InputState, simulates, writes Position+Rotation)
    m_physics.step_and_writeback(m_world, dt);

    // Step 3 — All ECS systems (transform propagation, camera matrix build)
    m_world.progress(dt);

    // Step 4 — Render (reads WorldTransform + CameraData, calls C renderer)
    m_render.submit_frame(m_world, viewport_aspect);
}

// =============================================================================
// Engine::shutdown
// =============================================================================

void Engine::shutdown() {
    m_render.shutdown();
    m_physics.shutdown();
    m_input.shutdown();
    m_assets.shutdown();
}

// =============================================================================
// Scene building helpers
// =============================================================================

EntityID Engine::create_mesh_entity(const char* mesh_path,
                                    const char* material_path,
                                    float x, float y, float z) {
    uint32_t mesh_id     = m_assets.load_mesh(mesh_path);
    uint32_t material_id = m_assets.load_material(material_path);

    flecs::entity e = m_world.entity()
        .set<Position>({{ x, y, z }})
        .set<Rotation>({})
        .set<Scale>({})
        .set<WorldTransform>({})
        .set<MeshHandle>({ mesh_id })
        .set<MaterialHandle>({ material_id });

    return from_entity(e);
}

EntityID Engine::create_camera(float fov, float near_plane, float far_plane,
                               float x, float y, float z) {
    // Remove IsCamera from any previous camera entity
    m_world.each([](flecs::entity e, IsCamera) {
        e.remove<IsCamera>();
    });

    flecs::entity e = m_world.entity("main_camera")
        .add<IsCamera>()
        .set<Position>({{ x, y, z }})
        .set<Rotation>({})
        .set<Scale>({})
        .set<WorldTransform>({})
        .set<CameraData>({ .fov=fov, .near_plane=near_plane, .far_plane=far_plane });

    return from_entity(e);
}

void Engine::set_parent(EntityID child, EntityID parent) {
    to_entity(child).child_of(to_entity(parent));
}

void Engine::add_physics_box(EntityID entity,
                             float hx, float hy, float hz, float mass) {
    m_physics.register_dynamic_body(m_world, to_entity(entity),
                                    hx, hy, hz, mass);
}

// =============================================================================
// Internal helpers
// =============================================================================

flecs::entity Engine::to_entity(EntityID id) {
    return m_world.entity(id.value);
}

EntityID Engine::from_entity(flecs::entity e) {
    return { e.id() };
}
