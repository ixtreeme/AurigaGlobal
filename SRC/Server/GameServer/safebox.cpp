#include "stdafx.h"
#include <utility>
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Base/grid.h>
#include "constants.h"
#include "safebox.h"
#include "packet.h"
#include "ecs/Registry.hpp"
#include "ecs/components/inventory_components.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "desc_client.h"
#include "config.h"

namespace {
constexpr int GridWidth = 16;
constexpr int MaxHeight = SAFEBOX_MAX_NUM / GridWidth;

struct ClosingStorageGuard {
    entt::entity owner;
    uint8_t window;
    ~ClosingStorageGuard() {
        if (!g_registry.valid(owner)) return;
        if (auto* refs = g_registry.try_get<ecs::SafeboxRef>(owner))
            (window == SAFEBOX ? refs->closingSafebox : refs->closingMall) = false;
    }
};
}

namespace SafeboxSystem {
std::shared_ptr<CSafebox> Get(entt::entity owner, uint8_t window)
{
    if (!g_registry.valid(owner) || (window != SAFEBOX && window != MALL)) return {};
    const auto* refs = g_registry.try_get<ecs::SafeboxRef>(owner);
    return refs ? (window == SAFEBOX ? refs->safebox : refs->mall) : nullptr;
}

std::shared_ptr<CSafebox> Open(entt::entity owner, uint8_t window, int height, uint32_t gold)
{
    if (!ecs::PlayerRuntime::IsPC(owner) || (window != SAFEBOX && window != MALL)) return {};
    auto& refs = g_registry.get_or_emplace<ecs::SafeboxRef>(owner);
    if (window == SAFEBOX ? refs.closingSafebox : refs.closingMall) return {};
    auto& slot = window == SAFEBOX ? refs.safebox : refs.mall;
    if (!slot) {
        auto storage = std::make_shared<CSafebox>(owner, height, gold);
        storage->SetWindowMode(window);
        slot = std::move(storage);
    }
    return slot;
}

void Close(entt::entity owner, uint8_t window, bool save)
{
    if (!g_registry.valid(owner) || (window != SAFEBOX && window != MALL)) return;
    auto* refs = g_registry.try_get<ecs::SafeboxRef>(owner);
    if (!refs) return;
    bool& closing = window == SAFEBOX ? refs->closingSafebox : refs->closingMall;
    if (closing) return;
    closing = true;
    ClosingStorageGuard guard {owner, window};
    // A caller may hold a lease through a callback. Unpublish before saving,
    // then retire contents immediately, without waiting for the last lease.
    auto storage = std::exchange(window == SAFEBOX ? refs->safebox : refs->mall, {});
    if (!storage) return;
    if (save) storage->Save();
    storage->Close();
}
}

CSafebox::CSafebox(entt::entity owner, int iSize, uint32_t dwGold)
	: m_owner(owner), m_iSize(0), m_lGold(dwGold), m_bWindowMode(SAFEBOX)
{
	m_items.fill(entt::null);
	ChangeSize(iSize);
}

CSafebox::~CSafebox()
{
	__Destroy();
}

void CSafebox::SetWindowMode(uint8_t bMode)
{
	if (m_destroying || (bMode != SAFEBOX && bMode != MALL) ||
		std::any_of(m_items.begin(), m_items.end(), [](entt::entity item) { return item != entt::null; }))
		return;
	m_bWindowMode = bMode;
}

bool CSafebox::OwnsItem(entt::entity item, uint32_t cell) const
{
	if (!ItemSystem::IsValidItem(item))
		return false;
	const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
	const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
	// Compare the stored versioned owner even during teardown after logout.
	// GetItemOwner intentionally hides invalid owners and cannot do this check.
	return owner && owner->owner == m_owner && location &&
		location->window == m_bWindowMode && location->cell == cell;
}

bool CSafebox::FitsGrid(uint32_t cell, uint8_t size) const
{
	return m_pkGrid && cell < m_items.size() && cell < m_pkGrid->GetSize() &&
		size > 0 && size <= m_iSize - static_cast<int>(cell / GridWidth);
}

