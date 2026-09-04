#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include <entt/entt.hpp>

namespace ecs {

struct ViewMap {
    std::unordered_set<entt::entity> visible;
    uint32_t generation { 0 };
};

// The entity's own view generation counter, bumped once per UpdateSectree
// sweep. ViewMap::generation and ViewAgeMap hold snapshots of it taken at
// insert time; this is the running value they are snapshots of.
struct ViewAge {
    int age { 0 };
};

struct ViewerMap {
    std::unordered_set<entt::entity> viewers;
};

struct ViewAgeMap {
    std::unordered_map<entt::entity, uint32_t> ageByEntity;
};

struct VisibilityDirty {};

} // namespace ecs
