#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"
#include "PointSystem.hpp"
#include "MountSystem.hpp"
#include "QuestSystem.hpp"
#include "AffectSystem.hpp"
#include "DragonSoulSystem.hpp"
#include "../../skill.h"
#include "../../marriage.h"
#include "../../questmanager.h"
#include "../../MountSystem.h"
#ifndef __ENABLE_EXTEND_INVEN_SYSTEM__
#include "../../belt_inventory_helper.h"
#endif
#include "../components/status_components.hpp"
#include "../components/social_components.hpp"
#include "../components/vital_components.hpp"
#include "../components/character_runtime_components.hpp"

#include "InventorySystem.hpp"
#include "ItemSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "ViewSystem.hpp"
#include "../CharacterAccessors.hpp"

#include "../../config.h"
#include "../../char.h"
#include "../../desc.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../MountInventory.h"
#ifdef ENABLE_SWITCHBOT
#include "../../new_switchbot.h"
#endif
#include "../../DragonSoul.h"
#include "../../packet.h"
#include "../../sectree_manager.h"
#include "../../../common/VnumHelper.h"
#include "../EntityFactory.hpp"
#include "../EntityInvariants.hpp"
#include "../SpatialHelpers.hpp"
#include "../services/SpatialService.hpp"
#include "../EventDispatcher.hpp"
#include "../events.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/inventory_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/visibility_components.hpp"
#include <Core/Logging.hpp>
#include <unordered_set>

namespace
{

ecs::QuickSlots* GetQuickSlots(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	return &g_registry.get_or_emplace<ecs::QuickSlots>(e);
}

// The remaining ground/ownership paths need a location component even when
// the item has not entered character storage yet.
void EnsureItemLocation(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	(void)g_registry.get_or_emplace<ecs::ItemLocation>(e);
}

// lastOwnerPID and ownershipPID used to be passed in from CItem members and
// written back into the component alongside the owner. Those members are gone
// and the component is their only home, so this writes just the two fields it
// actually changes rather than rewriting the other two with themselves.
void SyncItemOwner(entt::entity e, entt::entity owner, uint32_t ownerPID)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& itemOwner = g_registry.get_or_emplace<ecs::ItemOwner>(e);
	itemOwner.owner = owner;
	itemOwner.ownerPID = ownerPID;
}

} // namespace

EVENTFUNC(ownership_event);

namespace InventorySystem {

bool CanEquipNow(entt::entity owner, entt::entity itemEntity)
{
    if (!g_registry.valid(owner) || !ItemSystem::IsValidItem(itemEntity)) return false;
    const auto* sourceProto = ItemSystem::GetItemProto(itemEntity);
    if (!sourceProto) return false;
    const TItemTable table = *sourceProto;
    const TItemTable* itemTable = &table;

#ifdef ENABLE_PVP_ADVANCED
	if ((ecs::PlayerRuntime::GetDuelOption(owner, "BlockChangeItem")))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 516, "");
#endif
		return false;
	}
#endif

	switch (ecs::PlayerRuntime::GetJob(owner))
	{
	case JOB_WARRIOR:
		if (ItemSystem::GetItemAntiFlag(itemEntity) & ITEM_ANTIFLAG_WARRIOR)
			return false;
		break;

	case JOB_ASSASSIN:
		if (ItemSystem::GetItemAntiFlag(itemEntity) & ITEM_ANTIFLAG_ASSASSIN)
			return false;
		break;

	case JOB_SHAMAN:
		if (ItemSystem::GetItemAntiFlag(itemEntity) & ITEM_ANTIFLAG_SHAMAN)
			return false;
		break;

	case JOB_SURA:
		if (ItemSystem::GetItemAntiFlag(itemEntity) & ITEM_ANTIFLAG_SURA)
			return false;
		break;
	}

	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		int32_t limit = itemTable->aLimits[i].lValue;
		switch (itemTable->aLimits[i].bType)
		{
		case LIMIT_LEVEL:
			if (ecs::PointSystem::GetLevel(owner) < limit) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 325, "%d", limit);
#endif
				return false;
			}
			break;
		case LIMIT_STR:
			if (ecs::PointSystem::Get(owner, POINT_ST) < limit) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 269, "%d", limit);
#endif
				return false;
			}
			break;
		case LIMIT_INT:
			if (ecs::PointSystem::Get(owner, POINT_IQ) < limit) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 468, "%d", limit);
#endif
				return false;
			}
			break;
		case LIMIT_DEX:
			if (ecs::PointSystem::Get(owner, POINT_DX) < limit) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 352, "%d", limit);
#endif
				return false;
			}
			break;

		case LIMIT_CON:
			if (ecs::PointSystem::Get(owner, POINT_HT) < limit) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 481, "%d", limit);
#endif
				return false;
			}
			break;
		}
	}

	if (ItemSystem::GetItemWearFlag(itemEntity) & WEARABLE_UNIQUE)
	{
		const bool bAllowDualUnique =
			ItemSystem::GetItemSubType(itemEntity) == 4 ||
			ItemSystem::GetItemSubType(itemEntity) == 5;

		if (!bAllowDualUnique &&
			(ItemSystem::IsSameSpecialGroup(
					ItemSystem::GetWearItem(owner, WEAR_UNIQUE1), itemEntity) ||
				ItemSystem::IsSameSpecialGroup(
					ItemSystem::GetWearItem(owner, WEAR_UNIQUE2), itemEntity) ||
				ItemSystem::IsSameSpecialGroup(
					ItemSystem::GetWearItem(owner, WEAR_COSTUME_MOUNT), itemEntity)))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 695, "");
#endif
			return false;
		}

		if (marriage::CManager::instance().IsMarriageUniqueItem(ItemSystem::GetItemVnum(itemEntity)) &&
			!marriage::CManager::instance().IsMarried(ecs::PlayerRuntime::GetPlayerID(owner)))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 696, "");
#endif
			return false;
		}
	}

#ifdef ENABLE_BUG_FIXES
	if (ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME && ItemSystem::GetItemSubType(itemEntity) == COSTUME_BODY)
	{
		const auto armor = ItemSystem::GetWearItem(owner, WEAR_BODY);
		if (armor != entt::null && !ItemSystem::IsValidItem(armor)) return false;
		if (armor != entt::null && (ItemSystem::GetItemVnum(armor) >= 11901 && ItemSystem::GetItemVnum(armor) <= 11914))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 1129, "");
#endif
			return false;
		}
	}

	if (ItemSystem::GetItemVnum(itemEntity) >= 11901 && ItemSystem::GetItemVnum(itemEntity) <= 11914)
	{
		const auto costume = ItemSystem::GetWearItem(owner, WEAR_COSTUME_BODY);
		if (costume != entt::null && !ItemSystem::IsValidItem(costume)) return false;
		if (costume != entt::null && (ItemSystem::GetItemType(costume) == ITEM_COSTUME && ItemSystem::GetItemSubType(costume) == COSTUME_BODY))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 1129, "");
#endif
			return false;
		}
	}
#endif

#ifdef ENABLE_DS_SET
	if ((DragonSoulSystem::IsDeckActivated(owner)) && (ItemSystem::IsDragonSoulItem(itemEntity))) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 76, "");
#endif
		return false;
	}
#endif

	return true;
}



bool IsEquipmentSexAllowed(entt::entity owner, entt::entity item)
{
    if (!g_registry.valid(owner) || !ItemSystem::IsValidItem(item)) return false;
    const uint32_t anti = ItemSystem::GetItemAntiFlag(item);
    const auto sex = ecs::PlayerRuntime::GetSex(owner);
    return !((anti & ITEM_ANTIFLAG_MALE) && sex == SEX_MALE) &&
        !((anti & ITEM_ANTIFLAG_FEMALE) && sex == SEX_FEMALE);
}

bool CanUnequipNow(entt::entity owner, entt::entity item, bool requireSpace)
{
    if (!g_registry.valid(owner) || !ItemSystem::IsValidItem(item) ||
        ItemSystem::GetItemOwner(item) != owner || !ItemSystem::IsItemEquipped(item))
        return false;
    if ((ItemSystem::GetItemFlags(item) & ITEM_FLAG_IRREMOVABLE) ||
        ItemSystem::IsItemExchanging(item) || ItemSystem::IsItemLocked(item)) return false;
    if ((ItemSystem::GetItemType(item) == ITEM_BELT && HasBeltItems(owner)) ||
        (requireSpace && ItemSystem::GetEmptyInventoryPositionEcs(owner, item) < 0))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 366, "");
#endif
        return false;
    }
#ifdef ENABLE_DS_SET
    if (ItemSystem::IsDragonSoulItem(item) && DragonSoulSystem::IsDeckActivated(owner))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 76, "");
#endif
        return false;
    }
#endif
    return true;
}


bool CanHandleItems(entt::entity owner, bool skipRefine, bool skipObserver)
{
    if (!g_registry.valid(owner))
        return false;
    const auto* status = g_registry.try_get<ecs::StatusFlags>(owner);
    if (!skipObserver && status && status->isObserverMode)
        return false;
    const auto* shop = g_registry.try_get<ecs::ShopState>(owner);
    if (shop && (shop->myShop || (!skipRefine && shop->underRefine)))
        return false;
    const auto* cube = g_registry.try_get<ecs::CubeWindowComponent>(owner);
    if (cube && g_registry.valid(cube->npc))
        return false;
    const auto* dragonSoul = g_registry.try_get<ecs::DragonSoulRuntimeStateComponent>(owner);
    if (dragonSoul && g_registry.valid(dragonSoul->refineWindowOpener))
        return false;
#ifdef __ATTR_TRANSFER_SYSTEM__
    const auto* transfer = g_registry.try_get<ecs::AttrTransferWindowComponent>(owner);
    if (transfer && (transfer->busy || g_registry.valid(transfer->npc)))
        return false;
#endif
    const auto* events = g_registry.try_get<ecs::LegacyCharEvents>(owner);
    if (events && events->warp)
        return false;
#ifdef ENABLE_ACCE_SYSTEM
    const auto* acce = g_registry.try_get<ecs::AcceWindowComponent>(owner);
    if (acce && (acce->combinationOpen || acce->absorptionOpen))
        return false;
#endif
    return true;
}

int GetInventorySize(entt::entity owner)
{
    if (!g_registry.valid(owner))
        return 0;
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    const auto* points = g_registry.try_get<ecs::CharacterPoints>(owner);
    const int64_t extension = points ? points->base.envanter : 0;
    return static_cast<int>(std::clamp<int64_t>(90 + 5 * extension, 0, INVENTORY_MAX_NUM));
#else
    return INVENTORY_MAX_NUM;
#endif
}

bool IsEmptyItemGrid(entt::entity owner, TItemPos position, uint8_t size, int exceptionCell)
{
    if (!g_registry.valid(owner) || size == 0)
        return false;

    // Use full-width indices: extra-inventory and belt slots exceed 255.
    const int cell = position.cell;
    const auto fits = [=](const auto* inventory, int limit, int columns, int pageSize)
    {
        if (cell >= limit)
            return false;
        const int exception = exceptionCell >= 0 && exceptionCell < limit ? exceptionCell + 1 : 0;
        for (int row = 0; row < size; ++row)
        {
            const int occupiedCell = cell + columns * row;
            if (occupiedCell >= limit || occupiedCell / pageSize != cell / pageSize)
                return false;
            const int occupied = inventory ? inventory->itemGrid[occupiedCell] : 0;
            if (occupied != 0 && occupied != exception)
                return false;
        }
        return true;
    };

    switch (position.window_type)
    {
        case INVENTORY:
        {
            const auto* inventory = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner);
            if (position.IsBeltInventoryPosition())
            {
                if (size != 1)
                    return false;
#ifndef __ENABLE_EXTEND_INVEN_SYSTEM__
                const auto belt = ItemSystem::GetWearItem(owner, WEAR_BELT);
                if (!ItemSystem::IsValidItem(belt) ||
                    !CBeltInventoryHelper::IsAvailableCell(cell - BELT_INVENTORY_SLOT_START,
                                                          ItemSystem::GetItemValue(belt, 0)))
                    return false;
#endif
                return fits(inventory, BELT_INVENTORY_SLOT_END, 1, BELT_INVENTORY_SLOT_END);
            }
            return fits(inventory, GetInventorySize(owner), INVENTORY_PAGE_COLUMN, INVENTORY_PAGE_SIZE);
        }
        case DRAGON_SOUL_INVENTORY:
            return fits(g_registry.try_get<ecs::DragonSoulInventoryComponent>(owner),
                        DRAGON_SOUL_INVENTORY_MAX_NUM, DRAGON_SOUL_BOX_COLUMN_NUM,
                        DRAGON_SOUL_INVENTORY_MAX_NUM);
#ifdef ENABLE_EXTRA_INVENTORY
        case EXTRA_INVENTORY:
        {
            if (cell >= EXTRA_INVENTORY_MAX_NUM)
                return false;
            const int category = cell / EXTRA_INVENTORY_CATEGORY_MAX_NUM;
            const int begin = category * EXTRA_INVENTORY_CATEGORY_MAX_NUM;
            int end = begin + EXTRA_INVENTORY_CATEGORY_MAX_NUM;
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
            static constexpr std::array<std::string_view, 6> unlockFlags {
                "lock_extra.cat1", "lock_extra.cat2", "lock_extra.cat3",
                "lock_extra.cat4", "lock_extra.cat5", "lock_extra.cat6"
            };
            if (category >= static_cast<int>(unlockFlags.size()))
                return false;
            constexpr int freeSlots = EXTRA_INVENTORY_PAGE_SIZE * 2 + 20;
            constexpr int maxUnlockSlots = 25 + EXTRA_INVENTORY_PAGE_SIZE;
            const int64_t unlocked = std::clamp<int64_t>(
                int64_t(ecs::QuestSystem::GetFlag(owner, unlockFlags[category])) * 5, 0, maxUnlockSlots);
            end = std::min(end, begin + freeSlots + static_cast<int>(unlocked));
#endif
            return fits(g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(owner),
                        end, EXTRA_INVENTORY_PAGE_COLUMN, EXTRA_INVENTORY_PAGE_SIZE);
        }
#endif
#ifdef ENABLE_SWITCHBOT
        case SWITCHBOT:
        {
            if (cell >= SWITCHBOT_SLOT_COUNT)
                return false;
            const auto* slots = g_registry.try_get<ecs::SwitchbotRuntimeComponent>(owner);
            return !slots || slots->items[cell] == entt::null;
        }
#endif
        default:
            return false;
    }
}

