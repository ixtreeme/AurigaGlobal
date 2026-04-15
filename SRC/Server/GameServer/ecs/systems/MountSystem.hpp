#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace MountSystem {

bool StartRiding(entt::entity rider);
bool StopRiding(entt::entity rider);
void SetRider(entt::entity horse, entt::entity rider);
entt::entity GetRider(entt::entity horse);
void HorseSummon(entt::entity owner, bool summon, bool fromFar = false, uint32_t vnum = 0, const char* petName = nullptr);
uint32_t GetMyHorseVnum(entt::entity owner);
void HorseDie(entt::entity owner);
bool ReviveHorse(entt::entity owner);
void ClearHorseInfo(entt::entity owner);
void SendHorseInfo(entt::entity owner);
bool CanUseHorseSkill(entt::entity owner);
void SetHorseLevel(entt::entity owner, int level);
bool IsRiding(entt::entity rider);
uint32_t GetMountVnum(entt::entity rider);

} // namespace MountSystem
