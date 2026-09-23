#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "db.h"
#include "desc.h"
#include "desc_manager.h"
#include "log.h"
#include "packet.h"
#include "protocol.h"
#include "utils.h"
#include "item_manager.h"
#include "item.h"
#include "safebox.h"
#include "refine.h"
#include "cuberenewal.h"
#include "unique_item.h"
#include "belt_inventory_helper.h"
#include "../ecs/components/dirty_components.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "desc_client.h"
#include "gm.h"
#include "input_item_helpers.hpp"
#include "DragonSoul.h"
#include "questmanager.h"
#include "../ecs/systems/AcceSystem.hpp"

void CInputMain::ItemUse(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemUse handler ECS
// DUAL-PATH: legacy only during migration window
	ItemSystem::UseItem(character, ((struct command_item_use *) data)->Cell);
}

void CInputMain::ItemToItem(entt::entity character, const char * pcData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemToItem handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGItemUseToItem * p = (TPacketCGItemUseToItem *) pcData;
	if (ecs::IsCharacter(character))
		ItemSystem::UseItem(character, p->Cell, p->TargetCell);
}

void CInputMain::ItemDrop(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDrop handler ECS
// DUAL-PATH: legacy only during migration window
	struct command_item_drop * pinfo = (struct command_item_drop *) data;
	if (!ecs::IsCharacter(character))
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	if (pinfo->gold > 0)
		ItemSystem::DropGold(character, pinfo->gold);
	else
		ItemSystem::DropItem(character, pinfo->Cell);
}

void CInputMain::ItemDrop2(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDrop2 handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGItemDrop2 * pinfo = (TPacketCGItemDrop2 *) data;
	if (!ecs::IsCharacter(character))
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	if (pinfo->gold > 0)
		ItemSystem::DropGold(character, pinfo->gold);
	else
		ItemSystem::DropItem(character, pinfo->Cell, pinfo->count);
}

void CInputMain::ItemMove(entt::entity character, const char * data)
{
	// Framing validates this fixed-size packet before dispatch. Copy packed data
	// so the transaction does not retain a pointer into the receive buffer.
	command_item_move packet {};
	memcpy(&packet, data, sizeof(packet));
	InventorySystem::MoveItem(character, packet.Cell, packet.CellTo, packet.count);
}

void CInputMain::ItemPickup(entt::entity character, const char* data)
{
    if (!data || !ecs::PlayerRuntime::IsPC(character)) return;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    const auto gm = ecs::PlayerRuntime::GetGMLevel(character);
    if (gm > GM_PLAYER && gm < GM_IMPLEMENTOR) return;
#endif
    command_item_pickup packet {};
    memcpy(&packet, data, sizeof(packet));
    ItemSystem::PickupItem(character, packet.vid);
}

void CInputMain::QuickslotAdd(entt::entity character, const char* data)
{
    if (!data) return;
    command_quickslot_add packet {};
    memcpy(&packet, data, sizeof(packet));
    InventorySystem::SetQuickslotFromClient(character, packet.pos, packet.slot);
}

void CInputMain::QuickslotDelete(entt::entity character, const char* data)
{
    if (!data) return;
    command_quickslot_del packet {};
    memcpy(&packet, data, sizeof(packet));
    InventorySystem::DelQuickslot(character, packet.pos);
}

void CInputMain::QuickslotSwap(entt::entity character, const char* data)
{
    if (!data) return;
    command_quickslot_swap packet {};
    memcpy(&packet, data, sizeof(packet));
    InventorySystem::SwapQuickslot(character, packet.pos, packet.change_pos);
}

void CInputMain::ItemGive(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemGive handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGGiveItem* p = (TPacketCGGiveItem*) c_pData;
	const entt::entity to_chEntity = CHARACTER_MANAGER::instance().FindEntity(p->dwTargetVID);


	if (to_chEntity != entt::null) {
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
		if ((ecs::PlayerRuntime::GetGMLevel(to_chEntity) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(to_chEntity) < GM_IMPLEMENTOR) || (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR)) {
			return;
		}
#endif

		ItemSystem::GiveItem(character, to_chEntity, p->ItemPos);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 403, "");
	}
#endif
}

void CInputMain::ItemDestroy(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ItemDestroy handler ECS
// DUAL-PATH: legacy only during migration window
	struct command_item_destroy * pinfo = (struct command_item_destroy *) data;
	if (ecs::IsCharacter(character)) {
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
		if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
			return;
		}
#endif
		ItemSystem::DestroyItem(character, pinfo->Cell);
	}
}

