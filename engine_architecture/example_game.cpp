// =============================================================================
// example_game.cpp — what a consumer of the engine static library looks like
//
// The game executable never includes flecs.h, cglm, or Jolt directly.
// It only sees engine_api (Engine + EntityID + EngineConfig).
// =============================================================================

#include "engine/core/engine.hpp"

int main() {
    // --- Create and configure the engine ---
    Engine engine;

    EngineConfig cfg;
    cfg.window_handle   = /* platform_create_window(1280, 720, "My Game") */ nullptr;
    cfg.gravity_y       = -9.81f;
    cfg.physics_threads = 2;

    engine.init(cfg);

    // --- Build the scene ---

    // Camera
    EntityID cam = engine.create_camera(
        60.f,   // fov degrees
        0.1f,   // near
        1000.f, // far
        0.f, 3.f, -10.f  // position
    );

    // A static ground plane
    EntityID ground = engine.create_mesh_entity(
        "assets/meshes/plane.obj",
        "assets/materials/ground.mat",
        0.f, 0.f, 0.f
    );

    // A dynamic crate that falls under gravity
    EntityID crate = engine.create_mesh_entity(
        "assets/meshes/cube.obj",
        "assets/materials/crate.mat",
        0.f, 5.f, 0.f   // start 5m up
    );
    engine.add_physics_box(crate, 0.5f, 0.5f, 0.5f, 10.f); // 1m box, 10kg

    // A child prop parented to the crate
    EntityID sticker = engine.create_mesh_entity(
        "assets/meshes/sticker.obj",
        "assets/materials/sticker.mat",
        0.2f, 0.6f, 0.f   // offset from parent
    );
    engine.set_parent(sticker, crate);  // sticker moves with the crate

    // --- Game loop (platform-specific, not the engine's concern) ---
    float dt = 1.f / 60.f;
    bool  running = true;

    while (running) {
        engine.tick(dt, 1280.f / 720.f);

        // Read input for camera control (editor-style example)
        if (engine.input().key_held(Key::Escape)) {
            running = false;
        }

        // dt = measure actual frame time here via platform timer
    }

    engine.shutdown();
    return 0;
}
