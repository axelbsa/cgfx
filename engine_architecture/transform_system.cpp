#include "transform_system.hpp"

namespace TransformSystem {

// =============================================================================
// build_trs — pure math, no Flecs, no state.  out = T * R * S
// =============================================================================
void build_trs(mat4 out, const vec3 pos, const versor rot, const vec3 scale) {
    mat4 T, R, S, TR;
    glm_translate_make(T, (float*)pos);
    glm_quat_mat4((float*)rot, R);
    glm_scale_make(S, (float*)scale);
    glm_mat4_mul(T, R, TR);     // TR  = T * R
    glm_mat4_mul(TR, S, out);   // out = T * R * S
}

// =============================================================================
// register_systems — roots, then children. Returns the children handle.
//
// The children pass uses .parent().cascade() so flecs orders parents before
// children by DEPTH. A flat .with(ChildOf, Wildcard) would NOT guarantee that for
// multi-level trees (a grandchild could read a stale parent matrix). See
// SCENE.txt sec 6 / PHASE_02 2.4.
// =============================================================================
flecs::system register_systems(flecs::world& world) {

    auto roots = world.system<const Position, const Rotation,
                              const Scale, WorldTransform>("Transform_Roots")
        .kind(flecs::OnUpdate)
        .without(flecs::ChildOf, flecs::Wildcard)        // root entities only
        .each([](const Position& p, const Rotation& r,
                 const Scale& s, WorldTransform& wt) {
            build_trs(wt.matrix, p.value, r.value, s.value);
        });

    flecs::system children = world.system<const Position, const Rotation,
                                          const Scale, WorldTransform>("Transform_Children")
        .kind(flecs::OnUpdate)
        .with(flecs::ChildOf, flecs::Wildcard)           // entities with a parent
        .parent().cascade()                              // depth order: parent first
        .after(roots)
        .each([](flecs::entity e, const Position& p, const Rotation& r,
                 const Scale& s, WorldTransform& wt) {
            mat4 local;
            build_trs(local, p.value, r.value, s.value);
            const WorldTransform* pw = e.parent().get<WorldTransform>();
            if (pw) glm_mat4_mul((vec4*)pw->matrix, local, wt.matrix);
            else    glm_mat4_copy(local, wt.matrix);     // defensive (parent has no transform)
        });

    return children;
}

} // namespace TransformSystem
