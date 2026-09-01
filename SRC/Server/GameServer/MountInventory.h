#pragma once

#include <memory>
#include <vector>

#include <Base/grid.h>
#include "constants.h"
#include "item.h"
#include <entt/entt.hpp>
#include <common/tables.h>
#include <common/CommonDefines.h>
#include "typedef.h"

constexpr int MOUNT_INVENTORY_WIDTH = 12;

class CMountInventory
{
public:
    CMountInventory(entt::entity owner, int iHeight);
    ~CMountInventory();

    bool Add(uint32_t pos, entt::entity item, bool skipSave = false);
    entt::entity Get(uint32_t pos) const;
    entt::entity Remove(uint32_t pos, bool skipDbDelete = false);
    bool RemoveByItem(entt::entity item, bool skipDbDelete = false);
    bool MoveItem(uint32_t from, uint32_t to);

    bool IsEmpty(uint32_t pos, uint8_t size) const;
    bool IsValidPosition(uint32_t pos) const;

    int GetSize() const { return m_iHeight; }
    int GetWidth() const { return MOUNT_INVENTORY_WIDTH; }
    void CollectItems(std::vector<TMountInventoryItemTable>& out) const;

private:
    bool DetachSlot(uint32_t pos, entt::entity expectedItem, bool skipDbDelete);
    void Destroy();
    void SaveItem(uint32_t pos, entt::entity item);
    void DeleteItem(uint32_t pos, uint32_t id);
    uint32_t GetAccountId() const;

private:
    entt::entity m_owner { entt::null };
    std::vector<entt::entity> m_items;
    std::unique_ptr<CGrid> m_grid;
    int m_iHeight;
};