bool HasBeltItems(entt::entity owner)
{
    if (!g_registry.valid(owner))
        return false;
    const auto* inventory = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner);
    if (!inventory)
        return false;
    for (int cell = BELT_INVENTORY_SLOT_START; cell < BELT_INVENTORY_SLOT_END; ++cell)
        if (ItemSystem::IsValidItem(inventory->items[cell]))
            return true;
    return false;
}

bool IsRefining(entt::entity owner)
{
    const auto* state = g_registry.valid(owner) ? g_registry.try_get<ecs::ShopState>(owner) : nullptr;
    return state && state->underRefine;
}

int GetRefineScrollCell(entt::entity owner)
{
    const auto* state = g_registry.valid(owner) ? g_registry.try_get<ecs::ShopState>(owner) : nullptr;
    return state ? state->refineCell : -1;
}

entt::entity GetRefineNPC(entt::entity owner)
{
    const auto* state = g_registry.valid(owner) ? g_registry.try_get<ecs::ShopState>(owner) : nullptr;
    return state && g_registry.valid(state->refineNPC) ? state->refineNPC : entt::null;
}

void SetRefineNPC(entt::entity owner, entt::entity npc)
{
    if (g_registry.valid(owner))
        g_registry.get_or_emplace<ecs::ShopState>(owner).refineNPC =
            g_registry.valid(npc) ? npc : entt::null;
}

void SetRefineMode(entt::entity owner, int additionalCell)
{
    if (!g_registry.valid(owner))
        return;
    auto& state = g_registry.get_or_emplace<ecs::ShopState>(owner);
    state.refineCell = additionalCell;
    state.underRefine = true;
}

void ClearRefineMode(entt::entity owner)
{
    if (!g_registry.valid(owner))
        return;
    if (auto* state = g_registry.try_get<ecs::ShopState>(owner))
    {
        state->underRefine = false;
        state->refineNPC = entt::null;
        // DoRefineWithScroll consumes the selected scroll AFTER closing the mode.
    }
}


static bool IsQuickslotValueValid(const TQuickslot& slot)
{
    switch (slot.type)
    {
#ifdef ENABLE_EXTRA_INVENTORY
        case QUICKSLOT_TYPE_ITEM_EXTRA: return slot.pos < EXTRA_INVENTORY_MAX_NUM;
#endif
        case QUICKSLOT_TYPE_ITEM:
        {
            const TItemPos position(INVENTORY, slot.pos);
            return position.IsDefaultInventoryPosition() || position.IsBeltInventoryPosition();
        }
        case QUICKSLOT_TYPE_SKILL: return slot.pos < SKILL_MAX_NUM;
        case QUICKSLOT_TYPE_COMMAND: return true;
        default: return false;
    }
}

static bool HasQuickslotRevision(entt::entity e, uint64_t revision)
{
    if (!g_registry.valid(e))
        return false;
    const auto* slots = g_registry.try_get<ecs::QuickSlots>(e);
    return slots && slots->revision == revision;
}

ecs::QuickSlots MakeQuickSlots(std::span<const TQuickslot, QUICKSLOT_MAX_NUM> saved)
{
    ecs::QuickSlots result {};
    for (size_t pos = 0; pos < saved.size(); ++pos)
    {
        const auto slot = saved[pos];
        if (!IsQuickslotValueValid(slot))
            continue;
        // Preserve the last valid occurrence, as the old ordered load did.
        for (size_t previous = 0; previous < pos; ++previous)
            if (result.slots[previous].type == slot.type && result.slots[previous].pos == slot.pos)
                result.slots[previous] = {};
        result.slots[pos] = slot;
    }
    return result;
}

void SendQuickslots(entt::entity e)
{
    if (!g_registry.valid(e)) return;
    const auto* slots = g_registry.try_get<ecs::QuickSlots>(e);
    if (!slots) return;
    const auto snapshot = *slots;
    for (uint8_t pos = 0; pos < QUICKSLOT_MAX_NUM && HasQuickslotRevision(e, snapshot.revision); ++pos)
    {
        if (snapshot.slots[pos].type == QUICKSLOT_TYPE_NONE)
            NetworkSyncSystem::SendQuickslotDelete(e, pos);
        else
            NetworkSyncSystem::SendQuickslotAdd(e, pos, snapshot.slots[pos]);
    }
}

void SyncQuickslot(entt::entity e, uint16_t type, uint16_t oldPos, uint16_t newPos)
{
    // Item cells can exceed the byte-sized quickslot position. Never wrap them
    // onto an unrelated quickslot (in particular during item destruction).
    if (oldPos > UINT8_MAX || newPos > UINT8_MAX || oldPos == newPos ||
        type == QUICKSLOT_TYPE_NONE || type >= QUICKSLOT_TYPE_MAX_NUM)
        return;
    const TQuickslot replacement {static_cast<uint8_t>(type), static_cast<uint8_t>(newPos)};
    if (newPos != UINT8_MAX && !IsQuickslotValueValid(replacement))
        return;
    auto* slots = GetQuickSlots(e);
    if (!slots)
        return;
    std::array<bool, QUICKSLOT_MAX_NUM> changed {};
    int target = -1;
    for (size_t i = 0; i < slots->slots.size(); ++i)
        if (slots->slots[i].type == type && slots->slots[i].pos == oldPos)
            target = static_cast<int>(i);
    if (target == -1) return;
    for (size_t i = 0; i < slots->slots.size(); ++i)
    {
        if (slots->slots[i].type != type || (slots->slots[i].pos != oldPos &&
            (newPos == UINT8_MAX || slots->slots[i].pos != newPos)))
            continue;
        changed[i] = true;
        slots->slots[i] = {};
    }
    if (newPos != UINT8_MAX) slots->slots[target] = replacement;
    const auto revision = ++slots->revision;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    // Publish the entire mutation before the first network callback.
    for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM && HasQuickslotRevision(e, revision); ++i)
        if (changed[i])
        {
            if (newPos != UINT8_MAX && i == target) NetworkSyncSystem::SendQuickslotAdd(e, i, replacement);
            else NetworkSyncSystem::SendQuickslotDelete(e, i);
        }
}

bool GetQuickslot(entt::entity e, uint8_t pos, TQuickslot& out)
{
    out = {};
    if (!g_registry.valid(e) || pos >= QUICKSLOT_MAX_NUM)
        return false;
    if (const auto* slots = g_registry.try_get<ecs::QuickSlots>(e))
        out = slots->slots[pos];
    return true;
}

bool SetQuickslot(entt::entity e, uint8_t pos, const TQuickslot& value)
{
    const TQuickslot slot = value;
    if (pos >= QUICKSLOT_MAX_NUM || !IsQuickslotValueValid(slot))
        return false;
    auto* slots = GetQuickSlots(e);
    if (!slots)
        return false;
    std::array<bool, QUICKSLOT_MAX_NUM> removed {};
    for (size_t i = 0; i < slots->slots.size(); ++i)
        if (i != pos && slots->slots[i].type == slot.type && slots->slots[i].pos == slot.pos)
        {
            removed[i] = true;
            slots->slots[i] = {};
        }
    slots->slots[pos] = slot;
    const auto revision = ++slots->revision;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM && HasQuickslotRevision(e, revision); ++i)
        if (removed[i])
            NetworkSyncSystem::SendQuickslotDelete(e, i);
    if (HasQuickslotRevision(e, revision))
        NetworkSyncSystem::SendQuickslotAdd(e, pos, slot);
    return true;
}

bool SetQuickslotFromClient(entt::entity e, uint8_t pos, TQuickslot slot)
{
    if (!g_registry.valid(e) || pos >= QUICKSLOT_MAX_NUM)
        return false;
#ifdef ENABLE_EXTRA_INVENTORY
    // Older clients use 12 for the extra-inventory shortcut. Normalize a copy,
    // never modify the incoming packet buffer.
    if (slot.type == 12) slot.type = QUICKSLOT_TYPE_ITEM_EXTRA;
#endif
    if (!IsQuickslotValueValid(slot))
        return false;
#ifdef ENABLE_BUG_FIXES
    if (slot.type == QUICKSLOT_TYPE_ITEM
#ifdef ENABLE_EXTRA_INVENTORY
        || slot.type == QUICKSLOT_TYPE_ITEM_EXTRA
#endif
    )
    {
        uint8_t window = INVENTORY;
#ifdef ENABLE_EXTRA_INVENTORY
        if (slot.type == QUICKSLOT_TYPE_ITEM_EXTRA) window = EXTRA_INVENTORY;
#endif
        const auto item = ItemSystem::GetItem(e, TItemPos(window, slot.pos));
        if (!ItemSystem::IsValidItem(item) || ItemSystem::GetItemOwner(item) != e)
            return false;
        const auto itemType = ItemSystem::GetItemType(item);
        if (itemType != ITEM_USE && itemType != ITEM_QUEST)
            return false;
#ifdef ENABLE_EXTRA_INVENTORY
        if (slot.type == QUICKSLOT_TYPE_ITEM_EXTRA && itemType == ITEM_USE &&
            ItemSystem::GetItemSubType(item) == USE_POTION)
            return false;
#endif
    }
#endif
    return SetQuickslot(e, pos, slot);
}

bool DelQuickslot(entt::entity e, uint8_t pos)
{
    if (pos >= QUICKSLOT_MAX_NUM)
        return false;
    auto* slots = GetQuickSlots(e);
    if (!slots)
        return false;
    slots->slots[pos] = {};
    ++slots->revision;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    NetworkSyncSystem::SendQuickslotDelete(e, pos);
    return true;
}

bool SwapQuickslot(entt::entity e, uint8_t a, uint8_t b)
{
    if (a >= QUICKSLOT_MAX_NUM || b >= QUICKSLOT_MAX_NUM)
        return false;
    auto* slots = GetQuickSlots(e);
    if (!slots)
        return false;
    std::swap(slots->slots[a], slots->slots[b]);
    ++slots->revision;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    NetworkSyncSystem::SendQuickslotSwap(e, a, b);
    return true;
}

} // namespace InventorySystem

void CHARACTER::SyncQuickslot(uint8_t type, uint8_t oldPos, uint8_t newPos)
{
    InventorySystem::SyncQuickslot(GetEntityHandle(), type, oldPos, newPos);
}


bool CItem::AddToGround(int32_t lMapIndex, const PIXEL_POSITION& pos, bool skipOwnerCheck)
{
	if (0 == lMapIndex)
	{
		LOG_ERROR("wrong map index argument: {}", lMapIndex);
		return false;
	}

	if (GetSectree())
	{
		LOG_ERROR("sectree already assigned");
		return false;
	}

	if (!skipOwnerCheck && GetOwnerEntity() != entt::null)
	{
		LOG_ERROR("owner pointer not null");
		return false;
	}

	LPSECTREE tree = ecs::SectorAt(lMapIndex, pos.x, pos.y);

	if (!tree)
	{
		LOG_ERROR("cannot find sectree by {}x{}", pos.x, pos.y);
		return false;
	}

	//tree->Touch();

	SetWindow(GROUND);
	SetXYZ(pos.x, pos.y, pos.z);

	const entt::entity itemEntity = GetEntityHandle();
	if (itemEntity != entt::null && g_registry.valid(itemEntity)) {
		g_registry.emplace_or_replace<ecs::SpatialEntity>(itemEntity);
		g_registry.emplace_or_replace<ecs::SpatialKindTag>(itemEntity, ecs::SpatialKindTag{ecs::SpatialKind::Item});
		g_registry.emplace_or_replace<ecs::Position>(itemEntity, pos.x, pos.y, pos.z);
		g_registry.emplace_or_replace<ecs::PositionZ>(itemEntity, ecs::PositionZ{pos.z});
		g_registry.emplace_or_replace<ecs::MapIndex>(itemEntity, lMapIndex);
		g_registry.emplace_or_replace<ecs::VIDComponent>(itemEntity, GetVID());
		g_registry.emplace_or_replace<ecs::ItemGroundPosition>(itemEntity, ecs::ItemGroundPosition{pos.x, pos.y, pos.z});
		(void)g_registry.get_or_emplace<ecs::ViewMap>(itemEntity);
		(void)g_registry.get_or_emplace<ecs::ViewerMap>(itemEntity);
		(void)g_registry.get_or_emplace<ecs::ViewAgeMap>(itemEntity);
		EnsureItemLocation(itemEntity);
		g_registry.remove<ecs::ItemOwner>(itemEntity);
		g_registry.remove<ecs::ItemEquipped>(itemEntity);
		ecs::Invariants::ValidateSpatialCoverage(g_registry, itemEntity, "item.add_to_ground");
	}

	if (!ecs::SpatialService::InsertEntity(g_registry, itemEntity, static_cast<uint32_t>(lMapIndex), pos.x, pos.y, pos.z))
	{
		LOG_ERROR("cannot insert ground item entity id {} vid {} by {}x{} mapindex {}",
			GetID(), GetVID(), pos.x, pos.y, lMapIndex);
		return false;
	}
	ecs::SpatialService::UpdateSectree(g_registry, itemEntity);
	Save();
	return true;
}



