#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include <Base/grid.h>
#include "constants.h"
#include "utils.h"
#include "config.h"
#include "shop.h"
#include "desc.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "log.h"
#include "db.h"
#include "questmanager.h"
#include "mob_manager.h"
#include "locale_service.h"
#include "battle_pass.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"

namespace
{
LPITEM ResolveShopItem(entt::entity item)
{
	if (!ItemSystem::IsValidItem(item))
		return nullptr;
	return ITEM_MANAGER::instance().Find(ItemSystem::GetItemID(item));
}
}
//#define ENABLE_SHOP_BLACKLIST
/* ------------------------------------------------------------------------------------ */
CShop::CShop()
	: m_dwVnum(0), m_dwNPCVnum(0), m_pkPC(nullptr)
{
#ifdef ENABLE_120_SHOP_SLOT_RAZOR93
	m_pGrid = M2_NEW CGrid(15, 9);
#else
	m_pGrid = M2_NEW CGrid(5, 9);
#endif
}

CShop::~CShop()
{
	TPacketGCShop pack;

	pack.header		= HEADER_GC_SHOP;
	pack.subheader	= SHOP_SUBHEADER_GC_END;
	pack.size		= sizeof(TPacketGCShop);

	Broadcast(&pack, sizeof(pack));

	GuestMapType::iterator it;

	it = m_map_guest.begin();

	while (it != m_map_guest.end())
	{
		LPCHARACTER ch = it->first;
		ch->SetShop(nullptr);
		++it;
	}

	M2_DELETE(m_pGrid);
}

void CShop::SetPCShop(LPCHARACTER ch)
{
	m_pkPC = ch;
}

bool CShop::Create(uint32_t dwVnum, uint32_t dwNPCVnum, TShopItemTable * pTable)
{
	/*
	   if (NULL == CMobManager::instance().Get(dwNPCVnum))
	   {
	   LOG_ERROR("No such a npc by vnum {}", dwNPCVnum);
	   return false;
	   }
	 */
	LOG_TRACE("SHOP #{} (Shopkeeper {})", dwVnum, dwNPCVnum);

	m_dwVnum = dwVnum;
	m_dwNPCVnum = dwNPCVnum;

	uint8_t bItemCount;

	for (bItemCount = 0; bItemCount < SHOP_HOST_ITEM_MAX_NUM; ++bItemCount)
		if (0 == (pTable + bItemCount)->vnum)
			break;

	SetShopItems(pTable, bItemCount);
	return true;
}

void CShop::SetShopItems(TShopItemTable * pTable, uint8_t bItemCount)
{
	if (bItemCount > SHOP_HOST_ITEM_MAX_NUM)
		return;

	m_pGrid->Clear();

	m_itemVector.assign(SHOP_HOST_ITEM_MAX_NUM, SHOP_ITEM{});

	for (int i = 0; i < bItemCount; ++i)
	{
		LPITEM pkItem = nullptr;
		entt::entity pkItemEntity = entt::null;
		const TItemTable * item_table;

		if (m_pkPC)
		{
			pkItem = m_pkPC->GetItem(pTable->pos);

			if (!pkItem)
			{
				LOG_ERROR("cannot find item on pos ({}, {}) (name: {})", static_cast<int>(pTable->pos.window_type), pTable->pos.cell, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(m_pkPC)).data());
				continue;
			}

			pkItemEntity = EntityFactory::CreateItemEntity(g_registry, pkItem);
			item_table = ItemSystem::GetItemProto(pkItemEntity);
		}
		else
		{
			if (!pTable->vnum)
				continue;

			item_table = ITEM_MANAGER::instance().GetTable(pTable->vnum);
		}

		if (!item_table)
		{
			LOG_ERROR("Shop: no item table by item vnum #{}", pTable->vnum);
			continue;
		}

		int iPos;

		if (IsPCShop())
		{
			LOG_INFO("MyShop: use position {}", static_cast<int>(pTable->display_pos));
			iPos = pTable->display_pos;
		}
		else
			iPos = m_pGrid->FindBlank(1, item_table->bSize);

		if (iPos < 0)
		{
			LOG_ERROR("not enough shop window");
			continue;
		}

		if (!m_pGrid->IsEmpty(iPos, 1, item_table->bSize))
		{
			if (IsPCShop())
			{
				LOG_ERROR("not empty position for pc shop {}[{}]", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(m_pkPC)).data(), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(m_pkPC)));
			}
			else
			{
				LOG_ERROR("not empty position for npc shop");
			}
			continue;
		}

		m_pGrid->Put(iPos, 1, item_table->bSize);

		SHOP_ITEM & item = m_itemVector[iPos];

		item.pkItem = pkItemEntity;
		item.itemid = 0;

		if (ItemSystem::IsValidItem(item.pkItem))
		{
			item.vnum = ItemSystem::GetItemVnum(item.pkItem);
			item.count = ItemSystem::GetItemCount(item.pkItem); // PC 샵의 경우 아이템 개수는 진짜 아이템의 개수여야 한다.
#ifndef ENABLE_BUY_WITH_ITEM
			item.price = pTable->price;
#endif
			item.itemid	= ItemSystem::GetItemID(item.pkItem);
		}
		else
		{
			item.vnum = pTable->vnum;
			item.count = pTable->count;
#ifndef ENABLE_BUY_WITH_ITEM
			if (IS_SET(item_table->dwFlags, ITEM_FLAG_COUNT_PER_1GOLD))
			{
				if (item_table->dwGold == 0)
					item.price = item.count;
				else
					item.price = item.count / item_table->dwGold;
			}
			else
				item.price = item_table->dwGold * item.count;
#endif
		}

