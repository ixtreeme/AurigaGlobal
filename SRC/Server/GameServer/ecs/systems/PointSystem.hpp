#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace ecs::PointSystem {

int64_t Get(entt::entity e, uint8_t type);
int64_t GetReal(entt::entity e, uint8_t type);
int64_t GetGold(entt::entity e);
int32_t GetMaxHP(entt::entity e);
int32_t GetMaxSP(entt::entity e);
int32_t GetLevel(entt::entity e);

#ifdef __ENABLE_BLOCK_EXP__
bool IsExperienceBlocked(entt::entity e);
bool SetExperienceBlocked(entt::entity e, bool blocked);
#endif

void Change(entt::entity e, uint8_t type, int64_t amount,
    bool bAmount = false, bool bBroadcast = false
#ifdef __ENABLE_BLOCK_EXP__
    , bool bForceExp = false
#endif
);

} // namespace ecs::PointSystem

