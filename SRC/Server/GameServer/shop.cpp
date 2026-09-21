#include "stdafx.h"
#include "ecs/systems/InventorySystem.hpp"
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
#include "ecs/components/social_components.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/SessionSystem.hpp"

//#define ENABLE_SHOP_BLACKLIST
/* ------------------------------------------------------------------------------------ */

namespace ecs
{
	ShopData::ShopData()
#ifdef ENABLE_120_SHOP_SLOT_RAZOR93
		: grid(std::make_unique<CGrid>(15, 9))
#else
		: grid(std::make_unique<CGrid>(5, 9))
#endif
	{
	}

	ShopData::~ShopData() = default;
}

namespace
{
	void Broadcast(entt::entity shopEntity, ecs::ShopData& shop, const void* data, int bytes)
	{
		LOG_INFO("Shop::Broadcast {} {}", static_cast<const void*>(data), bytes);

		auto it = shop.guests.begin();

		while (it != shop.guests.end())
		{
			const entt::entity guest = it->first;

			if (LPDESC desc = ecs::PlayerRuntime::GetDesc(guest))
				desc->Packet(data, bytes);

			++it;
		}
	}
}

namespace ShopSystem
{
	using namespace std;

	bool IsValid(entt::entity shop)
	{
		return Find(shop) != nullptr;
	}

	void Initialize(entt::entity shop)
	{
		ecs::ShopData* state = Find(shop);
		if (!state)
			return;

		state->vnum = 0;
		state->npcVnum = 0;
		state->owner = entt::null;
		state->extended = false;
		state->guests.clear();
		state->tabs.clear();
		state->items.clear();

		if (state->grid)
			state->grid->Clear();
	}

	void Destroy(entt::entity shopEntity)
	{
		ecs::ShopData* shop = Find(shopEntity);
		if (!shop)
			return;

		TPacketGCShop pack;

		pack.header		= HEADER_GC_SHOP;
		pack.subheader	= SHOP_SUBHEADER_GC_END;
		pack.size		= sizeof(TPacketGCShop);

		Broadcast(shopEntity, *shop, &pack, sizeof(pack));

		for (auto& row : shop->guests)
		{
			const entt::entity guest = row.first;
			ecs::SocialSystem::SetShop(guest, entt::null);
		}
		shop->guests.clear();

		g_registry.destroy(shopEntity);
	}

	bool Create(entt::entity shop, uint32_t dwVnum, uint32_t dwNPCVnum, TShopItemTable * pTable)
	{
		ecs::ShopData* state = Find(shop);
		if (!state)
			return false;

		LOG_TRACE("SHOP #{} (Shopkeeper {})", dwVnum, dwNPCVnum);

		state->vnum = dwVnum;
		state->npcVnum = dwNPCVnum;

		uint8_t bItemCount;

		for (bItemCount = 0; bItemCount < SHOP_HOST_ITEM_MAX_NUM; ++bItemCount)
			if (0 == (pTable + bItemCount)->vnum)
				break;

		SetShopItems(shop, pTable, bItemCount);
		return true;
	}

