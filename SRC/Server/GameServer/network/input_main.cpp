#include "stdafx.h"
#include <Core/packet_string.h>
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/ViewSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/AcceSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/services/SpatialService.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/CharacterAccessors.hpp"


#include "constants.h"
#include "config.h"
#include "utils.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "protocol.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "cmd.h"
#include "shop.h"
#include "shop_manager.h"
#include "safebox.h"
#include "regen.h"
#include "battle.h"
#include "exchange.h"
#include "questmanager.h"
#include "profiler.h"
#include "messenger_manager.h"
#include "party.h"
#include "p2p.h"
#include "affect.h"
#include "guild.h"
#include "guild_manager.h"
#include "log.h"
#include "banword.h"
#include "empire_text_convert.h"
#include "unique_item.h"
#include "building.h"
#include "locale_service.h"
#include "gm.h"
#include "spam.h"


#include "motion.h"
#include "OXEvent.h"
#include "locale_service.h"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/EntityFactory.hpp"
#ifdef __NEWPET_SYSTEM__
#include "New_PetSystem.h"
#endif
#include "belt_inventory_helper.h" // @fixme119
#include "mount_inventory_helper.h"
#include "MountInventory.h"
#include "input.h"
#include "input_item_helpers.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/VIDRegistry.hpp"
#include "../ecs/components/combat_components.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/components/dirty_components.hpp"
#include "../ecs/components/movement_components.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"

#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#endif

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	#include "refine.h"
#endif

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
	#include "whisper_admin.h"
#endif
#ifdef __INGAME_WIKI__
#include "mob_manager.h"
#endif
#include <common/CommonDefines.h>
#include "../item/TypedItemOps.hpp"


#define ENABLE_CHECK_GHOSTMODE


static const int ComboSequenceBySkillLevel[3][8] =
{
	// 0   1   2   3   4   5   6   7
	{ 14, 15, 16, 17,  0,  0,  0,  0 },
	{ 14, 15, 16, 18, 20,  0,  0,  0 },
	{ 14, 15, 16, 18, 19, 17,  0,  0 },
};


#ifdef __SKILL_COLOR_SYSTEM__
void CInputMain::SetSkillColor(entt::entity character, const char* pcData)
{
    if (!pcData)
        return;
    TPacketCGSkillColor packet {};
    std::memcpy(&packet, pcData, sizeof(packet));
    SkillSystem::ChangeSkillColor(character, packet.skill,
        {packet.col1, packet.col2, packet.col3, packet.col4, packet.col5});
}
#endif


// SCRIPT_SELECT_ITEM
// END_OF_SCRIPT_SELECT_ITEM


bool IsInputInventoryPosition(TItemPos position)
{
    return position.window_type == INVENTORY
#ifdef ENABLE_EXTRA_INVENTORY
        || position.window_type == EXTRA_INVENTORY
#endif
        ;
}

bool IsInputItemAt(entt::entity owner, entt::entity item, TItemPos position)
{
    if (!ecs::PlayerRuntime::IsPC(owner) || !ItemSystem::IsValidItem(item))
        return false;
    const auto* ownership = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    return ownership && ownership->owner == owner && location &&
        location->window == position.window_type && location->cell == position.cell;
}

bool IsDetachedInputItem(entt::entity item)
{
    if (!ItemSystem::IsValidItem(item))
        return false;
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    return owner && owner->owner == entt::null && location && location->window == RESERVED_WINDOW;
}

bool RestoreInputItem(entt::entity owner, entt::entity item, TItemPos position)
{
    return ecs::PlayerRuntime::IsPC(owner) && IsDetachedInputItem(item) &&
        InventorySystem::IsEmptyItemGrid(owner, position, ItemSystem::GetItemSize(item)) &&
        ItemSystem::PlaceItemEcs(owner, item, position.window_type, position.cell);
}


// PARTY_JOIN_BUG_FIX

// END_OF_PARTY_JOIN_BUG_FIX


void CInputMain::Hack(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Hack handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGHack * p = (TPacketCGHack *) c_pData;

	char buf[sizeof(p->szBuf)];
	strlcpy(buf, p->szBuf, sizeof(buf));

	LOG_ERROR("HACK_DETECT: {} {}", ecs::PlayerRuntime::GetName(character).data(), buf);

	ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);
}




