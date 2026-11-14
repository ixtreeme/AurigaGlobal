#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PetInventorySystem.hpp"
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
#include "horsename_manager.h"
#include "PetInventory.h"
#include "PetSystem.h"
#include "pet_inventory_helper.h"
#include "item_manager.h"
#include "item.h"
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

void CInputMain::PetInventoryCheckin(entt::entity character, const char* c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

	const entt::entity ownerEntity = character;
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity))
		return;

	const auto request = *reinterpret_cast<const TPacketCGPetInventoryCheckin*>(c_pData);
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

	const entt::entity mountInventory = PetInventorySystem::GetPetInventory(character);
	if (mountInventory == entt::null)
		return;

	const entt::entity itemEntity = ItemSystem::GetItem(ownerEntity, p->ItemPos);
	if (!IsInputItemAt(character, itemEntity, p->ItemPos))
		return;

	if (!PetInventorySystem::IsPetInventoryPositionEmpty(
		character, p->wPetPos, ItemSystem::GetItemSize(itemEntity)))
	{
		return;
	}

	if (ItemSystem::IsItemEquipped(itemEntity) ||
		ItemSystem::IsItemExchanging(itemEntity) ||
		ItemSystem::IsItemLocked(itemEntity) ||
		ItemSystem::IsExtraItem(itemEntity))
	{
		return;
	}

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	if (ItemSystem::GetItemCell(itemEntity) >= InventorySystem::GetInventorySize(character) &&
		IS_SET(ItemSystem::GetItemFlags(itemEntity), ITEM_FLAG_IRREMOVABLE))
#else
	if (ItemSystem::GetItemCell(itemEntity) >= INVENTORY_MAX_NUM &&
		IS_SET(ItemSystem::GetItemFlags(itemEntity), ITEM_FLAG_IRREMOVABLE))
#endif
	{
		return;
	}

	if (!CPetInventoryHelper::CanMoveIntoPetInventory(itemEntity))
		return;

	const uint32_t vnum = ItemSystem::GetItemVnum(itemEntity);
	const int totalSlots =
		PetInventorySystem::GetPetInventoryWidth(character) *
		PetInventorySystem::GetPetInventorySize(character);
	for (int slot = 0; slot < totalSlots; ++slot)
	{
		const entt::entity storedItem = PetInventorySystem::GetPetInventoryItem(character, slot);
		if (!ItemSystem::IsValidItem(storedItem))
			continue;

		if (ItemSystem::GetItemVnum(storedItem) == vnum)
		{
			ecs::ChatSystem::Send(
				ownerEntity,
				CHAT_TYPE_INFO,
				"This pet is already in your account inventory.");
			return;
		}
	}

	

	const TItemPos originalPos = p->ItemPos;
	const bool clearQuickslot = !ItemSystem::IsDragonSoulItem(itemEntity) && !ItemSystem::IsExtraItem(itemEntity);
	if (!ItemSystem::RemoveItemEcs(itemEntity))
		return;


	if (IsDetachedInputItem(itemEntity))
		ItemSystem::FlushDelayedSaveEcs(itemEntity);
	if (PetInventorySystem::GetPetInventory(character) != mountInventory || !IsDetachedInputItem(itemEntity) ||
		!PetInventorySystem::AddPetInventoryItem(character, p->wPetPos, itemEntity))
	{
		if (IsDetachedInputItem(itemEntity))
			ItemSystem::SetItemSkipSave(itemEntity, false);
		if (RestoreInputItem(character, itemEntity, originalPos) &&
			IsInputItemAt(character, itemEntity, originalPos))
			ItemSystem::FlushDelayedSaveEcs(itemEntity);
		return;
	}
	if (!IsInputItemAt(character, itemEntity, TItemPos(PET_INVENTORY, p->wPetPos)))
		return;
	if (clearQuickslot && ItemSystem::GetItem(character, originalPos) == entt::null)
		InventorySystem::SyncQuickslot(character, QUICKSLOT_TYPE_ITEM, originalPos.cell, 255);

	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	PetInventorySystem::SendPetInventory(character);
	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	ecs::PointSystem::Compute(character);
	NetworkSyncSystem::PointsPacket(ownerEntity);

}

void CInputMain::PetInventoryCheckout(entt::entity character, const char* c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

	const entt::entity ownerEntity = character;
	if (ownerEntity == entt::null || !g_registry.valid(ownerEntity))
		return;

	const auto request = *reinterpret_cast<const TPacketCGPetInventoryCheckout*>(c_pData);
	const auto* p = &request;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(ownerEntity) > GM_PLAYER &&
		ecs::PlayerRuntime::GetGMLevel(ownerEntity) < GM_IMPLEMENTOR)
	{
		return;
	}
#endif

	const entt::entity mountInventory = PetInventorySystem::GetPetInventory(character);
	if (mountInventory == entt::null ||
		!PetInventorySystem::IsPetInventoryPositionValid(character, p->wPetPos))
		return;

	if (p->ItemPos.window_type != INVENTORY ||
		p->ItemPos.IsBeltInventoryPosition())
	{
		return;
	}

	const entt::entity itemEntity = PetInventorySystem::GetPetInventoryItem(character, p->wPetPos);
	if (!IsInputItemAt(character, itemEntity, TItemPos(PET_INVENTORY, p->wPetPos)) ||
		ItemSystem::IsItemExchanging(itemEntity) ||
		ItemSystem::IsItemLocked(itemEntity))
	{
		return;
	}

	if (!InventorySystem::IsEmptyItemGrid(character, p->ItemPos, ItemSystem::GetItemSize(itemEntity)))
		return;

	if (PetInventorySystem::RemovePetInventoryItem(character, p->wPetPos) != itemEntity ||
		!IsDetachedInputItem(itemEntity))
		return;

	ItemSystem::SetItemSkipSave(itemEntity, false);
	if (!RestoreInputItem(ownerEntity, itemEntity, p->ItemPos))
	{
		if (PetInventorySystem::GetPetInventory(character) == mountInventory && IsDetachedInputItem(itemEntity))
			PetInventorySystem::AddPetInventoryItem(character, p->wPetPos, itemEntity);
		return;
	}

	if (!IsInputItemAt(character, itemEntity, p->ItemPos))
		return;
	ItemSystem::FlushDelayedSaveEcs(itemEntity);
	if (!IsInputItemAt(character, itemEntity, p->ItemPos))
		return;

	const uint32_t itemId = ItemSystem::GetItemID(itemEntity);
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(itemId));
	db_clientdesc->Packet(&itemId, sizeof(itemId));

	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	PetInventorySystem::SendPetInventory(character);
	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	ecs::PointSystem::Compute(character);
	NetworkSyncSystem::PointsPacket(ownerEntity);

}

void CInputMain::PetInventoryItemMove(entt::entity character, const char* data)
{

	const auto p = reinterpret_cast<const TPacketCGPetInventoryItemMove*>(data);

	if (!data || !ecs::PlayerRuntime::IsPC(character) || !InventorySystem::CanHandleItems(character))
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(character) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(character) < GM_IMPLEMENTOR)
		return;
#endif

	if (PetInventorySystem::GetPetInventory(character) == entt::null)
		return;

	PetInventorySystem::MovePetInventoryItem(character, p->wPetPos, p->wDestPos);
	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	PetInventorySystem::SendPetInventory(character);

	if (!ecs::PlayerRuntime::IsPC(character))
		return;
	ecs::PointSystem::Compute(character);


}