	void SetShopItems(entt::entity shop, TShopItemTable * pTable, uint8_t bItemCount)
	{
		ecs::ShopData* state = Find(shop);
		if (!state)
			return;

		if (bItemCount > SHOP_HOST_ITEM_MAX_NUM)
			return;

		state->grid->Clear();

		state->items.assign(SHOP_HOST_ITEM_MAX_NUM, ecs::ShopItem{});

		for (int i = 0; i < bItemCount; ++i)
		{
			entt::entity pkItemEntity = entt::null;
			const TItemTable * item_table;

			if (state->owner != entt::null)
			{
				pkItemEntity = ItemSystem::GetItem(state->owner, pTable->pos);

				if (pkItemEntity == entt::null)
				{
					LOG_ERROR("cannot find item on pos ({}, {}) (name: {})", static_cast<int>(pTable->pos.window_type), pTable->pos.cell, ecs::PlayerRuntime::GetName(state->owner).data());
					continue;
				}

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

			if (IsPCShop(shop))
			{
				LOG_INFO("MyShop: use position {}", static_cast<int>(pTable->display_pos));
				iPos = pTable->display_pos;
			}
			else
				iPos = state->grid->FindBlank(1, item_table->bSize);

			if (iPos < 0)
			{
				LOG_ERROR("not enough shop window");
				continue;
			}

			if (!state->grid->IsEmpty(iPos, 1, item_table->bSize))
			{
				if (IsPCShop(shop))
				{
					LOG_ERROR("not empty position for pc shop {}[{}]", ecs::PlayerRuntime::GetName(state->owner).data(), ecs::PlayerRuntime::GetPlayerID(state->owner));
				}
				else
				{
					LOG_ERROR("not empty position for npc shop");
				}
				continue;
			}

			state->grid->Put(iPos, 1, item_table->bSize);

			ecs::ShopItem & item = state->items[iPos];

			item.item = pkItemEntity;
			item.itemid = 0;

			if (ItemSystem::IsValidItem(item.item))
			{
				item.vnum = ItemSystem::GetItemVnum(item.item);
				item.count = ItemSystem::GetItemCount(item.item);
#ifndef ENABLE_BUY_WITH_ITEM
				item.price = pTable->price;
#endif
				item.itemid	= ItemSystem::GetItemID(item.item);
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

	void SetOwner(entt::entity shop, entt::entity owner)
	{
		ecs::ShopData* state = Find(shop);
		if (!state)
			return;

		state->owner = owner;
	}

	bool IsPCShop(entt::entity shop)
	{
		ecs::ShopData* state = Find(shop);
		return state && state->owner != entt::null;
	}

	uint32_t GetVnum(entt::entity shop)
	{
		ecs::ShopData* state = Find(shop);
		return state ? state->vnum : 0;
	}

	uint32_t GetNPCVnum(entt::entity shop)
	{
		ecs::ShopData* state = Find(shop);
		return state ? state->npcVnum : 0;
	}

	bool AddShopTable(entt::entity shop, TShopTableEx& shopTable)
	{
		ecs::ShopData* state = Find(shop);
		if (!state)
			return false;

		for (auto it = state->tabs.begin(); it != state->tabs.end(); it++)
		{
			const TShopTableEx& _shopTable = *it;
			if (0 != _shopTable.dwVnum && _shopTable.dwVnum == shopTable.dwVnum)
				return false;
			if (0 != _shopTable.dwNPCVnum && _shopTable.dwNPCVnum == shopTable.dwVnum)
				return false;
		}
		state->tabs.push_back(shopTable);
		return true;
	}

	size_t GetTabCount(entt::entity shop)
	{
		ecs::ShopData* state = Find(shop);
		return state ? state->tabs.size() : 0;
	}

	// The extended (tabbed) shop tells its guests about the tabs instead of the
	// grid items; the guest relation and the header are the same.
	bool AddGuest(entt::entity shopEntity, entt::entity guest, uint32_t owner_vid, bool bOtherEmpire)
	{
		ecs::ShopData* shop = Find(shopEntity);
		if (!shop)
			return false;

		if (guest == entt::null || !g_registry.valid(guest))
			return false;

		if (ecs::SocialSystem::HasExchange(guest) || ecs::SocialSystem::GetShop(guest) != entt::null)
			return false;

		LPDESC desc = ecs::PlayerRuntime::GetDesc(guest);
		if (!desc)
			return false;

		ecs::SocialSystem::SetShop(guest, shopEntity);
		shop->guests.insert(std::unordered_map<entt::entity, bool>::value_type(guest, bOtherEmpire));

		if (shop->extended)
		{
			TPacketGCShop pack;

			pack.header		= HEADER_GC_SHOP;
			pack.subheader	= SHOP_SUBHEADER_GC_START_EX;

			TPacketGCShopStartEx pack2;

			memset(&pack2, 0, sizeof(pack2));

			pack2.owner_vid = owner_vid;
			pack2.shop_tab_count = shop->tabs.size();
			// The tab payload is one TSubPacketShopTab per tab; the size follows
			// SHOP_HOST_ITEM_MAX_NUM, which grows with the 120-slot shops. The old
			// fixed 8096-byte buffer overflowed there.
			std::vector<char> buffer(shop->tabs.size() * sizeof(TPacketGCShopStartEx::TSubPacketShopTab));
			char* buf = buffer.data();
			size_t size = 0;
			for (auto it = shop->tabs.begin(); it != shop->tabs.end(); it++)
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
			desc->Packet(buffer.data(), size);

			return true;
		}

		TPacketGCShop pack;

		pack.header		= HEADER_GC_SHOP;
		pack.subheader	= SHOP_SUBHEADER_GC_START;

		TPacketGCShopStart pack2;

		memset(&pack2, 0, sizeof(pack2));
		pack2.owner_vid = owner_vid;

		for (uint32_t i = 0; i < shop->items.size() && i < SHOP_HOST_ITEM_MAX_NUM; ++i)
		{
			const ecs::ShopItem & item = shop->items[i];

#ifdef ENABLE_SHOP_BLACKLIST
			//HIVALUE_ITEM_EVENT
			if (quest::CQuestManager::instance().GetEventFlag("hivalue_item_sell") == 0)
			{
				// Hivalue item event
				if (item.vnum == 70024 || item.vnum == 70035)
				{
					continue;
				}
			}
#endif
			//END_HIVALUE_ITEM_EVENT
			if (shop->owner != entt::null && !ItemSystem::IsValidItem(item.item))
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

			if (ItemSystem::IsValidItem(item.item))
			{
				for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
					pack2.items[i].alSockets[socket] = ItemSystem::GetItemSocket(item.item, socket);
				for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
					pack2.items[i].aAttr[attr] = ItemSystem::GetItemAttribute(item.item, attr);
#ifdef ATTR_LOCK
				pack2.items[i].lockedattr = ItemSystem::GetItemLockedAttributeIndex(item.item);
#endif
			}
		}

		pack.size = sizeof(pack) + sizeof(pack2);

		desc->BufferedPacket(&pack, sizeof(TPacketGCShop));
		desc->Packet(&pack2, sizeof(TPacketGCShopStart));
		return true;
	}

	void RemoveGuest(entt::entity shopEntity, entt::entity guest)
	{
		ecs::ShopData* shop = Find(shopEntity);
		if (!shop)
			return;

		if (guest == entt::null || !g_registry.valid(guest) || ecs::SocialSystem::GetShop(guest) != shopEntity)
			return;

		shop->guests.erase(guest);
		ecs::SocialSystem::SetShop(guest, entt::null);

		TPacketGCShop pack;

		pack.header		= HEADER_GC_SHOP;
		pack.subheader	= SHOP_SUBHEADER_GC_END;
		pack.size		= sizeof(TPacketGCShop);

		if (LPDESC desc = ecs::PlayerRuntime::GetDesc(guest))
			desc->Packet(&pack, sizeof(pack));
	}

	namespace
	{
		int64_t BuyNormal(entt::entity shopEntity, ecs::ShopData& shop, entt::entity ch, uint8_t pos
#ifdef ENABLE_BUY_STACK_FROM_SHOP
			, bool multiple
#endif
		)
		{
			if (!ecs::IsCharacter(ch))
				return SHOP_SUBHEADER_GC_END;
#ifdef ENABLE_BUY_STACK_FROM_SHOP
			bool ismultiple = multiple;
#else
			bool ismultiple = false;
#endif

			if (!ismultiple) {
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
				if (ecs::PlayerRuntime::GetGMLevel(ch) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(ch) < GM_IMPLEMENTOR) {
					return SHOP_SUBHEADER_GC_OK;
				}
#endif

#ifdef ENABLE_LIMIT_BUY_SPEED
				int iPulse = thecore_pulse() - ecs::SocialSystem::GetLastBuyTime(ch);
				if (iPulse < PASSES_PER_SEC(1)) {
					return SHOP_SUBHEADER_GC_OK;
				}
#endif
			}
#ifdef ENABLE_BUY_STACK_FROM_SHOP
			else
			{
				if (shop.owner != entt::null)
				{
					return SHOP_SUBHEADER_GC_OK;
				}
			}
#endif

			if (pos >= shop.items.size())
			{
				LOG_INFO("Shop::Buy : invalid position {} : {}", static_cast<int>(pos), ecs::PlayerRuntime::GetName(ch).data());
				return SHOP_SUBHEADER_GC_INVALID_POS;
			}

			auto it = shop.guests.find(ch);
			if (it == shop.guests.end()) {
				return SHOP_SUBHEADER_GC_END;
			}

			ecs::ShopItem& r_item = shop.items[pos];
			if (!ismultiple) {
				const entt::entity selectedItem = r_item.item;

				if (IsPCShop(shopEntity)) {
					if (selectedItem == entt::null || !ItemSystem::IsValidItem(selectedItem)) {
						LOG_INFO("Shop::Buy : Critical: This user seems to be a hacker : invalid pcshop item : BuyerPID:{} SellerPID:{}", ecs::PlayerRuntime::GetPlayerID(ch), ecs::PlayerRuntime::GetPlayerID(shop.owner));
						return SHOP_SUBHEADER_GC_SOLD_OUT;
					} else if (ItemSystem::GetItemOwner(selectedItem) != shop.owner) {
						LOG_INFO("Shop::Buy : Critical: This user seems to be a hacker : invalid pcshop item : BuyerPID:{} SellerPID:{}", ecs::PlayerRuntime::GetPlayerID(ch), ecs::PlayerRuntime::GetPlayerID(shop.owner));
						return SHOP_SUBHEADER_GC_SOLD_OUT;
					}
				}
			}


			int64_t dwPrice = r_item.price;

			if (ecs::PointSystem::GetGold(ch) < dwPrice)
			{
				return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
			}

#ifdef ENABLE_BUY_WITH_ITEM
			uint32_t dwPriceVnum = 0, dwPriceCount = 0, dwHaveCount = 0;
			for (int i = 0; i < MAX_SHOP_PRICES; i++) {
				dwPriceVnum = r_item.itemprice[i].vnum;
				if (dwPriceVnum > 0) {
					dwPriceCount = r_item.itemprice[i].count;
					dwHaveCount = ItemSystem::CountItem(ch, dwPriceVnum);
					if (dwHaveCount < dwPriceCount) {
						LOG_INFO("Shop::Buy : Not enough item : {} has {}, price {}.", ecs::PlayerRuntime::GetName(ch).data(), dwHaveCount, dwPriceCount);
						return SHOP_SUBHEADER_GC_NOT_ENOUGH_ITEM;
					}
				}
			}
#endif

			entt::entity itemEntity = shop.owner != entt::null ? r_item.item : ITEM_MANAGER::instance().CreateItem(r_item.vnum, r_item.count, 0, true);
			if (!ItemSystem::IsValidItem(itemEntity))
				return SHOP_SUBHEADER_GC_SOLD_OUT;

#ifdef ENABLE_SHOP_BLACKLIST
			if (shop.owner == entt::null)
			{
				if (quest::CQuestManager::instance().GetEventFlag("hivalue_item_sell") == 0)
				{
					// Hivalue item event
					if (ItemSystem::GetItemVnum(itemEntity) == 70024 || ItemSystem::GetItemVnum(itemEntity) == 70035)
					{
						return SHOP_SUBHEADER_GC_END;
					}
				}
			}
#endif

			int iEmptyPos;
			if (ItemSystem::IsDragonSoulItem(itemEntity))
			{
				iEmptyPos = ItemSystem::GetEmptyDragonSoulInventory(ch, itemEntity);
			}
#ifdef ENABLE_EXTRA_INVENTORY
			else if (ItemSystem::IsExtraItem(itemEntity))
			{
				iEmptyPos = ItemSystem::GetEmptyExtraInventory(ch, itemEntity);
			}
#endif
			else
			{
				iEmptyPos = InventorySystem::GetEmptyInventory(ch, ItemSystem::GetItemSize(itemEntity));
			}

			if (iEmptyPos < 0)
			{
				if (shop.owner != entt::null)
				{
					LOG_INFO("Shop::Buy at PC Shop : Inventory full : {} size {}", ecs::PlayerRuntime::GetName(ch).data(), ItemSystem::GetItemSize(itemEntity));
					return SHOP_SUBHEADER_GC_INVENTORY_FULL;
				}
				else
				{
					LOG_INFO("Shop::Buy : Inventory full : {} size {}", ecs::PlayerRuntime::GetName(ch).data(), ItemSystem::GetItemSize(itemEntity));
					ItemSystem::DestroyItemEntityEcs(
						itemEntity,
						"SHOP_TRANSACTION");
					return SHOP_SUBHEADER_GC_INVENTORY_FULL;
				}
			}

			if (dwPrice > 0) {
				ecs::PointSystem::Change(ch, POINT_GOLD, -dwPrice, false);
			}

#ifdef ENABLE_BUY_WITH_ITEM
			for (int i = 0; i < MAX_SHOP_PRICES; i++) {
				dwPriceVnum = r_item.itemprice[i].vnum;
				if (dwPriceVnum > 0) {
					dwPriceCount = r_item.itemprice[i].count;
					if (dwPriceCount > 0) {
						ItemSystem::RemoveSpecifyItemEcs(ch, dwPriceVnum, r_item.itemprice[i].count);
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

			// Personal shop sale: the seller is paid the taxed price.
			if (shop.owner != entt::null)
			{
#ifdef ENABLE_EXTRA_INVENTORY
				if (ItemSystem::IsExtraItem(itemEntity)) {
					InventorySystem::SyncQuickslot(shop.owner, QUICKSLOT_TYPE_ITEM_EXTRA, ItemSystem::GetItemCell(itemEntity), 255);
				} else {
					InventorySystem::SyncQuickslot(shop.owner, QUICKSLOT_TYPE_ITEM, ItemSystem::GetItemCell(itemEntity), 255);
				}
#else
				InventorySystem::SyncQuickslot(shop.owner, QUICKSLOT_TYPE_ITEM, ItemSystem::GetItemCell(itemEntity), 255);
#endif

				{
					char buf[512];

					if (ItemSystem::GetItemVnum(itemEntity) >= 80003 && ItemSystem::GetItemVnum(itemEntity) <= 80007)
					{
						snprintf(buf, sizeof(buf), "%s FROM: %u TO: %u PRICE: %lld", ItemSystem::GetItemName(itemEntity), ecs::PlayerRuntime::GetPlayerID(ch), ecs::PlayerRuntime::GetPlayerID(shop.owner), dwPrice);
						LogManager::instance().GoldBarLog(ecs::PlayerRuntime::GetPlayerID(ch), ItemSystem::GetItemID(itemEntity), SHOP_BUY, buf);
						LogManager::instance().GoldBarLog(ecs::PlayerRuntime::GetPlayerID(shop.owner), ItemSystem::GetItemID(itemEntity), SHOP_SELL, buf);
					}

					InventorySystem::RemoveFromCharacter(itemEntity);

					if (ItemSystem::IsDragonSoulItem(itemEntity)) {
						InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyPos));
					}
#ifdef ENABLE_EXTRA_INVENTORY
					else if (ItemSystem::IsExtraItem(itemEntity)) {
#ifdef ENABLE_25082021
						if (ItemSystem::IsItemStackable(itemEntity) && !IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
							int
#else
							uint8_t
#endif
							bCount = ItemSystem::GetItemCount(itemEntity);
							for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i) {
								const entt::entity item2 = ItemSystem::GetExtraInventoryItem(ch, i);
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
										itemEntity = entt::null;
										break;
									}
								}
							}

							if (ItemSystem::IsValidItem(itemEntity)) {
								ItemSystem::SetItemCountEcs(
									itemEntity,
									bCount);
								InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
							}
						} else {
							InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
						}
#else
						InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
#endif
					}
#endif
					else {
#ifdef ENABLE_25082021
						if (ItemSystem::IsItemStackable(itemEntity) && !IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
							int
#else
							uint8_t
#endif
							bCount = ItemSystem::GetItemCount(itemEntity);
							for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
								const entt::entity item2 = ItemSystem::GetInventoryItem(ch, i);
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
										itemEntity = entt::null;
										break;
									}
								}
							}

							if (ItemSystem::IsValidItem(itemEntity)) {
								ItemSystem::SetItemCountEcs(
									itemEntity,
									bCount);
								InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(INVENTORY, iEmptyPos));
							}
						} else {
							InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(INVENTORY, iEmptyPos));
						}
#else
						InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(INVENTORY, iEmptyPos));
#endif
					}

