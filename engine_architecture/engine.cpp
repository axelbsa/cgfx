#include "engine.hpp"
#include <flecs.h>
#include "../core/components.hpp"
#include "../render/renderer.hpp"
#include "../render/render_bridge.hpp"
#include "../assets/asset_manager.hpp"
#include "../systems/input_system.hpp"
#include "../systems/transform_system.hpp"
#include "../systems/camera_system.hpp"
#include "../physics/physics_bridge.hpp"
#include "../scene/scene_manager.hpp"

// =============================================================================
// Engine::Impl — owns the world and every subsystem. None of this is visible to
// consumers (engine.hpp is a full PIMPL). This is a SKETCH: shapes match the
// design docs; flecs/scene APIs vary slightly by version.
// =============================================================================
struct Engine::Impl {
    flecs::world  world;
    Renderer      renderer;     // owns the CgfxCtx (created unless headless)
    AssetManager  assets;
    InputSystem   input;
    PhysicsBridge physics;
    RenderBridge  render;
    SceneManager  scenes;
    EntityID      player{};
    bool          enable_physics = true;
    bool          headless = false;

    flecs::entity to_entity(EntityID id) { return world.entity(id.value); }
    EntityID      from_entity(flecs::entity e) { return { e.id() }; }
    flecs::entity to_scene(SceneID s)    { return world.entity(s.value); }
};

// Register every component + the relationship cleanup traits. Once, in init.
static void register_world(flecs::world& w) {
    w.component<Position>();   w.component<Rotation>();   w.component<Scale>();
    w.component<WorldTransform>(); w.component<MeshHandle>(); w.component<MaterialHandle>();
    w.component<CameraData>(); w.component<ViewportInfo>(); w.component<InputState>();
    w.component<PhysicsBodyID>(); w.component<CharacterState>();
    // Scene ownership relationship: one scene per entity; deleting the scene
    // deletes its members (SCENE.txt sec 1).
    w.component<SceneMember>()
        .add(flecs::Exclusive)
        .add(flecs::OnDeleteTarget, flecs::Delete);
    // Transform parent delete cascades to children.
    w.component(flecs::ChildOf).add(flecs::OnDeleteTarget, flecs::Delete);
}

Engine::Engine()  = default;
Engine::~Engine() = default;     // defined here where Impl is complete (PIMPL rule)

// -----------------------------------------------------------------------------
void Engine::init(const EngineConfig& cfg) {
    m_impl = std::make_unique<Impl>();
    Impl& I = *m_impl;
    I.enable_physics = cfg.enable_physics;
    I.headless = cfg.headless;

    register_world(I.world);

    if (!cfg.headless) I.renderer.init(cfg);                 // creates the CgfxCtx
    I.assets.init(cfg.headless ? nullptr : I.renderer.ctx());
    I.input.init();
    if (!cfg.headless) I.input.attach(I.renderer.window());  // void* GLFWwindow*
    if (cfg.enable_physics) {
        PhysicsConfig p; p.gravity_y = cfg.gravity_y; p.num_threads = cfg.physics_threads;
        I.physics.init(I.world, p);                          // also registers on_remove observer
    }
    I.render.init(cfg.headless ? nullptr : &I.renderer, &I.assets);

    // ECS systems: transform roots -> children -> camera (each .after the prior).
    auto children = TransformSystem::register_systems(I.world);
    CameraSystem::register_systems(I.world, children);

    // A default scene + a player entity for InputState.
    SceneID main = create_scene("main");
    set_active_scene(main);
    I.player = spawn(main, "player");
    I.to_entity(I.player).set<InputState>({});
}

void Engine::shutdown() {
    if (!m_impl) return;
    Impl& I = *m_impl;
    I.render.shutdown();
    if (I.enable_physics) I.physics.shutdown();
    I.input.shutdown();
    I.assets.shutdown();
    if (!I.headless) I.renderer.shutdown();
    m_impl.reset();
}

// -----------------------------------------------------------------------------
// THE FRAME ORDER (ARCHITECTURE.txt). The platform loop calls glfwPollEvents()
// then engine.tick(); cgfx is caller-polled, so input only READS state here.
// -----------------------------------------------------------------------------
void Engine::tick(float dt, float viewport_aspect) {
    Impl& I = *m_impl;
    I.input.poll_and_write(I.world, I.to_entity(I.player));   // [1] InputState
    I.world.set<ViewportInfo>({ viewport_aspect });           // [2] aspect for CameraSystem
    // [3] MovementSystem (Phase 9) runs here, before physics.
    if (I.enable_physics) I.physics.step_and_writeback(I.world, dt);  // [4] fixed substeps + flip
    I.world.progress(dt);                                     // [5] transform roots/children, camera
    I.render.submit_frame(I.world);                           // [6] read CameraData/WorldTransform -> cgfx
}

