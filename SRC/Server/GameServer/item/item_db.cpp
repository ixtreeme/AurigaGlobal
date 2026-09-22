#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "packet.h"
#include "protocol.h"
#include "char.h"
#include "constants.h"
#include "utils.h"
#include "item.h"
#include "item_manager.h"
#include "safebox.h"
#include "unique_item.h"
#include "desc_client.h"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "char_manager.h"
#include "desc.h"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"

void CInputDB::ItemLoad(LPDESC d, const char * c_pData)
{
	const entt::entity chEntity = d ? d->GetEntity() : entt::null;

	if (!ecs::IsCharacter(chEntity))
		return;

	if (InventorySystem::IsItemLoaded(chEntity))
		return;

	uint32_t dwCount = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	LOG_INFO("ITEM_LOAD: COUNT {} {}", ecs::PlayerRuntime::GetName(chEntity).data(), dwCount);

	std::vector<entt::entity> deferredItems;
	TPlayerItem * p = (TPlayerItem *) c_pData;
	uint32_t duplicatePurgeCount = 0;

	for (uint32_t i = 0; i < dwCount; ++i, ++p)
	{
		const entt::entity staleItem = ItemSystem::FindItemByID(p->id);
		if (staleItem != entt::null && ItemSystem::IsValidItem(staleItem))
		{
			const entt::entity staleOwner = ItemSystem::GetItemOwner(staleItem);
			const bool samePlayer =
				(staleOwner == chEntity) ||
				(ItemSystem::GetItemLastOwnerPID(staleItem) == ecs::PlayerRuntime::GetPlayerID(chEntity));

#ifdef ENABLE_EXTRA_INVENTORY
			const bool extraInventoryWindow = (p->window == EXTRA_INVENTORY);
#else
			const bool extraInventoryWindow = false;
#endif

			if (samePlayer || extraInventoryWindow)
			{
				++duplicatePurgeCount;
				LOG_ERROR("DUP_ITEM_PURGE_BEGIN index={} id={} owner_pid={} window={} entity={}",
					i, p->id, (ecs::PlayerRuntime::GetPlayerID(chEntity)), p->window, static_cast<uint32_t>(staleItem));
				const bool destroyed = ItemSystem::DestroyLoadedDuplicateItem(staleItem);
				LOG_ERROR("DUP_ITEM_PURGE_END index={} id={} owner_pid={} window={} destroyed={}",
					i, p->id, (ecs::PlayerRuntime::GetPlayerID(chEntity)), p->window, destroyed);
			}
		}

		const entt::entity item = ITEM_MANAGER::instance().CreateItem(p->vnum, p->count, p->id);

		if (!ItemSystem::IsValidItem(item))
		{
			LOG_ERROR("cannot create item by vnum {} (name {} id {})", p->vnum, ecs::PlayerRuntime::GetName(chEntity).data(), p->id);
			continue;
		}
		const entt::entity itemEntity = item;
		if (!ItemSystem::IsValidItem(itemEntity))
		{
			ITEM_MANAGER::instance().RemoveItem(item);
			continue;
		}

		ItemSystem::SetItemSkipSave(itemEntity, true);
		ItemSystem::SetItemSockets(item, p->alSockets);
		ItemSystem::SetItemAttributes(item, p->aAttr);
#ifdef ATTR_LOCK
		ItemSystem::SetItemLockedAttr(item, p->lockedattr);
#endif
#ifdef ENABLE_BELT_INVENTORY_EX
		if (p->window == BELT_INVENTORY)
		{
			p->window = INVENTORY;
			p->pos = p->pos + BELT_INVENTORY_SLOT_START;
		}
#endif

		if ((p->window == INVENTORY && ItemSystem::IsValidItem(ItemSystem::GetInventoryItem(chEntity, p->pos))) ||
				(p->window == EQUIPMENT && ItemSystem::IsValidItem(ItemSystem::GetWearItem(chEntity, p->pos))))
		{
			LOG_INFO("ITEM_RESTORE: {} {}", ecs::PlayerRuntime::GetName(chEntity).data(), ItemSystem::GetItemName(item));
			deferredItems.push_back(itemEntity);
		}
		else
		{
			switch (p->window)
			{
				case INVENTORY:
				case DRAGON_SOUL_INVENTORY:
#ifdef ENABLE_EXTRA_INVENTORY
				case EXTRA_INVENTORY:
#endif
#ifdef ENABLE_SWITCHBOT
				case SWITCHBOT:
#endif
#ifdef ENABLE_MOUNT_INVENTORY_FIX_RAZOR93_off
				 case MOUNT_INVENTORY:
					               // safety: never load these into CHARACTER inventory arrays
						deferredItems.push_back(itemEntity);
					break;
#else
				case MOUNT_INVENTORY:
#ifdef __HIGHLIGHT_SYSTEM__
					InventorySystem::AddToCharacter(item, chEntity, TItemPos(p->window, p->pos), false);
#else
					InventorySystem::AddToCharacter(item, chEntity, TItemPos(p->window, p->pos));
#endif
					break;
#endif
				case EQUIPMENT:
					if (ItemSystem::CheckItemUseLevel(item, (ecs::PointSystem::GetLevel(chEntity))) == true )
					{
						if (InventorySystem::EquipTo(item, chEntity, p->pos) == false )
						{
							deferredItems.push_back(itemEntity);
						}
					}
					else
					{
						deferredItems.push_back(itemEntity);
					}
					break;
			}
		}

		if (false == ItemSystem::OnAfterCreatedItem(item))
			LOG_ERROR("Failed to call ITEM::OnAfterCreatedItem (vnum: {}, id: {})", ItemSystem::GetItemVnum(itemEntity), ItemSystem::GetItemID(itemEntity));

		ItemSystem::SetItemSkipSave(itemEntity, false);
	}

	if (duplicatePurgeCount > 0)
	{
		LOG_ERROR("DUP_ITEM_PURGE_SUMMARY owner_pid={} name={} count={} loaded_count={}",
			(ecs::PlayerRuntime::GetPlayerID(chEntity)), ecs::PlayerRuntime::GetName(chEntity).data(), duplicatePurgeCount, dwCount);
	}

	for (const entt::entity itemEntity : deferredItems)
	{
		if (!ItemSystem::IsValidItem(itemEntity))
			continue;


		const int pos = InventorySystem::GetEmptyInventory(chEntity, ItemSystem::GetItemSize(itemEntity));
		if (pos < 0)
		{
			PIXEL_POSITION coord;
			coord.x = ecs::PlayerRuntime::GetX(chEntity);
			coord.y = ecs::PlayerRuntime::GetY(chEntity);

			ItemSystem::PlaceItemOnGround(itemEntity, ecs::PlayerRuntime::GetMapIndex(chEntity), coord);
			ItemSystem::SetGroundOwnership(itemEntity, chEntity, 180);
		}
		else
#ifdef __HIGHLIGHT_SYSTEM__
			InventorySystem::AddToCharacter(itemEntity, chEntity, TItemPos(INVENTORY, pos), false);
#else
			InventorySystem::AddToCharacter(itemEntity, chEntity, TItemPos(INVENTORY, pos));
#endif
	}
	ecs::PointSystem::CheckMaximumPoints(chEntity);
	NetworkSyncSystem::PointsPacket(chEntity);

	InventorySystem::SetItemLoaded(chEntity);
}

