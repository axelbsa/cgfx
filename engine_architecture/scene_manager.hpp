#pragma once
#include <flecs.h>
#include "../core/scene_id.hpp"
#include "../core/components.hpp"   // SceneMember

// =============================================================================
// SceneManager — tracks the active scene and provides exact-set clearing.
//
// A "scene" is a flecs entity used as (a) the target of the SceneMember
// relationship its content points at, and (b) a name scope for that content.
// Unloading a scene is just scene_entity.destruct() (the relationship's
// (OnDeleteTarget, Delete) trait destroys the members) — Engine does that
// directly; this class only holds the active-scene pointer and clears members.
// See SCENE.txt.
// =============================================================================

class SceneManager {
public:
    SceneID add(SceneID s)       { if (!m_active.valid()) m_active = s; return s; }
    void    remove(SceneID s)    { if (m_active == s) m_active = SceneID::null(); }
    SceneID active() const       { return m_active; }
    void    set_active(SceneID s){ m_active = s; }

    // Destroy a scene's members but keep the scene entity itself.
    // (Query syntax is flecs-version dependent; this is the intent.)
    void clear_members(flecs::world& world, SceneID s) {
        flecs::entity scene = world.entity(s.value);
        world.query_builder().with<SceneMember>(scene).build()
            .each([](flecs::entity e) { e.destruct(); });
    }

private:
    SceneID m_active{};
};
