#include "transform_system.hpp"

namespace TransformSystem {

// =============================================================================
// build_trs — pure math, no Flecs, no state
// =============================================================================

void build_trs(mat4 out, const vec3 pos, const versor rot, const vec3 scale) {
    mat4 t, r, s;
    glm_translate_make(t, (float*)pos);
    glm_quat_mat4(*(versor*)rot, r);
    glm_scale_make(s, (float*)scale);

    // out = T * R * S
    mat4 tmp;
    glm_mat4_mul(t, r, tmp);
    glm_mat4_mul(tmp, s, out);
}

// =============================================================================
// register_systems — called once from Engine::init()
//
// Two passes are needed because Flecs doesn't automatically propagate
// WorldTransform down the ChildOf chain. We do it ourselves:
//
//   Pass 1: entities WITHOUT a parent — build WorldTransform from local TRS
//   Pass 2: entities WITH a parent   — parent WorldTransform * local TRS
//
// Pass 2 is registered .after(pass1) so roots are always computed first.
// Flecs also topologically sorts ChildOf internally, so grandchildren
// see their parent's updated WorldTransform in the same frame.
// =============================================================================

void register_systems(flecs::world& world) {

    // --- Pass 1: root entities (no ChildOf relationship) ---
    auto pass1 = world.system<const Position,
                               const Rotation,
                               const Scale,
                               WorldTransform>("Transform_Roots")
        .kind(flecs::OnUpdate)
        .without(flecs::ChildOf, flecs::Wildcard)  // exclude entities with a parent
        .each([](const Position& p, const Rotation& r,
                 const Scale& s, WorldTransform& wt) {
            build_trs(wt.matrix, p.value, r.value, s.value);
        });

    // --- Pass 2: child entities ---
    world.system<const Position,
                  const Rotation,
                  const Scale,
                  WorldTransform>("Transform_Children")
        .kind(flecs::OnUpdate)
        .after(pass1)
        .with(flecs::ChildOf, flecs::Wildcard)     // only entities with a parent
        .each([](flecs::entity e,
                 const Position& p, const Rotation& r,
                 const Scale& s, WorldTransform& wt) {
            mat4 local;
            build_trs(local, p.value, r.value, s.value);

            // Multiply parent's world matrix by our local matrix
            flecs::entity parent = e.parent();
            if (const WorldTransform* parent_wt = parent.get<WorldTransform>()) {
                glm_mat4_mul((float(*)[4])parent_wt->matrix, local, wt.matrix);
            } else {
                // Parent has no WorldTransform — treat as root
                glm_mat4_copy(local, wt.matrix);
            }
        });
}

} // namespace TransformSystem
