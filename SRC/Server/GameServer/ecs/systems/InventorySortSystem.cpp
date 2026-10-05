#include "../../core/stdafx.h"

#include "InventorySortSystem.hpp"

#ifdef ENABLE_SORT_INVEN

#include "InventorySystem.hpp"
#include "ItemSystem.hpp"
#include "ChatSystem.hpp"
#include "CombatSystem.hpp"
#include "OfflineShopSystem.hpp"
#include "SessionSystem.hpp"
#include "SocialSystem.hpp"
#include "../CharacterAccessors.hpp"
#include "../Registry.hpp"
#include "../components/character_runtime_components.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace InventorySortSystem {
namespace {

constexpr int32_t kSortCooldownSeconds = 30;

struct SortEntry {
    entt::entity item { entt::null };
    uint16_t oldCell { 0 };
    std::string name;
    uint32_t vnum { 0 };
};

int32_t GetSortAvailableTime(entt::entity e, bool extra)
{
    const auto* cooldowns = g_registry.try_get<ecs::CommandCooldowns>(e);
    if (!cooldowns)
        return 0;
    return extra ? cooldowns->sortExtraInvUntil : cooldowns->sortInvUntil;
}

void SetSortAvailableTime(entt::entity e, bool extra)
{
    auto& cooldowns = g_registry.get_or_emplace<ecs::CommandCooldowns>(e);
    (extra ? cooldowns.sortExtraInvUntil : cooldowns.sortInvUntil) =
        get_global_time() + kSortCooldownSeconds;
}

bool IsExpiringItem(entt::entity item)
{
    if (!ItemSystem::IsValidItem(item))
        return false;

    for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
    {
        const uint8_t limitType = ItemSystem::GetItemLimitType(item, i);
        if (limitType == LIMIT_REAL_TIME || limitType == LIMIT_REAL_TIME_START_FIRST_USE ||
            limitType == LIMIT_TIMER_BASED_ON_WEAR)
            return true;
    }

    return false;
}

// AutoGive merges a stackable into the first matching stack. The sorter must
// not merge a normal stack into an expiring one that was deliberately left in
// place (see the live server's special-inventory fix).
bool HasMatchingExpiringItem(entt::entity owner, entt::entity item, bool extra)
{
    if (!ItemSystem::IsValidItem(item))
        return false;

    const uint32_t vnum = ItemSystem::GetItemVnum(item);
    const int size = extra ? EXTRA_INVENTORY_MAX_NUM : InventorySystem::GetInventorySize(owner);
    for (int cell = 0; cell < size; ++cell)
    {
        const entt::entity other = extra
            ? ItemSystem::GetExtraInventoryItem(owner, static_cast<uint16_t>(cell))
            : ItemSystem::GetInventoryItem(owner, static_cast<uint16_t>(cell));
        if (other == entt::null || other == item)
            continue;
        if (ItemSystem::GetItemVnum(other) != vnum)
            continue;
        if (IsExpiringItem(other))
            return true;
    }

    return false;
}

bool IsSortBlocked(entt::entity ch)
{
    if (CombatSystem::IsDead(ch) ||
        ecs::SocialSystem::HasExchange(ch) ||
        ecs::SocialSystem::GetMyShop(ch) != entt::null ||
        ecs::SocialSystem::GetShopOwner(ch) != entt::null ||
        ecs::SessionSystem::IsSafeboxOpen(ch) ||
        ecs::SessionSystem::IsCubeOpen(ch))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 540, "");
#endif
        return true;
    }

    if (ecs::OfflineShopSystem::GetOfflineShopGuest(ch) || ecs::OfflineShopSystem::GetAuctionGuest(ch))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 782, "");
#endif
        return true;
    }

    return false;
}

bool CanMergeStackable(entt::entity item, bool extra)
{
    if (!ItemSystem::IsItemStackable(item) || ItemSystem::GetItemType(item) == ITEM_BLEND)
        return false;
#ifdef ENABLE_NEW_USE_POTION
    // New potions carry per-stack state; never re-create them during a sort.
    if (ItemSystem::GetItemType(item) == ITEM_USE &&
        ItemSystem::GetItemSubType(item) == USE_NEW_POTIION)
        return false;
#endif
    (void)extra;
    return true;
}

void SortWindow(entt::entity ch, bool extra)
{
    if (!ecs::IsCharacter(ch) || !g_registry.valid(ch))
        return;

    if (get_global_time() < GetSortAvailableTime(ch, extra))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 1290, "");
