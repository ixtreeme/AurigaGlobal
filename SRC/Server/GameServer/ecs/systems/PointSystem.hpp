#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace ecs::PointSystem {

void Change(entt::entity e, uint8_t type, int64_t amount,
    bool bAmount = false, bool bBroadcast = false
#ifdef __ENABLE_BLOCK_EXP__
    , bool bForceExp = false
#endif
);

} // namespace ecs::PointSystem

