#pragma once
#include <cstdint>

// =============================================================================
// Opaque handles for scenes and prefabs (mirror EntityID). Wrap a flecs entity
// id; consumers never see flecs. Part of the public API (engine_api.hpp).
// =============================================================================

struct SceneID {
    uint64_t value = 0;
    bool valid() const { return value != 0; }
    static SceneID null() { return {0}; }
    bool operator==(const SceneID& o) const { return value == o.value; }
    bool operator!=(const SceneID& o) const { return value != o.value; }
};

struct PrefabID {
    uint64_t value = 0;
    bool valid() const { return value != 0; }
    static PrefabID null() { return {0}; }
    bool operator==(const PrefabID& o) const { return value == o.value; }
    bool operator!=(const PrefabID& o) const { return value != o.value; }
};