#ifdef ENABLE_BUY_WITH_ITEM
		item.price = pTable->price;
		for (int i = 0; i < MAX_SHOP_PRICES; i++) {
			item.itemprice[i].vnum = pTable->itemprice[i].vnum;
			item.itemprice[i].count = pTable->itemprice[i].count;
		}
#endif

		char name[256];
		snprintf(name, sizeof(name), "%s (v: %d) (c: %d)", item_table->szName, item.vnum, item.count);
		LOG_TRACE("SHOP_ITEM: ITEM: {} PRICE: {}", name, item.price);
		++pTable;
	}
}

int64_t CShop::Buy(LPCHARACTER ch, uint8_t pos
#ifdef ENABLE_BUY_STACK_FROM_SHOP
, bool multiple
#endif
)

{
#ifdef ENABLE_BUY_STACK_FROM_SHOP
	bool ismultiple = multiple;
#else
	bool ismultiple = false;
#endif

	if (!ismultiple) {
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
		if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
			return SHOP_SUBHEADER_GC_OK;
		}
#endif

#ifdef ENABLE_LIMIT_BUY_SPEED
		int iPulse = thecore_pulse() - ch->GetLastBuyTime();
		if (iPulse < PASSES_PER_SEC(1)) {
			return SHOP_SUBHEADER_GC_OK;
		}
#endif
	}
#ifdef ENABLE_BUY_STACK_FROM_SHOP
	else
	{
		if (m_pkPC)
		{
			return SHOP_SUBHEADER_GC_OK;
		}
	}
#endif

	if (pos >= m_itemVector.size())
	{
		LOG_INFO("Shop::Buy : invalid position {} : {}", static_cast<int>(pos), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		return SHOP_SUBHEADER_GC_INVALID_POS;
	}

	GuestMapType::iterator it = m_map_guest.find(ch);
	if (it == m_map_guest.end()) {
		return SHOP_SUBHEADER_GC_END;
	}

	SHOP_ITEM& r_item = m_itemVector[pos];
	if (!ismultiple) {
		const entt::entity selectedItem = r_item.pkItem;

		if (IsPCShop()) {
			if (selectedItem == entt::null || !ItemSystem::IsValidItem(selectedItem)) {
				LOG_INFO("Shop::Buy : Critical: This user seems to be a hacker : invalid pcshop item : BuyerPID:{} SellerPID:{}", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(m_pkPC)));
				return SHOP_SUBHEADER_GC_SOLD_OUT;
			} else if (ItemSystem::GetItemOwner(selectedItem) != AIHelpers::EcsOf(m_pkPC)) {
				LOG_INFO("Shop::Buy : Critical: This user seems to be a hacker : invalid pcshop item : BuyerPID:{} SellerPID:{}", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(m_pkPC)));
				return SHOP_SUBHEADER_GC_SOLD_OUT;
			}
		}
	}


	int64_t dwPrice = r_item.price;

	if (ecs::PointSystem::GetGold(AIHelpers::EcsOf(ch)) < dwPrice)
	{
		return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
	}

