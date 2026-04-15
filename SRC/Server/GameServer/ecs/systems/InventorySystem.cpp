#include "../../stdafx.h"

#include "InventorySystem.hpp"

#include "../../char.h"
#include "../../desc.h"
#include "../../item.h"
#include "../../packet.h"
#include "../AIHelpers.hpp"
#include "../components/dirty_components.hpp"

namespace
{

ecs::QuickSlots* GetQuickSlots(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	return &g_registry.get_or_emplace<ecs::QuickSlots>(e);
}

} // namespace

namespace InventorySystem {

void SyncQuickslot(entt::entity e, uint8_t bType, uint8_t bOldPos, uint8_t bNewPos)
{
	if (bOldPos == bNewPos)
		return;

	auto* qs = GetQuickSlots(e);
	if (!qs)
		return;

	for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (qs->slots[i].type == bType && qs->slots[i].pos == bOldPos)
		{
			if (bNewPos == 255)
				memset(&qs->slots[i], 0, sizeof(TQuickslot));
			else
			{
				qs->slots[i].type = bType;
				qs->slots[i].pos = bNewPos;
			}
		}
	}

	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

bool GetQuickslot(entt::entity e, uint8_t pos, TQuickslot& out)
{
	auto* qs = GetQuickSlots(e);
	if (!qs || pos >= QUICKSLOT_MAX_NUM)
		return false;

	out = qs->slots[pos];
	return true;
}

void SetQuickslot(entt::entity e, uint8_t pos, const TQuickslot& slot)
{
	auto* qs = GetQuickSlots(e);
	if (!qs || pos >= QUICKSLOT_MAX_NUM)
		return;

	qs->slots[pos] = slot;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void DelQuickslot(entt::entity e, uint8_t pos)
{
	auto* qs = GetQuickSlots(e);
	if (!qs || pos >= QUICKSLOT_MAX_NUM)
		return;

	memset(&qs->slots[pos], 0, sizeof(TQuickslot));
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void SwapQuickslot(entt::entity e, uint8_t posA, uint8_t posB)
{
	auto* qs = GetQuickSlots(e);
	if (!qs || posA >= QUICKSLOT_MAX_NUM || posB >= QUICKSLOT_MAX_NUM)
		return;

	const TQuickslot slot = qs->slots[posA];
	qs->slots[posA] = qs->slots[posB];
	qs->slots[posB] = slot;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

} // namespace InventorySystem

void CHARACTER::SyncQuickslot(uint8_t bType, uint8_t bOldPos, uint8_t bNewPos)
{
	if (bOldPos == bNewPos)
		return;

	for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (m_quickslot[i].type == bType && m_quickslot[i].pos == bOldPos)
		{
			if (bNewPos == 255)
				DelQuickslot(i);
			else
			{
				TQuickslot slot;
				slot.type = bType;
				slot.pos = bNewPos;

				SetQuickslot(i, slot);
			}
		}
	}

	InventorySystem::SyncQuickslot(AIHelpers::EcsOf(this), bType, bOldPos, bNewPos);
}

bool CHARACTER::GetQuickslot(uint8_t pos, TQuickslot** ppSlot)
{
	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	*ppSlot = &m_quickslot[pos];

	TQuickslot ecsSlot {};
	if (InventorySystem::GetQuickslot(AIHelpers::EcsOf(this), pos, ecsSlot))
		m_quickslot[pos] = ecsSlot;
	else
		InventorySystem::SetQuickslot(AIHelpers::EcsOf(this), pos, m_quickslot[pos]);

	return true;
}

bool CHARACTER::SetQuickslot(uint8_t pos, TQuickslot& rSlot)
{
	packet_quickslot_add pack_quickslot_add;

	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	if (rSlot.type >= QUICKSLOT_TYPE_MAX_NUM)
		return false;

	for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (rSlot.type == 0)
			continue;
		else if (m_quickslot[i].type == rSlot.type && m_quickslot[i].pos == rSlot.pos)
			DelQuickslot(i);
	}

#ifdef ENABLE_EXTRA_INVENTORY
	uint8_t type = rSlot.type == QUICKSLOT_TYPE_ITEM_EXTRA ? EXTRA_INVENTORY : INVENTORY;
	TItemPos srcCell(type, rSlot.pos);
#else
	TItemPos srcCell(INVENTORY, rSlot.pos);
#endif

	switch (rSlot.type)
	{
#ifdef ENABLE_EXTRA_INVENTORY
		case QUICKSLOT_TYPE_ITEM_EXTRA:
			{
				if (rSlot.pos >= EXTRA_INVENTORY_MAX_NUM)
					return false;

				break;
			}
#endif

		case QUICKSLOT_TYPE_ITEM:
			{
				if (false == srcCell.IsDefaultInventoryPosition() && false == srcCell.IsBeltInventoryPosition())
					return false;
			}
			break;

		case QUICKSLOT_TYPE_SKILL:
			if (rSlot.pos >= SKILL_MAX_NUM)
				return false;
			break;

		case QUICKSLOT_TYPE_COMMAND:
			break;

		default:
			return false;
	}

	m_quickslot[pos] = rSlot;
	InventorySystem::SetQuickslot(AIHelpers::EcsOf(this), pos, rSlot);

	if (GetDesc())
	{
		pack_quickslot_add.header = HEADER_GC_QUICKSLOT_ADD;
		pack_quickslot_add.pos = pos;
		pack_quickslot_add.slot = m_quickslot[pos];

		GetDesc()->Packet(&pack_quickslot_add, sizeof(pack_quickslot_add));
	}

	return true;
}

bool CHARACTER::DelQuickslot(uint8_t pos)
{
	packet_quickslot_del pack_quickslot_del;

	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	memset(&m_quickslot[pos], 0, sizeof(TQuickslot));
	InventorySystem::DelQuickslot(AIHelpers::EcsOf(this), pos);

	pack_quickslot_del.header = HEADER_GC_QUICKSLOT_DEL;
	pack_quickslot_del.pos = pos;

	if (GetDesc())
		GetDesc()->Packet(&pack_quickslot_del, sizeof(pack_quickslot_del));

	return true;
}

bool CHARACTER::SwapQuickslot(uint8_t a, uint8_t b)
{
	packet_quickslot_swap pack_quickslot_swap;

	if (a >= QUICKSLOT_MAX_NUM || b >= QUICKSLOT_MAX_NUM)
		return false;

	const TQuickslot quickslot = m_quickslot[a];

	m_quickslot[a] = m_quickslot[b];
	m_quickslot[b] = quickslot;
	InventorySystem::SwapQuickslot(AIHelpers::EcsOf(this), a, b);

	pack_quickslot_swap.header = HEADER_GC_QUICKSLOT_SWAP;
	pack_quickslot_swap.pos = a;
	pack_quickslot_swap.pos_to = b;

	if (GetDesc())
		GetDesc()->Packet(&pack_quickslot_swap, sizeof(pack_quickslot_swap));

	return true;
}

void CHARACTER::ChainQuickslotItem(LPITEM pItem, uint8_t bType, uint8_t bOldPos)
{
	if (pItem->IsDragonSoul())
		return;

	for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (m_quickslot[i].type == bType && m_quickslot[i].pos == bOldPos)
		{
			TQuickslot slot;
			slot.type = bType;
			slot.pos = pItem->GetCell();

			SetQuickslot(i, slot);
			break;
		}
	}
}
