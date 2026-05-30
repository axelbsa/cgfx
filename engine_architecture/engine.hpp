#pragma once
#include <flecs.h>
#include "../assets/asset_manager.hpp"
#include "../systems/input_system.hpp"
#include "../physics/physics_bridge.hpp"
#include "../render/render_bridge.hpp"
#include "../core/entity_id.hpp"
#include "../core/components.hpp"

// =============================================================================
// Engine
//
// Owns all subsystems. Defines the frame tick order. Exposes a narrow
// scene-building API so consumers (game, editor) never touch Flecs directly.
//
// Owns:
//   flecs::world    — entity/component storage and system scheduler
//   AssetManager    — mesh + material GPU resources
//   InputSystem     — platform input → ECS InputState
//   PhysicsBridge   — Jolt simulation → ECS Position/Rotation
//   RenderBridge    — ECS WorldTransform → C renderer draw calls
//
// Does NOT own:
//   The platform window (created by the game/editor executable)
//   The game loop (also the executable's responsibility)
// =============================================================================

struct EngineConfig {
    void*  window_handle  = nullptr;   // platform window (HWND / GLFWwindow*)
    float  gravity_y      = -9.81f;
    int    physics_threads = 2;
    bool   enable_physics  = true;
};

class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    // Non-copyable, non-moveable — owns too many resources
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // --- Lifecycle ---
    void init(const EngineConfig& cfg);
    void shutdown();

    // --- Main tick — call this from your game loop ---
    // dt = seconds since last frame
    void tick(float dt, float viewport_aspect);

    // --- Scene building API ---
    // Returns opaque EntityID handles. Callers never see flecs::entity.

    // Create a visible, moveable object in the world
    EntityID create_mesh_entity(const char* mesh_path,
                                const char* material_path,
                                float x, float y, float z);

    // Create the active camera (only one IsCamera at a time)
    EntityID create_camera(float fov, float near_plane, float far_plane,
                           float x, float y, float z);

    // Parent/child scene graph
    void set_parent(EntityID child, EntityID parent);

    // Physics registration
    void add_physics_box(EntityID entity,
                         float hx, float hy, float hz,
                         float mass = 1.f);

    // --- Subsystem accessors ---
    // Expose subsystems for advanced use (e.g. editor querying raw input)
    InputSystem&   input()  { return m_input; }
    AssetManager&  assets() { return m_assets; }
    flecs::world&  ecs()    { return m_world; }  // escape hatch for power users

private:
    // The tick order is defined by the ORDER of calls in tick(), not by
    // subsystem declaration order here.

    flecs::world   m_world;
    AssetManager   m_assets;
    InputSystem    m_input;
    PhysicsBridge  m_physics;
    RenderBridge   m_render;

    flecs::entity  m_player_entity;  // InputSystem writes here

    // Helpers
    flecs::entity  to_entity(EntityID id);
    EntityID       from_entity(flecs::entity e);
};