#ifdef ENABLE_BUY_WITH_ITEM
	uint32_t dwPriceVnum = 0, dwPriceCount = 0, dwHaveCount = 0;
	for (int i = 0; i < MAX_SHOP_PRICES; i++) {
		dwPriceVnum = r_item.itemprice[i].vnum;
		if (dwPriceVnum > 0) {
			dwPriceCount = r_item.itemprice[i].count;
			dwHaveCount = ch->CountSpecifyItem(dwPriceVnum);
			if (dwHaveCount < dwPriceCount) {
				LOG_INFO("Shop::Buy : Not enough item : {} has {}, price {}.", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), dwHaveCount, dwPriceCount);
				return SHOP_SUBHEADER_GC_NOT_ENOUGH_ITEM;
			}
		}
	}
#endif

	LPITEM item = m_pkPC ? ResolveShopItem(r_item.pkItem) : ITEM_MANAGER::instance().CreateItem(r_item.vnum, r_item.count, 0, true);
	if (!item) {
		return SHOP_SUBHEADER_GC_SOLD_OUT;
	}
	const entt::entity itemEntity = EntityFactory::CreateItemEntity(g_registry, item);
	if (!ItemSystem::IsValidItem(itemEntity))
		return SHOP_SUBHEADER_GC_SOLD_OUT;

#ifdef ENABLE_SHOP_BLACKLIST
	if (!m_pkPC)
	{
		if (quest::CQuestManager::instance().GetEventFlag("hivalue_item_sell") == 0)
		{
			//축복의 구슬 && 만년한철 이벤트
			if (ItemSystem::GetItemVnum(itemEntity) == 70024 || ItemSystem::GetItemVnum(itemEntity) == 70035)
			{
				return SHOP_SUBHEADER_GC_END;
			}
		}
	}
#endif

	int iEmptyPos;
	if (item->IsDragonSoul())
	{
		iEmptyPos = ch->GetEmptyDragonSoulInventory(item);
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (item->IsExtraItem())
	{
		iEmptyPos = ch->GetEmptyExtraInventory(item);
	}
#endif
	else
	{
		iEmptyPos = ch->GetEmptyInventory(item->GetSize());
	}

	if (iEmptyPos < 0)
	{
		if (m_pkPC)
		{
			LOG_INFO("Shop::Buy at PC Shop : Inventory full : {} size {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), item->GetSize());
			return SHOP_SUBHEADER_GC_INVENTORY_FULL;
		}
		else
		{
			LOG_INFO("Shop::Buy : Inventory full : {} size {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), item->GetSize());
			ItemSystem::DestroyItemEntityEcs(
				itemEntity,
				"SHOP_TRANSACTION");
			return SHOP_SUBHEADER_GC_INVENTORY_FULL;
		}
	}

	if (dwPrice > 0) {
		ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GOLD, -dwPrice, false);
	}

#ifdef ENABLE_BUY_WITH_ITEM
	for (int i = 0; i < MAX_SHOP_PRICES; i++) {
		dwPriceVnum = r_item.itemprice[i].vnum;
		if (dwPriceVnum > 0) {
			dwPriceCount = r_item.itemprice[i].count;
			if (dwPriceCount > 0) {
				ch->RemoveSpecifyItem(dwPriceVnum, r_item.itemprice[i].count);
			}
		}
	}
#endif

	if (!ismultiple) {
		uint32_t dwTax = 0;
		int iVal = 0;

		{
			iVal = quest::CQuestManager::instance().GetEventFlag("personal_shop");

			if (0 < iVal)
			{
				if (iVal > 100)
					iVal = 100;

				dwTax = dwPrice * iVal / 100;
				dwPrice = dwPrice - dwTax;
			}
			else
			{
				iVal = 0;
				dwTax = 0;
			}
		}
	}

	// 군주 시스템 : 세금 징수
	if (m_pkPC)
	{
#ifdef ENABLE_EXTRA_INVENTORY
		if (item->IsExtraItem()) {
			m_pkPC->SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, ItemSystem::GetItemCell(itemEntity), 255);
		} else {
			m_pkPC->SyncQuickslot(QUICKSLOT_TYPE_ITEM, ItemSystem::GetItemCell(itemEntity), 255);
		}
#else
		m_pkPC->SyncQuickslot(QUICKSLOT_TYPE_ITEM, ItemSystem::GetItemCell(itemEntity), 255);
#endif

		if (ItemSystem::GetItemVnum(itemEntity) == 90008 || ItemSystem::GetItemVnum(itemEntity) == 90009) // VCARD
		{
			VCardUse(m_pkPC, ch, item);
			item = nullptr;
		}
		else
		{
			char buf[512];

			if (ItemSystem::GetItemVnum(itemEntity) >= 80003 && ItemSystem::GetItemVnum(itemEntity) <= 80007)
			{
				snprintf(buf, sizeof(buf), "%s FROM: %u TO: %u PRICE: %lld", item->GetName(), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(m_pkPC)), dwPrice);
				LogManager::instance().GoldBarLog(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ItemSystem::GetItemID(itemEntity), SHOP_BUY, buf);
				LogManager::instance().GoldBarLog(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(m_pkPC)), ItemSystem::GetItemID(itemEntity), SHOP_SELL, buf);
			}

			item->RemoveFromCharacter();

			if (item->IsDragonSoul()) {
				item->AddToCharacter(ch, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyPos));
			}