#define ENABLE_IMMUNE_FIX
// return false on error state
// The cell lives in ecs::ItemLocation. SetCell is the legacy-facing name for
// what SetItemCell already does, so it forwards rather than mirroring a field.
// The owner lives in ecs::ItemOwner. GetOwner keeps returning a pointer
// because that is what its callers are typed on; GetOwnerEntity is the form
// this migration moves them to.
entt::entity CItem::GetOwnerEntity() const
{
	return ItemSystem::GetItemOwner(GetEntityHandle());
}

void CItem::SetOwnerEntity(entt::entity owner)
{
	const entt::entity itemEntity = GetEntityHandle();
	if (itemEntity == entt::null || !g_registry.valid(itemEntity))
		return;

	auto& itemOwner = g_registry.get_or_emplace<ecs::ItemOwner>(itemEntity);
	itemOwner.owner = owner;
	itemOwner.ownerPID = ecs::PlayerRuntime::GetPlayerID(owner);
}

// Same shape as SetCell: ItemSystem::SetItemWindow mirrors through this
// method, so the component is written here directly.
void CItem::SetWindow(uint8_t b)
{
	const entt::entity itemEntity = GetEntityHandle();
	if (itemEntity != entt::null && g_registry.valid(itemEntity))
		g_registry.get_or_emplace<ecs::ItemLocation>(itemEntity).window = b;
}

uint8_t CItem::GetWindow() const
{
	return ItemSystem::GetItemWindow(GetEntityHandle());
}

uint16_t CItem::GetCell() const
{
	return ItemSystem::GetItemCell(GetEntityHandle());
}

namespace InventorySystem {

void SetOwnership(entt::entity itemEntity, entt::entity character, int iSec)
{
	if (character == entt::null)
	{
		if (ItemSystem::GetItemEvents(itemEntity).ownership)
		{
			event_cancel(&ItemSystem::GetItemEvents(itemEntity).ownership);
			ItemSystem::SetItemOwnershipPID(itemEntity, 0);

			TPacketGCItemOwnership p;

			p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
			p.dwVID = ItemSystem::GetItemVID(itemEntity);
			p.szName[0] = '\0';

			ecs::ViewSystem::PacketView(itemEntity, &p, sizeof(p));

					SyncItemOwner(itemEntity, entt::null, 0);
			if (itemEntity != entt::null && g_registry.valid(itemEntity))
				g_registry.remove<ecs::ItemOwnershipDisplay>(itemEntity);
		}
		return;
	}

	if (ItemSystem::GetItemEvents(itemEntity).ownership)
		return;

	if (iSec <= 10)
		iSec = 30;

	ItemSystem::SetItemOwnershipPID(itemEntity, ecs::PlayerRuntime::GetPlayerID(character));

	item_event_info* info = AllocEventInfo<item_event_info>();
	strlcpy(info->szOwnerName, ecs::PlayerRuntime::GetName(character).data(), sizeof(info->szOwnerName));
	info->item = itemEntity;

	ItemSystem::GetItemEvents(itemEntity).ownership = event_create(ownership_event, info, PASSES_PER_SEC(iSec));

	TPacketGCItemOwnership p;

	p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
	p.dwVID = ItemSystem::GetItemVID(itemEntity);
	strlcpy(p.szName, ecs::PlayerRuntime::GetName(character).data(), sizeof(p.szName));

	ecs::ViewSystem::PacketView(itemEntity, &p, sizeof(p));

	const entt::entity ownerEntity = ItemSystem::GetItemOwner(itemEntity);
	SyncItemOwner(itemEntity, ownerEntity, ecs::PlayerRuntime::GetPlayerID(ownerEntity));
	if (itemEntity != entt::null && g_registry.valid(itemEntity))
		g_registry.emplace_or_replace<ecs::ItemOwnershipDisplay>(
			itemEntity, ecs::ItemOwnershipDisplay{ecs::PlayerRuntime::GetName(character).data()});
}

entt::entity RemoveFromGround(entt::entity itemEntity)
{
	if (itemEntity == entt::null || !g_registry.valid(itemEntity))
	{
		// The method this replaced fell through to GetSectree()->RemoveEntity(this)
		// here. That path removed the item from the sectree but emitted no
		// SendRemove, so the item stayed rendered on every client in range -
		// the exact symptom the fixup-5 note in DestroyItemEntityAndLegacy
		// describes. Nothing can broadcast without a valid entity either way,
		// so this says so instead of doing it quietly.
		LOG_ERROR("RemoveFromGround: no valid entity ({}), skipping",
			static_cast<uint32_t>(itemEntity));
		return itemEntity;
	}

	if (!ecs::PlayerRuntime::GetSectree(itemEntity))
		return itemEntity;

	SetOwnership(itemEntity, entt::null, 10);


	ecs::SpatialService::RemoveEntity(g_registry, itemEntity);

	g_registry.remove<ecs::SectorPlacement>(itemEntity);
	g_registry.remove<ecs::ViewActiveTag>(itemEntity);
	g_registry.remove<ecs::SpatialEntity>(itemEntity);
	// LPENTITY.4-fixup-item: keep SpatialKindTag intact. Removing it
	// here breaks any subsequent EntityNetworkDispatch::SendRemove
	// (e.g. PC UpdateSectree age-out) since SendRemove returns
	// silently on missing SpatialKindTag. SpatialEntity is the
	// gating tag for spatial queries; the kind tag is identity.
	g_registry.remove<ecs::ItemGroundPosition>(itemEntity);

	ecs::ViewSystem::ViewCleanup(itemEntity);

	ItemSystem::SaveItem(itemEntity);

	EnsureItemLocation(itemEntity);
	g_registry.remove<ecs::ItemOwner>(itemEntity);
	g_registry.remove<ecs::ItemEquipped>(itemEntity);

	return itemEntity;
}

namespace
{
std::unordered_set<entt::entity> unequipping;
struct UnequipGuard
{
    entt::entity item;
    ~UnequipGuard() { unequipping.erase(item); }
};

template <typename T>
bool EnsureComponent(entt::entity entity)
{
    if (!g_registry.valid(entity)) return false;
    // EnTT emplace/get_or_emplace obtains a reference AFTER on_construct.
    // A listener may already have destroyed the entity/component by then.
    // Single-element insert publishes the same signal but returns no reference.
    if (!g_registry.all_of<T>(entity)) g_registry.insert<T>(&entity, &entity + 1);
    return g_registry.valid(entity) && g_registry.all_of<T>(entity);
}

template <typename Function>
bool VisitStorage(entt::entity owner, TItemPos position, Function&& function)
{
    if (!g_registry.valid(owner)) return false;
    switch (position.window_type)
    {
    case INVENTORY:
    case EQUIPMENT: // Internal packet publication uses an absolute wear cell.
        if (auto* inventory = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner))
            return function(*inventory, position.IsBeltInventoryPosition() ? 1 : INVENTORY_PAGE_COLUMN);
        break;
    case DRAGON_SOUL_INVENTORY:
        if (auto* inventory = g_registry.try_get<ecs::DragonSoulInventoryComponent>(owner))
            return function(*inventory, DRAGON_SOUL_BOX_COLUMN_NUM);
        break;
#ifdef ENABLE_EXTRA_INVENTORY
    case EXTRA_INVENTORY:
        if (auto* inventory = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(owner))
            return function(*inventory, EXTRA_INVENTORY_PAGE_COLUMN);
        break;
#endif
#ifdef ENABLE_SWITCHBOT
    case SWITCHBOT:
        if (auto* inventory = g_registry.try_get<ecs::SwitchbotRuntimeComponent>(owner))
            return function(*inventory, 1);
        break;
#endif
    }
    return false;
}

bool EnsureStorage(entt::entity owner, uint8_t window)
{
    switch (window)
    {
    case INVENTORY: return EnsureComponent<ecs::MainInventoryRuntimeComponent>(owner);
    case DRAGON_SOUL_INVENTORY: return EnsureComponent<ecs::DragonSoulInventoryComponent>(owner);
#ifdef ENABLE_EXTRA_INVENTORY
    case EXTRA_INVENTORY: return EnsureComponent<ecs::ExtraInventoryRuntimeComponent>(owner);
#endif
#ifdef ENABLE_SWITCHBOT
    case SWITCHBOT: return EnsureComponent<ecs::SwitchbotRuntimeComponent>(owner);
#endif
    default: return false;
    }
}

bool Unowned(entt::entity item)
{
    if (!ItemSystem::IsValidItem(item) ||
        g_registry.any_of<ecs::SpatialEntity, ecs::SectorPlacement>(item))
        return false;
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    return (!owner || owner->owner == entt::null) && !ItemSystem::IsItemEquipped(item);
}

bool At(entt::entity owner, entt::entity item, TItemPos position)
{
    if (!g_registry.valid(owner) || !ItemSystem::IsValidItem(item)) return false;
    const auto* currentOwner = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* currentPosition = g_registry.try_get<ecs::ItemLocation>(item);
    return currentOwner && currentOwner->owner == owner && currentPosition &&
        currentPosition->window == position.window_type && currentPosition->cell == position.cell;
}

void SendStorageSlot(entt::entity owner, TItemPos position, bool highlight)
{
    if (!g_registry.valid(owner)) return;
    auto* desc = ecs::PlayerRuntime::GetDesc(owner);
    if (!desc || desc->GetEntity() != owner) return;
    entt::entity item = entt::null;
    if (!VisitStorage(owner, position, [&](auto& storage, int) {
        if (position.cell >= storage.items.size()) return false;
        item = storage.items[position.cell]; return true;
    })) return;
    if (At(owner, item, position))
    {
        TPacketGCItemSet packet {};
        packet.header = HEADER_GC_ITEM_SET; packet.Cell = position;
        packet.vnum = ItemSystem::GetItemVnum(item); packet.count = ItemSystem::GetItemCount(item);
        packet.flags = ItemSystem::GetItemFlags(item); packet.anti_flags = ItemSystem::GetItemAntiFlag(item);
        packet.highlight = highlight;
#ifdef ATTR_LOCK
        packet.lockedattr = ItemSystem::GetItemLockedAttributeIndex(item);
#endif
        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) packet.alSockets[i] = ItemSystem::GetItemSocket(item, i);
        for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) packet.aAttr[i] = ItemSystem::GetItemAttribute(item, i);
        desc->Packet(&packet, sizeof(packet));
    }
    else if (item == entt::null)
    {
        TPacketGCItemDelDeprecated packet {};
        packet.header = HEADER_GC_ITEM_DEL; packet.Cell = position;
#ifdef ATTR_LOCK
        packet.lockedattr = -1;
#endif
        desc->Packet(&packet, sizeof(packet));
    }
}

bool HasInventoryReference(entt::entity owner, entt::entity item)
{
    const auto contains = [item](const auto* storage) {
        return storage && std::find(storage->items.begin(), storage->items.end(), item) != storage->items.end();
    };
    return contains(g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner)) ||
        contains(g_registry.try_get<ecs::DragonSoulInventoryComponent>(owner))
#ifdef ENABLE_EXTRA_INVENTORY
        || contains(g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(owner))
#endif
#ifdef ENABLE_SWITCHBOT
        || contains(g_registry.try_get<ecs::SwitchbotRuntimeComponent>(owner))
#endif
        ;
}

bool DestinationFits(entt::entity owner, entt::entity item, TItemPos position)
{
    if (!Unowned(item) || !ecs::PlayerRuntime::IsValid(owner) ||
        !g_registry.all_of<ecs::PlayerID>(owner) || HasInventoryReference(owner, item)) return false;
    const uint8_t size = ItemSystem::GetItemSize(item);
    if (!IsEmptyItemGrid(owner, position, size)) return false;
    if (position.window_type == DRAGON_SOUL_INVENTORY)
    {
        if (!ItemSystem::IsDragonSoulItem(item)) return false;
        const uint16_t base = DSManager::instance().GetBasePosition(item);
        if (base == WORD_MAX || position.cell < base ||
            uint32_t(position.cell) + uint32_t(size - 1) * DRAGON_SOUL_BOX_COLUMN_NUM >= uint32_t(base) + DRAGON_SOUL_BOX_SIZE)
            return false;
    }
#ifdef ENABLE_EXTRA_INVENTORY
    if (position.window_type == EXTRA_INVENTORY &&
        (!ItemSystem::IsExtraItem(item) || position.cell / EXTRA_INVENTORY_CATEGORY_MAX_NUM != ItemSystem::GetItemExtraCategory(item)))
        return false;
#endif
    return VisitStorage(owner, position, [&](auto& storage, int columns) {
        int rows = size;
        if constexpr (!requires { storage.itemGrid; }) rows = 1;
        for (int row = 0; row < rows; ++row)
        {
            const size_t cell = size_t(position.cell) + row * columns;
            if (cell >= storage.items.size() || storage.items[cell] != entt::null) return false;
            if constexpr (requires { storage.itemGrid; })
                if (storage.itemGrid[cell] != 0) return false;
        }
        return true;
    });
}