int CInputMain::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{

	if (!d || !c_pData)
		return 0;
	const entt::entity character = d->GetEntity();
	if (!ecs::PlayerRuntime::IsPC(character) || ecs::PlayerRuntime::GetDesc(character) != d)
	{
		LOG_ERROR("no character on desc");
		d->SetPhase(PHASE_CLOSE);
		return (0);
	}


	int iExtraLen = 0;

	if (test_server && bHeader != HEADER_CG_MOVE)
		LOG_INFO("CInputMain::Analyze() ==> Header [{}] ", bHeader);

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_TIME_SYNC:
			Handshake(d, c_pData);
			break;

		case HEADER_CG_CHAT:
			if ((iExtraLen = Chat(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_WHISPER:
			if ((iExtraLen = Whisper(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_MOVE:
			Move(character, c_pData);
			// @fixme103 (removed CheckClientVersion since useless in here)
			break;

		case HEADER_CG_CHARACTER_POSITION:
			Position(character, c_pData);
			break;

		case HEADER_CG_ITEM_USE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemUse(character, c_pData);
			break;

		case HEADER_CG_ITEM_DROP:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
			{
				ItemDrop(character, c_pData);
			}
			break;

#ifdef ENABLE_ACCE_SYSTEM
		case HEADER_CG_ACCE:
			Acce(character, c_pData);
			break;
#endif

		case HEADER_CG_ITEM_DROP2:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemDrop2(character, c_pData);
			break;
		case HEADER_CG_ITEM_DESTROY:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemDestroy(character, c_pData);
			break;
		case HEADER_CG_ITEM_DIVISION:
			{
				if (!ecs::PlayerRuntime::IsObserverMode(character))
					ItemDivision(character, c_pData);
			}
			break;
		case HEADER_CG_ITEM_MOVE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemMove(character, c_pData);
			break;


#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		case ENVANTER_BLACK:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				InventoryExpansion(character, c_pData);
		break;
#endif

		case HEADER_CG_ITEM_PICKUP:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemPickup(character, c_pData);
			break;

		case HEADER_CG_ITEM_USE_TO_ITEM:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemToItem(character, c_pData);
			break;

		case HEADER_CG_ITEM_GIVE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				ItemGive(character, c_pData);
			break;

		case HEADER_CG_EXCHANGE:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				Exchange(character, c_pData);
			break;

		case HEADER_CG_ATTACK:
		case HEADER_CG_SHOOT:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
			{
				Attack(character, bHeader, c_pData);
			}
			break;

		case HEADER_CG_USE_SKILL:
			if (!ecs::PlayerRuntime::IsObserverMode(character))
				UseSkill(character, c_pData);
			break;

#ifdef __SKILL_COLOR_SYSTEM__
		case HEADER_CG_SKILL_COLOR:
			SetSkillColor(character, c_pData);
			break;
#endif
#ifdef ENABLE_OPENSHOP_PACKET
		case HEADER_CG_OPENSHOP: {
				TPacketOpenShop* p = reinterpret_cast<TPacketOpenShop*>((void*)c_pData);
				if (p->shopid > 0) {
					if (!(ecs::PlayerRuntime::IsObserverMode(character) || ecs::SessionSystem::IsSafeboxOpen(character) || ecs::SocialSystem::HasExchange(character) || ecs::SessionSystem::IsCubeOpen(character) || CombatSystem::IsStun(character) || CombatSystem::IsDead(character)
#ifdef __ATTR_TRANSFER_SYSTEM__
						 || AttrTransfer_is_open(character)
#endif
					)) {
						const entt::entity shop = CShopManager::instance().Get(p->shopid);
						if (shop != entt::null) {
							ShopSystem::AddGuest(shop, character, 0, false);
							ecs::SocialSystem::SetShopOwner(character, entt::null);
						}
					}
				}
			} break;
#endif
		case HEADER_CG_QUICKSLOT_ADD:
			QuickslotAdd(character, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_DEL:
			QuickslotDelete(character, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_SWAP:
			QuickslotSwap(character, c_pData);
			break;

		case HEADER_CG_SHOP:
			if ((iExtraLen = Shop(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_MESSENGER:
			if ((iExtraLen = Messenger(character, c_pData, m_iBufferLeft))<0)
				return -1;
			break;


		case HEADER_CG_ON_CLICK:
			OnClick(character, c_pData);
			break;

		case HEADER_CG_SYNC_POSITION:
			if ((iExtraLen = SyncPosition(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_ADD_FLY_TARGETING:
		case HEADER_CG_FLY_TARGETING:
			FlyTarget(character, c_pData, bHeader);
			break;

		case HEADER_CG_SCRIPT_BUTTON:
			ScriptButton(character, c_pData);
			break;

			// SCRIPT_SELECT_ITEM
		case HEADER_CG_SCRIPT_SELECT_ITEM:
			ScriptSelectItem(character, c_pData);
			break;
			// END_OF_SCRIPT_SELECT_ITEM

		case HEADER_CG_SCRIPT_ANSWER:
			ScriptAnswer(character, c_pData);
			break;

		case HEADER_CG_QUEST_INPUT_STRING:
			QuestInputString(character, c_pData);
			break;

		case HEADER_CG_QUEST_CONFIRM:
			QuestConfirm(character, c_pData);
			break;

		case HEADER_CG_TARGET:
			Target(character, c_pData);
			break;

		case HEADER_CG_WARP:
			Warp(character, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKIN:
			SafeboxCheckin(character, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKOUT:
			SafeboxCheckout(character, c_pData, false);
			break;

		case HEADER_CG_SAFEBOX_ITEM_MOVE:
			SafeboxItemMove(character, c_pData);
			break;

		case HEADER_CG_MALL_CHECKOUT:
			SafeboxCheckout(character, c_pData, true);
			break;


		case HEADER_CG_MOUNT_INVENTORY_CHECKIN:
			MountInventoryCheckin(character, c_pData);
			break;

		case HEADER_CG_MOUNT_INVENTORY_CHECKOUT:
			MountInventoryCheckout(character, c_pData);
			break;

		case HEADER_CG_MOUNT_INVENTORY_ITEM_MOVE:
			MountInventoryItemMove(character, c_pData);
			break;

		case HEADER_CG_TYPED_ITEM_MOVE:
			HandleTypedItemMovePacket(character, c_pData);
			break;

		case HEADER_CG_TYPED_ITEM_USE:
			HandleTypedItemUsePacket(character, c_pData);
			break;

		case HEADER_CG_PARTY_INVITE:
			PartyInvite(character, c_pData);
			break;

		case HEADER_CG_PARTY_REMOVE:
			PartyRemove(character, c_pData);
			break;

		case HEADER_CG_PARTY_INVITE_ANSWER:
			PartyInviteAnswer(character, c_pData);
			break;

		case HEADER_CG_PARTY_SET_STATE:
			PartySetState(character, c_pData);
			break;

		case HEADER_CG_PARTY_USE_SKILL:
			PartyUseSkill(character, c_pData);
			break;

		case HEADER_CG_PARTY_PARAMETER:
			PartyParameter(character, c_pData);
			break;
#ifdef __INGAME_WIKI__
		case InGameWiki::HEADER_CG_WIKI:
			RecvWikiPacket(character, c_pData);
			break;
#endif
		case HEADER_CG_ANSWER_MAKE_GUILD:
#ifdef ENABLE_NEWGUILDMAKE
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "<%s> AnswerMakeGuild disabled", __FUNCTION__);
#else
			AnswerMakeGuild(character, c_pData);
#endif
			break;

		case HEADER_CG_GUILD:
			if ((iExtraLen = Guild(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_FISHING:
			Fishing(character, c_pData);
			break;
		case HEADER_CG_HACK:
			Hack(character, c_pData);
			break;

#ifdef __NEWPET_SYSTEM__
		case HEADER_CG_PetSetName:
			BraveRequestPetName(character, c_pData);
			break;
#endif
		case HEADER_CG_MYSHOP:
			if ((iExtraLen = MyShop(character, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_REFINE:
			Refine(character, c_pData);
			break;

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
		case HEADER_CG_WHISPER_ADMIN:
			CWhisperAdmin::instance().Manager(character, c_pData);
			break;
#endif

		case HEADER_CG_CLIENT_VERSION:
			Version(character, c_pData);
			break;


#ifdef ENABLE_MULTI_LANGUAGE
		case HEADER_CG_CHANGE_LANGUAGE:
			{
				TPacketChangeLanguage* p = reinterpret_cast <TPacketChangeLanguage*>((void*)c_pData);
				ChangeLanguage(character, p->bLanguage);
			}
			break;
		case HEADER_CG_REQUEST_LANGUAGE:
			{
				TPacketRequestLang* p = reinterpret_cast <TPacketRequestLang*>((void*)c_pData);
				RequestLanguage(character, p->targetName);
			}
			break;
#endif
#ifdef __SEND_TARGET_INFO__
		case HEADER_CG_TARGET_INFO_LOAD:
			{
				TargetInfoLoad(character, c_pData);
			}
			break;
#endif
#ifdef ENABLE_SWITCHBOT
		case HEADER_CG_SWITCHBOT:
			if ((iExtraLen = Switchbot(character, c_pData, m_iBufferLeft)) < 0)
			{
				return -1;
			}
			break;
#endif
#ifdef ENABLE_MAP_TELEPORTER
		case HEADER_CG_MAP_TELEPORTER:
			MapTeleporter(character, (TPacketCGMapTeleporter*) c_pData);
			break;
#endif
#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
		case HEADER_CG_CUBE_RENEWAL:
			CubeRenewalSend(character, c_pData);
			break;
#endif
#ifdef ENABLE_NEW_FISHING_SYSTEM
		case HEADER_CG_FISHING_NEW:
			{
				FishingNew(character, c_pData);
			}
			break;
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
		case HEADER_CG_WHEEL_DESTINY:
		{
			WheelDestiny(character, c_pData);
		}
		break;
#endif
	}
	return (iExtraLen);
}

int CInputDead::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{
	if (!d || !c_pData)
		return 0;
	const entt::entity character = d->GetEntity();
	if (!ecs::PlayerRuntime::IsPC(character) || ecs::PlayerRuntime::GetDesc(character) != d)
	{
		LOG_ERROR("no character on desc");
		return 0;
	}


	int iExtraLen = 0;

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_TIME_SYNC:
			Handshake(d, c_pData);
			break;

		case HEADER_CG_CHAT:
			if ((iExtraLen = Chat(character, c_pData, m_iBufferLeft)) < 0)
				return -1;

			break;

		case HEADER_CG_WHISPER:
			if ((iExtraLen = Whisper(character, c_pData, m_iBufferLeft)) < 0)
				return -1;

			break;

		case HEADER_CG_HACK:
			Hack(character, c_pData);
			break;

		default:
			return (0);
	}

	return (iExtraLen);
}


#ifdef ENABLE_MULTI_LANGUAGE
void CInputMain::ChangeLanguage(entt::entity character, uint8_t bLanguage)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	if (!ecs::PlayerRuntime::GetDesc(character))
		return;

	uint8_t bCurrentLanguage = ecs::PlayerRuntime::GetDesc(character)->GetLanguage();

	if(bCurrentLanguage == bLanguage)
		return;

	if(bLanguage > LANGUAGE_DEFAULT && bLanguage < LANGUAGE_MAX_NUM)
	{
		DBManager::instance().Query("UPDATE account.account SET language = %d WHERE id = %d;", bLanguage, ecs::PlayerRuntime::GetAccountID(character));
		ecs::PlayerRuntime::GetDesc(character)->SetLanguage(bLanguage);
	}
}

void CInputMain::RequestLanguage(entt::entity character, const char* targetName)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate RequestLanguage handler ECS
// DUAL-PATH: legacy only during migration window
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	LPDESC d = ecs::PlayerRuntime::GetDesc(character);
	if (!d)
		return;

	int id = 0;
	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQueryPrepared(
		"SELECT account_id FROM player.player WHERE name=?",
		{ SqlParam::Text(targetName) }));
	if (pMsg->Get()->uiNumRows != 0) {
		MYSQL_ROW row = pMsg->Get()->FetchRow();
		id = atoi(row[0]);
	}

	if (id == 0)
		return;

	std::unique_ptr<SQLMsg> pMsg2(DBManager::instance().DirectQueryPrepared(
		"SELECT language FROM account.account WHERE id=?",
		{ SqlParam::UInt(static_cast<uint32_t>(id)) }));
	if (pMsg2->Get()->uiNumRows != 0) {
		MYSQL_ROW row = pMsg2->Get()->FetchRow();

		TPacketRecvLang p;
		p.bHeader = HEADER_GC_RECV_LANGUAGE;
		strncpy(p.targetName, targetName, sizeof(p.targetName));
		strncpy(p.targetLanguage, row[0], sizeof(p.targetLanguage));
		d->Packet(&p, sizeof(p));
	}
}
#endif

