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
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
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

int CInputMain::Shop(entt::entity character, const char * data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Shop handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGShop * p = (TPacketCGShop *) data;

	if (uiBytes < sizeof(TPacketCGShop))
		return -1;

	if (test_server)
		LOG_INFO("CInputMain::Shop() ==> SubHeader {}", p->subheader);

	const char * c_pData = data + sizeof(TPacketCGShop);
	uiBytes -= sizeof(TPacketCGShop);

	switch (p->subheader)
	{
		case SHOP_SUBHEADER_CG_END:
			LOG_INFO("INPUT: {} SHOP: END", ecs::PlayerRuntime::GetName(character).data());
			CShopManager::instance().StopShopping(character);
			return 0;

		case SHOP_SUBHEADER_CG_BUY:
			{
				if (uiBytes < sizeof(uint8_t) + sizeof(uint8_t))
					return -1;

				uint8_t bPos = *(c_pData + 1);
				LOG_INFO("INPUT: {} SHOP: BUY {}", ecs::PlayerRuntime::GetName(character).data(), bPos);
				CShopManager::instance().Buy(character, bPos);
				return (sizeof(uint8_t) + sizeof(uint8_t));
			}
#ifndef ENABLE_EXTRA_INVENTORY
		case SHOP_SUBHEADER_CG_SELL:
			{
				if (uiBytes < sizeof(uint8_t))
					return -1;

				uint8_t pos = *c_pData;

				LOG_INFO("INPUT: {} SHOP: SELL", ecs::PlayerRuntime::GetName(character).data());
				CShopManager::instance().Sell(character, pos);
				return sizeof(uint8_t);
			}
#endif
		case SHOP_SUBHEADER_CG_SELL2:
			{
				if (uiBytes < sizeof(uint8_t)
#ifdef ENABLE_EXTRA_INVENTORY
				+ sizeof(uint16_t)
#else
				+ sizeof(uint8_t)
#endif
#ifdef ENABLE_NEW_STACK_LIMIT
				+ sizeof(uint16_t)
#else
				+ sizeof(uint8_t)
#endif
				)
					return -1;

#ifdef ENABLE_EXTRA_INVENTORY
				uint8_t window = *(c_pData);
				uint16_t cell = *(uint16_t*)(c_pData + 1);
#else
				uint8_t pos = *(c_pData++);
#endif
#ifdef ENABLE_NEW_STACK_LIMIT
				uint16_t count = *(uint16_t*)(c_pData + sizeof(uint16_t));
#else
				uint8_t count = *(c_pData);
#endif

				LOG_INFO("INPUT: {} SHOP: SELL2", ecs::PlayerRuntime::GetName(character).data());
				CShopManager::instance().Sell(character,
#ifdef ENABLE_EXTRA_INVENTORY
				TItemPos(window, cell),
#else
				pos,
#endif
				count);
				return sizeof(uint8_t)
#ifdef ENABLE_EXTRA_INVENTORY
				+ sizeof(uint16_t)
#else
				+ sizeof(uint8_t)
#endif
#ifdef ENABLE_NEW_STACK_LIMIT
				+ sizeof(uint16_t)
#else
				+ sizeof(uint8_t)
#endif
				;
			}
#ifdef ENABLE_BUY_STACK_FROM_SHOP
		case SHOP_SUBHEADER_CG_BUY2:
			{
				size_t size = sizeof(uint8_t) + sizeof(uint8_t);
				if (uiBytes < size) {
					return -1;
				}

				uint8_t p = *(c_pData++);
				uint8_t c = *(c_pData);
				LOG_INFO("INPUT: {} SHOP: MULTIPLE BUY {} COUNT {}", ecs::PlayerRuntime::GetName(character).data(), p, c);
				CShopManager::instance().MultipleBuy(character, p, c);
				return size;
			}
#endif
		default:
			LOG_ERROR("CInputMain::Shop : Unknown subheader {} : {}", p->subheader, ecs::PlayerRuntime::GetName(character).data());
			break;
	}

	return 0;
}

void CInputMain::Exchange(entt::entity character, const char* data)
{
    if (!ecs::PlayerRuntime::IsPC(character) || !data)
        return;
    // The packet decoder already checked the fixed packet size. Copying avoids
    // alignment assumptions and keeps the request stable across callbacks.
    command_exchange request {};
    std::memcpy(&request, data, sizeof(request));
    switch (request.sub_header)
    {
        case EXCHANGE_SUBHEADER_CG_START:
            if (request.arg1 > 0 && request.arg1 <= UINT32_MAX)
                ExchangeSystem::Start(character, CHARACTER_MANAGER::instance().FindEntity(static_cast<uint32_t>(request.arg1)));
            break;
        case EXCHANGE_SUBHEADER_CG_ITEM_ADD:
            ExchangeSystem::AddItem(character, request.Pos, request.arg2);
            break;
        case EXCHANGE_SUBHEADER_CG_ITEM_DEL:
            if (request.arg1 >= 0 && request.arg1 < EXCHANGE_ITEM_MAX_NUM)
                ExchangeSystem::RemoveItem(character, static_cast<uint32_t>(request.arg1));
            break;
        case EXCHANGE_SUBHEADER_CG_ELK_ADD:
            ExchangeSystem::AddGold(character, request.arg1);
            break;
        case EXCHANGE_SUBHEADER_CG_ACCEPT:
            ExchangeSystem::Accept(character);
            break;
        case EXCHANGE_SUBHEADER_CG_CANCEL:
            // Closing is always allowed, including after death/window changes.
            ExchangeSystem::Cancel(character);
            break;
    }
}

int CInputMain::MyShop(entt::entity character, const char * c_pData, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate MyShop handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGMyShop * p = (TPacketCGMyShop *) c_pData;
	int iExtraLen = p->bCount * sizeof(TShopItemTable);

	if (uiBytes < sizeof(TPacketCGMyShop) + iExtraLen)
		return -1;

	if (ecs::PointSystem::GetGold(character) >= GOLD_MAX)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 226,
		"%lld"

		, GOLD_MAX);
#endif
		return (iExtraLen);
	}

	if (CombatSystem::IsStun(character) || CombatSystem::IsDead(character))
		return (iExtraLen);

	if (ecs::SocialSystem::HasExchange(character) || ecs::SessionSystem::IsSafeboxOpen(character) || ecs::SocialSystem::GetShopOwner(character) != entt::null || ecs::SessionSystem::IsCubeOpen(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 292, "");
#endif
		return (iExtraLen);
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (AttrTransfer_is_open(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 292, "");
#endif
		return (iExtraLen);
	}
#endif

	LOG_INFO("MyShop count {}", p->bCount);
	ecs::SocialSystem::OpenMyShop(character, p->szSign, (TShopItemTable *) (c_pData + sizeof(TPacketCGMyShop)), p->bCount
#ifdef KASMIR_PAKET_SYSTEM
	, p->dwKasmirNpc, p->bKasmirBaslik
#endif
	);
	return (iExtraLen);
}