void CInputMain::ItemDivision(entt::entity character, const char * data)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;
	struct command_item_division * pinfo = (struct command_item_division *) data;
	ItemSystem::ItemDivision(character, pinfo->pos);
}

void CInputMain::SafeboxCheckin(entt::entity character, const char * c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

	const entt::entity ownerEntity = character;
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity))
		return;

	auto* questPC = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(ownerEntity));
	if (!questPC || questPC->IsRunning())
	{
		return;
	}

	const auto request = *reinterpret_cast<const TPacketCGSafeboxCheckin*>(c_pData);
	const auto* p = &request;
	if (!IsInputInventoryPosition(p->ItemPos))
		return;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(ownerEntity) > GM_PLAYER &&
		ecs::PlayerRuntime::GetGMLevel(ownerEntity) < GM_IMPLEMENTOR)
	{
		return;
	}
#endif

	if (p->ItemPos.IsBeltInventoryPosition())
	{
		ecs::ChatSystem::Send(
			ownerEntity,
			CHAT_TYPE_INFO,
			"You cannot place items from the Belt inventory into the safebox.");
		return;
	}

	const auto safebox = SafeboxSystem::Get(character, SAFEBOX);
	const entt::entity itemEntity = ItemSystem::GetItem(ownerEntity, p->ItemPos);
	if (safebox == entt::null || !IsInputItemAt(character, itemEntity, p->ItemPos) || ItemSystem::IsItemExchanging(itemEntity))
		return;

#ifdef ENABLE_BUG_FIXES
	if (ItemSystem::IsItemEquipped(itemEntity))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 1244, "");
#endif
		return;
	}
#endif

#if defined(ENABLE_EXTRA_INVENTORY) && !defined(ENABLE_SPECIAL_INV_TO_SAFEBOX)
	if (ItemSystem::IsExtraItem(itemEntity))
		return;
#endif

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	if (ItemSystem::GetItemCell(itemEntity) >= InventorySystem::GetInventorySize(character) &&
		IS_SET(ItemSystem::GetItemFlags(itemEntity), ITEM_FLAG_IRREMOVABLE))
#else
	if (ItemSystem::GetItemCell(itemEntity) >= INVENTORY_MAX_NUM &&
		IS_SET(ItemSystem::GetItemFlags(itemEntity), ITEM_FLAG_IRREMOVABLE))
#endif
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 640, "");
#endif
		return;
	}

	if (!SafeboxSystem::IsEmpty(safebox, p->bSafePos, ItemSystem::GetItemSize(itemEntity)))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 641, "");
#endif
		return;
	}

	if (ItemSystem::GetItemVnum(itemEntity) == UNIQUE_ITEM_SAFEBOX_EXPAND ||
		IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_SAFEBOX) ||
		ItemSystem::IsItemLocked(itemEntity))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 187, "");
#endif
		return;
	}

	if (ItemSystem::GetItemType(itemEntity) == ITEM_BELT &&
		InventorySystem::HasBeltItems(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 385, "");
#endif
		return;
	}

	const TItemPos originalPos = p->ItemPos;
	const bool clearQuickslot = !ItemSystem::IsDragonSoulItem(itemEntity) && !ItemSystem::IsExtraItem(itemEntity);
	if (!ItemSystem::RemoveItemEcs(itemEntity))
		return;


	if (SafeboxSystem::Get(character, SAFEBOX) != safebox || !IsDetachedInputItem(itemEntity) ||
		!SafeboxSystem::Add(safebox, p->bSafePos, itemEntity))
	{
		RestoreInputItem(character, itemEntity, originalPos);
		return;
	}
	if (!IsInputItemAt(character, itemEntity, TItemPos(SAFEBOX, p->bSafePos)))
		return;
	if (clearQuickslot && ItemSystem::GetItem(character, originalPos) == entt::null)
		InventorySystem::SyncQuickslot(character, QUICKSLOT_TYPE_ITEM, originalPos.cell, 255);
	if (!IsInputItemAt(character, itemEntity, TItemPos(SAFEBOX, p->bSafePos)))
		return;

	char hint[128];
	snprintf(
		hint,
		sizeof(hint),
		"%s %u",
		ItemSystem::GetItemName(itemEntity),
		ItemSystem::GetItemCount(itemEntity));
	LogManager::instance().ItemLogEntity(character, itemEntity, "SAFEBOX PUT", hint);
}

