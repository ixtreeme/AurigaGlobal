#pragma once

#include <cstdint>
#include <type_traits>

#include <entt/entt.hpp>

namespace ecs::PointSystem {

int64_t Get(entt::entity e, uint8_t type);
// Precision is retained for bonuses; counters, IDs, HP and final damage stay integral.
bool IsPrecisionPoint(uint8_t type);
double GetPrecise(entt::entity e, uint8_t type);
int64_t GetPreciseRaw(entt::entity e, uint8_t type);
void ChangePrecise(entt::entity e, uint8_t type, double amount, bool bAmount = false, bool bBroadcast = false);
void SendPrecisionPoints(entt::entity e);
int64_t GetReal(entt::entity e, uint8_t type);
int64_t GetGold(entt::entity e);
uint64_t GetMaxHP(entt::entity e);
double GetLimitPoint(entt::entity e, uint8_t type);
uint64_t GetMaxSP(entt::entity e);
void CheckMaximumPoints(entt::entity e);
int32_t GetLevel(entt::entity e);
bool Set(entt::entity e, uint8_t type, int64_t value);
bool SetPrecise(entt::entity e, uint8_t type, double value);
template<class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
bool Set(entt::entity e, uint8_t type, T value) { return SetPrecise(e, type, static_cast<double>(value)); }
bool SetReal(entt::entity e, uint8_t type, int64_t value);
void SetRandomHP(entt::entity e, int value);
void SetRandomSP(entt::entity e, int value);
#ifdef ENABLE_GAYA_SYSTEM
int GetGaya(entt::entity e);
void SetGaya(entt::entity e, int value);
#endif
int GetRandomHP(entt::entity e);
int GetRandomSP(entt::entity e);
bool SetLevelFromQuest(entt::entity e, int newLevel);
bool ResetStat(entt::entity e, int statIndex);
bool ResetAllPoints(entt::entity e, int level);
void Compute(entt::entity e);
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
int GetInventoryExpansion(entt::entity e);
void SetInventoryExpansion(entt::entity e, int value);
#endif

#ifdef __ENABLE_BLOCK_EXP__
bool IsExperienceBlocked(entt::entity e);
bool SetExperienceBlocked(entt::entity e, bool blocked);
#endif

double GetPolymorphPoint(entt::entity e, uint8_t type);
void ComputeBattlePoints(entt::entity e);
void ApplyPoint(entt::entity e, uint8_t bApplyType, double iVal);

void Change(entt::entity e, uint8_t type, int64_t amount,
    bool bAmount = false, bool bBroadcast = false
#ifdef __ENABLE_BLOCK_EXP__
    , bool bForceExp = false
#endif
);

template<class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void Change(entt::entity e, uint8_t type, T amount, bool bAmount = false, bool bBroadcast = false)
{
    ChangePrecise(e, type, static_cast<double>(amount), bAmount, bBroadcast);
}

} // namespace ecs::PointSystem