void CSafebox::__Destroy()
{
	if (m_destroying)
		return;
	m_destroying = true;
	const auto items = m_items;
	m_items.fill(entt::null);
	m_pkGrid.reset();
	// Unpublish the entire container before flush/detach/destruction callbacks.
	for (size_t cell = 0; cell < items.size(); ++cell)
	{
		const auto item = items[cell];
		if (!OwnsItem(item, static_cast<uint32_t>(cell)))
			continue;

		const bool previousSkipSave = ItemSystem::GetItemSkipSave(item);
		ItemSystem::SetItemSkipSave(item, true);
		ItemSystem::FlushDelayedSaveEcs(item);
		if (!OwnsItem(item, static_cast<uint32_t>(cell)))
		{
			if (ItemSystem::IsValidItem(item))
				ItemSystem::SetItemSkipSave(item, previousSkipSave);
			continue;
		}
		const bool removed = ItemSystem::RemoveItemEcs(item);
		if (!ItemSystem::IsValidItem(item))
			continue;
		const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
		if (!removed || !owner || owner->owner != entt::null || ItemSystem::GetItemWindow(item) != RESERVED_WINDOW)
		{
			ItemSystem::SetItemSkipSave(item, previousSkipSave);
			LOG_ERROR("SAFEBOX: item {} changed ownership during teardown; not destroying", ItemSystem::GetItemID(item));
			continue;
		}
		ItemSystem::DestroyItemEntityEcs(item, "SAFEBOX_DESTRUCT");
	}
}

bool CSafebox::Add(uint32_t dwPos, entt::entity item)
{
	const entt::entity chrOwner = m_owner;
	if (m_destroying || !ecs::PlayerRuntime::IsPC(chrOwner) || !IsValidPosition(dwPos) || !ItemSystem::IsValidItem(item))
	{
		LOG_ERROR("SAFEBOX: item on wrong position at {}", dwPos);
		return false;
	}
	const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
	const uint8_t size = ItemSystem::GetItemSize(item);
	if ((owner && owner->owner != entt::null) ||
		std::find(m_items.begin(), m_items.end(), item) != m_items.end() || !IsEmpty(dwPos, size))
		return false;

	if (!m_pkGrid->Put(dwPos, 1, size))
		return false;
	const auto oldOwner = owner ? *owner : ecs::ItemOwner {};
	const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
	const auto oldLocation = location ? *location : ecs::ItemLocation {RESERVED_WINDOW, 0};
	ItemSystem::SetItemWindow(item, m_bWindowMode);
	ItemSystem::SetItemCell(item, chrOwner, dwPos);
	m_items[dwPos] = item;
	const bool saved = ItemSystem::SaveItemEcs(item);
	if (!saved || m_destroying || !OwnsItem(item, dwPos) || !ecs::PlayerRuntime::IsPC(chrOwner))
	{
		// Never restore over a moved/destroyed entity or a replacement slot.
		if (m_items[dwPos] == item)
		{
			m_items[dwPos] = entt::null;
			if (m_pkGrid) m_pkGrid->Get(dwPos, 1, size);
		}
		if (OwnsItem(item, dwPos))
		{
			g_registry.get<ecs::ItemOwner>(item) = oldOwner;
			g_registry.get<ecs::ItemLocation>(item) = oldLocation;
		}
		return false;
	}

	TPacketGCItemSet pack{};
	pack.header = m_bWindowMode == SAFEBOX ? HEADER_GC_SAFEBOX_SET : HEADER_GC_MALL_SET;
	pack.Cell = TItemPos(m_bWindowMode, dwPos);
	pack.vnum = ItemSystem::GetItemVnum(item);
	pack.count = ItemSystem::GetItemCount(item);
	pack.flags = ItemSystem::GetItemFlags(item);
#ifdef ATTR_LOCK
	pack.lockedattr = ItemSystem::GetItemLockedAttributeIndex(item);
#endif
	pack.anti_flags = ItemSystem::GetItemAntiFlags(item);
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		pack.alSockets[i] = ItemSystem::GetItemSocket(item, i);
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		pack.aAttr[i] = ItemSystem::GetItemAttribute(item, i);

	if (LPDESC desc = ecs::PlayerRuntime::GetDesc(chrOwner))
		desc->Packet(&pack, sizeof(pack));
	LOG_INFO("SAFEBOX: ADD {} {} count {}", ecs::PlayerRuntime::GetName(chrOwner).data(), ItemSystem::GetItemName(item), ItemSystem::GetItemCount(item));
	return true;
}

