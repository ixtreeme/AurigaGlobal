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
bool Set(entt::entity e, uint8_t type, int64_t value);
bool SetReal(entt::entity e, uint8_t type, int64_t value);
void SetRandomHP(entt::entity e, int value);
void SetRandomSP(entt::entity e, int value);
int GetRandomHP(entt::entity e);
int GetRandomSP(entt::entity e);
bool SetLevelFromQuest(entt::entity e, int newLevel);
bool ResetStat(entt::entity e, int statIndex);
bool ResetAllPoints(entt::entity e, int level);
void Compute(entt::entity e);

#ifdef __ENABLE_BLOCK_EXP__
bool IsExperienceBlocked(entt::entity e);
bool SetExperienceBlocked(entt::entity e, bool blocked);
#endif

int GetPolymorphPoint(entt::entity e, uint8_t type);
void ComputeBattlePoints(entt::entity e);
void ApplyPoint(entt::entity e, uint8_t bApplyType, int iVal);

void Change(entt::entity e, uint8_t type, int64_t amount,
    bool bAmount = false, bool bBroadcast = false
#ifdef __ENABLE_BLOCK_EXP__
    , bool bForceExp = false
#endif
);

} // namespace ecs::PointSystem

