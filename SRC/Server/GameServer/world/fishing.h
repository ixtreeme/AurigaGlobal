#pragma once

#include <entt/entity/entity.hpp>

namespace fishing
{
inline constexpr unsigned CAMPFIRE_MOB = 12000;
inline constexpr unsigned FISHER_MOB = 9009;
bool PredictFish(entt::entity owner);
}