entt::entity CSafebox::Get(uint32_t dwPos) const
{
	if (m_destroying || !m_pkGrid || dwPos >= m_items.size() || dwPos >= m_pkGrid->GetSize())
		return entt::null;

	const entt::entity item = m_items[dwPos];
	return OwnsItem(item, dwPos) ? item : entt::null;
}

entt::entity CSafebox::Remove(uint32_t dwPos)
{
	const entt::entity chrOwner = m_owner;
	if (m_destroying || !ecs::PlayerRuntime::IsPC(chrOwner))
		return entt::null;
	const entt::entity item = Get(dwPos);
	if (!ItemSystem::IsValidItem(item))
		return entt::null;

	const uint8_t size = ItemSystem::GetItemSize(item);
	if (!FitsGrid(dwPos, size))
		return entt::null;
	m_pkGrid->Get(dwPos, 1, size);

	m_items[dwPos] = entt::null;
	if (!ItemSystem::RemoveItemEcs(item))
	{
		if (!m_destroying && OwnsItem(item, dwPos) && m_items[dwPos] == entt::null && m_pkGrid->Put(dwPos, 1, size))
			m_items[dwPos] = item;
		return entt::null;
	}
	if (m_destroying || !ecs::PlayerRuntime::IsPC(chrOwner) || m_items[dwPos] != entt::null)
		return entt::null;

	TPacketGCItemDel pack{};
	pack.header = m_bWindowMode == SAFEBOX ? HEADER_GC_SAFEBOX_DEL : HEADER_GC_MALL_DEL;
	pack.pos = dwPos;
	if (LPDESC desc = ecs::PlayerRuntime::GetDesc(chrOwner))
		desc->Packet(&pack, sizeof(pack));
	if (!ItemSystem::IsValidItem(item))
		return entt::null;
	const auto* detachedOwner = g_registry.try_get<ecs::ItemOwner>(item);
	if (!detachedOwner || detachedOwner->owner != entt::null || ItemSystem::GetItemWindow(item) != RESERVED_WINDOW)
		return entt::null;
	LOG_INFO("SAFEBOX: REMOVE {} {} count {}", ecs::PlayerRuntime::GetName(chrOwner).data(), ItemSystem::GetItemName(item), ItemSystem::GetItemCount(item));
	return item;
}

void CSafebox::Save()
{
	const entt::entity chrOwner = m_owner;
	if (m_destroying || !ecs::PlayerRuntime::IsPC(chrOwner) || !db_clientdesc)
		return;
	TSafeboxTable t {};
	t.dwID = ecs::PlayerRuntime::GetAccountID(chrOwner);
	if (!t.dwID)
		return;
	t.dwGold = m_lGold;

	db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_SAVE, 0, &t, sizeof(TSafeboxTable));
	LOG_INFO("SAFEBOX: SAVE {}", ecs::PlayerRuntime::GetName(chrOwner).data());
}

bool CSafebox::IsEmpty(uint32_t dwPos, uint8_t bSize)
{
	if (m_destroying || !FitsGrid(dwPos, bSize))
		return false;

	return m_pkGrid->IsEmpty(dwPos, 1, bSize);
}

void CSafebox::ChangeSize(int iSize)
{
	if (m_destroying || iSize <= m_iSize || iSize > MaxHeight)
		return;
	// Allocate/copy first; the old grid remains owned if allocation fails.
	auto grid = m_pkGrid ? std::make_unique<CGrid>(m_pkGrid.get(), GridWidth, iSize)
		: std::make_unique<CGrid>(GridWidth, iSize);
	m_pkGrid = std::move(grid);
	m_iSize = iSize;
}