					if (ItemSystem::IsValidItem(itemEntity)) {
						ItemSystem::FlushDelayedSaveEcs(itemEntity);
					}
				}

				r_item.item = entt::null;
				BroadcastUpdateItem(shopEntity, pos);

				ecs::PointSystem::Change(shop.owner, POINT_GOLD, dwPrice, false);
			}
			else
			{
				if (ItemSystem::IsDragonSoulItem(itemEntity)) {
					InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyPos));
				}
#ifdef ENABLE_EXTRA_INVENTORY
				else if (ItemSystem::IsExtraItem(itemEntity)) {
#ifdef ENABLE_25082021
					if (ItemSystem::IsItemStackable(itemEntity) && !IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
						bCount = ItemSystem::GetItemCount(itemEntity);
						for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i) {
							const entt::entity item2 = ItemSystem::GetExtraInventoryItem(ch, i);
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
									itemEntity = entt::null;
									break;
								}
							}
						}

						if (ItemSystem::IsValidItem(itemEntity)) {
							ItemSystem::SetItemCountEcs(
								itemEntity,
								bCount);
							InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
						}
					} else {
						InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
					}
#else
					InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
#endif
				}
#endif
				else {
#ifdef ENABLE_25082021
					if (ItemSystem::IsItemStackable(itemEntity) && !IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_STACK)) {
#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
						bCount = ItemSystem::GetItemCount(itemEntity);
						for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
							const entt::entity item2 = ItemSystem::GetInventoryItem(ch, i);
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
									itemEntity = entt::null;
									break;
								}
							}
						}

						if (ItemSystem::IsValidItem(itemEntity)) {
							ItemSystem::SetItemCountEcs(
								itemEntity,
								bCount);
							InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(INVENTORY, iEmptyPos));
						}
					} else {
						InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(INVENTORY, iEmptyPos));
					}