#ifdef ENABLE_EXTRA_INVENTORY
			else if (item->IsExtraItem()) {
#ifdef ENABLE_25082021
				if (item->IsStackable() && !IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
					int
#else
					uint8_t
#endif
					bCount = ItemSystem::GetItemCount(itemEntity);
					for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i) {
						const entt::entity item2 = ItemSystem::GetExtraInventoryItem(AIHelpers::EcsOf(ch), i);
						if (!ItemSystem::IsValidItem(item2))
							continue;

						if (ItemSystem::GetItemVnum(item2) == ItemSystem::GetItemVnum(itemEntity)) {
							int j = 0;
							for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
								if (ItemSystem::GetItemSocket(item2, j) != ItemSystem::GetItemSocket(itemEntity, j))
									break;

							if (j != ITEM_SOCKET_MAX_NUM)
								continue;

#ifdef ENABLE_NEW_STACK_LIMIT
							int
#else
							uint8_t
#endif
							bCount2 = MIN(g_bItemCountLimit - ItemSystem::GetItemCount(item2), bCount);
							bCount -= bCount2;

							ItemSystem::AddItemCountEcs(
								item2,
								bCount2);
							if (bCount == 0) {
								ItemSystem::DestroyItemEntityEcs(
				itemEntity,
				"SHOP_TRANSACTION");
								item = nullptr;
								break;
							}
						}
					}

					if (item != nullptr) {
						ItemSystem::SetItemCountEcs(
						itemEntity,
						bCount);
						item->AddToCharacter(ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
					}
				} else {
					item->AddToCharacter(ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
				}
#else
				item->AddToCharacter(ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
#endif
			}
#endif
			else {
#ifdef ENABLE_25082021
				if (item->IsStackable() && !IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
					int
#else
					uint8_t
#endif
					bCount = ItemSystem::GetItemCount(itemEntity);
					for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
						const entt::entity item2 = ItemSystem::GetInventoryItem(AIHelpers::EcsOf(ch), i);
						if (!ItemSystem::IsValidItem(item2))
							continue;

						if (ItemSystem::GetItemVnum(item2) == ItemSystem::GetItemVnum(itemEntity)) {
							int j = 0;
							for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
								if (ItemSystem::GetItemSocket(item2, j) != ItemSystem::GetItemSocket(itemEntity, j))
									break;

							if (j != ITEM_SOCKET_MAX_NUM)
								continue;

#ifdef ENABLE_NEW_STACK_LIMIT
							int
#else
							uint8_t
#endif
							bCount2 = MIN(g_bItemCountLimit - ItemSystem::GetItemCount(item2), bCount);
							bCount -= bCount2;

							ItemSystem::AddItemCountEcs(
								item2,
								bCount2);
							if (bCount == 0) {
								ItemSystem::DestroyItemEntityEcs(
				itemEntity,
				"SHOP_TRANSACTION");
								item = nullptr;
								break;
							}
						}
					}

					if (item != nullptr) {
						ItemSystem::SetItemCountEcs(
						itemEntity,
						bCount);
						item->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
					}
				} else {
					item->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
				}
#else
				item->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
#endif
			}

			if (item != nullptr) {
				ITEM_MANAGER::instance().FlushDelayedSave(item);
			}
		}

		r_item.pkItem = entt::null;
		BroadcastUpdateItem(pos);

		ecs::PointSystem::Change(AIHelpers::EcsOf(m_pkPC), POINT_GOLD, dwPrice, false);
	}
	else
	{
		if (item->IsDragonSoul()) {
			item->AddToCharacter(ch, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyPos));
		}
