#pragma once
#include <cstdint>
#include <memory>
#include "../core/entity_id.hpp"
#include "../core/scene_id.hpp"           // SceneID, PrefabID
#include "../systems/input_keys.hpp"      // Key, MouseButton (GLFW-free)

// =============================================================================
// Engine — the public face of the static library.
//
// FULL PIMPL: this header pulls in NONE of flecs / cgfx / cglm / Jolt / GLFW.
// All subsystems (the flecs world, AssetManager, InputSystem, PhysicsBridge,
// Renderer, RenderBridge, SceneManager) live in Engine::Impl, defined in
// engine.cpp. A consumer that includes engine_api.hpp sees only opaque handles
// and POD config (FOUNDATIONS sec 5). Positions are plain floats here so the
// public API stays cglm-free.
//
// Frame order and ownership: ARCHITECTURE.txt. Scenes: SCENE.txt.
// =============================================================================

struct EngineConfig {
    // Windowed (GLFW) path — cgfx CREATES the window from these:
    int         width        = 1280;
    int         height       = 720;
    const char* title        = "engine";
    bool        depth_buffer = true;

    // Editor/embedded path — if non-null, render into this native window (HWND)
    // via cgfx_ctx_init_external instead of creating one.
    void*       external_window = nullptr;

    // Subsystems
    float       gravity_y       = -9.81f;
    int         physics_threads = 2;
    bool        enable_physics  = true;

    // CI / tests: no window, no Renderer (RenderBridge no-ops). See PHASE_08.
    bool        headless = false;
};

class Engine {
public:
    Engine();
    ~Engine();                          // defined in engine.cpp (PIMPL needs complete Impl)
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // --- Lifecycle ---
    void init(const EngineConfig& cfg);
    void shutdown();

    // --- Main tick (call once per frame; the platform loop does glfwPollEvents first) ---
    void tick(float dt, float viewport_aspect);

    // GLFWwindow* as void* (no GLFW in this header). null in headless/external cases.
    void* window() const;

    // --- Scenes (SCENE.txt) ---
    SceneID create_scene(const char* name);
    void    unload_scene(SceneID);          // destroys exactly the scene's members
    void    clear_scene(SceneID);           // destroys members, keeps the scene
    SceneID active_scene() const;
    void    set_active_scene(SceneID);

    // --- Spawning (added to the given scene; default = active scene) ---
    EntityID spawn(SceneID, const char* name = nullptr);
    EntityID spawn_mesh(SceneID, const char* name,
                        const char* mesh_path, const char* material_path,
                        float x, float y, float z);
    EntityID spawn_camera(SceneID, const char* name,
                          float fov_deg, float near_p, float far_p,
                          float x, float y, float z);
    void     despawn(EntityID);

    // Convenience wrappers over the active scene (kept for the early phases).
    EntityID create_mesh_entity(const char* mesh_path, const char* material_path,
                                float x, float y, float z);
    EntityID create_camera(float fov_deg, float near_p, float far_p,
                           float x, float y, float z);

    // --- Hierarchy (transform parenting; distinct from scene membership) ---
    void     set_parent(EntityID child, EntityID parent);
    EntityID find(SceneID, const char* name);

    // --- Physics ---
    void add_physics_box(EntityID, float hx, float hy, float hz, float mass = 1.f);

    // --- Prefabs & serialization (Phase 6.5 / Phase 10) ---
    PrefabID load_prefab(const char* path);
    EntityID instantiate_prefab(SceneID, PrefabID, float x, float y, float z);
    SceneID  load_scene_file(const char* path);
    void     save_scene_file(SceneID, const char* path);

    // --- Input queries (GLFW-free enums; thin forwarders to InputSystem) ---
    bool key_held(Key) const;
    bool key_pressed(Key) const;
    bool key_released(Key) const;

private:
    struct Impl;                        // owns the world + all subsystems (engine.cpp)
    std::unique_ptr<Impl> m_impl;
};