void CInputMain::SafeboxCheckout(entt::entity character, const char * c_pData, bool bMall)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

	const entt::entity ownerEntity = character;
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity))
		return;

	const auto request = *reinterpret_cast<const TPacketCGSafeboxCheckout*>(c_pData);
	const auto* p = &request;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(ownerEntity) > GM_PLAYER &&
		ecs::PlayerRuntime::GetGMLevel(ownerEntity) < GM_IMPLEMENTOR)
	{
		return;
	}
#endif

	const uint8_t window = bMall ? MALL : SAFEBOX;
	const auto safebox = SafeboxSystem::Get(character, window);
	if (safebox == entt::null)
		return;

	const entt::entity itemEntity = SafeboxSystem::GetItem(safebox, p->bSafePos);
	if (!IsInputItemAt(character, itemEntity, TItemPos(window, p->bSafePos)) ||
		ItemSystem::IsItemLocked(itemEntity) || ItemSystem::IsItemExchanging(itemEntity))
		return;

	TItemPos destination = p->ItemPos;
	if (!InventorySystem::IsEmptyItemGrid(character, destination, ItemSystem::GetItemSize(itemEntity)))
		return;

	if (ItemSystem::IsDragonSoulItem(itemEntity))
	{
		if (bMall)
		{
			DSManager::instance().DragonSoulItemInitialize(itemEntity);
			if (SafeboxSystem::Get(character, window) != safebox ||
				!IsInputItemAt(character, itemEntity, TItemPos(window, p->bSafePos)))
				return;
		}

		if (destination.window_type != DRAGON_SOUL_INVENTORY)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 643, "");
#endif
			return;
		}

		if (!DSManager::instance().IsValidCellForThisItem(itemEntity, destination))
		{
			const int emptyCell =
				ItemSystem::GetEmptyDragonSoulInventory(ownerEntity, itemEntity);
			if (emptyCell < 0)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 644, "");
#endif
				return;
			}
			destination = TItemPos(DRAGON_SOUL_INVENTORY, emptyCell);
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (ItemSystem::IsExtraItem(itemEntity))
	{
		if (destination.window_type != EXTRA_INVENTORY)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ownerEntity, CHAT_TYPE_INFO, 1292, "");
#endif
			return;
		}

		const uint32_t category = ItemSystem::GetItemExtraCategory(itemEntity);
		const uint32_t categoryBegin = category * EXTRA_INVENTORY_CATEGORY_MAX_NUM;
		const uint32_t categoryEnd = categoryBegin + EXTRA_INVENTORY_CATEGORY_MAX_NUM;
		if (destination.cell < categoryBegin || destination.cell >= categoryEnd)
			return;
	}
#endif
	else
	{
		if (destination.window_type != INVENTORY ||
			destination.IsBeltInventoryPosition())
		{
			ecs::ChatSystem::Send(
				ownerEntity,
				CHAT_TYPE_INFO,
				"You cannot place this item directly into that inventory.");
			return;
		}
	}

	if (SafeboxSystem::Get(character, window) != safebox ||
		!InventorySystem::IsEmptyItemGrid(character, destination, ItemSystem::GetItemSize(itemEntity)))
		return;
	const entt::entity removedItem = SafeboxSystem::Remove(safebox, p->bSafePos);
	if (removedItem != itemEntity || !IsDetachedInputItem(itemEntity))
		return;

	if (!RestoreInputItem(ownerEntity, itemEntity, destination))
	{
		if (SafeboxSystem::Get(character, window) == safebox && IsDetachedInputItem(itemEntity))
			SafeboxSystem::Add(safebox, p->bSafePos, itemEntity);
		return;
	}

	if (!IsInputItemAt(character, itemEntity, destination))
		return;
	ItemSystem::FlushDelayedSaveEcs(itemEntity);
	if (!IsInputItemAt(character, itemEntity, destination))
		return;

	const uint32_t itemId = ItemSystem::GetItemID(itemEntity);
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(itemId));
	db_clientdesc->Packet(&itemId, sizeof(itemId));

	if (!IsInputItemAt(character, itemEntity, destination))
		return;
	char hint[128];
	snprintf(
		hint,
		sizeof(hint),
		"%s %u",
		ItemSystem::GetItemName(itemEntity),
		ItemSystem::GetItemCount(itemEntity));
	LogManager::instance().ItemLogEntity(
		character, itemEntity, bMall ? "MALL GET" : "SAFEBOX GET", hint);
}