#else
					InventorySystem::AddToCharacter(itemEntity, ch, TItemPos(INVENTORY, iEmptyPos));
#endif
				}

				if (ItemSystem::IsValidItem(itemEntity)) {
					ItemSystem::FlushDelayedSaveEcs(itemEntity);
				}
			}

#ifdef ENABLE_BATTLE_PASS
			{
				uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(ch);
				if(bBattlePassId)
				{
					uint32_t dwYangCount, dwNotUsed;
					if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, SPENT_YANG, &dwNotUsed, &dwYangCount))
					{
						if(ecs::PlayerRuntime::GetMissionProgress(ch, SPENT_YANG, bBattlePassId) < dwYangCount)
							ecs::PlayerRuntime::UpdateMissionProgress(ch, SPENT_YANG, bBattlePassId, dwPrice, dwYangCount);
					}
				}
			}
#endif

			if (!ismultiple) {
#ifdef ENABLE_LIMIT_BUY_SPEED
				ecs::SocialSystem::SetLastBuyTime(ch);
#endif
				ecs::SessionSystem::Save(ch);
			}

			return (SHOP_SUBHEADER_GC_OK);
		}

		// The extended shop buys from its tabs; the window never holds item
		// entities.
		int64_t BuyExtended(entt::entity shopEntity, ecs::ShopData& shop, entt::entity ch, uint8_t pos)
		{
			if (!ecs::IsCharacter(ch))
				return SHOP_SUBHEADER_GC_END;
			uint8_t tabIdx = pos / SHOP_HOST_ITEM_MAX_NUM;
			uint8_t slotPos = pos % SHOP_HOST_ITEM_MAX_NUM;
			if (tabIdx >= shop.tabs.size())
			{
				LOG_INFO("ShopEx::Buy : invalid position {} : {}", pos, ecs::PlayerRuntime::GetName(ch).data());
				return SHOP_SUBHEADER_GC_INVALID_POS;
			}

			LOG_INFO("ShopEx::Buy : name {} pos {}", ecs::PlayerRuntime::GetName(ch).data(), pos);

			auto it = shop.guests.find(ch);

			if (it == shop.guests.end())
				return SHOP_SUBHEADER_GC_END;

			TShopTableEx& shopTab = shop.tabs[tabIdx];
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

				if (ecs::PointSystem::GetGold(ch) < dwPrice)
				{
					LOG_INFO("ShopEx::Buy : Not enough money : {} has {}, price {}", ecs::PlayerRuntime::GetName(ch).data(), ecs::PointSystem::GetGold(ch), dwPrice);
					return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
				}

				break;
			case SHOP_COIN_TYPE_SECONDARY_COIN:
				{
					uint32_t count = ItemSystem::CountTypeItem(ch, ITEM_SECONDARY_COIN);
					if (count < dwPrice)
					{
						LOG_INFO("ShopEx::Buy : Not enough myeongdojun : {} has {}, price {}", ecs::PlayerRuntime::GetName(ch).data(), count, dwPrice);
						return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY_EX;
					}
				}
				break;
			}

			entt::entity item = entt::null;

			item = ITEM_MANAGER::instance().CreateItem(r_item.vnum, r_item.count);

			if (!ItemSystem::IsValidItem(item))
				return SHOP_SUBHEADER_GC_SOLD_OUT;

			int iEmptyPos;
			if (ItemSystem::IsDragonSoulItem(item))
			{
				iEmptyPos = ItemSystem::GetEmptyDragonSoulInventory(ch, item);
			}