bool InsertInventoryItem(entt::entity item, entt::entity owner, TItemPos position, bool highlight)
{
    if (!Unowned(item) || !ecs::PlayerRuntime::IsValid(owner) ||
        !g_registry.all_of<ecs::PlayerID>(owner) ||
        !EnsureStorage(owner, position.window_type) ||
        !EnsureComponent<ecs::ItemOwner>(item) || !EnsureComponent<ecs::ItemLocation>(item) ||
        !EnsureComponent<ecs::ItemEquipped>(item) || !DestinationFits(owner, item, position))
        return false;

    // Move the timer lease out before cancelling it: a callback cannot leave
    // event_cancel holding the address of a destroyed item component.
    if (auto* events = g_registry.try_get<ecs::ItemEvents>(item); events && events->destroy)
    {
        auto timer = std::move(events->destroy);
        event_cancel(&timer);
    }
    if (!DestinationFits(owner, item, position) ||
        !g_registry.all_of<ecs::ItemOwner, ecs::ItemLocation, ecs::ItemEquipped>(item)) return false;
    const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(owner);
    const auto size = ItemSystem::GetItemSize(item);
    const auto itemID = ItemSystem::GetItemID(item);

    // No callbacks between slot/grid and ownership stores. Unlike the old
    // PlaceItemEcs, there is no metadata prewrite and no legacy rollback/resync.
    VisitStorage(owner, position, [&](auto& storage, int columns) {
        storage.items[position.cell] = item;
        if constexpr (requires { storage.itemGrid; })
            for (int row = 0; row < size; ++row)
                storage.itemGrid[position.cell + row * columns] = position.cell + 1;
        return true;
    });
    auto& ownership = g_registry.get<ecs::ItemOwner>(item);
    ownership.owner = owner; ownership.ownerPID = pid;
    if (pid) ownership.lastOwnerPID = pid;
    g_registry.get<ecs::ItemLocation>(item) = {position.window_type, position.cell};
    g_registry.get<ecs::ItemEquipped>(item) = {};
#ifdef ENABLE_SWITCHBOT
    if (position.window_type == SWITCHBOT)
        CSwitchbotManager::instance().RegisterItem(pid, itemID, position.cell);
#endif
    if (At(owner, item, position)) ItemSystem::SaveItem(item);
    SendStorageSlot(owner, position, highlight);
    // A publication callback may move/destroy the committed item. That is not
    // an insertion failure and must not trigger an old-state rollback.
    return true;
}

bool Detached(entt::entity item)
{
    if (!ItemSystem::IsValidItem(item)) return false;
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    return owner && owner->owner == entt::null && location && location->window == RESERVED_WINDOW;
}
}

#ifdef __HIGHLIGHT_SYSTEM__
bool AddToCharacter(entt::entity item, entt::entity owner, TItemPos position, bool highlight)
#else
bool AddToCharacter(entt::entity item, entt::entity owner, TItemPos position)
#endif
{
#ifndef __HIGHLIGHT_SYSTEM__
    const bool highlight = position.window_type == DRAGON_SOUL_INVENTORY;
#endif
    if (!Unowned(item) || !ecs::PlayerRuntime::IsValid(owner) || !g_registry.all_of<ecs::PlayerID>(owner)) return false;

#ifdef ENABLE_RUNE_SYSTEM
    // Keep the acquisition rule here, separate from pure PlaceItemEcs used by
    // transfers/rollback. The old path SetWear twice and ignored EquipTo failure.
    if (position.window_type == INVENTORY && ItemSystem::IsRuneItem(item))
    {
        const int cell = ItemSystem::FindEquipCell(owner, item);
        if (cell < 0 || cell >= WEAR_MAX_NUM || ItemSystem::GetWearItem(owner, cell) != entt::null)
            return false;
        if (!EquipTo(item, owner, cell)) return false;
        if (ItemSystem::IsValidItem(item))
            if (auto* events = g_registry.try_get<ecs::ItemEvents>(item); events && events->destroy)
            {
                auto timer = std::move(events->destroy);
                event_cancel(&timer);
            }
        if (g_registry.valid(owner) && ItemSystem::IsValidItem(item) && ItemSystem::GetItemOwner(item) == owner)
        {
            ItemSystem::SetItemLastOwnerPID(item, ecs::PlayerRuntime::GetPlayerID(owner));
            ItemSystem::SaveItem(item);
        }
        return true;
    }
#endif
    if (!EnsureStorage(owner, position.window_type) || !DestinationFits(owner, item, position)) return false;
#ifdef ENABLE_ACCE_SYSTEM
    if (ItemSystem::GetItemType(item) == ITEM_COSTUME && ItemSystem::GetItemSubType(item) == COSTUME_ACCE &&
        ItemSystem::GetItemSocket(item, ACCE_ABSORPTION_SOCKET) == 0)
    {
        int absorption = ACCE_GRADE_1_ABS;
        switch (ItemSystem::GetItemValue(item, ACCE_GRADE_VALUE_FIELD))
        {
        case 2: absorption = ACCE_GRADE_2_ABS; break;
        case 3: absorption = ACCE_GRADE_3_ABS; break;
        case 4: absorption = number(ACCE_GRADE_4_ABS_MIN, ACCE_GRADE_4_ABS_MAX_COMB); break;
        }
        if (!ItemSystem::SetItemSocket(item, ACCE_ABSORPTION_SOCKET, absorption)) return false;
    }
#endif
    return InsertInventoryItem(item, owner, position, highlight);
}

} // namespace InventorySystem

namespace InventorySystem {

bool Unequip(entt::entity itemEntity)
{
    if (!ItemSystem::IsValidItem(itemEntity) ||
        !g_registry.all_of<ecs::ItemOwner, ecs::ItemLocation, ecs::ItemEquipped>(itemEntity) ||
        !unequipping.insert(itemEntity).second) return false;
    const UnequipGuard guard {itemEntity};
	if (ItemSystem::GetItemOwner(itemEntity) == entt::null || ItemSystem::GetItemCell(itemEntity) < INVENTORY_MAX_NUM)
	{
		LOG_ERROR("{} {} owner {}, GetCell {}", ItemSystem::GetItemName(itemEntity), ItemSystem::GetItemID(itemEntity), static_cast<uint32_t>(ItemSystem::GetItemOwner(itemEntity)), ItemSystem::GetItemCell(itemEntity));
		return false;
	}

	const entt::entity charEntity = ItemSystem::GetItemOwner(itemEntity);
    const auto originalCell = ItemSystem::GetItemCell(itemEntity);
    const auto originalWindow = ItemSystem::GetItemWindow(itemEntity);
    if (originalCell >= INVENTORY_MAX_NUM + WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
        return false;
    const auto unchanged = [&] {
        return g_registry.valid(charEntity) && ItemSystem::IsValidItem(itemEntity) &&
            g_registry.all_of<ecs::ItemOwner, ecs::ItemLocation, ecs::ItemEquipped>(itemEntity) &&
            ItemSystem::GetItemOwner(itemEntity) == charEntity &&
            ItemSystem::GetItemCell(itemEntity) == originalCell &&
            ItemSystem::GetItemWindow(itemEntity) == originalWindow && ItemSystem::IsItemEquipped(itemEntity);
    };
    if (!unchanged() || !ItemSystem::GetItemProto(itemEntity)) return false;
    const bool hasWearTimer = ItemSystem::GetItemProto(itemEntity)->cLimitTimerBasedOnWearIndex != -1;
	if (ItemSystem::GetWearItem(
			charEntity, static_cast<uint8_t>(ItemSystem::GetItemCell(itemEntity) - INVENTORY_MAX_NUM)) != itemEntity)
	{
		LOG_ERROR("GetWearItem(owner, cell) is not this item");
		return false;
	}


#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (ItemSystem::IsMountItem(itemEntity))
		MountSystem::MountUnsummon(charEntity, itemEntity);
#endif

	if (!unchanged()) return false;
	if (ItemSystem::IsRideItem(itemEntity))
		ItemSystem::ClearMountAttributeAndAffect(itemEntity);

	if (!unchanged()) return false;
	if (ItemSystem::IsDragonSoulItem(itemEntity))
	{
		DSManager::instance().DeactivateDragonSoul(itemEntity);
	}
#ifdef ENABLE_RUNE_SYSTEM
	else if (ItemSystem::IsRuneItem(itemEntity)) {
		if (ItemSystem::GetItemSocket(itemEntity, 1) == 1)
			ItemSystem::ModifyPoints(itemEntity, false);
	}
#endif
	else
	{
		ItemSystem::ModifyPoints(itemEntity, false);
	}

	if (!unchanged()) return false;
	ItemSystem::StopUniqueExpireEvent(itemEntity);
	if (!unchanged()) return false;

	if (hasWearTimer)
		ItemSystem::StopTimerBasedOnWearExpireEvent(itemEntity);

	if (!unchanged()) return false;
	ItemSystem::StopAccessorySocketExpireEvent(itemEntity);
	if (!unchanged()) return false;

	ecs::PlayerRuntime::BuffOnAttr_RemoveBuffsFromItem(charEntity, itemEntity);

	if (!unchanged()) return false;
    // Commit the wear slot and item metadata together before publishing any
    // packet/event. SetItemCell(oldOwner, 0) would reattach the owner here.
    auto* inventory = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(charEntity);
    if (!inventory || originalCell >= inventory->items.size() || inventory->items[originalCell] != itemEntity)
        return false;
    inventory->items[originalCell] = entt::null;
    for (auto& grid : inventory->itemGrid) if (grid == originalCell + 1) grid = 0;
    auto& ownership = g_registry.get<ecs::ItemOwner>(itemEntity);
    ownership.owner = entt::null; ownership.ownerPID = 0;
    g_registry.get<ecs::ItemLocation>(itemEntity) = {RESERVED_WINDOW, 0};
    g_registry.get<ecs::ItemEquipped>(itemEntity) = {};
    const auto stillDetached = [&] { return g_registry.valid(charEntity) && Detached(itemEntity); };

#ifndef ENABLE_BUG_FIXES
    if (originalCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
    {
        if (AffectSystem::IsAffectFlag(charEntity, AFF_GWIGUM))
            AffectSystem::RemoveAffect(charEntity, SKILL_GWIGEOM);
        if (!stillDetached()) return true;
        if (AffectSystem::IsAffectFlag(charEntity, AFF_GEOMGYEONG))
            AffectSystem::RemoveAffect(charEntity, SKILL_GEOMKYUNG);
        if (!stillDetached()) return true;
    }
#endif

#ifndef ENABLE_IMMUNE_FIX
	uint32_t dwImmuneFlag = 0;

	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		const entt::entity item = ItemSystem::GetWearItem(charEntity, i);
		if (ItemSystem::IsValidItem(item))
		{
			SET_BIT(dwImmuneFlag, ItemSystem::GetItemImmuneFlags(item));
		}
	}

	ecs::PlayerRuntime::SetImmuneFlag(charEntity, dwImmuneFlag);
#endif

	if (!stillDetached()) return true;
	ecs::PointSystem::ComputeBattlePoints(charEntity);
	if (!stillDetached()) return true;

	NetworkSyncSystem::UpdatePacket(charEntity);
	if (!stillDetached()) return true;
#ifdef ENABLE_COSTUME_PET
    if (!stillDetached()) return true;
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_PET_SKIN)) {
		MountSystem::UpdatePetSkin(charEntity);
	}
#endif
#ifdef ENABLE_COSTUME_MOUNT
	if (!stillDetached()) return true;
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_MOUNT_SKIN)) {
		MountSystem::UpdateMountSkin(charEntity);
	}
#endif
    if (!stillDetached()) return true;
    SendStorageSlot(charEntity, TItemPos(EQUIPMENT, originalCell), false);
    if (!stillDetached()) return true;
    g_dispatcher.trigger(ecs::EvItemUnequipped { charEntity, itemEntity });
    return true;
}