void CInputMain::SafeboxItemMove(entt::entity character, const char * data)
{

	const auto* pinfo = reinterpret_cast<const command_item_move*>(data);

	if (!data || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
		return;
	}
#endif

	const auto safebox = SafeboxSystem::Get(character, SAFEBOX);
	if (safebox == entt::null)
		return;

	SafeboxSystem::MoveItem(safebox, pinfo->Cell.cell, pinfo->CellTo.cell, pinfo->count);
}

void CInputMain::Refine(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Refine handler ECS
// DUAL-PATH: legacy only during migration window
	const TPacketCGRefine* p = reinterpret_cast<const TPacketCGRefine*>(c_pData);
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR) {
		InventorySystem::ClearRefineMode(character);
		return;
	}
#endif

	if (ecs::SocialSystem::HasExchange(character) || ecs::SessionSystem::IsSafeboxOpen(character) || ecs::SocialSystem::GetShopOwner(character) != entt::null || ecs::SocialSystem::GetMyShop(character) != entt::null || ecs::SessionSystem::IsCubeOpen(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 502, "");
#endif
		InventorySystem::ClearRefineMode(character);
		return;
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (AttrTransfer_is_open(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 292, "");
#endif
		InventorySystem::ClearRefineMode(character);
		return;
	}
#endif

	if (p->type == 255)
	{
		// DoRefine Cancel
		InventorySystem::ClearRefineMode(character);
		return;
	}

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	if (p->pos >= InventorySystem::GetInventorySize(character))
#else
	if (p->pos >= INVENTORY_MAX_NUM)
#endif
	{
		InventorySystem::ClearRefineMode(character);
		return;
	}

	const entt::entity owner = character;
	const entt::entity itemEntity = ItemSystem::GetInventoryItem(owner, p->pos);

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	if (!CRefineManager::instance().GetPercentage(owner, p->lLow, p->lMedium, p->lExtra, p->lTotal, itemEntity))
	{
		InventorySystem::ClearRefineMode(character);
		return;
	}

	CRefineManager::instance().Increase(owner, p->lLow, p->lMedium, p->lExtra);
#endif

	if (!ItemSystem::IsValidItem(itemEntity))
	{
		InventorySystem::ClearRefineMode(character);
		return;
	}

	ecs::SocialSystem::SetRefineTime(owner);

	if (p->type == REFINE_TYPE_NORMAL)
	{
		LOG_INFO("refine_type_noraml");
		ItemSystem::DoRefine(owner, itemEntity);
	}
	else if (p->type == REFINE_TYPE_SCROLL || p->type == REFINE_TYPE_HYUNIRON || p->type == REFINE_TYPE_MUSIN || p->type == REFINE_TYPE_BDRAGON)
	{
		LOG_INFO("refine_type_scroll, ...");
		ItemSystem::DoRefineWithScroll(owner, itemEntity);
	}

#ifdef ENABLE_SOUL_SYSTEM
	else if (p->type == REFINE_TYPE_SOUL)
	{
		LOG_INFO("refine_type_soul, ...");
		ItemSystem::DoRefineItemSoul(owner, itemEntity);
	}
#endif
	else if (p->type == REFINE_TYPE_MONEY_ONLY) {
		if (ItemSystem::IsValidItem(itemEntity)) {
			if (ecs::QuestSystem::GetFlag(character, "deviltower_zone.can_refine"))
			{
#ifdef ENABLE_BUG_FIXES
				if (ItemSystem::DoRefine(owner, itemEntity, true)) {
					ecs::QuestSystem::SetFlag(character, "deviltower_zone.can_refine", 0);
				}
#else
				ItemSystem::DoRefine(owner, itemEntity, true);
				ecs::QuestSystem::SetFlag(character, "deviltower_zone.can_refine", 0);
#endif
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 361, "");
			}
#endif
		}
	}

	InventorySystem::ClearRefineMode(character);
}

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
void CInputMain::InventoryExpansion(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate InventoryExpansion handler ECS
// DUAL-PATH: legacy only during migration window
	InventorySystem::ExpandInventory(character);
}
#endif