#ifdef ENABLE_EXTRA_INVENTORY
			else if (ItemSystem::IsExtraItem(item))
			{
				iEmptyPos = ItemSystem::GetEmptyExtraInventory(ch, item);
			}
#endif
			else
			{
				iEmptyPos = InventorySystem::GetEmptyInventory(ch, ItemSystem::GetItemSize(item));
			}

			if (iEmptyPos < 0)
			{
				LOG_INFO("ShopEx::Buy : Inventory full : {} size {}", ecs::PlayerRuntime::GetName(ch).data(), ItemSystem::GetItemSize(item));
				ItemSystem::DestroyItemEntityEcs(
					item,
					"SHOP_EX_TRANSACTION");
				return SHOP_SUBHEADER_GC_INVENTORY_FULL;
			}

			switch (shopTab.coinType)
			{
			case SHOP_COIN_TYPE_GOLD:
				ecs::PointSystem::Change(ch, POINT_GOLD, -dwPrice, false);
				break;
			case SHOP_COIN_TYPE_SECONDARY_COIN:
				ItemSystem::RemoveSpecifyTypeItem(ch, ITEM_SECONDARY_COIN, dwPrice);
				break;
			}


			if (ItemSystem::IsDragonSoulItem(item))
				InventorySystem::AddToCharacter(item, ch, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyPos));
