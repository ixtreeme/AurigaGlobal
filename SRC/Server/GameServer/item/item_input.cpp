#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "TypedLoot.hpp"
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
#include "ForgeNpcs.hpp"
#include "../ecs/components/dirty_components.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include "../ecs/systems/PlayerItemActions.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "desc_client.h"
#include "gm.h"
#include "questmanager.h"

void CInputMain::ItemPickup(entt::entity character, const char* data)
{
    if (!data || !ecs::PlayerRuntime::IsPC(character)) return;
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    const auto gm = ecs::PlayerRuntime::GetGMLevel(character);
    if (gm > GM_PLAYER && gm < GM_IMPLEMENTOR) return;
#endif
    command_item_pickup packet {};
    memcpy(&packet, data, sizeof(packet));
    PickupTypedLoot(character, packet.vid);
}

void CInputMain::QuickslotAdd(entt::entity character, const char* data)
{
    if (!data) return;
    command_quickslot_add packet {};
    memcpy(&packet, data, sizeof(packet));
    PlayerItemActions::SetQuickslot(character, packet.pos, packet.slot);
}

void CInputMain::QuickslotDelete(entt::entity character, const char* data)
{
    if (!data) return;
    command_quickslot_del packet {};
    memcpy(&packet, data, sizeof(packet));
    PlayerItemActions::DelQuickslot(character, packet.pos);
}

void CInputMain::QuickslotSwap(entt::entity character, const char* data)
{
    if (!data) return;
    command_quickslot_swap packet {};
    memcpy(&packet, data, sizeof(packet));
    PlayerItemActions::SwapQuickslot(character, packet.pos, packet.change_pos);
}

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
void CInputMain::WheelDestiny(entt::entity character, const char* data)
{
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
        if (auto wheel = ecs::PlayerRuntime::GetWheelDestiny(character))
        {
            wheel->Open();
            break;
        }
        auto wheel = std::make_shared<CWheelDestiny>(character);
        if (wheel->Open())
            ecs::PlayerRuntime::SetWheelDestiny(character, std::move(wheel));
        else
            ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "A Szerencsekerék jelenleg nem érhető el.");
        break;
    }
    case CLOSE:
    {
        if (auto wheel = ecs::PlayerRuntime::GetWheelDestiny(character))
        {
            if (!wheel->CanClose())
            {
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1307, "");
#endif
                break;
            }
            wheel->Close();
            ecs::PlayerRuntime::SetWheelDestiny(character, nullptr);
        }
        break;
    }
	case TURN:
	{
		if (ecs::SocialSystem::GetDungeon(character) != entt::null || ecs::PlayerRuntime::GetMapIndex(character) >= 10000)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Dungeonban nem tudsz p�rgetni./You cannot in dungeon");
			return;
		}
		if (ecs::PlayerRuntime::GetWheelDestiny(character))
		{
			ecs::PlayerRuntime::GetWheelDestiny(character)->TurnWheel();
		}
	}
	break;

	case GIVE:
	{
		if (ecs::PlayerRuntime::GetWheelDestiny(character))
		{
			ecs::PlayerRuntime::GetWheelDestiny(character)->FinishAnimation();
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
