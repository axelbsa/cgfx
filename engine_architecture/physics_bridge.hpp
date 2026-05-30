#pragma once
#include <cstdint>
#include <unordered_map>
#include <flecs.h>
#include "../core/components.hpp"

// Forward-declare Jolt types so consumers don't need Jolt headers
namespace JPH { class PhysicsSystem; class TempAllocatorImpl;
                class JobSystemThreadPool; }

// =============================================================================
// PhysicsBridge
//
// Owns the Jolt PhysicsSystem and translates between Jolt's body registry
// and the ECS. This is a class because it owns non-trivial resources
// (allocators, thread pool, the physics world itself).
//
// Data flow each frame:
//   1. read InputState / velocity intentions from ECS → push to Jolt bodies
//   2. PhysicsSystem::Update()
//   3. read Jolt body transforms → write Position + Rotation onto ECS entities
//
// The ECS never knows Jolt exists. Jolt never knows Flecs exists.
// This bridge is the only place they touch.
// =============================================================================

struct PhysicsConfig {
    float gravity_y    = -9.81f;
    int   max_bodies   = 1024;
    int   num_threads  = 2;
};

class PhysicsBridge {
public:
    // Lifecycle
    void init(const PhysicsConfig& cfg = {});
    void shutdown();

    // Called by Engine::tick() BEFORE world.progress().
    // Simulates one step and writes results back into ECS.
    void step_and_writeback(flecs::world& world, float dt);

    // --- Body registration ---
    // Call these when building a scene, not per-frame.

    // Creates a Jolt dynamic body and links it to the given ECS entity.
    // The entity must already have Position + Rotation components.
    void register_dynamic_body(flecs::world& world, flecs::entity e,
                               float half_extent_x,
                               float half_extent_y,
                               float half_extent_z,
                               float mass = 1.f);

    // Creates a Jolt static body (collider, never moved by physics).
    void register_static_body(flecs::world& world, flecs::entity e,
                              float half_extent_x,
                              float half_extent_y,
                              float half_extent_z);

    void unregister_body(flecs::entity e);

private:
    // Jolt internals — forward-declared, allocated in init()
    JPH::PhysicsSystem*       m_physics_system = nullptr;
    JPH::TempAllocatorImpl*   m_temp_allocator = nullptr;
    JPH::JobSystemThreadPool* m_job_system     = nullptr;

    // ECS entity <-> Jolt BodyID mapping
    // uint64 = flecs entity id, uint32 = JPH::BodyID::GetIndexAndSequenceNumber()
    std::unordered_map<uint64_t, uint32_t> m_entity_to_body;
    std::unordered_map<uint32_t, uint64_t> m_body_to_entity;
};
