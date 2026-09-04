#include "stdafx.h"
#include "ecs/systems/InventorySystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
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
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "item.h"
#include "item_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "log.h"
#include "db.h"
#include "questmanager.h"
#include "mob_manager.h"
#include "locale_service.h"
#include "desc_client.h"
#include "shopEx.h"
#include "group_text_parse_tree.h"

bool CShopEx::Create(uint32_t dwVnum, uint32_t dwNPCVnum)
{
	m_dwVnum = dwVnum;
	m_dwNPCVnum = dwNPCVnum;
	return true;
}

bool CShopEx::AddShopTable(TShopTableEx& shopTable)
{
	for (auto it = m_vec_shopTabs.begin(); it != m_vec_shopTabs.end(); it++)
	{
		const TShopTableEx& _shopTable = *it;
		if (0 != _shopTable.dwVnum && _shopTable.dwVnum == shopTable.dwVnum)
			return false;
		if (0 != _shopTable.dwNPCVnum && _shopTable.dwNPCVnum == shopTable.dwVnum)
			return false;
	}
	m_vec_shopTabs.push_back(shopTable);
	return true;
}

bool CShopEx::AddGuest(entt::entity guest, uint32_t owner_vid, bool bOtherEmpire)
{
	if (guest == entt::null || !g_registry.valid(guest))
		return false;

	if (ecs::SocialSystem::GetExchange(guest) || ecs::SocialSystem::GetShop(guest))
		return false;

	LPDESC desc = ecs::PlayerRuntime::GetDesc(guest);
	if (!desc)
		return false;

	ecs::SocialSystem::SetShop(guest, this);
	m_map_guest.insert(GuestMapType::value_type(guest, bOtherEmpire));

	TPacketGCShop pack;

	pack.header		= HEADER_GC_SHOP;
	pack.subheader	= SHOP_SUBHEADER_GC_START_EX;

	TPacketGCShopStartEx pack2;

	memset(&pack2, 0, sizeof(pack2));

	pack2.owner_vid = owner_vid;
	pack2.shop_tab_count = m_vec_shopTabs.size();
	char temp[8096]; // �ִ� 1728 * 3
	char* buf = &temp[0];
	size_t size = 0;
	for (auto it = m_vec_shopTabs.begin(); it != m_vec_shopTabs.end(); it++)
	{
		const TShopTableEx& shop_tab = *it;
		TPacketGCShopStartEx::TSubPacketShopTab pack_tab;
		pack_tab.coin_type = shop_tab.coinType;
		memcpy(pack_tab.name, shop_tab.name.c_str(), SHOP_TAB_NAME_MAX);

		for (uint8_t i = 0; i < SHOP_HOST_ITEM_MAX_NUM; i++)
		{
			pack_tab.items[i].vnum = shop_tab.items[i].vnum;
			pack_tab.items[i].count = shop_tab.items[i].count;
			switch(shop_tab.coinType)
			{
			case SHOP_COIN_TYPE_GOLD:
#ifdef ENABLE_NEWSTUFF
				if (bOtherEmpire && !g_bEmpireShopPriceTripleDisable) // no empire price penalty for pc shop
#else
				if (bOtherEmpire) // no empire price penalty for pc shop
#endif
					pack_tab.items[i].price = shop_tab.items[i].price * 3;
				else
					pack_tab.items[i].price = shop_tab.items[i].price;
				break;
			case SHOP_COIN_TYPE_SECONDARY_COIN:
				pack_tab.items[i].price = shop_tab.items[i].price;
				break;
			}
			memset(pack_tab.items[i].aAttr, 0, sizeof(pack_tab.items[i].aAttr));
			memset(pack_tab.items[i].alSockets, 0, sizeof(pack_tab.items[i].alSockets));
		}

		memcpy(buf, &pack_tab, sizeof(pack_tab));
		buf += sizeof(pack_tab);
		size += sizeof(pack_tab);
	}

	pack.size = sizeof(pack) + sizeof(pack2) + size;

	desc->BufferedPacket(&pack, sizeof(TPacketGCShop));
	desc->BufferedPacket(&pack2, sizeof(TPacketGCShopStartEx));
	desc->Packet(temp, size);

	return true;
}

