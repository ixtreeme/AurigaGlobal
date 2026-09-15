#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "../components/social_components.hpp"
#include "../../horse_rider.h"

class CMountInventory;
class CMountSystem;

namespace MountSystem {

bool IsRiding(entt::entity rider);

// Skin and unsummon, reached from an entity. The subsystem pointers come
// from MountRuntimeRefs / PetRuntimeRefs, not from CHARACTER members.

void UpdateMountSkin(entt::entity e);
void MountUnsummon(entt::entity e, entt::entity mountItem);
CMountSystem* GetMountSystem(entt::entity e);
void CheckMount(entt::entity e);
void UpdatePetSkin(entt::entity e);
bool IsSummoned(entt::entity rider);
bool IsRidingCostume(entt::entity rider);
bool IsOwnedHorse(entt::entity rider, entt::entity horse);
void SummonHorse(entt::entity rider, bool summon, bool fromFar = false,
    uint32_t vnum = 0, const char* name = nullptr);
uint32_t GetMountVnum(entt::entity rider);
void SetMountVnum(entt::entity rider, uint32_t vnum);
void MountSummon(entt::entity rider, entt::entity mountItem);
CMountInventory* GetMountInventory(entt::entity rider);
entt::entity GetMountInventoryItem(entt::entity rider, uint32_t cell);
void SendMountInventory(entt::entity owner);
void ComputeMountInventoryBonuses(entt::entity owner);
void UpdateMountCountOverheadToViewers(entt::entity owner);
void SetMountInventory(entt::entity rider, CMountInventory* inventory);
ecs::MountState& GetMountStateRef(entt::entity rider);
uint32_t GetLastMountTime(entt::entity rider);
uint32_t GetMyHorseVnum(entt::entity rider);
int GetBeltCount(entt::entity e);
int GetMountCount(entt::entity e);
void UpdateMountInventoryCountOverhead(entt::entity source, entt::entity viewer);
entt::entity GetSummonedHorse(entt::entity rider);
void SetSummonedHorse(entt::entity rider, entt::entity horse);
entt::entity GetRider(entt::entity horse);
void SetRider(entt::entity horse, entt::entity rider);
bool IsHorseRiding(entt::entity rider);
void ForceClearRidingState(entt::entity rider);

} // namespace MountSystem
