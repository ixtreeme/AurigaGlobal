#pragma once

#include <cstdint>
#include <vector>

#include <common/tables.h>
#include <entt/entt.hpp>

#include "../components/social_components.hpp"
#include "../../pet/horse_rider.h"

struct MountInventoryLoadRequest {
    entt::entity character { entt::null };
    uint32_t accountId { 0 };
    uint64_t requestId { 0 };
};

namespace MountSystem {

bool IsRiding(entt::entity rider);

void UpdateMountSkin(entt::entity e);
void MountUnsummon(entt::entity e, entt::entity mountItem);
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
bool IsCostumeMountSummoned(entt::entity rider);
size_t CountCostumeMounts(entt::entity rider);
void SummonCostumeMount(entt::entity rider, entt::entity mountItem, bool spawnFar);
void MountCostume(entt::entity rider, entt::entity mountItem);
void UnmountCostume(entt::entity rider);
void DestroyCostumeMountRuntime(entt::entity rider);
entt::entity GetMountInventory(entt::entity rider);
entt::entity GetMountInventoryItem(entt::entity rider, uint32_t cell);
bool IsMountInventoryPositionValid(entt::entity rider, uint32_t pos);
bool IsMountInventoryPositionEmpty(entt::entity rider, uint32_t pos, uint8_t size);
int GetMountInventorySize(entt::entity rider);
int GetMountInventoryWidth(entt::entity rider);
bool AddMountInventoryItem(entt::entity rider, uint32_t pos, entt::entity item,
    bool skipSave = false);
entt::entity RemoveMountInventoryItem(entt::entity rider, uint32_t pos,
    bool skipDbDelete = false);
bool RemoveMountInventoryItemByEntity(entt::entity rider, entt::entity item,
    bool skipDbDelete = false);
bool MoveMountInventoryItem(entt::entity rider, uint32_t from, uint32_t to);
void CollectMountInventoryItems(entt::entity rider,
    std::vector<TMountInventoryItemTable>& out);
void SendMountInventory(entt::entity owner);
// Asks the database for the account mount inventory, once.
void QueryMountInventory(entt::entity e);
void LoadMountInventory(entt::entity e, uint32_t accountId, uint64_t requestId,
    const std::vector<TMountInventoryItemTable>& items);
entt::entity CreateMountInventory(entt::entity owner, uint32_t accountId,
    uint8_t height = 16);
void ComputeMountInventoryBonuses(entt::entity owner);
void UpdateMountCountOverheadToViewers(entt::entity owner);
bool SetMountInventory(entt::entity rider, entt::entity inventory);
void DestroyMountInventory(entt::entity rider);
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
