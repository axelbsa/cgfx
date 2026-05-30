#pragma once
#include <flecs.h>
#include "../core/components.hpp"

// =============================================================================
// TransformSystem
//
// Not a class — just free functions that Engine::init() registers as Flecs
// systems. There's no persistent state, so a class would be pointless.
//
// Runs in two passes:
//   Pass 1 — Entities WITHOUT a parent: build WorldTransform from local TRS.
//   Pass 2 — Entities WITH a parent:    parent_world * local TRS (recursive).
//
// Flecs guarantees Pass 2 runs after Pass 1 because it's registered with
// .after(pass1_system). Children are always processed after their parent
// because Flecs topologically sorts ChildOf relationships.
// =============================================================================

namespace TransformSystem {

// Build a mat4 from position, rotation (quat), scale.
// Free function — pure, no side effects.
void build_trs(mat4 out,
               const vec3 pos,
               const versor rot,
               const vec3 scale);

// Registers both passes as Flecs systems.
// Call once from Engine::init().
void register_systems(flecs::world& world);

} // namespace TransformSystem
