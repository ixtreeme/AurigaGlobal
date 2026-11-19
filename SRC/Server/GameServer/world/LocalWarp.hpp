#pragma once
#include <entt/entity/entity.hpp>
#include <cstdint>
namespace ecs::MovementSystem {
// Keep the current party/core when the authored entrance is hosted locally;
// otherwise use the normal public-map routing table.
bool WarpSetOnCurrentCore(entt::entity,std::int32_t x,std::int32_t y,std::int32_t privateMapIndex=0);
}