void* Engine::window() const { return m_impl->headless ? nullptr : m_impl->renderer.window(); }

// -----------------------------------------------------------------------------
// Scenes
// -----------------------------------------------------------------------------
SceneID Engine::create_scene(const char* name) {
    flecs::entity s = m_impl->world.entity(name);   // also used as a name scope
    return m_impl->scenes.add(SceneID{ s.id() });
}
void Engine::unload_scene(SceneID s)    { m_impl->to_scene(s).destruct(); m_impl->scenes.remove(s); }
void Engine::clear_scene(SceneID s)     { m_impl->scenes.clear_members(m_impl->world, s); }
SceneID Engine::active_scene() const    { return m_impl->scenes.active(); }
void Engine::set_active_scene(SceneID s){ m_impl->scenes.set_active(s); }

// -----------------------------------------------------------------------------
// Spawning (adds (SceneMember, scene) + a scene-scoped name)
// -----------------------------------------------------------------------------
EntityID Engine::spawn(SceneID scene, const char* name) {
    Impl& I = *m_impl;
    flecs::entity e = name ? I.world.scope(I.to_scene(scene)).entity(name)
                           : I.world.entity();
    e.add<SceneMember>(I.to_scene(scene));
    e.set<Position>({}).set<Rotation>({}).set<Scale>({}).set<WorldTransform>({});
    return I.from_entity(e);
}

EntityID Engine::spawn_mesh(SceneID scene, const char* name,
                            const char* mesh_path, const char* material_path,
                            float x, float y, float z) {
    Impl& I = *m_impl;
    uint32_t m = I.assets.load_mesh(mesh_path);
    uint32_t mat = I.assets.load_material(material_path);
    EntityID id = spawn(scene, name);
    I.to_entity(id).set<Position>({{x,y,z}})
        .set<MeshHandle>({m}).set<MaterialHandle>({mat});
    return id;
}

EntityID Engine::spawn_camera(SceneID scene, const char* name,
                              float fov_deg, float near_p, float far_p,
                              float x, float y, float z) {
    Impl& I = *m_impl;
    // Single active camera: strip IsCamera from any existing camera.
    I.world.each([](flecs::entity e, IsCamera){ e.remove<IsCamera>(); });
    EntityID id = spawn(scene, name);
    CameraData cam{}; cam.fov_deg = fov_deg; cam.near_plane = near_p; cam.far_plane = far_p;
    I.to_entity(id).add<IsCamera>().set<Position>({{x,y,z}}).set<CameraData>(cam);
    return id;
}

void Engine::despawn(EntityID e) { m_impl->to_entity(e).destruct(); }

EntityID Engine::create_mesh_entity(const char* mesh_path, const char* material_path,
                                    float x, float y, float z) {
    return spawn_mesh(active_scene(), nullptr, mesh_path, material_path, x, y, z);
}
EntityID Engine::create_camera(float fov_deg, float near_p, float far_p,
                               float x, float y, float z) {
    return spawn_camera(active_scene(), "camera", fov_deg, near_p, far_p, x, y, z);
}

// -----------------------------------------------------------------------------
void Engine::set_parent(EntityID child, EntityID parent) {
    m_impl->to_entity(child).child_of(m_impl->to_entity(parent));
}
EntityID Engine::find(SceneID scene, const char* name) {
    flecs::entity e = m_impl->world.scope(m_impl->to_scene(scene)).lookup(name);
    return e ? m_impl->from_entity(e) : EntityID::null();
}
void Engine::add_physics_box(EntityID e, float hx, float hy, float hz, float mass) {
    ShapeDesc s; s.kind = ShapeKind::Box; s.hx=hx; s.hy=hy; s.hz=hz;
    m_impl->physics.register_body(m_impl->world, m_impl->to_entity(e), s, /*dynamic*/true, mass);
}

// Prefabs / serialization: see PHASE_06_5 / PHASE_10. (Sketch stubs.)
PrefabID Engine::load_prefab(const char*) { return PrefabID::null(); }
EntityID Engine::instantiate_prefab(SceneID, PrefabID, float, float, float) { return EntityID::null(); }
SceneID  Engine::load_scene_file(const char*) { return SceneID::null(); }
void     Engine::save_scene_file(SceneID, const char*) {}

// Input forwarders (keep InputSystem out of the public header).
bool Engine::key_held(Key k)     const { return m_impl->input.key_held(k); }
bool Engine::key_pressed(Key k)  const { return m_impl->input.key_pressed(k); }
bool Engine::key_released(Key k) const { return m_impl->input.key_released(k); }
