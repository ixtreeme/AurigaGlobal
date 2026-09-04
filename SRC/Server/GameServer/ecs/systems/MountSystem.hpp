#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "../components/social_components.hpp"

class CMountInventory;

namespace MountSystem {

bool IsRiding(entt::entity rider);

// Skin and unsummon, reached from an entity. The subsystem pointers come
// from MountRuntimeRefs / PetRuntimeRefs, not from CHARACTER members.

void UpdateMountSkin(entt::entity e);
void MountUnsummon(entt::entity e, entt::entity mountItem);
void UpdatePetSkin(entt::entity e);
bool IsSummoned(entt::entity rider);
bool IsRidingCostume(entt::entity rider);
bool IsOwnedHorse(entt::entity rider, entt::entity horse);
bool StartRiding(entt::entity rider);
bool StopRiding(entt::entity rider);
void SummonHorse(entt::entity rider, bool summon, bool fromFar = false,
    uint32_t vnum = 0, const char* name = nullptr);
uint32_t GetMountVnum(entt::entity rider);
void SetMountVnum(entt::entity rider, uint32_t vnum);
void MountSummon(entt::entity rider, entt::entity mountItem);
CMountInventory* GetMountInventory(entt::entity rider);
void SetMountInventory(entt::entity rider, CMountInventory* inventory);
ecs::MountState& GetMountStateRef(entt::entity rider);
uint32_t GetLastMountTime(entt::entity rider);
uint32_t GetMyHorseVnum(entt::entity rider);
entt::entity GetSummonedHorse(entt::entity rider);
void SetSummonedHorse(entt::entity rider, entt::entity horse);
bool IsHorseRiding(entt::entity rider);
int GetHorseArmor(entt::entity rider);
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
