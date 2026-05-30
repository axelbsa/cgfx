#pragma once
#include <cstdint>
#include <cglm/cglm.h>

// =============================================================================
// COMPONENTS — pure data structs. No methods, no constructors that allocate.
// Flecs stores and queries these; systems read/write them.
//
// This file matches the design docs (ARCHITECTURE.txt component-flow table).
// cglm types (vec3/versor/mat4) are used deliberately: they are the renderer's
// native types, so the ECS->render seam is zero-conversion. This header is NOT
// part of the public API surface, so the cglm include never reaches consumers.
// Any TU including this must link cgfx PUBLIC (inherits cglm + the LH/ZO defines;
// see FOUNDATIONS sec 2).
// =============================================================================

// --- Transform primitives (local space; set by user or physics writeback) ---

struct Position { vec3 value = {0.f, 0.f, 0.f}; };

struct Rotation { versor value = {0.f, 0.f, 0.f, 1.f}; };  // identity quaternion

struct Scale { vec3 value = {1.f, 1.f, 1.f}; };

// --- Computed by TransformSystem each frame; never set manually. ---
// IMPORTANT: initialize to identity. An uninitialized mat4 is garbage until a
// system writes it, and a frame-1 read (e.g. by the camera) would see junk.
struct WorldTransform { mat4 matrix = GLM_MAT4_IDENTITY_INIT; };

// --- Renderables (opaque handles into AssetManager; this is the agnostic seam) ---
struct MeshHandle     { uint32_t id = 0; };   // index into AssetManager meshes
struct MaterialHandle { uint32_t id = 0; };   // index into AssetManager materials

// --- Camera (written by CameraSystem, read by RenderBridge) ---
// view/proj/position are computed each frame; fov_deg/near/far are inputs.
// Matrices are LEFT-HANDED, depth [0,1] (cgfx convention; RENDER_INTEGRATION 2/6).
struct CameraData {
    mat4  view       = GLM_MAT4_IDENTITY_INIT;
    mat4  proj       = GLM_MAT4_IDENTITY_INIT;
    vec3  position   = {0.f, 0.f, 0.f};
    float fov_deg    = 60.f;     // vertical field of view, degrees
    float near_plane = 0.1f;
    float far_plane  = 1000.f;
};

// --- Per-world singleton: the current viewport aspect. Engine::tick sets it
//     before world.progress so CameraSystem can recompute the projection
//     (fixes the resize-stretch bug; RENDER_INTEGRATION 6). ---
struct ViewportInfo { float aspect = 16.f / 9.f; };

// --- Input (written by InputSystem, read by gameplay/MovementSystem) ---
struct InputState {
    vec2  move_axis      = {0.f, 0.f};   // WASD / left stick, components in [-1,1]
    vec2  look_axis      = {0.f, 0.f};   // mouse delta this frame
    bool  jump_pressed   = false;        // edge (pressed this frame)
    bool  attack_pressed = false;
};

// --- Physics ---
// Set by PhysicsBridge when a body is registered. MUST be set, or the on_remove
// cleanup observer never fires (PHASE_07 7.2/7.4). UINT32_MAX = no body.
struct PhysicsBodyID { uint32_t value = UINT32_MAX; };

// --- Character (Phase 9; written by PhysicsBridge after each step) ---
struct CharacterState {
    bool is_grounded = false;
    vec3 velocity    = {0.f, 0.f, 0.f};
};

// =============================================================================
// RELATIONSHIPS & TAGS
// =============================================================================

// Scene OWNERSHIP relationship (a flecs pair target). An entity with
// (SceneMember, scene) belongs to that scene. Registered with Exclusive +
// (OnDeleteTarget, Delete) so scene.destruct() destroys exactly its members.
// ORTHOGONAL to ChildOf (which is TRANSFORM parenting). See SCENE.txt sec 1.
struct SceneMember {};

// Tag components — zero size, used purely as query filters.
struct IsCamera    {};   // marks the active camera entity (exactly one at a time)
struct IsStatic    {};   // never moved; skip physics writeback
struct CastsShadow {};   // included in the shadow pass (folded into the main query)