int64_t CShopEx::Buy(LPCHARACTER ch, uint8_t pos)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	uint8_t tabIdx = pos / SHOP_HOST_ITEM_MAX_NUM;
	uint8_t slotPos = pos % SHOP_HOST_ITEM_MAX_NUM;
	if (tabIdx >= GetTabCount())
	{
		LOG_INFO("ShopEx::Buy : invalid position {} : {}", pos, ecs::PlayerRuntime::GetName(chEntity).data());
		return SHOP_SUBHEADER_GC_INVALID_POS;
	}

	LOG_INFO("ShopEx::Buy : name {} pos {}", ecs::PlayerRuntime::GetName(chEntity).data(), pos);

	GuestMapType::iterator it = m_map_guest.find(ch ? ch->GetEntityHandle() : entt::null);

	if (it == m_map_guest.end())
		return SHOP_SUBHEADER_GC_END;

	TShopTableEx& shopTab = m_vec_shopTabs[tabIdx];
	TShopItemTable& r_item = shopTab.items[slotPos];

	if (r_item.price <= 0)
	{
		LogManager::instance().HackLog("SHOP_BUY_GOLD_OVERFLOW", ch);
		return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
	}

	int64_t dwPrice = r_item.price;

	switch (shopTab.coinType)
	{
	case SHOP_COIN_TYPE_GOLD:
		if (it->second)	// if other empire, price is triple
			dwPrice *= 3;

		if (ecs::PointSystem::GetGold(chEntity) < dwPrice)
		{
			LOG_INFO("ShopEx::Buy : Not enough money : {} has {}, price {}", ecs::PlayerRuntime::GetName(chEntity).data(), ecs::PointSystem::GetGold(chEntity), dwPrice);
			return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
		}

		break;
	case SHOP_COIN_TYPE_SECONDARY_COIN:
		{
			uint32_t count = ch->CountSpecifyTypeItem(ITEM_SECONDARY_COIN);
			if (count < dwPrice)
			{
				LOG_INFO("ShopEx::Buy : Not enough myeongdojun : {} has {}, price {}", ecs::PlayerRuntime::GetName(chEntity).data(), count, dwPrice);
				return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY_EX;
			}
		}
		break;
	}

	LPITEM item;

	item = ITEM_MANAGER::instance().CreateItem(r_item.vnum, r_item.count);

	if (!item)
		return SHOP_SUBHEADER_GC_SOLD_OUT;

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
		LOG_INFO("ShopEx::Buy : Inventory full : {} size {}", ecs::PlayerRuntime::GetName(chEntity).data(), item->GetSize());
		ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"SHOP_EX_TRANSACTION");
		return SHOP_SUBHEADER_GC_INVENTORY_FULL;
	}

	switch (shopTab.coinType)
	{
	case SHOP_COIN_TYPE_GOLD:
		ecs::PointSystem::Change(chEntity, POINT_GOLD, -dwPrice, false);
		break;
	case SHOP_COIN_TYPE_SECONDARY_COIN:
		ch->RemoveSpecifyTypeItem(ITEM_SECONDARY_COIN, dwPrice);
		break;
	}


	if (item->IsDragonSoul())
		InventorySystem::AddToCharacter(item->GetEntityHandle(), ch->GetEntityHandle(), TItemPos(DRAGON_SOUL_INVENTORY, iEmptyPos));
#ifdef ENABLE_EXTRA_INVENTORY
	else if (item->IsExtraItem())
		InventorySystem::AddToCharacter(item->GetEntityHandle(), ch->GetEntityHandle(), TItemPos(EXTRA_INVENTORY, iEmptyPos));
#endif
	else
		InventorySystem::AddToCharacter(item->GetEntityHandle(), ch->GetEntityHandle(), TItemPos(INVENTORY, iEmptyPos));

	ITEM_MANAGER::instance().FlushDelayedSave(item);
	LogManager::instance().ItemLog(ch, item, "BUY", item->GetName());

	if (ItemSystem::GetItemVnum((item ? item->GetEntityHandle() : entt::null)) >= 80003 && ItemSystem::GetItemVnum((item ? item->GetEntityHandle() : entt::null)) <= 80007)
	{
		LogManager::instance().GoldBarLog((ecs::PlayerRuntime::GetPlayerID(chEntity)), ItemSystem::GetItemID((item ? item->GetEntityHandle() : entt::null)), PERSONAL_SHOP_BUY, "");
	}

	DBManager::instance().SendMoneyLog(MONEY_LOG_SHOP, ItemSystem::GetItemVnum((item ? item->GetEntityHandle() : entt::null)), -dwPrice);

	if (item)
		LOG_INFO("ShopEx: BUY: name {} {}(x {}):{} price {}", ecs::PlayerRuntime::GetName(chEntity).data(), item->GetName(), ItemSystem::GetItemCount((item ? item->GetEntityHandle() : entt::null)), ItemSystem::GetItemID((item ? item->GetEntityHandle() : entt::null)), dwPrice);

#ifdef ENABLE_FLUSH_CACHE_FEATURE // @warme006
	{
		ch->SaveReal();
		db_clientdesc->DBPacketHeader(HEADER_GD_FLUSH_CACHE, 0, sizeof(uint32_t));
		uint32_t pid = (ecs::PlayerRuntime::GetPlayerID(chEntity));
		db_clientdesc->Packet(&pid, sizeof(uint32_t));
	}
#else
	{
		ch->Save();
	}
#endif

    return (SHOP_SUBHEADER_GC_OK);
}


