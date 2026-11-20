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
#include "exchange.h"
#include "messenger_manager.h"
#include "shop.h"
#include "shop_manager.h"
#include "new_switchbot.h"
#include "priv_manager.h"
#include "buffer_manager.h"
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

int CInputMain::Messenger(entt::entity character, const char* c_pData, uint64_t uiBytes)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character))
		return -1;
	TPacketCGMessenger* p = (TPacketCGMessenger*) c_pData;

	if (uiBytes < sizeof(TPacketCGMessenger))
		return -1;

	c_pData += sizeof(TPacketCGMessenger);
	uiBytes -= sizeof(TPacketCGMessenger);

	switch (p->subheader)
	{
		case MESSENGER_SUBHEADER_CG_ADD_BY_VID:
			{
				if (uiBytes < sizeof(TPacketCGMessengerAddByVID))
					return -1;

				TPacketCGMessengerAddByVID * p2 = (TPacketCGMessengerAddByVID *) c_pData;
				const entt::entity ch_companionEntity = CHARACTER_MANAGER::instance().FindEntity(p2->vid);


				if (!ecs::PlayerRuntime::IsPC(ch_companionEntity))
					return sizeof(TPacketCGMessengerAddByVID);

				if (ecs::PlayerRuntime::IsObserverMode(character))
					return sizeof(TPacketCGMessengerAddByVID);

				if (ecs::PlayerRuntime::IsBlockMode(ch_companionEntity, BLOCK_MESSENGER_INVITE))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 370, "%s", ecs::PlayerRuntime::GetName(ch_companionEntity).data());
#endif
					return sizeof(TPacketCGMessengerAddByVID);
				}

				LPDESC d = ecs::PlayerRuntime::GetDesc(ch_companionEntity);

				if (!d)
					return sizeof(TPacketCGMessengerAddByVID);

				if (ecs::PlayerRuntime::GetGMLevel(character) == GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(ch_companionEntity) != GM_PLAYER)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 184, "");
#endif
					return sizeof(TPacketCGMessengerAddByVID);
				}

				if (ecs::PlayerRuntime::GetDesc(character) == d)
					return sizeof(TPacketCGMessengerAddByVID);

				MessengerManager::instance().RequestToAdd(character, ch_companionEntity);
			}
			return sizeof(TPacketCGMessengerAddByVID);

		case MESSENGER_SUBHEADER_CG_ADD_BY_NAME:
			{
				if (uiBytes < CHARACTER_NAME_MAX_LEN)
					return -1;

				char name[CHARACTER_NAME_MAX_LEN + 1];
				memcpy(name, c_pData, CHARACTER_NAME_MAX_LEN);
				name[CHARACTER_NAME_MAX_LEN] = '\0';

				if (ecs::PlayerRuntime::GetGMLevel(character) == GM_PLAYER && gm_get_level(name) != GM_PLAYER)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 184, "");
#endif
					return CHARACTER_NAME_MAX_LEN;
				}

				const entt::entity tch = CHARACTER_MANAGER::instance().FindPCEntity(name);
				if (ecs::PlayerRuntime::IsPC(tch))
				{
					if (tch == character)
						return CHARACTER_NAME_MAX_LEN;

					if (ecs::PlayerRuntime::IsBlockMode(tch, BLOCK_MESSENGER_INVITE) == true)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 370, "%s", ecs::PlayerRuntime::GetName(tch).data());
#endif
					}
					else
					{
						MessengerManager::instance().RequestToAdd(character, tch);
					}
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 108, "%s", name);
				}
#endif
			}
			return CHARACTER_NAME_MAX_LEN;

		case MESSENGER_SUBHEADER_CG_REMOVE:
			{
				if (uiBytes < CHARACTER_NAME_MAX_LEN)
					return -1;

				char char_name[CHARACTER_NAME_MAX_LEN + 1];
				memcpy(char_name, c_pData, CHARACTER_NAME_MAX_LEN);
				char_name[CHARACTER_NAME_MAX_LEN] = '\0';
				MessengerManager::instance().RemoveFromList(ecs::PlayerRuntime::GetName(character).data(), char_name);
#ifdef ENABLE_BUG_FIXES
				MessengerManager::instance().RemoveFromList(char_name, ecs::PlayerRuntime::GetName(character).data());
#endif
			}
			return CHARACTER_NAME_MAX_LEN;

		default:
			LOG_ERROR("CInputMain::Messenger : Unknown subheader {} : {}", p->subheader, ecs::PlayerRuntime::GetName(character).data());
			break;
	}

	return 0;
}

#ifdef ENABLE_SWITCHBOT
int CInputMain::Switchbot(entt::entity character, const char* data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Switchbot handler ECS
// DUAL-PATH: legacy only during migration window
	const TPacketCGSwitchbot* p = reinterpret_cast<const TPacketCGSwitchbot*>(data);

	if (uiBytes < sizeof(TPacketCGSwitchbot))
	{
		return -1;
	}

	const char* c_pData = data + sizeof(TPacketCGSwitchbot);
	uiBytes -= sizeof(TPacketCGSwitchbot);

	switch (p->subheader)
	{
	case SUBHEADER_CG_SWITCHBOT_START:
	{
		size_t extraLen = sizeof(TSwitchbotAttributeAlternativeTable) * SWITCHBOT_ALTERNATIVE_COUNT;
		if (uiBytes < extraLen)
		{
			return -1;
		}

		std::vector<TSwitchbotAttributeAlternativeTable> vec_alternatives;

		for (uint8_t alternative = 0; alternative < SWITCHBOT_ALTERNATIVE_COUNT; ++alternative)
		{
			const TSwitchbotAttributeAlternativeTable* pAttr = reinterpret_cast<const TSwitchbotAttributeAlternativeTable*>(c_pData);
			c_pData += sizeof(TSwitchbotAttributeAlternativeTable);

			vec_alternatives.emplace_back(*pAttr);
		}

		CSwitchbotManager::Instance().Start(ecs::PlayerRuntime::GetPlayerID(character), p->slot, vec_alternatives);
		return extraLen;
	}

	case SUBHEADER_CG_SWITCHBOT_STOP:
	{
		CSwitchbotManager::Instance().Stop(ecs::PlayerRuntime::GetPlayerID(character), p->slot);
		return 0;
	}
	}

	return 0;
}
#endif
