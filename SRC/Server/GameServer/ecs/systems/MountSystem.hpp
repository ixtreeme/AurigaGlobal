#pragma once

#include <cstdint>
#include <vector>

#include <common/tables.h>
#include <entt/entt.hpp>

#include "../components/social_components.hpp"
#include "../../pet/horse_rider.h"

namespace MountSystem {

bool IsRiding(entt::entity rider);

void UpdateMountSkin(entt::entity e);
void MountUnsummon(entt::entity e, entt::entity mountItem);
void CheckMount(entt::entity e);
bool IsSummoned(entt::entity rider);
bool IsRidingCostume(entt::entity rider);
bool IsOwnedHorse(entt::entity rider, entt::entity horse);
void SummonHorse(entt::entity rider, bool summon, bool fromFar = false,
    uint32_t vnum = 0, const char* name = nullptr);
uint32_t GetMountVnum(entt::entity rider);
void SetMountVnum(entt::entity rider, uint32_t vnum);
void MountSummon(entt::entity rider, entt::entity mountItem);
bool IsCostumeMountSummoned(entt::entity rider);
size_t CountCostumeMounts(entt::entity rider);
void SummonCostumeMount(entt::entity rider, entt::entity mountItem, bool spawnFar);
void MountCostume(entt::entity rider, entt::entity mountItem);
void UnmountCostume(entt::entity rider);
void DestroyCostumeMountRuntime(entt::entity rider);
#if defined(AURIGA_LEGACY_ITEM_TESTS)
entt::entity GetMountInventoryItem(entt::entity rider, uint32_t cell);
#endif
void UpdateMountCountOverheadToViewers(entt::entity owner);
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