bool EquipTo(entt::entity itemEntity, entt::entity charEntity, uint8_t bWearCell)
{
	if (!ecs::PlayerRuntime::IsValid(charEntity) || !g_registry.all_of<ecs::PlayerID>(charEntity) ||
        !ItemSystem::IsValidItem(itemEntity) || !ItemSystem::GetItemProto(itemEntity) ||
        unequipping.contains(itemEntity))
	{
		LOG_ERROR("EquipTo: nil character");
		return false;
	}

	if (ItemSystem::IsDragonSoulItem(itemEntity))
	{
		if (bWearCell < WEAR_MAX_NUM || bWearCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
		{
			LOG_ERROR("EquipTo: invalid dragon soul cell (item: #{} {} wearflag: {} cell: {})", ItemSystem::GetItemOriginalVnum(itemEntity), ItemSystem::GetItemName(itemEntity), ItemSystem::GetItemSubType(itemEntity), bWearCell - WEAR_MAX_NUM);
			return false;
		}
	}
	else
	{
		if (bWearCell >= WEAR_MAX_NUM)
		{
			LOG_ERROR("EquipTo: invalid wear cell (item: #{} {} wearflag: {} cell: {})", ItemSystem::GetItemOriginalVnum(itemEntity), ItemSystem::GetItemName(itemEntity), ItemSystem::GetItemWearFlag(itemEntity), bWearCell);
			return false;
		}
	}

	const entt::entity occupied = ItemSystem::GetWearItem(charEntity, bWearCell);
	if (occupied != entt::null)
	{
        LOG_ERROR("EquipTo: occupied wear cell {}", bWearCell);
		return false;
	}

    const auto target = TItemPos(EQUIPMENT, INVENTORY_MAX_NUM + bWearCell);
    const auto rawOwner = g_registry.try_get<ecs::ItemOwner>(itemEntity);
    if (rawOwner && rawOwner->owner != entt::null)
    {
        if (rawOwner->owner != charEntity || RemoveFromCharacter(itemEntity) == entt::null)
            return false;
    }
    if (!Unowned(itemEntity) || !g_registry.valid(charEntity) ||
        !EnsureStorage(charEntity, INVENTORY) ||
        !EnsureComponent<ecs::ItemOwner>(itemEntity) || !EnsureComponent<ecs::ItemLocation>(itemEntity) ||
        !EnsureComponent<ecs::ItemEquipped>(itemEntity)) return false;
    if (auto* events = g_registry.try_get<ecs::ItemEvents>(itemEntity); events && events->destroy)
    {
        auto timer = std::move(events->destroy);
        event_cancel(&timer);
    }
    if (!Unowned(itemEntity) || !g_registry.valid(charEntity) ||
        !g_registry.all_of<ecs::PlayerID>(charEntity) ||
        !g_registry.all_of<ecs::ItemOwner, ecs::ItemLocation, ecs::ItemEquipped>(itemEntity) ||
        HasInventoryReference(charEntity, itemEntity)) return false;
    auto* inventory = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(charEntity);
    if (!inventory || target.cell >= inventory->items.size() ||
        inventory->items[target.cell] != entt::null || inventory->itemGrid[target.cell] != 0) return false;
    const auto* proto = ItemSystem::GetItemProto(itemEntity);
    if (!proto) return false;
    const bool hasWearTimer = proto->cLimitTimerBasedOnWearIndex != -1;
    const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(charEntity);
    inventory->items[target.cell] = itemEntity;
    inventory->itemGrid[target.cell] = target.cell + 1;
    auto& owner = g_registry.get<ecs::ItemOwner>(itemEntity);
    owner.owner = charEntity; owner.ownerPID = pid;
    if (pid) owner.lastOwnerPID = pid;
    g_registry.get<ecs::ItemLocation>(itemEntity) = {EQUIPMENT, target.cell};
    g_registry.get<ecs::ItemEquipped>(itemEntity) = {true, bWearCell};
    const auto stillWorn = [&] {
        if (!At(charEntity, itemEntity, target) || !ItemSystem::IsItemEquipped(itemEntity)) return false;
        const auto* current = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(charEntity);
        return current && current->items[target.cell] == itemEntity;
    };

#ifndef ENABLE_IMMUNE_FIX
	uint32_t dwImmuneFlag = 0;

	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		const entt::entity item = ItemSystem::GetWearItem(charEntity, i);
		if (ItemSystem::IsValidItem(item))
		{
			SET_BIT(dwImmuneFlag, ItemSystem::GetItemImmuneFlags(item));
		}
	}

	ecs::PlayerRuntime::SetImmuneFlag(charEntity, dwImmuneFlag);
#endif

	if (!stillWorn()) return true;
	if (ItemSystem::IsDragonSoulItem(itemEntity))
	{
		DSManager::instance().ActivateDragonSoul(itemEntity);
	}
	else
	{
#ifdef ENABLE_RUNE_SYSTEM
		if (!ItemSystem::IsRuneItem(itemEntity))
			ItemSystem::ModifyPoints(itemEntity, true);
		else if (ItemSystem::GetItemSocket(itemEntity, 1) == 1)
			ItemSystem::ModifyPoints(itemEntity, true);
#else
		ItemSystem::ModifyPoints(itemEntity, true);
#endif
		if (!stillWorn()) return true;
		ItemSystem::StartUniqueExpireEvent(itemEntity);
        if (!stillWorn()) return true;
		if (hasWearTimer)
			ItemSystem::StartTimerBasedOnWearExpireEvent(itemEntity);

		// ACCESSORY_REFINE
		if (!stillWorn()) return true;
		ItemSystem::StartAccessorySocketExpireEvent(itemEntity);
		// END_OF_ACCESSORY_REFINE
	}

	if (!stillWorn()) return true;
	ecs::PlayerRuntime::BuffOnAttr_AddBuffsFromItem(charEntity, itemEntity);

	if (!stillWorn()) return true;
	ecs::PointSystem::ComputeBattlePoints(charEntity);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (!stillWorn()) return true;
	if (ItemSystem::IsMountItem(itemEntity))
		MountSystem::MountSummon(charEntity, itemEntity);
#endif
	if (!stillWorn()) return true;
	NetworkSyncSystem::UpdatePacket(charEntity);
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
    if (!stillWorn()) return true;
    if (bWearCell == WEAR_BELT)
        NetworkSyncSystem::UpdateItemOnTitleName(g_registry, charEntity, true);
#endif

#ifdef ENABLE_COSTUME_PET
	if (!stillWorn()) return true;
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_PET_SKIN)) {
		if (!stillWorn()) return true;
		MountSystem::UpdatePetSkin(charEntity);
	}
#endif
#ifdef ENABLE_COSTUME_MOUNT
    if (!stillWorn()) return true;
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_MOUNT_SKIN)) {
		if (!stillWorn()) return true;
		MountSystem::UpdateMountSkin(charEntity);
	}
#endif

    if (!stillWorn()) return true;
    SendStorageSlot(charEntity, target, false);
    if (!stillWorn()) return true;
    g_dispatcher.trigger(ecs::EvItemEquipped { charEntity, itemEntity });
    if (stillWorn()) ItemSystem::SaveItem(itemEntity);
    return true;
}

entt::entity RemoveFromCharacter(entt::entity item)
{
    if (!ItemSystem::IsValidItem(item)) return entt::null;
    const auto* ownership = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    if (!ownership || !location) return entt::null;
    const auto owner = ownership->owner;
    const TItemPos oldPosition(location->window, location->cell);
    const uint32_t ownerPID = ownership->ownerPID;
    if (owner == entt::null) return Detached(item) ? item : entt::null;

    if (oldPosition.window_type == SAFEBOX || oldPosition.window_type == MALL)
    {
        auto container = SafeboxSystem::Get(owner, oldPosition.window_type);
        if (container && container->Get(oldPosition.cell) == item)
            return container->Remove(oldPosition.cell);
        // Container Remove/Close unpublishes its slot before calling us back.
    }

    const bool wasEquipped = ItemSystem::IsItemEquipped(item);
    if (wasEquipped)
    {
        // Equipment effects still use the existing entity-based unequip engine.
        // Crucially it receives the real owner/cell, not pre-cleared metadata.
        if (!Unequip(item)) return entt::null;
        if (!ItemSystem::IsValidItem(item)) return entt::null;
        const auto* current = g_registry.try_get<ecs::ItemOwner>(item);
        if (!current || current->owner != entt::null) return entt::null;
    }

    // Components can be absent after a preceding ground/container operation.
    if (!EnsureComponent<ecs::ItemEquipped>(item) || !ItemSystem::IsValidItem(item) ||
        !g_registry.all_of<ecs::ItemOwner, ecs::ItemLocation>(item))
        return entt::null;
    auto& currentOwner = g_registry.get<ecs::ItemOwner>(item);
    const auto& currentLocation = g_registry.get<ecs::ItemLocation>(item);
    if (wasEquipped ? !Detached(item) :
        (currentOwner.owner != owner || currentLocation.window != oldPosition.window_type ||
            currentLocation.cell != oldPosition.cell)) return entt::null;

    std::vector<TItemPos> removedSlots;
    std::vector<uint16_t> switchSlots;
    const auto collectStorage = [&](auto* storage, uint8_t window) {
        if (!storage) return;
        for (size_t cell = 0; cell < storage->items.size(); ++cell)
        {
            if (storage->items[cell] != item) continue;
#ifdef ENABLE_SWITCHBOT
            if (window == SWITCHBOT) switchSlots.push_back(static_cast<uint16_t>(cell));
#endif
            removedSlots.emplace_back(window, static_cast<uint16_t>(cell));
        }
    };
    if (g_registry.valid(owner))
    {
        collectStorage(g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner), INVENTORY);
        collectStorage(g_registry.try_get<ecs::DragonSoulInventoryComponent>(owner), DRAGON_SOUL_INVENTORY);
#ifdef ENABLE_EXTRA_INVENTORY
        collectStorage(g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(owner), EXTRA_INVENTORY);
#endif
#ifdef ENABLE_SWITCHBOT
        collectStorage(g_registry.try_get<ecs::SwitchbotRuntimeComponent>(owner), SWITCHBOT);
#endif
    }
    // All temporary allocation precedes the first slot/owner mutation.
    for (const auto position : removedSlots)
        VisitStorage(owner, position, [&](auto& storage, int) {
            storage.items[position.cell] = entt::null;
            if constexpr (requires { storage.itemGrid; })
                for (auto& grid : storage.itemGrid) if (grid == position.cell + 1) grid = 0;
            return true;
        });
    currentOwner.owner = entt::null; currentOwner.ownerPID = 0;
    g_registry.get<ecs::ItemLocation>(item) = {RESERVED_WINDOW, 0};
    g_registry.get<ecs::ItemEquipped>(item) = {};

    // External containers have already unpublished their slot in normal Remove/
    // Close flows. Mount's item-based removal is idempotent when invoked here.
    if (oldPosition.window_type == MOUNT_INVENTORY && g_registry.valid(owner))
        if (auto* mounts = MountSystem::GetMountInventory(owner)) mounts->RemoveByItem(item);
#ifdef ENABLE_SWITCHBOT
    for (const auto cell : switchSlots)
        if (g_registry.valid(owner))
        {
            const auto* slots = g_registry.try_get<ecs::SwitchbotRuntimeComponent>(owner);
            if (slots && slots->items[cell] == entt::null)
                CSwitchbotManager::instance().UnregisterItem(ownerPID, cell);
        }
#endif
    if (Detached(item)) ItemSystem::SaveItem(item);
    for (const auto position : removedSlots) SendStorageSlot(owner, position, false);
    return item;
}

} // namespace InventorySystem

namespace ItemSystem {

entt::entity GetWearItem(entt::entity owner, uint8_t slot)
{
    // Dragon-soul decks occupy the wear range immediately after ordinary gear,
    // just as CHARACTER::GetWear did. They must not be reported as empty.
    if (!g_registry.valid(owner) || slot >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
        return entt::null;
    const auto* inventory = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner);
    return inventory ? inventory->items[INVENTORY_MAX_NUM + slot] : entt::null;
}

namespace {
std::unordered_set<entt::entity> equipmentActions;
struct EquipmentAction {
    entt::entity owner;
    bool entered;
    explicit EquipmentAction(entt::entity e) : owner(e), entered(equipmentActions.insert(e).second) {}
    ~EquipmentAction() { if (entered) equipmentActions.erase(owner); }
};
bool WornBy(entt::entity owner, entt::entity item) {
    if (!g_registry.valid(owner) || !IsValidItem(item) || GetItemOwner(item) != owner ||
        !IsItemEquipped(item) || GetItemWindow(item) != EQUIPMENT) return false;
    const auto cell = GetItemCell(item);
    const auto* inventory = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner);
    return inventory && cell >= INVENTORY_MAX_NUM && cell < INVENTORY_MAX_NUM + WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX &&
        inventory->items[cell] == item;
}
bool EquipmentType(entt::entity item) {
    switch (GetItemType(item)) {
    case ITEM_COSTUME: case ITEM_ARMOR: case ITEM_WEAPON: case ITEM_ROD:
    case ITEM_PICK: case ITEM_UNIQUE: case ITEM_DS: case ITEM_SPECIAL_DS: case ITEM_RING: return true;
    case ITEM_BELT: return GetItemValue(item, 5) == 1;
    default: return false;
    }
}
uint8_t StorageWindow(entt::entity item) {
    if (IsDragonSoulItem(item)) return DRAGON_SOUL_INVENTORY;
#ifdef ENABLE_EXTRA_INVENTORY
    if (IsExtraItem(item)) return EXTRA_INVENTORY;
#endif
    return INVENTORY;
}
bool CarryDetached(entt::entity owner, entt::entity item, TItemPos preferred) {
    if (!g_registry.valid(owner) || !InventorySystem::Detached(item)) return false;
    if (InventorySystem::InsertInventoryItem(item, owner, preferred, false)) return true;
    if (!g_registry.valid(owner) || !InventorySystem::Detached(item)) return false;
    const int cell = GetEmptyInventoryPositionEcs(owner, item);
    return cell >= 0 && InventorySystem::InsertInventoryItem(item, owner, TItemPos(StorageWindow(item), cell), false);
}
void RestoreDetached(entt::entity owner, entt::entity item, TItemPos position) {
    if (!g_registry.valid(owner) || !InventorySystem::Detached(item)) return;
    if (position.window_type == EQUIPMENT) {
        if (position.cell >= INVENTORY_MAX_NUM && position.cell < INVENTORY_MAX_NUM + WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX &&
            InventorySystem::EquipTo(item, owner, position.cell - INVENTORY_MAX_NUM)) return;
        // A callback can legitimately occupy the old wear slot. Preserve its
        // occupant and recover our detached item into its carrying inventory.
        if (!g_registry.valid(owner) || !InventorySystem::Detached(item)) return;
        const int cell = GetEmptyInventoryPositionEcs(owner, item);
        if (cell >= 0 && CarryDetached(owner, item, TItemPos(StorageWindow(item), cell))) return;
    } else if (CarryDetached(owner, item, position)) return;
    LOG_ERROR("Equipment recovery failed: owner {} item {} window {} cell {}",
        entt::to_integral(owner), entt::to_integral(item), position.window_type, position.cell);
}
bool UnequipAction(entt::entity owner, entt::entity item) {
    if (!WornBy(owner, item) || !InventorySystem::CanUnequipNow(owner, item)) return false;
    const uint16_t wearCell = GetItemCell(item);
    const auto unchanged = [&] { return WornBy(owner, item) && GetItemCell(item) == wearCell; };
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
    if (wearCell == INVENTORY_MAX_NUM + WEAR_WEAPON) {
        const auto costume = GetWearItem(owner, WEAR_COSTUME_WEAPON);
        if (costume != entt::null && (!UnequipAction(owner, costume) || !unchanged())) return false;
    }
#endif
    if (!unchanged()) return false;
    const int cell = GetEmptyInventoryPositionEcs(owner, item);
    if (cell < 0) return false;
    const TItemPos destination(StorageWindow(item), cell);
    if (InventorySystem::RemoveFromCharacter(item) == entt::null || !InventorySystem::Detached(item)) return false;
    if (!CarryDetached(owner, item, destination)) {
        RestoreDetached(owner, item, TItemPos(EQUIPMENT, wearCell));
        return false;
    }
    if (!g_registry.valid(owner) || !IsValidItem(item) || GetItemOwner(item) != owner || IsItemEquipped(item)) return true;
    const int64_t hp = ecs::PointSystem::Get(owner, POINT_HP), maxHP = ecs::PointSystem::GetMaxHP(owner);
    if (hp > maxHP) ecs::PointSystem::Change(owner, POINT_HP, maxHP - hp);
    if (!g_registry.valid(owner)) return true;
    const int64_t sp = ecs::PointSystem::Get(owner, POINT_SP), maxSP = ecs::PointSystem::GetMaxSP(owner);
    if (sp > maxSP) ecs::PointSystem::Change(owner, POINT_SP, maxSP - sp);
    if (!g_registry.valid(owner)) return true;
#ifdef ENABLE_BUG_FIXES
    if (wearCell == INVENTORY_MAX_NUM + WEAR_WEAPON) {
        if (AffectSystem::IsAffectFlag(owner, AFF_GWIGUM)) AffectSystem::RemoveAffect(owner, SKILL_GWIGEOM);
        if (!g_registry.valid(owner)) return true;
        if (AffectSystem::IsAffectFlag(owner, AFF_GEOMGYEONG)) AffectSystem::RemoveAffect(owner, SKILL_GEOMKYUNG);
        if (!g_registry.valid(owner)) return true;
    }
#endif
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
    if (wearCell == INVENTORY_MAX_NUM + WEAR_BELT) NetworkSyncSystem::UpdateItemOnTitleName(g_registry, owner);
#endif
    return true;
}
bool FitsReplacement(entt::entity owner, entt::entity outgoing, entt::entity incoming, TItemPos source) {
    if (source.window_type != StorageWindow(incoming)) return false;
    if (!InventorySystem::IsEmptyItemGrid(owner, source, GetItemSize(incoming), source.cell)) return false;
    return InventorySystem::VisitStorage(owner, source, [&](const auto& storage, int columns) {
        for (int row = 0; row < GetItemSize(incoming); ++row) {
            const size_t cell = source.cell + row * columns;
            if (cell >= storage.items.size()) return false;
            if (storage.items[cell] != entt::null && storage.items[cell] != outgoing) return false;
        }
        return true;
    });
}
void FinishEquip(entt::entity owner, entt::entity item);
} // namespace

