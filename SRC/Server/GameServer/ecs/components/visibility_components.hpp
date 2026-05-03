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

struct ViewerMap {
    std::unordered_set<entt::entity> viewers;
};

struct ViewAgeMap {
    std::unordered_map<entt::entity, uint32_t> ageByEntity;
};

struct VisibilityDirty {};

} // namespace ecs