#ifdef ENABLE_EXTRA_INVENTORY
		else if (item->IsExtraItem()) {
#ifdef ENABLE_25082021
			if (item->IsStackable() && !IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
				bCount = ItemSystem::GetItemCount(itemEntity);
				for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i) {
					const entt::entity item2 = ItemSystem::GetExtraInventoryItem(AIHelpers::EcsOf(ch), i);
					if (!ItemSystem::IsValidItem(item2))
						continue;

					if (ItemSystem::GetItemVnum(item2) == ItemSystem::GetItemVnum(itemEntity)) {
						int j = 0;
						for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
							if (ItemSystem::GetItemSocket(item2, j) != ItemSystem::GetItemSocket(itemEntity, j))
								break;

						if (j != ITEM_SOCKET_MAX_NUM)
							continue;

#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
						bCount2 = MIN(g_bItemCountLimit - ItemSystem::GetItemCount(item2), bCount);
						bCount -= bCount2;

						ItemSystem::AddItemCountEcs(
								item2,
								bCount2);
						if (bCount == 0) {
							ItemSystem::DestroyItemEntityEcs(
				itemEntity,
				"SHOP_TRANSACTION");
							item = nullptr;
							break;
						}
					}
				}

				if (item != nullptr) {
					ItemSystem::SetItemCountEcs(
						itemEntity,
						bCount);
					item->AddToCharacter(ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
				}
			} else {
				item->AddToCharacter(ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
			}
#else
			item->AddToCharacter(ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
#endif
		}
#endif
		else {
#ifdef ENABLE_25082021
			if (item->IsStackable() && !IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
				bCount = ItemSystem::GetItemCount(itemEntity);
				for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
					const entt::entity item2 = ItemSystem::GetInventoryItem(AIHelpers::EcsOf(ch), i);
					if (!ItemSystem::IsValidItem(item2))
						continue;

					if (ItemSystem::GetItemVnum(item2) == ItemSystem::GetItemVnum(itemEntity)) {
						int j = 0;
						for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
							if (ItemSystem::GetItemSocket(item2, j) != ItemSystem::GetItemSocket(itemEntity, j))
								break;

						if (j != ITEM_SOCKET_MAX_NUM)
							continue;

#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
						bCount2 = MIN(g_bItemCountLimit - ItemSystem::GetItemCount(item2), bCount);
						bCount -= bCount2;

						ItemSystem::AddItemCountEcs(
								item2,
								bCount2);
						if (bCount == 0) {
							ItemSystem::DestroyItemEntityEcs(
				itemEntity,
				"SHOP_TRANSACTION");
							item = nullptr;
							break;
						}
					}
				}

				if (item != nullptr) {
					ItemSystem::SetItemCountEcs(
						itemEntity,
						bCount);
					item->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
				}
			} else {
				item->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
			}
#else
			item->AddToCharacter(ch, TItemPos(INVENTORY, iEmptyPos));
#endif
		}

		if (item != nullptr) {
			ITEM_MANAGER::instance().FlushDelayedSave(item);
		}
	}

#ifdef ENABLE_BATTLE_PASS
	{
		uint8_t bBattlePassId = ch->GetBattlePassId();
		if(bBattlePassId)
		{
			uint32_t dwYangCount, dwNotUsed;
			if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, SPENT_YANG, &dwNotUsed, &dwYangCount))
			{
				if(ch->GetMissionProgress(SPENT_YANG, bBattlePassId) < dwYangCount)
					ch->UpdateMissionProgress(SPENT_YANG, bBattlePassId, dwPrice, dwYangCount);
			}
		}
	}
#endif

	if (!ismultiple) {
#ifdef ENABLE_LIMIT_BUY_SPEED
		ch->SetLastBuyTime();
#endif
		ch->Save();
	}

	return (SHOP_SUBHEADER_GC_OK);
}

#ifdef ENABLE_BUY_STACK_FROM_SHOP
uint8_t CShop::MultipleBuy(LPCHARACTER ch, uint8_t p, uint8_t c) {
	if (p < 0 || c <= 0 || c > MULTIPLE_BUY_LIMIT) {
		return SHOP_SUBHEADER_GC_OK;
	}

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) < GM_IMPLEMENTOR) {
		return SHOP_SUBHEADER_GC_OK;
	}
#endif