#ifdef ENABLE_EXTRA_INVENTORY
			else if (ItemSystem::IsExtraItem(item))
				InventorySystem::AddToCharacter(item, ch, TItemPos(EXTRA_INVENTORY, iEmptyPos));
#endif
			else
				InventorySystem::AddToCharacter(item, ch, TItemPos(INVENTORY, iEmptyPos));

			ItemSystem::FlushDelayedSaveEcs(item);
			LogManager::instance().ItemLogEntity(ch, item, "BUY", ItemSystem::GetItemName(item));

			if (ItemSystem::GetItemVnum(item) >= 80003 && ItemSystem::GetItemVnum(item) <= 80007)
			{
				LogManager::instance().GoldBarLog((ecs::PlayerRuntime::GetPlayerID(ch)), ItemSystem::GetItemID(item), PERSONAL_SHOP_BUY, "");
			}

			DBManager::instance().SendMoneyLog(MONEY_LOG_SHOP, ItemSystem::GetItemVnum(item), -dwPrice);

			if (ItemSystem::IsValidItem(item))
				LOG_INFO("ShopEx: BUY: name {} {}(x {}):{} price {}", ecs::PlayerRuntime::GetName(ch).data(), ItemSystem::GetItemName(item), ItemSystem::GetItemCount(item), ItemSystem::GetItemID(item), dwPrice);

