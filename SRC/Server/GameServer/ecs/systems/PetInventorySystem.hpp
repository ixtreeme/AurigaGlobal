#pragma once
#include <cstdint>
#include <vector>
#include <common/tables.h>
#include <entt/entt.hpp>
struct PetInventoryLoadRequest {
    entt::entity character { entt::null };
    uint32_t accountId { 0 };
    uint64_t requestId { 0 };
};
namespace PetInventorySystem {
entt::entity GetPetInventory(entt::entity rider);
entt::entity GetPetInventoryItem(entt::entity rider, uint32_t cell);
bool IsPetInventoryPositionValid(entt::entity rider, uint32_t pos);
bool IsPetInventoryPositionEmpty(entt::entity rider, uint32_t pos, uint8_t size);
int GetPetInventorySize(entt::entity rider);
int GetPetInventoryWidth(entt::entity rider);
bool AddPetInventoryItem(entt::entity rider, uint32_t pos, entt::entity item,
    bool skipSave = false);
entt::entity RemovePetInventoryItem(entt::entity rider, uint32_t pos,
    bool skipDbDelete = false);
bool RemovePetInventoryItemByEntity(entt::entity rider, entt::entity item,
    bool skipDbDelete = false);
bool MovePetInventoryItem(entt::entity rider, uint32_t from, uint32_t to);
void CollectPetInventoryItems(entt::entity rider,
    std::vector<TPetInventoryItemTable>& out);
void SendPetInventory(entt::entity owner);
// Asks the database for the account mount inventory, once.
void QueryPetInventory(entt::entity e);
void LoadPetInventory(entt::entity e, uint32_t accountId, uint64_t requestId,
    const std::vector<TPetInventoryItemTable>& items);
entt::entity CreatePetInventory(entt::entity owner, uint32_t accountId,
    uint8_t height = 16);
void ComputePetInventoryBonuses(entt::entity owner);
bool SetPetInventory(entt::entity rider, entt::entity inventory);
void DestroyPetInventory(entt::entity rider);

}