void CInputDB::SafeboxLoad(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	TSafeboxTable * p = (TSafeboxTable *) c_pData;

	if (d->GetAccountTable().id != p->dwID)
	{
		LOG_ERROR("SafeboxLoad: safebox has different id {} != {}", d->GetAccountTable().id, p->dwID);
		return;
	}

	if (!ecs::IsCharacter(d->GetEntity()))
		return;

	uint8_t bSize = 1;

	const entt::entity chEntity = d->GetEntity();


	//PREVENT_TRADE_WINDOW
	if (ecs::SocialSystem::GetShopOwner(chEntity) != entt::null || ecs::SocialSystem::HasExchange(chEntity) || ecs::SocialSystem::GetMyShop(chEntity) != entt::null || ecs::SessionSystem::IsCubeOpen(chEntity) )
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 296, "");
#endif
		ecs::SessionSystem::SetSafeboxLoading(chEntity, false);
		return;
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (AttrTransfer_is_open(chEntity))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 296, "");
#endif
		ecs::SessionSystem::SetSafeboxLoading(chEntity, false);
		return;
	}
#endif
	//END_PREVENT_TRADE_WINDOW

	// ADD_PREMIUM
	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(chEntity, PREMIUM_SAFEBOX) > 0 || ItemSystem::IsEquipUniqueGroup(chEntity, UNIQUE_GROUP_LARGE_SAFEBOX))
		bSize = 3;
	// END_OF_ADD_PREMIUM

	//ecs::SessionSystem::LoadSafebox(chEntity, p->bSize * SAFEBOX_PAGE_SIZE, p->dwGold, p->wItemCount, (TPlayerItem *) (c_pData + sizeof(TSafeboxTable)));
	ecs::SessionSystem::LoadSafebox(chEntity, bSize * SAFEBOX_PAGE_SIZE, p->dwGold, p->wItemCount, (TPlayerItem *) (c_pData + sizeof(TSafeboxTable)));
}

void CInputDB::SafeboxChangeSize(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	uint8_t bSize = *(uint8_t *) c_pData;

	if (!ecs::IsCharacter(d->GetEntity()))
		return;

	ecs::SessionSystem::ChangeSafeboxSize(d->GetEntity(), bSize);
}

void CInputDB::SafeboxWrongPassword(LPDESC d)
{
	if (!d)
		return;

	if (!ecs::IsCharacter(d->GetEntity()))
		return;

	TPacketCGSafeboxWrongPassword p;
	p.bHeader = HEADER_GC_SAFEBOX_WRONG_PASSWORD;
	d->Packet(&p, sizeof(p));

	ecs::SessionSystem::SetSafeboxLoading(d->GetEntity(), false);
}

void CInputDB::SafeboxChangePasswordAnswer(LPDESC d, const char* c_pData)
{
	if (!d)
		return;

	if (!ecs::IsCharacter(d->GetEntity()))
		return;

#ifdef TEXTS_IMPROVEMENT
	TSafeboxChangePasswordPacketAnswer* p = (TSafeboxChangePasswordPacketAnswer*) c_pData;
	if (p->flag) {
		ecs::ChatSystem::SendNew(d->GetEntity(), CHAT_TYPE_INFO, 187, "");
	}
	else {
		ecs::ChatSystem::SendNew(d->GetEntity(), CHAT_TYPE_INFO, 186, "");
	}
#endif
}

void CInputDB::MallLoad(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	TSafeboxTable * p = (TSafeboxTable *) c_pData;

	if (d->GetAccountTable().id != p->dwID)
	{
		LOG_ERROR("safebox has different id {} != {}", d->GetAccountTable().id, p->dwID);
		return;
	}

	if (!ecs::IsCharacter(d->GetEntity()))
		return;

	ecs::SessionSystem::LoadMall(d->GetEntity(), p->wItemCount, (TPlayerItem *) (c_pData + sizeof(TSafeboxTable)));
}