#ifdef ENABLE_FLUSH_CACHE_FEATURE // @warme006
			{
				ecs::SessionSystem::SaveReal(ch);
				db_clientdesc->DBPacketHeader(HEADER_GD_FLUSH_CACHE, 0, sizeof(uint32_t));
				uint32_t pid = (ecs::PlayerRuntime::GetPlayerID(ch));
				db_clientdesc->Packet(&pid, sizeof(uint32_t));
			}
#else
			{
				ecs::SessionSystem::Save(ch);
			}
#endif

			return (SHOP_SUBHEADER_GC_OK);
		}
	}

	int64_t Buy(entt::entity shopEntity, entt::entity ch, uint8_t pos
#ifdef ENABLE_BUY_STACK_FROM_SHOP
		, bool multiple
#endif
	)
	{
		ecs::ShopData* shop = Find(shopEntity);
		if (!shop)
			return SHOP_SUBHEADER_GC_END;

		if (shop->extended)
			return BuyExtended(shopEntity, *shop, ch, pos);

		return BuyNormal(shopEntity, *shop, ch, pos
#ifdef ENABLE_BUY_STACK_FROM_SHOP
			, multiple
#endif
		);
	}

#ifdef ENABLE_BUY_STACK_FROM_SHOP
	uint8_t MultipleBuy(entt::entity shopEntity, entt::entity ch, uint8_t p, uint8_t c) {
		ecs::ShopData* shop = Find(shopEntity);
		if (!shop)
			return SHOP_SUBHEADER_GC_END;

		if (!ecs::IsCharacter(ch))
			return SHOP_SUBHEADER_GC_END;
		if (p < 0 || c <= 0 || c > MULTIPLE_BUY_LIMIT) {
			return SHOP_SUBHEADER_GC_OK;
		}

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
		if (ecs::PlayerRuntime::GetGMLevel(ch) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(ch) < GM_IMPLEMENTOR) {
			return SHOP_SUBHEADER_GC_OK;
		}
#endif

#ifdef ENABLE_LIMIT_BUY_SPEED
		int32_t iPulse = thecore_pulse() - ecs::SocialSystem::GetLastBuyTime(ch);
		if (iPulse < PASSES_PER_SEC(1)) {
			return SHOP_SUBHEADER_GC_OK;
		}
#endif

		if (IsPCShop(shopEntity)) {
			return SHOP_SUBHEADER_GC_OK;
		}

		if (p >= shop->items.size()) {
			LOG_INFO("Shop::MultipleBuy: invalid position {} : {}", static_cast<int>(p), ecs::PlayerRuntime::GetName(ch).data());
			return SHOP_SUBHEADER_GC_INVALID_POS;
		}

		auto it = shop->guests.find(ch);
		if (it == shop->guests.end()) {
			return SHOP_SUBHEADER_GC_END;
		}

		ecs::ShopItem& r_item = shop->items[p];

		int64_t price = r_item.price * c;

		if (ecs::PointSystem::GetGold(ch) < price) {
			return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
		}

#ifdef ENABLE_BUY_WITH_ITEM
		int32_t price_vnum = 0, price_count = 0, have_count = 0;
		for (int32_t i = 0; i < MAX_SHOP_PRICES; i++) {
			price_vnum = r_item.itemprice[i].vnum;
			if (price_vnum > 0) {
				price_count = r_item.itemprice[i].count * c;
				have_count = ItemSystem::CountItem(ch, price_vnum);
				if (have_count < price_count) {
					LOG_INFO("Shop::MultipleBuy: Not enough item : {} has {}, price {}.", ecs::PlayerRuntime::GetName(ch).data(), have_count, price_count);
					return SHOP_SUBHEADER_GC_NOT_ENOUGH_ITEM;
				}
			}
		}
#endif


		int64_t r;

		while (c > 0) {
			r = Buy(shopEntity, ch, p, true);
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
		ecs::SocialSystem::SetLastBuyTime(ch);
#endif
		ecs::SessionSystem::Save(ch);
		return c <= 0 ? SHOP_SUBHEADER_GC_OK : r;
	}
#endif

	void BroadcastUpdateItem(entt::entity shopEntity, uint8_t pos)
	{
		ecs::ShopData* shop = Find(shopEntity);
		if (!shop)
			return;

		TPacketGCShop pack;
		TPacketGCShopUpdateItem pack2;

		TEMP_BUFFER	buf;

		pack.header		= HEADER_GC_SHOP;
		pack.subheader	= SHOP_SUBHEADER_GC_UPDATE_ITEM;
		pack.size		= sizeof(pack) + sizeof(pack2);

		pack2.pos		= pos;

		if (shop->owner != entt::null && !ItemSystem::IsValidItem(shop->items[pos].item))
			pack2.item.vnum = 0;
		else
		{
			pack2.item.vnum	= shop->items[pos].vnum;
			if (ItemSystem::IsValidItem(shop->items[pos].item))
			{
				for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
					pack2.item.alSockets[socket] = ItemSystem::GetItemSocket(shop->items[pos].item, socket);
				for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
					pack2.item.aAttr[attr] = ItemSystem::GetItemAttribute(shop->items[pos].item, attr);
			}
			else
			{
				memset(pack2.item.alSockets, 0, sizeof(pack2.item.alSockets));
				memset(pack2.item.aAttr, 0, sizeof(pack2.item.aAttr));
			}
		}

		pack2.item.price	= shop->items[pos].price;
		pack2.item.count	= shop->items[pos].count;
#ifdef ENABLE_BUY_WITH_ITEM
		for (int i = 0; i < MAX_SHOP_PRICES; i++) {
			pack2.item.itemprice[i].vnum = shop->items[pos].itemprice[i].vnum;
			pack2.item.itemprice[i].count = shop->items[pos].itemprice[i].count;
		}
#endif

		buf.write(&pack, sizeof(pack));
		buf.write(&pack2, sizeof(pack2));
		Broadcast(shopEntity, *shop, buf.read_peek(), buf.size());
	}

	int GetNumberByVnum(entt::entity shopEntity, uint32_t dwVnum)
	{
		ecs::ShopData* shop = Find(shopEntity);
		if (!shop)
			return 0;

		int itemNumber = 0;

		for (uint32_t i = 0; i < shop->items.size() && i < SHOP_HOST_ITEM_MAX_NUM; ++i)
		{
			const ecs::ShopItem & item = shop->items[i];

			if (item.vnum == dwVnum)
			{
				itemNumber += item.count;
			}
		}

		return itemNumber;
	}

	bool IsSellingItem(entt::entity shopEntity, uint32_t itemID)
	{
		ecs::ShopData* shop = Find(shopEntity);
		if (!shop)
			return false;

		// The extended shop has no per-item entity identity to check.
		if (shop->extended)
			return false;

		bool isSelling = false;

		for (uint32_t i = 0; i < shop->items.size() && i < SHOP_HOST_ITEM_MAX_NUM; ++i)
		{
			if ((unsigned int)(shop->items[i].itemid) == itemID)
			{
				isSelling = true;
				break;
			}
		}

		return isSelling;
	}
}