bool UnequipItemEcs(entt::entity owner, entt::entity item) {
    if (!g_registry.valid(owner) || !IsValidItem(item)) return false;
    const EquipmentAction action(owner);
    return action.entered && UnequipAction(owner, item);
}

bool EquipItemEcs(entt::entity owner, entt::entity item, int candidateCell) {
    if (!g_registry.valid(owner) || !g_registry.all_of<ecs::PlayerID>(owner) || !IsValidItem(item) ||
        IsItemExchanging(item) || IsItemLocked(item) || IsItemEquipped(item) || !EquipmentType(item)) return false;
    const auto rawOwner = [&] {
        const auto* ownership = g_registry.try_get<ecs::ItemOwner>(item);
        return ownership ? ownership->owner : entt::entity(entt::null);
    }();
    if (rawOwner != entt::null && rawOwner != owner) return false;
    const EquipmentAction action(owner);
    if (!action.entered || !InventorySystem::CanEquipNow(owner, item)) return false;
    const int wear = FindEquipCell(owner, item, candidateCell);
    if (wear < 0 || wear >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX ||
        (candidateCell >= 0 && candidateCell != wear)) return false;
    const uint32_t vnum = GetItemVnum(item);
    if (wear == WEAR_BODY && MountSystem::IsRiding(owner) && vnum >= 11901 && vnum <= 11904) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 693, "");
#endif
        return false;
    }
    if (wear != WEAR_ARROW && AffectSystem::IsPolymorphed(owner)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 315, "");
#endif
        return false;
    }
    if (!InventorySystem::IsEquipmentSexAllowed(owner, item)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 496, "");
#endif
        return false;
    }
    const TItemPos original(GetItemWindow(item), GetItemCell(item));
    const bool carried = rawOwner == owner;
    if (carried && (!InventorySystem::At(owner, item, original) ||
        !InventorySystem::VisitStorage(owner, original, [&](const auto& inventory, int) {
            return original.cell < inventory.items.size() && inventory.items[original.cell] == item;
        }) ||
        (original.window_type != INVENTORY && original.window_type != DRAGON_SOUL_INVENTORY
#ifdef ENABLE_EXTRA_INVENTORY
         && original.window_type != EXTRA_INVENTORY
#endif
        ))) return false;
    const auto sourceUnchanged = [&] {
        if (!g_registry.valid(owner) || !IsValidItem(item) || IsItemEquipped(item) ||
            IsItemExchanging(item) || IsItemLocked(item)) return false;
        return carried ? InventorySystem::At(owner, item, original) &&
            InventorySystem::VisitStorage(owner, original, [&](const auto& inventory, int) {
                return original.cell < inventory.items.size() && inventory.items[original.cell] == item;
            }) : InventorySystem::Unowned(item);
    };
    if (IsRideItem(item) && MountSystem::IsRiding(owner) && MountSystem::GetMountVnum(owner) &&
        GetWearItem(owner, WEAR_COSTUME_MOUNT) == entt::null)
        MountSystem::ForceClearRidingState(owner);
    if (!sourceUnchanged()) return false;
    if (IsRideItem(item) && MountSystem::IsRiding(owner)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 532, "");
#endif
        return false;
    }
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
    if (wear == WEAR_WEAPON) {
        const auto costume = GetWearItem(owner, WEAR_COSTUME_WEAPON);
        if (costume != entt::null && !IsValidItem(costume)) return false;
        if (costume != entt::null && (GetItemType(item) != ITEM_WEAPON || GetItemValue(costume, 3) != GetItemSubType(item))) {
            if (!UnequipAction(owner, costume) || !sourceUnchanged()) return false;
        }
    } else if (wear == WEAR_COSTUME_WEAPON) {
        const auto weapon = GetWearItem(owner, WEAR_WEAPON);
        if (!IsValidItem(weapon) || GetItemType(weapon) != ITEM_WEAPON || GetItemValue(item, 3) != GetItemSubType(weapon)) {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 694, "");
#endif
            return false;
        }
    }
#endif
    if (!sourceUnchanged()) return false;
    const auto displaced = GetWearItem(owner, wear);
    if (displaced != entt::null) {
        if (!InventorySystem::CanHandleItems(owner)) return false;
        if (!IsValidItem(displaced) || IsDragonSoulItem(item) || !WornBy(owner, displaced) ||
            !InventorySystem::CanUnequipNow(owner, displaced, false) || GetItemWearFlag(item) == WEARABLE_ABILITY)
            return false;
        if (!carried || !FitsReplacement(owner, item, displaced, original)) return false;
        // Both identities and the full-width source cell are retained across
        // effect/save callbacks. Rollback only touches still-detached originals.
        if (InventorySystem::RemoveFromCharacter(item) == entt::null || !InventorySystem::Detached(item)) return false;
        if (!WornBy(owner, displaced) || GetItemCell(displaced) != INVENTORY_MAX_NUM + wear ||
            !InventorySystem::CanUnequipNow(owner, displaced, false) ||
            InventorySystem::RemoveFromCharacter(displaced) == entt::null || !InventorySystem::Detached(displaced)) {
            RestoreDetached(owner, item, original); return false;
        }
        if (!InventorySystem::EquipTo(item, owner, wear) || !WornBy(owner, item) || GetItemCell(item) != INVENTORY_MAX_NUM + wear) {
            RestoreDetached(owner, displaced, TItemPos(EQUIPMENT, INVENTORY_MAX_NUM + wear));
            RestoreDetached(owner, item, original); return false;
        }
        if (!CarryDetached(owner, displaced, original)) {
            // A callback may have occupied the planned return cell. Undo only
            // this operation's still-worn replacement, never another item that
            // a callback moved into the equipment slot or to another owner.
            if (WornBy(owner, item) && GetItemCell(item) == INVENTORY_MAX_NUM + wear)
                InventorySystem::RemoveFromCharacter(item);
            RestoreDetached(owner, displaced, TItemPos(EQUIPMENT, INVENTORY_MAX_NUM + wear));
            RestoreDetached(owner, item, original);
            return false;
        }
    } else {
        if (!InventorySystem::EquipTo(item, owner, wear)) { RestoreDetached(owner, item, original); return false; }
        if (!WornBy(owner, item) || GetItemCell(item) != INVENTORY_MAX_NUM + wear) return true;
    }
    if (carried && original.window_type == INVENTORY)
        InventorySystem::SyncQuickslot(owner, QUICKSLOT_TYPE_ITEM, original.cell, wear);
    if (WornBy(owner, item)) FinishEquip(owner, item);
    return true;
}

namespace {
void FinishEquip(entt::entity owner, entt::entity item) {
    if (!WornBy(owner, item)) return;
    const uint16_t wearCell = GetItemCell(item);
    const auto unchanged = [&] { return WornBy(owner, item) && GetItemCell(item) == wearCell; };
    const auto* proto = GetItemProto(item);
    if (!proto) return;
    const int firstUse = proto->cLimitRealTimeFirstUseIndex;
    if (firstUse >= 0 && firstUse < ITEM_LIMIT_MAX_NUM) {
        const int64_t configuredDuration = proto->aLimits[firstUse].lValue;
        if (GetItemSocket(item, 1) == 0) {
            int64_t duration = GetItemSocket(item, 0);
            if (duration == 0) duration = configuredDuration;
            if (duration <= 0) duration = 60 * 60 * 24 * 7;
            const int64_t expires = std::clamp<int64_t>(int64_t(time(nullptr)) + duration, 0, UINT32_MAX);
            if (!SetItemSocket(item, 0, static_cast<uint32_t>(expires)) || !unchanged()) return;
            StartRealTimeExpireEventEcs(item);
            if (!unchanged()) return;
        }
        const uint32_t used = GetItemSocket(item, 1);
        if (!SetItemSocket(item, 1, used == UINT32_MAX ? used : used + 1) || !unchanged()) return;
    }
    const uint32_t vnum = GetItemVnum(item);
    const auto type = GetItemType(item), subtype = GetItemSubType(item);
    int effect = -1;
    if (CItemVnumHelper::IsRamadanMoonRing(vnum)) effect = SE_EQUIP_RAMADAN_RING;
    else if (CItemVnumHelper::IsHalloweenCandy(vnum)) effect = SE_EQUIP_HALLOWEEN_CANDY;
    else if (CItemVnumHelper::IsHappinessRing(vnum)) effect = SE_EQUIP_HAPPINESS_RING;
    else if (CItemVnumHelper::IsLovePendant(vnum)) effect = SE_EQUIP_LOVE_PENDANT;
    else if (type == ITEM_UNIQUE && GetItemSIGVnum(item)) {
        if (const auto* group = ITEM_MANAGER::instance().GetSpecialItemGroup(GetItemSIGVnum(item)))
            if (const auto* attributes = ITEM_MANAGER::instance().GetSpecialAttrGroup(group->GetAttrVnum(vnum))) {
                const std::string filename = attributes->m_stEffectFileName;
                NetworkSyncSystem::BroadcastSpecificEffect(g_registry, owner, filename.c_str());
            }
    }
#ifdef ENABLE_ACCE_SYSTEM
    else if (type == ITEM_COSTUME && subtype == COSTUME_ACCE) effect = SE_EFFECT_ACCE_EQUIP;
#endif
#ifdef ENABLE_STOLE_COSTUME
    else if (type == ITEM_COSTUME && subtype == COSTUME_STOLE) effect = SE_EFFECT_ACCE_EQUIP;
#endif
#ifdef ENABLE_TALISMAN_EFFECT
    else if (vnum >= 9600 && vnum <= 9800) effect = SE_EFFECT_TALISMAN_EQUIP_FIRE;
    else if (vnum >= 9830 && vnum <= 10030) effect = SE_EFFECT_TALISMAN_EQUIP_ICE;
    else if (vnum >= 10520 && vnum <= 10720) effect = SE_EFFECT_TALISMAN_EQUIP_WIND;
    else if (vnum >= 10060 && vnum <= 10260) effect = SE_EFFECT_TALISMAN_EQUIP_EARTH;
    else if (vnum >= 10290 && vnum <= 10490) effect = SE_EFFECT_TALISMAN_EQUIP_DARK;
    else if (vnum >= 10750 && vnum <= 10950) effect = SE_EFFECT_TALISMAN_EQUIP_ELEC;
#endif
    if (!unchanged()) return;
    if (effect >= 0) NetworkSyncSystem::BroadcastEffect(g_registry, owner, static_cast<uint8_t>(effect));
    if (!unchanged()) return;
    const bool questUse = type == ITEM_UNIQUE &&
        (subtype == UNIQUE_SPECIAL_RIDE || subtype == UNIQUE_SPECIAL_MOUNT_RIDE) &&
        (GetItemFlags(item) & ITEM_FLAG_QUEST_USE);
    if (questUse
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
        || (type == ITEM_COSTUME && subtype == COSTUME_MOUNT)
#endif
    ) quest::CQuestManager::instance().UseItem(ecs::PlayerRuntime::GetPlayerID(owner), item, false);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (!unchanged()) return;
    if (type == ITEM_COSTUME && subtype == COSTUME_MOUNT)
        if (const auto* refs = g_registry.try_get<ecs::MountRuntimeRefs>(owner); refs && refs->mountSystem)
            refs->mountSystem->Mount(GetItemValue(item, 1), item);
#endif
}
} // namespace