#ifdef ENABLE_ACCE_SYSTEM
void CInputMain::Acce(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Acce handler ECS
// DUAL-PATH: legacy only during migration window

	quest::PC * pPC = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(character));
	if (pPC->IsRunning())
		return;

	TPacketAcce * sPacket = (TPacketAcce*) c_pData;
	switch (sPacket->subheader)
	{
	case ACCE_SUBHEADER_CG_CLOSE:
	{
		ecs::AcceSystem::Close(character);
	}
	break;
	case ACCE_SUBHEADER_CG_ADD:
	{
		ecs::AcceSystem::AddMaterial(character, sPacket->tPos, sPacket->bPos);
	}
	break;
	case ACCE_SUBHEADER_CG_REMOVE:
	{
		ecs::AcceSystem::RemoveMaterial(character, sPacket->bPos);
	}
	break;
	case ACCE_SUBHEADER_CG_REFINE:
	{
		ecs::AcceSystem::Refine(character);
	}
	break;
	default:
		break;
	}
}
#endif

#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
void CInputMain::CubeRenewalSend(entt::entity character, const char* data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate CubeRenewalSend handler ECS
// DUAL-PATH: legacy only during migration window
	struct packet_send_cube_renewal * pinfo = (struct packet_send_cube_renewal *) data;
	switch (pinfo->subheader)
	{
		case CUBE_RENEWAL_SUB_HEADER_MAKE_ITEM:
		{

			if (pinfo->index_item > static_cast<uint32_t>(INT_MAX) ||
				pinfo->count_item == 0 ||
				pinfo->count_item > static_cast<uint32_t>(g_bItemCountLimit))
			{
				return;
			}

			int index_item_improve = -1;
			if (pinfo->index_item_improve != UINT32_MAX)
			{
				if (pinfo->index_item_improve >= INVENTORY_MAX_NUM)
					return;
				index_item_improve = static_cast<int>(pinfo->index_item_improve);
			}

			Cube_Make(
				character,
				static_cast<int>(pinfo->index_item),
				static_cast<int>(pinfo->count_item),
				index_item_improve);
		}
		break;

		case CUBE_RENEWAL_SUB_HEADER_CLOSE:
		{
			Cube_close(character);
		}
		break;
	}
}
#endif

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
void CInputMain::WheelDestiny(entt::entity character, const char* data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate WheelDestiny handler ECS
// DUAL-PATH: legacy only during migration window
	if (!ecs::IsCharacter(character))
	{
		return;
	}

	if (ecs::PlayerRuntime::IsObserverMode(character) || ecs::SocialSystem::HasExchange(character))
	{
		return;
	}

	const auto pinfo = reinterpret_cast<const TPacketCGWheelDestiny*>(data);
	enum { OPEN, CLOSE, TURN, GIVE };

	switch (pinfo->option)
	{
	case OPEN:
	{

		if (!ecs::PlayerRuntime::GetWheelDestiny(character))
		{
			ecs::PlayerRuntime::SetWheelDestiny(character, std::make_shared<CWheelDestiny>(character));
		}
	}
	break;
	case CLOSE:

	{
		if (ecs::PlayerRuntime::GetWheelDestiny(character))
		{


			if (ecs::PlayerRuntime::GetWheelDestiny(character)->GetGiftVnum())
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1307, "");
#endif
			}
			else
			{
				ecs::PlayerRuntime::SetWheelDestiny(character, nullptr);
				ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "BINARY_WHEEL_CLOSE");
			}
		}
	}
	break;
	case TURN:
	{
		if (ecs::SocialSystem::GetDungeon(character) != entt::null || ecs::PlayerRuntime::GetMapIndex(character) >= 10000)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Dungeonban nem tudsz p�rgetni./You cannot in dungeon");
			return;
		}
		if (ecs::PlayerRuntime::GetWheelDestiny(character))
		{
			static const uint32_t WHEEL_TICKET_VNUM = 70610;

			if (ItemSystem::CountItem(character, WHEEL_TICKET_VNUM) < 1)
			{

				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You Dont have Battle Pass Ticket");
				return;
			}

			ItemSystem::RemoveSpecifyItemEcs(character, WHEEL_TICKET_VNUM, 1);

			ecs::PlayerRuntime::GetWheelDestiny(character)->TurnWheel();
		}
	}
	break;

	case GIVE:
	{
		if (ecs::PlayerRuntime::GetWheelDestiny(character))
		{
			ecs::PlayerRuntime::GetWheelDestiny(character)->GiveMyFuckingGift();
		}
	}
	break;
	default:
	{
		LOG_ERROR("CInputMain::WheelDestiny : Unknown option {} : {}", pinfo->option, ecs::PlayerRuntime::GetName(character).data());
	}
	break;
	}
}
#endif