entt::entity CSafebox::GetItem(uint32_t bCell) const
{
	if (bCell >= m_items.size() || bCell >= static_cast<uint32_t>(GridWidth * m_iSize))
	{
		LOG_ERROR("CHARACTER::GetItem: invalid item cell {}", bCell);
		return entt::null;
	}

	return Get(bCell);
}

bool CSafebox::MoveItem(uint32_t bCell, uint32_t bDestCell, uint32_t count)
{
	const entt::entity chrOwner = m_owner;
	if (m_destroying || !ecs::PlayerRuntime::IsPC(chrOwner) ||
		!IsValidPosition(bCell) || !IsValidPosition(bDestCell))
		return false;

	const entt::entity item = GetItem(bCell);
	if (!ItemSystem::IsValidItem(item) || ItemSystem::IsItemExchanging(item) || ItemSystem::IsItemLocked(item))
		return false;

	const uint32_t sourceCount = ItemSystem::GetItemCount(item);
	if (sourceCount < count)
		return false;

	const entt::entity destination = GetItem(bDestCell);
	if (ItemSystem::IsValidItem(destination) &&
		(ItemSystem::IsItemExchanging(destination) || ItemSystem::IsItemLocked(destination)))
		return false;
	if (ItemSystem::IsValidItem(destination) && destination != item &&
		(ItemSystem::GetItemFlags(destination) & ITEM_FLAG_STACKABLE) &&
		!(ItemSystem::GetItemAntiFlags(destination) & ITEM_ANTIFLAG_STACK) &&
		ItemSystem::GetItemVnum(destination) == ItemSystem::GetItemVnum(item))
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			if (ItemSystem::GetItemSocket(destination, i) != ItemSystem::GetItemSocket(item, i))
				return false;

		if (count == 0)
			count = sourceCount;

		const uint32_t destinationCount = ItemSystem::GetItemCount(destination);
		if (g_bItemCountLimit <= 0 || destinationCount >= static_cast<uint32_t>(g_bItemCountLimit))
			return false;
		count = std::min(static_cast<uint32_t>(g_bItemCountLimit) - destinationCount, count);
		if (count == 0)
			return false;

		if (count >= sourceCount && Remove(bCell) == entt::null)
			return false;

		if (!ItemSystem::ConsumeItemEcs(item, count))
		{
			if (count >= sourceCount && ItemSystem::IsValidItem(item) && !Add(bCell, item))
				LOG_ERROR("SAFEBOX: failed to restore source {} after rejected consumption", ItemSystem::GetItemID(item));
			return false;
		}
		if (!ItemSystem::AddItemCountEcs(destination, count))
		{
			LOG_ERROR("SAFEBOX: destination credit failed after source debit at {} -> {}", bCell, bDestCell);
			return false;
		}
		LOG_INFO("SAFEBOX: STACK {} {} -> {} {} count {}", ecs::PlayerRuntime::GetName(chrOwner).data(), static_cast<int>(bCell), static_cast<int>(bDestCell), ItemSystem::GetItemName(destination), ItemSystem::GetItemCount(destination));
		return true;
	}

	if (!IsEmpty(bDestCell, ItemSystem::GetItemSize(item)))
		return false;

	LOG_INFO("SAFEBOX: MOVE {} {} -> {} {} count {}", ecs::PlayerRuntime::GetName(chrOwner).data(), static_cast<int>(bCell), static_cast<int>(bDestCell), ItemSystem::GetItemName(item), sourceCount);
	if (Remove(bCell) == entt::null)
		return false;
	return Add(bDestCell, item);
}

bool CSafebox::IsValidPosition(uint32_t dwPos)
{
	if (m_destroying || !m_pkGrid || dwPos >= m_items.size())
		return false;

	if (dwPos >= m_pkGrid->GetSize())
		return false;

	return true;
}
