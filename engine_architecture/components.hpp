#pragma once
#include <cstdint>
#include <cglm/cglm.h>

// =============================================================================
// COMPONENTS — pure data structs. No methods, no constructors.
// Flecs stores and queries these. Systems read/write them.
// =============================================================================

// --- Transform primitives (local space, set by user / physics writeback) ---

struct Position {
    vec3 value = {0.f, 0.f, 0.f};
};

struct Rotation {
    versor value = {0.f, 0.f, 0.f, 1.f};  // quaternion, identity default
};

struct Scale {
    vec3 value = {1.f, 1.f, 1.f};
};

// --- Computed by TransformSystem each frame. Never set manually. ---
struct WorldTransform {
    mat4 matrix;  // model-to-world, includes full parent chain
};

// --- Renderables ---
struct MeshHandle {
    uint32_t id = 0;      // index into AssetManager::m_meshes
};

struct MaterialHandle {
    uint32_t id = 0;      // index into AssetManager::m_materials
};

// --- Camera (written by CameraSystem, read by RenderBridge) ---
struct CameraData {
    mat4  view       = {};
    mat4  proj       = {};
    vec3  position   = {};
    float fov        = 60.f;
    float near_plane = 0.1f;
    float far_plane  = 1000.f;
};

// --- Input (written by InputSystem, read by gameplay systems) ---
struct InputState {
    vec2  move_axis       = {};   // WASD / left stick
    vec2  look_axis       = {};   // mouse delta / right stick
    bool  jump_pressed    = false;
    bool  attack_pressed  = false;
};

// --- Physics (written by PhysicsBridge after each Jolt step) ---
struct PhysicsBodyID {
    uint32_t value = UINT32_MAX;  // Jolt BodyID, UINT32_MAX = no body
};

// =============================================================================
// TAG COMPONENTS — zero size, used purely for filtering queries
// =============================================================================

struct IsCamera    {};   // marks the active camera entity
struct IsStatic    {};   // never moved, skip physics writeback
struct CastsShadow {};   // included in shadow render pass