#ifdef ENABLE_LIMIT_BUY_SPEED
	int32_t iPulse = thecore_pulse() - ch->GetLastBuyTime();
	if (iPulse < PASSES_PER_SEC(1)) {
		return SHOP_SUBHEADER_GC_OK;
	}
#endif

	if (IsPCShop()) {
		return SHOP_SUBHEADER_GC_OK;
	}

	if (p >= m_itemVector.size()) {
		LOG_INFO("Shop::MultipleBuy: invalid position {} : {}", static_cast<int>(p), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		return SHOP_SUBHEADER_GC_INVALID_POS;
	}

	GuestMapType::iterator it = m_map_guest.find(ch);
	if (it == m_map_guest.end()) {
		return SHOP_SUBHEADER_GC_END;
	}

	SHOP_ITEM& r_item = m_itemVector[p];

	int64_t price = r_item.price * c;

	if (ecs::PointSystem::GetGold(AIHelpers::EcsOf(ch)) < price) {
		return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
	}

#ifdef ENABLE_BUY_WITH_ITEM
	int32_t price_vnum = 0, price_count = 0, have_count = 0;
	for (int32_t i = 0; i < MAX_SHOP_PRICES; i++) {
		price_vnum = r_item.itemprice[i].vnum;
		if (price_vnum > 0) {
			price_count = r_item.itemprice[i].count * c;
			have_count = ch->CountSpecifyItem(price_vnum);
			if (have_count < price_count) {
				LOG_INFO("Shop::MultipleBuy: Not enough item : {} has {}, price {}.", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), have_count, price_count);
				return SHOP_SUBHEADER_GC_NOT_ENOUGH_ITEM;
			}
		}
	}
#endif


	int64_t r;

	while (c > 0) {
		r = Buy(ch, p, true);
		if (r == SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY ||
#ifdef ENABLE_BUY_WITH_ITEM
			r == SHOP_SUBHEADER_GC_NOT_ENOUGH_ITEM ||
#endif
			r == SHOP_SUBHEADER_GC_INVENTORY_FULL ||
			r == SHOP_SUBHEADER_GC_END) {
			break;
		} else {
			c--;
		}
	}

#ifdef ENABLE_LIMIT_BUY_SPEED
	ch->SetLastBuyTime();
#endif
	ch->Save();
	return c <= 0 ? SHOP_SUBHEADER_GC_OK : r;
}
#endif

bool CShop::AddGuest(LPCHARACTER ch, uint32_t owner_vid, bool bOtherEmpire)
{
	if (!ch)
		return false;

	if (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)))
		return false;

	if (ch->GetShop())
		return false;

	ch->SetShop(this);

	m_map_guest.insert(GuestMapType::value_type(ch, bOtherEmpire));

	TPacketGCShop pack;

	pack.header		= HEADER_GC_SHOP;
	pack.subheader	= SHOP_SUBHEADER_GC_START;

	TPacketGCShopStart pack2;

	memset(&pack2, 0, sizeof(pack2));
	pack2.owner_vid = owner_vid;

	for (uint32_t i = 0; i < m_itemVector.size() && i < SHOP_HOST_ITEM_MAX_NUM; ++i)
	{
		const SHOP_ITEM & item = m_itemVector[i];

#ifdef ENABLE_SHOP_BLACKLIST
		//HIVALUE_ITEM_EVENT
		if (quest::CQuestManager::instance().GetEventFlag("hivalue_item_sell") == 0)
		{
			//축복의 구슬 && 만년한철 이벤트
			if (item.vnum == 70024 || item.vnum == 70035)
			{
				continue;
			}
		}
#endif
		//END_HIVALUE_ITEM_EVENT
		if (m_pkPC && !ItemSystem::IsValidItem(item.pkItem))
			continue;

		pack2.items[i].vnum = item.vnum;

		// REMOVED_EMPIRE_PRICE_LIFT
#ifdef ENABLE_NEWSTUFF
		if (bOtherEmpire && !g_bEmpireShopPriceTripleDisable) // no empire price penalty for pc shop
#else
		if (bOtherEmpire) // no empire price penalty for pc shop
#endif
		{
			pack2.items[i].price = item.price * 3;
		}
		else
			pack2.items[i].price = item.price;

#ifdef ENABLE_BUY_WITH_ITEM
		for (int j = 0; j < MAX_SHOP_PRICES; j++) {
			pack2.items[i].itemprice[j].vnum = item.itemprice[j].vnum;
			pack2.items[i].itemprice[j].count = item.itemprice[j].count;
		}
#endif
		// END_REMOVED_EMPIRE_PRICE_LIFT

		pack2.items[i].count = item.count;

		if (ItemSystem::IsValidItem(item.pkItem))
		{
			for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
				pack2.items[i].alSockets[socket] = ItemSystem::GetItemSocket(item.pkItem, socket);
			for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
				pack2.items[i].aAttr[attr] = ItemSystem::GetItemAttribute(item.pkItem, attr);
#ifdef ATTR_LOCK
			pack2.items[i].lockedattr = ItemSystem::GetItemLockedAttributeIndex(item.pkItem);
#endif
		}
	}

	pack.size = sizeof(pack) + sizeof(pack2);

	ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->BufferedPacket(&pack, sizeof(TPacketGCShop));
	ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&pack2, sizeof(TPacketGCShopStart));
	return true;
}

