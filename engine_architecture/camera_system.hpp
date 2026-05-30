#pragma once
#include <flecs.h>
#include "../core/components.hpp"

// =============================================================================
// CameraSystem
//
// Also a namespace of free functions rather than a class.
// Reads WorldTransform on the entity tagged IsCamera,
// writes CameraData (view + proj matrices).
//
// Your existing C camera logic moves into compute_view() and compute_proj().
// The system just calls them and stores results as a component.
//
// Registered with .after(TransformSystem) so WorldTransform is always fresh.
// =============================================================================

namespace CameraSystem {

// Pure math helpers — these are where your existing C camera code lives.
void compute_view(mat4 out_view, const mat4 world_transform);
void compute_proj(mat4 out_proj, float fov_deg, float aspect,
                  float near_plane, float far_plane);

// Registers the Flecs system. Call once from Engine::init().
void register_systems(flecs::world& world);

} // namespace CameraSystem
