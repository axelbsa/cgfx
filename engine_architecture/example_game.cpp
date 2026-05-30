// =============================================================================
// example_game.cpp — what a consumer of the engine static library looks like.
//
// Includes ONLY engine_api.hpp. No flecs, no cglm, no Jolt, no cgfx, no GLFW.
// (FOUNDATIONS sec 5 / Milestone 6.) cgfx CREATES the window from EngineConfig,
// so the consumer does not create one; it just runs the loop and calls tick().
// =============================================================================
#include "engine/engine_api.hpp"

int main() {
    Engine engine;

    EngineConfig cfg;
    cfg.width  = 1280;
    cfg.height = 720;
    cfg.title  = "My Game";
    cfg.depth_buffer = true;
    cfg.gravity_y    = -9.81f;

    engine.init(cfg);

    // --- Build a scene ---
    SceneID level = engine.create_scene("level");
    engine.set_active_scene(level);

    EntityID cam = engine.spawn_camera(level, "camera",
                                       60.f, 0.1f, 1000.f,   // fov, near, far
                                       0.f, 3.f, -10.f);     // position
    (void)cam;

    engine.spawn_mesh(level, "ground",
                      "assets/meshes/plane.glb", "assets/materials/ground.mat",
                      0.f, 0.f, 0.f);

    EntityID crate = engine.spawn_mesh(level, "crate",
                                       "assets/meshes/cube.glb", "assets/materials/crate.mat",
                                       0.f, 5.f, 0.f);
    engine.add_physics_box(crate, 0.5f, 0.5f, 0.5f, 10.f);   // 1m box, 10kg, falls

    EntityID sticker = engine.spawn_mesh(level, "sticker",
                                         "assets/meshes/sticker.glb", "assets/materials/sticker.mat",
                                         0.2f, 0.6f, 0.f);
    engine.set_parent(sticker, crate);                        // moves with the crate

    // --- Game loop (the platform loop polls events; cgfx is caller-polled) ---
    double last = /* now_seconds() */ 0.0;
    while (true) {
        // glfwPollEvents(engine.window());   // done by your platform layer
        // double t = now_seconds(); float dt = float(t - last); last = t;
        float dt = 1.f / 60.f; (void)last;
        int w = 1280, h = 720;                 // from the framebuffer-size callback
        engine.tick(dt, float(w) / float(h));

        if (engine.key_pressed(Key::Escape)) break;
    }

    engine.shutdown();
    return 0;
}
