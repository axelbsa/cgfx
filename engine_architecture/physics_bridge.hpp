#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <flecs.h>
#include "../core/components.hpp"

// Forward-declare Jolt types so consumers never need Jolt headers (all Jolt use is
// confined to physics_bridge.cpp).
namespace JPH { class PhysicsSystem; class TempAllocatorImpl;
                class JobSystemThreadPool; class CharacterVirtual; }

// =============================================================================
// PhysicsBridge — owns the Jolt PhysicsSystem; the ONLY place Jolt and the ECS
// touch. Jolt is RIGHT-HANDED; the engine is LEFT-HANDED. The LH<->RH coordinate
// flip happens ONLY here (register_body + step_and_writeback): negate Z on
// position/linear-velocity, negate x,y on the quaternion. Sizes are NOT flipped.
// See RENDER_INTEGRATION sec 2 and PHASE_07.
// =============================================================================

struct PhysicsConfig {
    float gravity_y   = -9.81f;
    int   max_bodies  = 1024;
    int   num_threads = 2;
};

enum class ShapeKind { Box, Sphere, Capsule };

struct ShapeDesc {
    ShapeKind kind = ShapeKind::Box;
    float hx = 0.5f, hy = 0.5f, hz = 0.5f;  // Box half-extents
    float radius = 0.5f;                     // Sphere / Capsule
    float half_height = 0.5f;                // Capsule cylinder half-height
};

class PhysicsBridge {
public:
    // init also registers the on_remove observer on PhysicsBodyID (cleanup).
    void init(flecs::world& world, const PhysicsConfig& cfg = {});
    void shutdown();

    // Called by Engine::tick BEFORE world.progress. Fixed substeps; flips RH->LH on
    // writeback; skips IsStatic. (PHASE_07 7.3)
    void step_and_writeback(flecs::world& world, float dt);

    // Body registration (scene setup, not per-frame). Reads the entity's
    // Position/Rotation (applies LH->RH flip), creates the body, stores both
    // map directions, AND sets PhysicsBodyID on the entity (required for cleanup).
    // A dynamic body's entity must be a transform ROOT (SCENE.txt 6).
    void register_body(flecs::world& world, flecs::entity e,
                       const ShapeDesc& shape, bool dynamic, float mass = 1.f);
    void unregister_body(flecs::entity e);

    // Thin wrappers for the box-only convenience API.
    void register_dynamic_body(flecs::world& w, flecs::entity e,
                               float hx, float hy, float hz, float mass = 1.f);
    void register_static_body(flecs::world& w, flecs::entity e,
                              float hx, float hy, float hz);

    // --- Phase 9 (character) ---
    void register_character(flecs::world& w, flecs::entity e,
                            float capsule_height, float capsule_radius);
    void set_character_velocity(flecs::entity e, const vec3 v);  // flips Z internally

private:
    std::unique_ptr<JPH::PhysicsSystem>       m_physics;
    std::unique_ptr<JPH::TempAllocatorImpl>   m_temp_alloc;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobs;
    float m_accum = 0.f;                       // fixed-timestep accumulator

    std::unordered_map<uint64_t, uint32_t> m_entity_to_body;  // flecs id -> BodyID bits
    std::unordered_map<uint32_t, uint64_t> m_body_to_entity;
    // Per-character CharacterVirtual handles (Phase 9) keyed by entity id, omitted here.
};