static int FindEmptyMainInventoryPosition(entt::entity owner, uint8_t itemSize)
{
    if (owner == entt::null || !g_registry.valid(owner) || itemSize == 0)
        return -1;

    const auto* inventory = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner);
    if (!inventory)
        return -1;

    const int inventoryLimit = InventorySystem::GetInventorySize(owner);

    constexpr int pageSize = INVENTORY_PAGE_SIZE;
    for (int cell = 0; cell < inventoryLimit; ++cell)
    {
        if (inventory->itemGrid[cell] != 0 || inventory->items[cell] != entt::null)
            continue;

        const int page = cell / pageSize;
        bool fits = true;
        for (int row = 1; row < itemSize; ++row)
        {
            const int occupiedCell = cell + (INVENTORY_PAGE_COLUMN * row);
            if (occupiedCell >= inventoryLimit || occupiedCell / pageSize != page ||
                inventory->itemGrid[occupiedCell] != 0 || inventory->items[occupiedCell] != entt::null)
            {
                fits = false;
                break;
            }
        }
        if (fits)
            return cell;
    }
    return -1;
}

#ifdef ENABLE_EXTRA_INVENTORY
static int FindEmptyExtraInventoryPosition(entt::entity owner, uint8_t itemSize,
                                           uint8_t category)
{
    if (owner == entt::null || !g_registry.valid(owner) || itemSize == 0 || category >= 6)
        return -1;

    const auto* inventory = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(owner);
    if (!inventory)
        return -1;

    const int begin = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category;
    int end = EXTRA_INVENTORY_CATEGORY_MAX_NUM * (category + 1);
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
    static constexpr std::array<std::string_view, 6> unlockFlags {
        "lock_extra.cat1", "lock_extra.cat2", "lock_extra.cat3",
        "lock_extra.cat4", "lock_extra.cat5", "lock_extra.cat6"
    };
    constexpr int freeSlots = (EXTRA_INVENTORY_PAGE_SIZE * 2) + 20;
    constexpr int maxUnlockSlots = 25 + EXTRA_INVENTORY_PAGE_SIZE;
    const int unlockedSlots = static_cast<int>(std::clamp(
        int64_t(ecs::QuestSystem::GetFlag(owner, unlockFlags[category])) * 5,
        int64_t(0), int64_t(maxUnlockSlots)));
    end = std::min(begin + freeSlots + unlockedSlots,
                   static_cast<int>(EXTRA_INVENTORY_MAX_NUM));
#endif

    for (int cell = begin; cell < end; ++cell)
    {
        if (inventory->itemGrid[cell] != 0 || inventory->items[cell] != entt::null)
            continue;

        const int page = cell / EXTRA_INVENTORY_PAGE_SIZE;
        bool fits = true;
        for (int row = 1; row < itemSize; ++row)
        {
            const int occupiedCell = cell + (EXTRA_INVENTORY_PAGE_COLUMN * row);
            if (occupiedCell >= end ||
                occupiedCell / EXTRA_INVENTORY_PAGE_SIZE != page ||
                inventory->itemGrid[occupiedCell] != 0 || inventory->items[occupiedCell] != entt::null)
            {
                fits = false;
                break;
            }
        }

        if (fits)
            return cell;
    }

    return -1;
}
#endif

#ifdef ENABLE_EXTRA_INVENTORY
int GetEmptyExtraInventory(entt::entity owner, entt::entity item)
{
    if (!IsValidItem(item) || !IsExtraItem(item)) return -1;
    return FindEmptyExtraInventoryPosition(owner, GetItemSize(item), GetItemExtraCategory(item));
}
#endif

int GetEmptyInventoryPositionEcs(entt::entity owner, entt::entity item)
{
    if (owner == entt::null || !g_registry.valid(owner) || !IsValidItem(item))
        return -1;

    if (IsDragonSoulItem(item))
        return GetEmptyDragonSoulInventory(owner, item);
#ifdef ENABLE_EXTRA_INVENTORY
    if (IsExtraItem(item))
        return FindEmptyExtraInventoryPosition(
            owner, GetItemSize(item), GetItemExtraCategory(item));
#endif
    return FindEmptyMainInventoryPosition(owner, GetItemSize(item));
}

bool HasMainInventorySpaceEcs(entt::entity owner, uint8_t itemSize)
{
    return FindEmptyMainInventoryPosition(owner, itemSize) != -1;
}

bool HasInventorySpaceForItemVnum(entt::entity owner, uint32_t itemVnum)
{
    const TItemTable* proto = ITEM_MANAGER::instance().GetTable(itemVnum);
    if (!proto)
        return false;

    return FindEmptyMainInventoryPosition(owner, proto->bSize) != -1;
}

int GetEmptyDragonSoulInventory(entt::entity owner, entt::entity item)
{
    if (owner == entt::null || !g_registry.valid(owner) || !IsValidItem(item) || !IsDragonSoulItem(item))
        return -1;

    const auto* inventory = g_registry.try_get<ecs::DragonSoulInventoryComponent>(owner);
    if (!inventory)
        return -1;

    const uint8_t itemSize = GetItemSize(item);
    const uint16_t baseCell = DSManager::instance().GetBasePosition(item);
    if (itemSize == 0 || baseCell == WORD_MAX ||
        baseCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
        return -1;

    const int boxEnd = std::min<int>(baseCell + DRAGON_SOUL_BOX_SIZE,
                                     DRAGON_SOUL_INVENTORY_MAX_NUM);
    for (int cell = baseCell; cell < boxEnd; ++cell)
    {
        if (inventory->itemGrid[cell] != 0 || inventory->items[cell] != entt::null)
            continue;

        bool fits = true;
        for (int row = 1; row < itemSize; ++row)
        {
            const int occupiedCell = cell + (DRAGON_SOUL_BOX_COLUMN_NUM * row);
            if (occupiedCell >= boxEnd || inventory->itemGrid[occupiedCell] != 0 || inventory->items[occupiedCell] != entt::null)
            {
                fits = false;
                break;
            }
        }

        if (fits)
            return cell;
    }

    return -1;
}

bool PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell)
{
    return InventorySystem::InsertInventoryItem(item, owner, TItemPos(window, cell), true);
}

bool RemoveItemEcs(entt::entity item)
{
    return InventorySystem::RemoveFromCharacter(item) != entt::null;
}


void ModifyPoints(entt::entity itemEntity, bool bAdd)
{
	const entt::entity ownerEntity = ItemSystem::GetItemOwner(itemEntity);
#ifdef ENABLE_BUG_FIXES
	if (ownerEntity == entt::null) {
		return;
	}
#endif

	const TItemTable* proto = ItemSystem::GetItemProto(itemEntity);
	int accessoryGrade;

	if (false == ItemSystem::IsAccessoryForSocket(itemEntity))
	{
		if (proto->bType == ITEM_WEAPON || proto->bType == ITEM_ARMOR)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				uint32_t dwVnum;

				if ((dwVnum = ItemSystem::GetItemSocket(itemEntity, i)) <= 2)
					continue;

				TItemTable* p = ITEM_MANAGER::instance().GetTable(dwVnum);

				if (!p)
				{
					LOG_ERROR("cannot find table by vnum {}", dwVnum);
					continue;
				}

				if (ITEM_METIN == p->bType)
				{
					for (auto& aApplie : p->aApplies)
					{
						if (aApplie.bType == APPLY_NONE)
							continue;

						if (aApplie.bType == APPLY_SKILL)
							ecs::PointSystem::ApplyPoint(ownerEntity, aApplie.bType, bAdd ? aApplie.lValue : aApplie.lValue ^ 0x00800000);
						else
							ecs::PointSystem::ApplyPoint(ownerEntity, aApplie.bType, bAdd ? aApplie.lValue : -aApplie.lValue);
					}
				}
			}
		}

		accessoryGrade = 0;
	}
	else
	{
		accessoryGrade = MIN(ItemSystem::GetItemAccessorySocketGrade(itemEntity), ITEM_ACCESSORY_SOCKET_MAX_NUM);
	}


#ifdef ENABLE_ACCE_SYSTEM
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_ACCE) && (ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORBED_SOCKET)))
	{
		TItemTable* pkItemAbsorbed = ITEM_MANAGER::instance().GetTable(ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORBED_SOCKET));
		if (pkItemAbsorbed)
		{
			/* 			if ((pkItemAbsorbed->bType == ITEM_ARMOR) && (pkItemAbsorbed->bSubType == ARMOR_BODY))
						{
							int32_t lDefGrade = pkItemAbsorbed->alValues[1] + int32_t(pkItemAbsorbed->alValues[5] * 2);
							double dValue = lDefGrade * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
							dValue = (double)dValue / 100;
							dValue = (double)dValue + .5;
							lDefGrade = (int32_t) dValue;
							if ((pkItemAbsorbed->alValues[1] > 0 && (lDefGrade <= 0)) || (pkItemAbsorbed->alValues[5] > 0 && (lDefGrade < 1)))
								lDefGrade += 1;
							else if ((pkItemAbsorbed->alValues[1] > 0) || (pkItemAbsorbed->alValues[5] > 0))
								lDefGrade += 1;

							ecs::PointSystem::ApplyPoint(ownerEntity, APPLY_DEF_GRADE_BONUS, bAdd ? lDefGrade : -lDefGrade);

							int32_t lDefMagicBonus = pkItemAbsorbed->alValues[0];
							dValue = lDefMagicBonus * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
							dValue = (double)dValue / 100;
							dValue = (double)dValue + .5;
							lDefMagicBonus = (int32_t) dValue;
							if ((pkItemAbsorbed->alValues[0] > 0) && (lDefMagicBonus < 1))
								lDefMagicBonus += 1;
							else if (pkItemAbsorbed->alValues[0] > 0)
								lDefMagicBonus += 1;

							ecs::PointSystem::ApplyPoint(ownerEntity, APPLY_MAGIC_DEF_GRADE, bAdd ? lDefMagicBonus : -lDefMagicBonus);
						} */
			/* else  */if (pkItemAbsorbed->bType == ITEM_WEAPON)
			{
				int32_t lAttGrade = pkItemAbsorbed->alValues[4] + pkItemAbsorbed->alValues[5];
				if (pkItemAbsorbed->alValues[3] > pkItemAbsorbed->alValues[4])
					lAttGrade = pkItemAbsorbed->alValues[3] + pkItemAbsorbed->alValues[5];

				double dValue = lAttGrade * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				lAttGrade = (int32_t)dValue;
				if (((pkItemAbsorbed->alValues[3] > 0) && (lAttGrade < 1)) || ((pkItemAbsorbed->alValues[4] > 0) && (lAttGrade < 1)))
					lAttGrade += 1;
				else if ((pkItemAbsorbed->alValues[3] > 0) || (pkItemAbsorbed->alValues[4] > 0))
					lAttGrade += 1;

				ecs::PointSystem::ApplyPoint(ownerEntity, APPLY_ATT_GRADE_BONUS, bAdd ? lAttGrade : -lAttGrade);

				int32_t lAttMagicGrade = pkItemAbsorbed->alValues[2] + pkItemAbsorbed->alValues[5];
				if (pkItemAbsorbed->alValues[1] > pkItemAbsorbed->alValues[2])
					lAttMagicGrade = pkItemAbsorbed->alValues[1] + pkItemAbsorbed->alValues[5];

				dValue = lAttMagicGrade * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				lAttMagicGrade = (int32_t)dValue;
				if (((pkItemAbsorbed->alValues[1] > 0) && (lAttMagicGrade < 1)) || ((pkItemAbsorbed->alValues[2] > 0) && (lAttMagicGrade < 1)))
					lAttMagicGrade += 1;
				else if ((pkItemAbsorbed->alValues[1] > 0) || (pkItemAbsorbed->alValues[2] > 0))
					lAttMagicGrade += 1;

				ecs::PointSystem::ApplyPoint(ownerEntity, APPLY_MAGIC_ATT_GRADE, bAdd ? lAttMagicGrade : -lAttMagicGrade);
			}
		}
	}
#endif


	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
#ifdef ENABLE_ACCE_SYSTEM
		if ((proto->aApplies[i].bType == APPLY_NONE) && (ItemSystem::GetItemType(itemEntity) != ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) != COSTUME_ACCE))
#else
		if (proto->aApplies[i].bType == APPLY_NONE)
#endif
			continue;

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (ItemSystem::IsMountItem(itemEntity))
			continue;
#endif

		int32_t value = proto->aApplies[i].lValue;
#ifdef ENABLE_ACCE_SYSTEM
		if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_ACCE))
		{
			TItemTable* pkItemAbsorbed = ITEM_MANAGER::instance().GetTable(ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORBED_SOCKET));
			if (pkItemAbsorbed)
			{
				if (pkItemAbsorbed->aApplies[i].bType == APPLY_NONE)
					continue;

				value = pkItemAbsorbed->aApplies[i].lValue;
				if (value < 0)
					continue;

				double dValue = value * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				value = (int32_t)dValue;
				if ((pkItemAbsorbed->aApplies[i].lValue > 0) && (value <= 0))
					value += 1;
			}
			else
				continue;
		}
#endif
		if (proto->aApplies[i].bType == APPLY_SKILL)
		{
			ecs::PointSystem::ApplyPoint(ownerEntity, proto->aApplies[i].bType, bAdd ? value : value ^ 0x00800000);
		}
		else
		{
			if (0 != accessoryGrade)
				value += MAX(accessoryGrade, value * aiAccessorySocketEffectivePct[accessoryGrade] / 100);

			ecs::PointSystem::ApplyPoint(ownerEntity, proto->aApplies[i].bType, bAdd ? value : -value);
		}
	}