#endif
        return;
    }

    if (IsSortBlocked(ch))
        return;

    const int window = extra ? EXTRA_INVENTORY : INVENTORY;
    const int size = extra ? EXTRA_INVENTORY_MAX_NUM : InventorySystem::GetInventorySize(ch);

    std::vector<SortEntry> entries;
    entries.reserve(static_cast<size_t>(size));

    for (int cell = 0; cell < size; ++cell)
    {
        const entt::entity item = extra
            ? ItemSystem::GetExtraInventoryItem(ch, static_cast<uint16_t>(cell))
            : ItemSystem::GetInventoryItem(ch, static_cast<uint16_t>(cell));
        if (item == entt::null || !ItemSystem::IsValidItem(item))
            continue;
        if (IsExpiringItem(item))
            continue;

        SortEntry entry;
        entry.item = item;
        entry.oldCell = static_cast<uint16_t>(cell);
        entry.vnum = ItemSystem::GetItemVnum(item);
        if (!extra)
            entry.name = ItemSystem::GetItemName(item);
        entries.push_back(std::move(entry));
    }

    // Main: by name (the live server's SortMyItems). Extra: by vnum.
    std::sort(entries.begin(), entries.end(), [extra](const SortEntry& a, const SortEntry& b) {
        if (extra)
            return a.vnum < b.vnum;
        return a.name < b.name;
    });

    for (const SortEntry& entry : entries)
    {
        if (!ItemSystem::IsValidItem(entry.item) || ItemSystem::GetItemOwner(entry.item) != ch)
            continue;

        entt::entity placed = entt::null;
        const uint16_t quickslotType = extra ? QUICKSLOT_TYPE_ITEM_EXTRA : QUICKSLOT_TYPE_ITEM;

        if (CanMergeStackable(entry.item, extra) &&
            !HasMatchingExpiringItem(ch, entry.item, extra))
        {
            const uint32_t vnum = entry.vnum;
            const uint32_t count = ItemSystem::GetItemCount(entry.item);

            if (InventorySystem::RemoveFromCharacter(entry.item) == entt::null)
                continue;
            if (ItemSystem::IsValidItem(entry.item) && ItemSystem::GetItemOwner(entry.item) == entt::null)
                ItemSystem::DestroyItemEntityEcs(entry.item, "INVENTORY_SORT_MERGE");

            placed = ItemSystem::AutoGiveItemEcs(ch, vnum, count, -1, false, false);
        }
        else
        {
            if (InventorySystem::RemoveFromCharacter(entry.item) == entt::null)
                continue;

            const int cell = ItemSystem::GetEmptyInventoryPositionEcs(ch, entry.item);
            bool added = false;
            if (cell >= 0)
            {
                const TItemPos position(static_cast<uint8_t>(window), static_cast<uint16_t>(cell));
#ifdef __HIGHLIGHT_SYSTEM__
                added = InventorySystem::AddToCharacter(entry.item, ch, position, false);
#else
                added = InventorySystem::AddToCharacter(entry.item, ch, position);
#endif
            }

            if (added)
            {
                placed = entry.item;
            }
            else
            {
                // The detached item has no home: replacement keeps the sort
                // atomic instead of leaving an ownerless entity behind.
                const uint32_t vnum = entry.vnum;
                const uint32_t count = ItemSystem::GetItemCount(entry.item);
                if (ItemSystem::DestroyItemEntityEcs(entry.item, "INVENTORY_SORT_PLACE_FAIL") ||
                    ItemSystem::GetItemOwner(entry.item) == entt::null)
                {
                    placed = ItemSystem::AutoGiveItemEcs(ch, vnum, count, -1, false, false);
                }
            }
        }

        if (placed != entt::null)
        {
            const uint16_t newCell = ItemSystem::GetItemCell(placed);
            InventorySystem::SyncQuickslot(ch, quickslotType, entry.oldCell, newCell);
        }
    }

    ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, extra ? "ext_sort_done" : "inv_sort_done");
    SetSortAvailableTime(ch, extra);
}

} // namespace

void SortInventory(entt::entity character)
{
    SortWindow(character, false);
}

void SortExtraInventory(entt::entity character)
{
    SortWindow(character, true);
}

} // namespace InventorySortSystem

#endif // ENABLE_SORT_INVEN
