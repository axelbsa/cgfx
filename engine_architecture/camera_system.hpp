#pragma once
#include <flecs.h>
#include "../core/components.hpp"

// =============================================================================
// CameraSystem — free functions, no state.
//
// Reads WorldTransform on the IsCamera entity + the ViewportInfo singleton, and
// writes CameraData.view/proj/position each frame. Registered .after the
// transform children pass so WorldTransform is fresh.
//
// Conventions (LEFT-HANDED, depth [0,1]) come from cgfx's cglm build; the generic
// glm_* calls already produce LH-ZO matrices (RENDER_INTEGRATION 2/6, PHASE_03):
//   compute_view  = glm_mat4_inv(WorldTransform)   (NOT glm_lookat — orientation
//                   comes from the transform/parent chain, so parenting the camera
//                   to a character in Phase 9 just works)
//   compute_proj  = glm_perspective(rad(fov), aspect, near, far)  -> LH-ZO
// =============================================================================

namespace CameraSystem {

void compute_view(mat4 out_view, const mat4 world_transform);
void compute_proj(mat4 out_proj, float fov_deg, float aspect,
                  float near_plane, float far_plane);

// Registered .after(transform_children) so the camera sees a fresh WorldTransform.
// Pass the handle returned by TransformSystem::register_systems().
void register_systems(flecs::world& world, flecs::system transform_children);

} // namespace CameraSystem