#ifdef ENABLE_ITEM_EXTRA_PROTO
	if (ItemSystem::GetItemExtraProto(itemEntity) != nullptr)
	{
#ifdef ENABLE_NEW_EXTRA_BONUS
		for (int i = 0; i < NEW_EXTRA_BONUS_COUNT; i++)
		{
			auto type = ItemSystem::GetItemExtraProto(itemEntity)->ExtraBonus[i].bType;
			if (type != APPLY_NONE) {
				auto value = ItemSystem::GetItemExtraProto(itemEntity)->ExtraBonus[i].lValue;
				ecs::PointSystem::ApplyPoint(ownerEntity, ItemSystem::GetItemExtraProto(itemEntity)->ExtraBonus[i].bType, bAdd ? value : -value);
			}
		}
#endif
	}
#endif

	if (true == CItemVnumHelper::IsRamadanMoonRing(ItemSystem::GetItemVnum(itemEntity)) || true == CItemVnumHelper::IsHalloweenCandy(ItemSystem::GetItemVnum(itemEntity))
		|| true == CItemVnumHelper::IsHappinessRing(ItemSystem::GetItemVnum(itemEntity)) || true == CItemVnumHelper::IsLovePendant(ItemSystem::GetItemVnum(itemEntity)))
	{
		// Do not anything.
	}
	else
	{
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			if (ItemSystem::GetItemAttributeType(itemEntity, i))
			{
				const TPlayerItemAttribute& ia = ItemSystem::GetItemAttribute(itemEntity, i);
				int32_t sValue = ia.sValue;
#ifdef ENABLE_ACCE_SYSTEM
				if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_ACCE)) {
					double dValue = sValue * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
					dValue = dValue / 100;
					dValue = dValue + .5;
					sValue = (int32_t)dValue;
					if ((ia.sValue > 0) && (sValue <= 0))
						sValue += 1;
				}
#endif

#ifdef ATTR_LOCK
				if (ItemSystem::GetItemLockedAttr(itemEntity) == i) {
					continue;
				}
#endif

				if (ia.bType == APPLY_SKILL)
					ecs::PointSystem::ApplyPoint(ownerEntity, ia.bType, bAdd ? sValue : sValue ^ 0x00800000);
				else
					ecs::PointSystem::ApplyPoint(ownerEntity, ia.bType, bAdd ? sValue : -sValue);
			}
		}
	}

	switch (proto->bType)
	{
	case ITEM_PICK:
	case ITEM_ROD:
	{
		if (bAdd)
		{
			if (ItemSystem::GetItemCell(itemEntity) == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, ItemSystem::GetItemVnum(itemEntity));
		}
		else
		{
			if (ItemSystem::GetItemCell(itemEntity) == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_WEAPON:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if ((ItemSystem::GetItemSubType(itemEntity) == WEAPON_SWORD) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_DAGGER) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_BOW) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_TWO_HANDED) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_BELL) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_FAN)) {
			const entt::entity item = ItemSystem::GetWearItem(
				ownerEntity, WEAR_COSTUME_EFFECT_WEAPON);
			if (ItemSystem::IsValidItem(item)) {
				uint32_t toSetValueEffect;
				switch (ItemSystem::GetItemSubType(itemEntity)) {
				case WEAPON_SWORD:
					toSetValueEffect = ItemSystem::GetItemValue(item, 0);
					break;
				case WEAPON_DAGGER:
					toSetValueEffect = ItemSystem::GetItemValue(item, 2);
					break;
				case WEAPON_BOW:
					toSetValueEffect = ItemSystem::GetItemValue(item, 3);
					break;
				case WEAPON_TWO_HANDED:
					toSetValueEffect = ItemSystem::GetItemValue(item, 1);
					break;
				case WEAPON_BELL:
					toSetValueEffect = ItemSystem::GetItemValue(item, 4);
					break;
				case WEAPON_FAN:
					toSetValueEffect = ItemSystem::GetItemValue(item, 5);
					break;
				default:
					toSetValueEffect = 0;
					break;
				}

				if (toSetValueEffect > 0) {
					uint32_t dwWeaponVnum = ItemSystem::GetItemVnum(itemEntity);
					if (((dwWeaponVnum >= 1180) && (dwWeaponVnum <= 1189)) ||
						((dwWeaponVnum >= 1090) && (dwWeaponVnum <= 1099)) ||
						(dwWeaponVnum == 1199) ||
						(dwWeaponVnum == 1209) ||
						(dwWeaponVnum == 1219) ||
						(dwWeaponVnum == 1229) ||
						(dwWeaponVnum == 40099) ||
						((dwWeaponVnum >= 7190) && (dwWeaponVnum <= 7199))
						)
						toSetValueEffect += 500;
				}

				if (!bAdd)
					toSetValueEffect = 0;

				ecs::PlayerRuntime::SetPart(ownerEntity, PART_EFFECT_WEAPON, toSetValueEffect);
			}
		}
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_WEAPON)))
			break;
#endif

		if (bAdd)
		{
			if (ItemSystem::GetItemCell(itemEntity) == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, ItemSystem::GetItemVnum(itemEntity));
		}
		else
		{
			if (ItemSystem::GetItemCell(itemEntity) == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_ARMOR:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if (ItemSystem::GetItemSubType(itemEntity) == ARMOR_BODY) {
			const entt::entity item = ItemSystem::GetWearItem(
				ownerEntity, WEAR_COSTUME_EFFECT_BODY);
			if (ItemSystem::IsValidItem(item)) {
				uint32_t toSetValueEffect;
				toSetValueEffect = ItemSystem::GetItemValue(item, 0);
				if ((!bAdd) &&
					!ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_BODY)))
					toSetValueEffect = 0;

				ecs::PlayerRuntime::SetPart(ownerEntity, PART_EFFECT_BODY, toSetValueEffect);
			}
		}
#endif

		if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_BODY)))
			break;

		if (ItemSystem::GetItemSubType(itemEntity) == ARMOR_BODY || ItemSystem::GetItemSubType(itemEntity) == ARMOR_HEAD || ItemSystem::GetItemSubType(itemEntity) == ARMOR_FOOTS || ItemSystem::GetItemSubType(itemEntity) == ARMOR_SHIELD)
		{
			if (bAdd)
			{
				if (ItemSystem::GetItemProto(itemEntity)->bSubType == ARMOR_BODY)
					ecs::PlayerRuntime::SetPart(ownerEntity, PART_MAIN, ItemSystem::GetItemVnum(itemEntity));
			}
			else
			{
				if (ItemSystem::GetItemProto(itemEntity)->bSubType == ARMOR_BODY)
					ecs::PlayerRuntime::SetPart(ownerEntity, PART_MAIN, ecs::PlayerRuntime::GetOriginalPart(ownerEntity, PART_MAIN));
			}
		}
	}
	break;

	case ITEM_COSTUME:
	{
		uint32_t toSetValue = ItemSystem::GetItemVnum(itemEntity);
		EParts toSetPart = PART_MAX_NUM;

		if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_BODY)
		{
#ifdef ENABLE_COSTUME_EFFECT
			const entt::entity item = ItemSystem::GetWearItem(
				ownerEntity, WEAR_COSTUME_EFFECT_BODY);
			if (ItemSystem::IsValidItem(item)) {
				uint32_t toSetValueEffect;
				toSetValueEffect = ItemSystem::GetItemValue(item, 0);
				if ((!bAdd) &&
					!ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, WEAR_BODY)))
					toSetValueEffect = 0;

				ecs::PlayerRuntime::SetPart(ownerEntity, PART_EFFECT_BODY, toSetValueEffect);
			}
#endif
			toSetPart = PART_MAIN;

			if (false == bAdd)
			{
				const entt::entity armor = ItemSystem::GetWearItem(ownerEntity, WEAR_BODY);
				toSetValue = ItemSystem::IsValidItem(armor)
					? ItemSystem::GetItemVnum(armor)
					: ecs::PlayerRuntime::GetOriginalPart(ownerEntity, PART_MAIN);
			}
		}
#ifdef ENABLE_RUNE_SYSTEM
		else if (ItemSystem::GetItemSubType(itemEntity) == RUNE_SLOT7)
		{
			toSetPart = PART_RUNE;
			toSetValue = (true == bAdd) ? ecs::PlayerRuntime::GetRuneEffect(ownerEntity) : 0;
		}
#endif
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_HAIR)
		{
			toSetPart = PART_HAIR;
			toSetValue = (true == bAdd) ? ItemSystem::GetItemValue(itemEntity, 3) : 0;
		}

#ifdef ENABLE_ACCE_SYSTEM
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_ACCE)
		{
			toSetValue -= 85000;
			if (ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
				toSetValue += 1000;

			toSetValue = (bAdd == true) ? toSetValue : 0;

#ifdef ENABLE_STOLE_COSTUME
			const entt::entity acce = ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_ACCE);
			if (ItemSystem::IsValidItem(acce)) {
				toSetValue = ItemSystem::GetItemVnum(acce);
				toSetValue -= 85000;
				toSetValue += 1000;
			}
#endif

			toSetPart = PART_ACCE;
		}
#endif
#ifdef ENABLE_STOLE_COSTUME
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_STOLE)
		{
			toSetValue -= 85000;
			if (!bAdd) {
				const entt::entity acce = ItemSystem::GetWearItem(
					ownerEntity, WEAR_COSTUME_ACCE_SLOT);
				if (!ItemSystem::IsValidItem(acce))
					toSetValue = 0;
				else {
					toSetValue = ItemSystem::GetItemVnum(acce);
					toSetValue -= 85000;
					if (ItemSystem::GetItemSocket(acce, ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
						toSetValue += 1000;
				}
			}
			else
				toSetValue += 1000;

			toSetPart = PART_ACCE;
		}
#endif
#ifdef ENABLE_COSTUME_EFFECT
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_EFFECT_BODY)
		{
			if (bAdd) {
				entt::entity item = ItemSystem::GetWearItem(ownerEntity, WEAR_BODY);
				toSetValue = ItemSystem::IsValidItem(item) ? ItemSystem::GetItemValue(itemEntity, 0) : 0;
				if (toSetValue == 0) {
					item = ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_BODY);
					toSetValue = ItemSystem::IsValidItem(item) ? ItemSystem::GetItemValue(itemEntity, 0) : 0;
				}
			}
			else
				toSetValue = 0;

			toSetPart = PART_EFFECT_BODY;
		}
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_EFFECT_WEAPON)
		{
			if (bAdd) {
				const entt::entity item = ItemSystem::GetWearItem(ownerEntity, WEAR_WEAPON);
				if (ItemSystem::IsValidItem(item)) {
					switch (ItemSystem::GetItemSubType(item)) {
					case WEAPON_SWORD:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 0);
						break;
					case WEAPON_DAGGER:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 2);
						break;
					case WEAPON_BOW:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 3);
						break;
					case WEAPON_TWO_HANDED:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 1);
						break;
					case WEAPON_BELL:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 4);
						break;
					case WEAPON_FAN:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 5);
						break;
					default:
						toSetValue = 0;
						break;
					}

					if (toSetValue > 0) {
					uint32_t dwWeaponVnum = ItemSystem::GetItemVnum(item);
						if (((dwWeaponVnum >= 1180) && (dwWeaponVnum <= 1189)) ||
							((dwWeaponVnum >= 1090) && (dwWeaponVnum <= 1099)) ||
							(dwWeaponVnum == 1199) ||
							(dwWeaponVnum == 1209) ||
							(dwWeaponVnum == 1219) ||
							(dwWeaponVnum == 1229) ||
							(dwWeaponVnum == 40099) ||
							((dwWeaponVnum >= 7190) && (dwWeaponVnum <= 7199))
							)
							toSetValue += 500;
					}
				}
				else
					toSetValue = 0;
			}
			else
				toSetValue = 0;

			toSetPart = PART_EFFECT_WEAPON;
		}
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_MOUNT)
		{
			// not need to do a thing in here
		}
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_WEAPON)
		{
			toSetPart = PART_WEAPON;
			if (false == bAdd)
			{
				const entt::entity weapon = ItemSystem::GetWearItem(ownerEntity, WEAR_WEAPON);
				if (ItemSystem::IsValidItem(weapon)) {
					toSetValue = ItemSystem::GetItemVnum(weapon);
				}
				else {
					toSetValue = 0;
				}
			}
		}
#endif

		if (PART_MAX_NUM != toSetPart)
		{
			ecs::PlayerRuntime::SetPart(ownerEntity, (uint8_t)toSetPart, toSetValue);
			NetworkSyncSystem::UpdatePacket(ownerEntity);

		}
	}
	break;
	case ITEM_UNIQUE:
	{
		if (0 != ItemSystem::GetItemSIGVnum(itemEntity))
		{
			const CSpecialItemGroup* pItemGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(ItemSystem::GetItemSIGVnum(itemEntity));
			if (nullptr == pItemGroup)
				break;
			uint32_t dwAttrVnum = pItemGroup->GetAttrVnum(ItemSystem::GetItemVnum(itemEntity));
			const CSpecialAttrGroup* pAttrGroup = ITEM_MANAGER::instance().GetSpecialAttrGroup(dwAttrVnum);
			if (nullptr == pAttrGroup)
				break;
			for (auto it = pAttrGroup->m_vecAttrs.begin(); it != pAttrGroup->m_vecAttrs.end(); ++it)
			{
				ecs::PointSystem::ApplyPoint(ownerEntity, it->apply_type, bAdd ? it->apply_value : it->apply_value); // -it->apply_value
			}
		}
	}
	break;
	}
}

} // namespace ItemSystem

void CItem::ModifyPoints(bool bAdd)
{
	ItemSystem::ModifyPoints(GetEntityHandle(), bAdd);
}
