#pragma once
#include <flecs.h>
#include "../core/components.hpp"

// =============================================================================
// TransformSystem — free functions, no state.
//
// Two passes (PHASE_02 / SCENE.txt sec 6):
//   Transform_Roots    : entities WITHOUT a parent -> WorldTransform = local TRS
//   Transform_Children : entities WITH a parent, ordered .parent().cascade()
//                        (parent-before-child for DEEP trees) -> parent.world * local
//
// register_systems RETURNS the children-pass handle so CameraSystem can register
// .after() it (the camera must see fresh WorldTransforms).
// =============================================================================

namespace TransformSystem {

// Pure math: out = T * R * S (column-major; rot is a cglm versor quaternion).
void build_trs(mat4 out, const vec3 pos, const versor rot, const vec3 scale);

// Registers both passes; returns the Transform_Children system handle.
flecs::system register_systems(flecs::world& world);

} // namespace TransformSystem