void CShop::RemoveGuest(LPCHARACTER ch)
{
	if (ch->GetShop() != this)
		return;

	m_map_guest.erase(ch);
	ch->SetShop(nullptr);

	TPacketGCShop pack;

	pack.header		= HEADER_GC_SHOP;
	pack.subheader	= SHOP_SUBHEADER_GC_END;
	pack.size		= sizeof(TPacketGCShop);

	ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&pack, sizeof(pack));
}

void CShop::Broadcast(const void * data, int bytes)
{
	LOG_INFO("Shop::Broadcast {} {}", static_cast<const void*>(data), bytes);

	GuestMapType::iterator it;

	it = m_map_guest.begin();

	while (it != m_map_guest.end())
	{
		LPCHARACTER ch = it->first;

		if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
			ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(data, bytes);

		++it;
	}
}

void CShop::BroadcastUpdateItem(uint8_t pos)
{
	TPacketGCShop pack;
	TPacketGCShopUpdateItem pack2;

	TEMP_BUFFER	buf;

	pack.header		= HEADER_GC_SHOP;
	pack.subheader	= SHOP_SUBHEADER_GC_UPDATE_ITEM;
	pack.size		= sizeof(pack) + sizeof(pack2);

	pack2.pos		= pos;

	if (m_pkPC && !ItemSystem::IsValidItem(m_itemVector[pos].pkItem))
		pack2.item.vnum = 0;
	else
	{
		pack2.item.vnum	= m_itemVector[pos].vnum;
		if (ItemSystem::IsValidItem(m_itemVector[pos].pkItem))
		{
			for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
				pack2.item.alSockets[socket] = ItemSystem::GetItemSocket(m_itemVector[pos].pkItem, socket);
			for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
				pack2.item.aAttr[attr] = ItemSystem::GetItemAttribute(m_itemVector[pos].pkItem, attr);
		}
		else
		{
			memset(pack2.item.alSockets, 0, sizeof(pack2.item.alSockets));
			memset(pack2.item.aAttr, 0, sizeof(pack2.item.aAttr));
		}
	}

	pack2.item.price	= m_itemVector[pos].price;
	pack2.item.count	= m_itemVector[pos].count;
#ifdef ENABLE_BUY_WITH_ITEM
	for (int i = 0; i < MAX_SHOP_PRICES; i++) {
		pack2.item.itemprice[i].vnum = m_itemVector[pos].itemprice[i].vnum;
		pack2.item.itemprice[i].count = m_itemVector[pos].itemprice[i].count;
	}
#endif

	buf.write(&pack, sizeof(pack));
	buf.write(&pack2, sizeof(pack2));
	Broadcast(buf.read_peek(), buf.size());
}

int CShop::GetNumberByVnum(uint32_t dwVnum)
{
	int itemNumber = 0;

	for (uint32_t i = 0; i < m_itemVector.size() && i < SHOP_HOST_ITEM_MAX_NUM; ++i)
	{
		const SHOP_ITEM & item = m_itemVector[i];

		if (item.vnum == dwVnum)
		{
			itemNumber += item.count;
		}
	}

	return itemNumber;
}

bool CShop::IsSellingItem(uint32_t itemID)
{
	bool isSelling = false;

	for (uint32_t i = 0; i < m_itemVector.size() && i < SHOP_HOST_ITEM_MAX_NUM; ++i)
	{
		if ((unsigned int)(m_itemVector[i].itemid) == itemID)
		{
			isSelling = true;
			break;
		}
	}

	return isSelling;

}
