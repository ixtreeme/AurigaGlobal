#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace MountSystem {

bool IsRiding(entt::entity rider);
bool IsSummoned(entt::entity rider);
bool IsRidingCostume(entt::entity rider);
bool IsOwnedHorse(entt::entity rider, entt::entity horse);
bool StartRiding(entt::entity rider);
bool StopRiding(entt::entity rider);
void SummonHorse(entt::entity rider, bool summon, bool fromFar = false,
    uint32_t vnum = 0, const char* name = nullptr);
uint32_t GetMountVnum(entt::entity rider);
void SetMountVnum(entt::entity rider, uint32_t vnum);
int GetHorseLevel(entt::entity rider);
void SetHorseLevel(entt::entity rider, int level);
int GetHorseHealth(entt::entity rider);
int GetHorseMaxHealth(entt::entity rider);
int GetHorseStamina(entt::entity rider);
int GetHorseMaxStamina(entt::entity rider);
int GetHorseGrade(entt::entity rider);
bool ReviveHorse(entt::entity rider);
void FeedHorse(entt::entity rider);
void ForceClearRidingState(entt::entity rider);

} // namespace MountSystem
