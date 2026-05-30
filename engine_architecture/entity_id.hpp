#pragma once
#include <cstdint>

// =============================================================================
// EntityID — opaque handle exposed to consumers of the static library.
// Internally wraps a flecs::entity uint64, but consumers never see flecs.
// =============================================================================

struct EntityID {
    uint64_t value = 0;

    bool valid() const { return value != 0; }

    static EntityID null() { return {0}; }

    bool operator==(const EntityID& o) const { return value == o.value; }
    bool operator!=(const EntityID& o) const { return value != o.value; }
};
