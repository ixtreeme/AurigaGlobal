#include "stdafx.h"
#include "ecs/systems/InventorySystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/SkillSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "utils.h"
#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "item_manager.h"
#include "sectree_manager.h"
#include "mob_manager.h"
#include "packet.h"
#include "cmd.h"
#include "regen.h"
#include "guild.h"
#include "guild_manager.h"
#include "p2p.h"
#include "buffer_manager.h"
#include "fishing.h"
#include "mining.h"
#include "questmanager.h"
#include "vector.h"
#include "affect.h"
#include "db.h"
#include "priv_manager.h"
#include "building.h"
#include "battle.h"
#include "arena.h"
#include "start_position.h"
#include "party.h"
#include <string_view>
#include "BattleArena.h"
#include "log.h"
#include "pcbang.h"
#include "unique_item.h"
#include "DragonSoul.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/Registry.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/VIDRegistry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/ActivitySystem.hpp"
#include <common/CommonDefines.h>
#include "LostCastleDungeon.h"
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
#endif

#ifdef __NEWPET_SYSTEM__
#include "New_PetSystem.h"
#include "ecs/CharacterAccessors.hpp"
#endif

extern bool DropEvent_RefineBox_SetValue(const std::string& name, int value);

// ADD_COMMAND_SLOW_STUN
enum
{
	COMMANDAFFECT_STUN,
	COMMANDAFFECT_SLOW,
};

void Command_ApplyAffect(LPCHARACTER ch, const char* argument, const char* affectName, int cmdAffect)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	LOG_INFO("{}", arg1);

	if (!*arg1)
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Usage: %s <name>", affectName);
		return;
	}

	const entt::entity tch = CHARACTER_MANAGER::instance().FindPCEntity(arg1);
	if (tch == entt::null)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 800, "%s", arg1);
#endif
		return;
	}

	switch (cmdAffect)
	{
		case COMMANDAFFECT_STUN:
			SkillAttackAffect(tch, 1000, IMMUNE_STUN, AFFECT_STUN, POINT_NONE, 0, AFF_STUN, 30, "GM_STUN");
			break;
		case COMMANDAFFECT_SLOW:
			SkillAttackAffect(tch, 1000, IMMUNE_SLOW, AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, 30, "GM_SLOW");
			break;
	}

	LOG_INFO("{} {}", arg1, affectName);
}
// END_OF_ADD_COMMAND_SLOW_STUN

// ---------------- GM: dungeon-fuggetlen klon teszt ----------------
// Hasznalat:
//   /spawn_clon sourcePlayer targetPlayer [count]
//   /spawn_clon targetPlayer [count]   (source = te)
//   /p_clon                (a te targetPid-edhez tartozo klonok torlese ezen a mapon)
//   /p_clon all            (osszes teszt klon torlese ezen a mapon)
//   /p_clon targetPlayer   (targetPlayer-hez tartozo klonok torlese ezen a mapon)

//ACMD(do_spawn_clon)
//{
//    if (!ch)
//        return;
//
//    char a1[256], a2[256], a3[256];
//    argument = one_argument(argument, a1, sizeof(a1));
//    argument = one_argument(argument, a2, sizeof(a2));
//    one_argument(argument, a3, sizeof(a3));
//
//    if (!*a1)
//    {
//        ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Hasznalat: /spawn_clon sourcePlayer targetPlayer [count]  |  /spawn_clon targetPlayer [count]");
//        return;
//    }
//
//    LPCHARACTER source = nullptr;
//    LPCHARACTER target = nullptr;
//    int count = 1;
//
//    if (*a2)
//    {
//        // /spawn_clon player1 player2 [count]
//        source = CHARACTER_MANAGER::instance().FindPC(a1);
//        target = CHARACTER_MANAGER::instance().FindPC(a2);
//        if (*a3)
//            str_to_number(count, a3);
//    }
//    else
//    {
//        // /spawn_clon targetPlayer [count]  (source = te)
//        source = ch;
//        target = CHARACTER_MANAGER::instance().FindPC(a1);
//        if (*a3)
//            str_to_number(count, a3);
//    }
//
//    if (!source || !target)
//    {
//        ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Hibas nev / celpont.");
//        return;
//    }
//
//    if (source->IsFakePlayer() || target->IsFakePlayer())
//    {
//        ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Klonra nem lehet klont inditani.");
//        return;
//    }
//
//    if (!ecs::PlayerRuntime::GetDesc(((source) ? (source)->GetEntityHandle() : entt::null)) || !ecs::PlayerRuntime::GetDesc(((target) ? (target)->GetEntityHandle() : entt::null)))
//    {
//        ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "A celpont nem elerheto.");
//        return;
//    }
//
//    if (count <= 0) count = 1;
//
//    if (!CLostCastleDungeon::instance().SpawnTestClones(source, target, count))
//    {
//        ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Nem sikerult klont spawnolni.");
//        return;
//    }
//
//    ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Klon spawn kesz: masolat=%s, target=%s, db=%d", ecs::PlayerRuntime::GetName(((source) ? (source)->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetName(((target) ? (target)->GetEntityHandle() : entt::null)).data(), count);
//}
//
//ACMD(do_p_clon)
//{
//    if (!ch)
//        return;
//
//    char a1[256];
//    one_argument(argument, a1, sizeof(a1));
//
//    const int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(((ch) ? (ch)->GetEntityHandle() : entt::null));
//
//    if (!*a1)
//    {
//        CLostCastleDungeon::instance().PurgeTestClonesForTargetPID((ecs::PlayerRuntime::GetPlayerID(((ch) ? (ch)->GetEntityHandle() : entt::null))), mapIndex);
//        ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Klonok torolve (target: te, map: %d)", mapIndex);
//        return;
//    }
//
//    if (!strncasecmp(a1, "all", 3))
//    {
//        CLostCastleDungeon::instance().PurgeTestClonesOnMap(mapIndex);
//        ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Osszes klon torolve ezen a mapon: %d", mapIndex);
//        return;
//    }
//
//    LPCHARACTER target = CHARACTER_MANAGER::instance().FindPC(a1);
//    if (!target)
//    {
//        ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Nincs ilyen player: %s", a1);
//        return;
//    }
//
//    CLostCastleDungeon::instance().PurgeTestClonesForTargetPID(ecs::PlayerRuntime::GetPlayerID(((target) ? (target)->GetEntityHandle() : entt::null)), mapIndex);
//    ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Klonok torolve (target: %s, map: %d)", ecs::PlayerRuntime::GetName(((target) ? (target)->GetEntityHandle() : entt::null)).data(), mapIndex);
//}


ACMD(do_pcbang_update)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	unsigned long PCBangID = 0;

	if (*arg1 == '\0')
		PCBangID = 0;
	else
		str_to_number(PCBangID, arg1);

	if (PCBangID == 0)
	{
		CPCBangManager::instance().RequestUpdateIPList(0);
	}
	else
	{
		CPCBangManager::instance().RequestUpdateIPList(PCBangID);
	}

	TPacketPCBangUpdate packet;
	packet.bHeader = HEADER_GG_PCBANG_UPDATE;
	packet.ulPCBangID = PCBangID;

	P2P_MANAGER::instance().Send(&packet, sizeof(TPacketPCBangUpdate));

}

ACMD(do_pcbang_check)
{
#ifdef TEXTS_IMPROVEMENT
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (CPCBangManager::instance().IsPCBangIP(arg1) == true) {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 801, "%s", arg1);
	} else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 802, "%s", arg1);
	}
#endif
}

ACMD(do_stun)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	Command_ApplyAffect(ch, argument, "stun", COMMANDAFFECT_STUN);
}

ACMD(do_slow)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	Command_ApplyAffect(ch, argument, "slow", COMMANDAFFECT_SLOW);
}

ACMD(do_transfer)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: transfer <name>");
		return;
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);
	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(arg1);

		if (pkCCI)
		{
			if (pkCCI->bChannel != g_bChannel)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 803, "%s#%d#%d", arg1, pkCCI->bChannel, g_bChannel);
#endif
				return;
			}

			TPacketGGTransfer pgg;

			pgg.bHeader = HEADER_GG_TRANSFER;
			strlcpy(pgg.szName, arg1, sizeof(pgg.szName));
			pgg.lX = ecs::PlayerRuntime::GetX(character);
			pgg.lY = ecs::PlayerRuntime::GetY(character);

			P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGTransfer));
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 804, "");
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 501, "%s", arg1);
		}
#endif
		return;
	}

	if (ch == tch) {
		return;
	}

	//tch->Show(ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ch->GetZ());
	ecs::MovementSystem::WarpSet(((tch) ? (tch)->GetEntityHandle() : entt::null), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetMapIndex(character));
}

// LUA_ADD_GOTO_INFO
struct GotoInfo
{
	std::string 	st_name;

	uint8_t 	empire;
	int 	mapIndex;
	uint32_t 	x, y;

	GotoInfo()
	{
		st_name 	= "";
		empire 		= 0;
		mapIndex 	= 0;

		x = 0;
		y = 0;
	}
	GotoInfo(const GotoInfo& c_src)
	{
		__copy__(c_src);
	}
	void operator = (const GotoInfo& c_src)
	{
		__copy__(c_src);
	}
	void __copy__(const GotoInfo& c_src)
	{
		st_name 	= c_src.st_name;
		empire 		= c_src.empire;
		mapIndex 	= c_src.mapIndex;

		x = c_src.x;
		y = c_src.y;
	}
};

static std::vector<GotoInfo> gs_vec_gotoInfo;

void CHARACTER_AddGotoInfo(std::string_view c_st_name, uint8_t empire, int mapIndex, uint32_t x, uint32_t y)
{
	GotoInfo newGotoInfo;
	newGotoInfo.st_name.assign(c_st_name.data(), c_st_name.size());
	newGotoInfo.empire = empire;
	newGotoInfo.mapIndex = mapIndex;
	newGotoInfo.x = x;
	newGotoInfo.y = y;
	gs_vec_gotoInfo.emplace_back(newGotoInfo);

	LOG_INFO("AddGotoInfo(name={}, empire={}, mapIndex={}, pos=({}, {}))", c_st_name, static_cast<int>(empire), mapIndex, x, y);
}

bool FindInString(const char * c_pszFind, const char * c_pszIn)
{
	const char * c = c_pszIn;
	const char * p;

	p = strchr(c, '|');

	if (!p)
		return (0 == strncasecmp(c_pszFind, c_pszIn, strlen(c_pszFind)));
	else
	{
		char sz[64 + 1];

		do
		{
			strlcpy(sz, c, MIN(sizeof(sz), (p - c) + 1));

			if (!strncasecmp(c_pszFind, sz, strlen(c_pszFind)))
				return true;

			c = p + 1;
		} while ((p = strchr(c, '|')));

		strlcpy(sz, c, sizeof(sz));

		if (!strncasecmp(c_pszFind, sz, strlen(c_pszFind)))
			return true;
	}

	return false;
}

bool CHARACTER_GoToName(LPCHARACTER ch, uint8_t empire, int mapIndex, const char* gotoName)
{
	//std::vector<GotoInfo>::iterator i;
	for (auto i = gs_vec_gotoInfo.begin(); i != gs_vec_gotoInfo.end(); ++i)
	{
		const GotoInfo& c_eachGotoInfo = *i;

		if (mapIndex != 0)
		{
			if (mapIndex != c_eachGotoInfo.mapIndex)
				continue;
		}
		else if (!FindInString(gotoName, c_eachGotoInfo.st_name.c_str()))
			continue;

		if (c_eachGotoInfo.empire == 0 || c_eachGotoInfo.empire == empire)
		{
			const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
			int x = c_eachGotoInfo.x * 100;
			int y = c_eachGotoInfo.y * 100;
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 737, "%d#%d", x, y);
#endif
			ecs::MovementSystem::WarpSet(chEntity, x, y);
			ecs::MovementSystem::Stop(chEntity);
			return true;
		}
	}
	return false;
}

ACMD(do_goto)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256];
	int x = 0, y = 0, z = 0;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 && !*arg2)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: goto <x meter> <y meter>");
		return;
	}

	if (isnhdigit(*arg1) && isnhdigit(*arg2))
	{
		str_to_number(x, arg1);
		str_to_number(y, arg2);

		PIXEL_POSITION p;

		if (SECTREE_MANAGER::instance().GetMapBasePosition(ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), p))
		{
			x += p.x / 100;
			y += p.y / 100;
		}

#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 737, "%d#%d", x, y);
#endif
	}
	else
	{
		int mapIndex = 0;
		uint8_t empire = 0;

		if (*arg1 == '#')
			str_to_number(mapIndex,  (arg1 + 1));

		if (*arg2 && isnhdigit(*arg2))
		{
			str_to_number(empire, arg2);
			empire = MINMAX(1, empire, 3);
		}
		else
			empire = (ecs::PlayerRuntime::GetEmpire(character));

		if (CHARACTER_GoToName(ch, empire, mapIndex, arg1))
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Cannot find map command syntax: /goto <mapname> [empire]");
			return;
		}

		return;
	}

	x *= 100;
	y *= 100;

	ecs::MovementSystem::Show(character, ecs::PlayerRuntime::GetMapIndex(character), x, y, z);
	ecs::MovementSystem::Stop(character);
}

ACMD(do_warp)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: warp <character name> | <x meter> <y meter>");
		return;
	}

	int x = 0, y = 0;
#ifdef __CMD_WARP_IN_DUNGEON__
	int mapIndex = 0;
#endif

	if (isnhdigit(*arg1) && isnhdigit(*arg2))
	{
		str_to_number(x, arg1);
		str_to_number(y, arg2);
	}
	else
	{
		LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);
		const entt::entity tchEntity = tch ? tch->GetEntityHandle() : entt::null;


		if (nullptr == tch)
		{
			const CCI* pkCCI = P2P_MANAGER::instance().Find(arg1);

			if (nullptr != pkCCI)
			{
				if (pkCCI->bChannel != g_bChannel)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 803, "%s#%d#%d", arg1, pkCCI->bChannel, g_bChannel);
#endif
					return;
				}

				ch->WarpToPID( pkCCI->dwPID );
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 501, "%s", arg1);
			}
#endif
			return;
		}
		else
		{
			x = ecs::PlayerRuntime::GetX(tchEntity) / 100;
			y = ecs::PlayerRuntime::GetY(tchEntity) / 100;
#ifdef __CMD_WARP_IN_DUNGEON__
			mapIndex = ecs::PlayerRuntime::GetMapIndex(tchEntity);
#endif
		}
	}

	x *= 100;
	y *= 100;

#ifdef __CMD_WARP_IN_DUNGEON__
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 805, "%d#%d#%d", mapIndex, x, y);
#endif
	ecs::MovementSystem::WarpSet(character, x, y, mapIndex);
#else
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 737, "%d#%d", x, y);
#endif
	ecs::MovementSystem::WarpSet(character, x, y);
#endif


	ecs::MovementSystem::Stop(character);
}

#ifdef ENABLE_NEWSTUFF
ACMD(do_rewarp)
{
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 737, "%d#%d", ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character));
#endif
	ecs::MovementSystem::WarpSet(character, ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character));
	ecs::MovementSystem::Stop(character);
}
#endif

ACMD(do_item)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: item <item vnum>");
		return;
	}

	int iCount = 1;

	if (*arg2)
	{
		str_to_number(iCount, arg2);
		iCount = MINMAX(1, iCount, g_bItemCountLimit);
	}

	uint32_t dwVnum;

	if (isnhdigit(*arg1))
		str_to_number(dwVnum, arg1);
	else
	{
		if (!ITEM_MANAGER::instance().GetVnum(arg1, dwVnum))
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "#%u item not exist by that vnum(%s).", dwVnum, arg1);
			return;
		}
	}

	LPITEM item = ITEM_MANAGER::instance().CreateItem(dwVnum, iCount, 0, true);

	if (item)
	{
		if (item->IsDragonSoul())
		{
			int iEmptyPos = ch->GetEmptyDragonSoulInventory(item);

			if (iEmptyPos != -1)
			{
				InventorySystem::AddToCharacter(item->GetEntityHandle(), ch->GetEntityHandle(), TItemPos(DRAGON_SOUL_INVENTORY, iEmptyPos));
				LogManager::instance().ItemLog(ch, item, "GM", item->GetName());
			}
			else
			{
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 626, "");
#endif
			}
		}
#ifdef ENABLE_EXTRA_INVENTORY
		else if (item->IsExtraItem())
		{
			int iEmptyPos = ch->GetEmptyExtraInventory(item);

			if (iEmptyPos != -1)
			{
				InventorySystem::AddToCharacter(item->GetEntityHandle(), ch->GetEntityHandle(), TItemPos(EXTRA_INVENTORY, iEmptyPos));
				LogManager::instance().ItemLog(ch, item, "GM", item->GetName());
			}
			else
			{
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 539, "");
#endif
			}
		}
#endif
		else
		{
			int iEmptyPos = ch->GetEmptyInventory(item->GetSize());

			if (iEmptyPos != -1)
			{
				InventorySystem::AddToCharacter(item->GetEntityHandle(), ch->GetEntityHandle(), TItemPos(INVENTORY, iEmptyPos));
				LogManager::instance().ItemLog(ch, item, "GM", item->GetName());
			}
			else
			{
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 366, "");
#endif
			}
		}
	}
	else
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "#%u item not exist by that vnum(%s).", dwVnum, arg1);
	}
}

ACMD(do_group_random)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: grrandom <group vnum>");
		return;
	}

	uint32_t dwVnum = 0;
	str_to_number(dwVnum, arg1);
	CHARACTER_MANAGER::instance().SpawnGroupGroup(dwVnum, ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character) - 500, ecs::PlayerRuntime::GetY(character) - 500, ecs::PlayerRuntime::GetX(character) + 500, ecs::PlayerRuntime::GetY(character) + 500);
}

ACMD(do_group)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: group <group vnum>");
		return;
	}

	uint32_t dwVnum = 0;
	str_to_number(dwVnum, arg1);

	if (test_server)
		LOG_INFO("COMMAND GROUP SPAWN {} at {} {} {}", dwVnum, ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character));

	CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character) - 500, ecs::PlayerRuntime::GetY(character) - 500, ecs::PlayerRuntime::GetX(character) + 500, ecs::PlayerRuntime::GetY(character) + 500);
}

ACMD(do_mob_coward)
{
	char	arg1[256], arg2[256];
	uint32_t	vnum = 0;
	LPCHARACTER	tch;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: mc <vnum>");
		return;
	}

	const CMob * pkMob;

	if (isdigit(*arg1))
	{
		str_to_number(vnum, arg1);

		if ((pkMob = CMobManager::instance().Get(vnum)) == nullptr)
			vnum = 0;
	}
	else
	{
		pkMob = CMobManager::Instance().Get(arg1, true);

		if (pkMob)
			vnum = pkMob->m_table.dwVnum;
	}

	if (vnum == 0)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "No such mob(%s) by that vnum", arg1);
		return;
	}

	int iCount = 0;

	if (*arg2)
		str_to_number(iCount, arg2);
	else
		iCount = 1;

	iCount = MIN(20, iCount);

	while (iCount--)
	{
		tch = CHARACTER_MANAGER::instance().SpawnMobRange(vnum,
				ecs::PlayerRuntime::GetMapIndex(character),
				ecs::PlayerRuntime::GetX(character) - number(200, 750),
				ecs::PlayerRuntime::GetY(character) - number(200, 750),
				ecs::PlayerRuntime::GetX(character) + number(200, 750),
				ecs::PlayerRuntime::GetY(character) + number(200, 750),
				true,
				pkMob->m_table.bType == CHAR_TYPE_STONE);
		if (tch)
			tch->SetCoward();
	}
}

ACMD(do_mob_map)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: mm <vnum>");
		return;
	}

	uint32_t vnum = 0;
	str_to_number(vnum, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().SpawnMobRandomPosition(vnum, ecs::PlayerRuntime::GetMapIndex(character));
	const entt::entity tchEntity = tch ? tch->GetEntityHandle() : entt::null;


	if (tch)
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s spawned in %dx%d", ecs::PlayerRuntime::GetName(tchEntity).data(), ecs::PlayerRuntime::GetX(tchEntity), ecs::PlayerRuntime::GetY(tchEntity));
	else
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Spawn failed.");
}

ACMD(do_mob_aggresive)
{
	char	arg1[256], arg2[256];
	uint32_t	vnum = 0;
	LPCHARACTER	tch;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: mob <mob vnum>");
		return;
	}

	const CMob * pkMob;

	if (isdigit(*arg1))
	{
		str_to_number(vnum, arg1);

		if ((pkMob = CMobManager::instance().Get(vnum)) == nullptr)
			vnum = 0;
	}
	else
	{
		pkMob = CMobManager::Instance().Get(arg1, true);

		if (pkMob)
			vnum = pkMob->m_table.dwVnum;
	}

	if (vnum == 0)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "No such mob(%s) by that vnum", arg1);
		return;
	}

	int iCount = 0;

	if (*arg2)
		str_to_number(iCount, arg2);
	else
		iCount = 1;

	iCount = MIN(20, iCount);

	while (iCount--)
	{
		tch = CHARACTER_MANAGER::instance().SpawnMobRange(vnum,
				ecs::PlayerRuntime::GetMapIndex(character),
				ecs::PlayerRuntime::GetX(character) - number(200, 750),
				ecs::PlayerRuntime::GetY(character) - number(200, 750),
				ecs::PlayerRuntime::GetX(character) + number(200, 750),
				ecs::PlayerRuntime::GetY(character) + number(200, 750),
				true,
				pkMob->m_table.bType == CHAR_TYPE_STONE);
		if (tch)
				{
					const entt::entity e = tch->GetEntityHandle();
					if (e != entt::null)
						AIHelpers::SetAggressive(e, true);
				}
	}
}

ACMD(do_mob)
{
	char	arg1[256], arg2[256];
	uint32_t	vnum = 0;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: mob <mob vnum>");
		return;
	}

	const CMob* pkMob = nullptr;

	if (isnhdigit(*arg1))
	{
		str_to_number(vnum, arg1);

		if ((pkMob = CMobManager::instance().Get(vnum)) == nullptr)
			vnum = 0;
	}
	else
	{
		pkMob = CMobManager::Instance().Get(arg1, true);

		if (pkMob)
			vnum = pkMob->m_table.dwVnum;
	}

	if (vnum == 0)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "No such mob(%s) by that vnum", arg1);
		return;
	}

	int iCount = 0;

	if (*arg2)
		str_to_number(iCount, arg2);
	else
		iCount = 1;

	if (test_server)
		iCount = MIN(40, iCount);
	else
		iCount = MIN(SPAWN_COUNT, iCount);

	while (iCount--)
	{
		CHARACTER_MANAGER::instance().SpawnMobRange(vnum,
				ecs::PlayerRuntime::GetMapIndex(character),
				ecs::PlayerRuntime::GetX(character) - number(200, 750),
				ecs::PlayerRuntime::GetY(character) - number(200, 750),
				ecs::PlayerRuntime::GetX(character) + number(200, 750),
				ecs::PlayerRuntime::GetY(character) + number(200, 750),
				true,
				pkMob->m_table.bType == CHAR_TYPE_STONE);
	}
}

ACMD(do_mob_ld)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char	arg1[256], arg2[256], arg3[256], arg4[256];
	uint32_t	vnum = 0;

	two_arguments(two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3), arg4, sizeof(arg4));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: mob <mob vnum>");
		return;
	}

	const CMob* pkMob = nullptr;

	if (isnhdigit(*arg1))
	{
		str_to_number(vnum, arg1);

		if ((pkMob = CMobManager::instance().Get(vnum)) == nullptr)
			vnum = 0;
	}
	else
	{
		pkMob = CMobManager::Instance().Get(arg1, true);

		if (pkMob)
			vnum = pkMob->m_table.dwVnum;
	}

	if (vnum == 0)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "No such mob(%s) by that vnum", arg1);
		return;
	}

	int dir = 1;
	int32_t x=0,y=0;

	if (*arg2)
		str_to_number(x, arg2);
	if (*arg3)
		str_to_number(y, arg3);
	if (*arg4)
		str_to_number(dir, arg4);


	CHARACTER_MANAGER::instance().SpawnMob(vnum,
		ecs::PlayerRuntime::GetMapIndex(character),
		x*100,
		y*100,
		ch->GetZ(),
		pkMob->m_table.bType == CHAR_TYPE_STONE,
		dir);
}

struct FuncPurge
{
	LPCHARACTER m_pkGM;
	bool	m_bAll;

	FuncPurge(LPCHARACTER ch) : m_pkGM(ch), m_bAll(false)
	{
	}

	void operator () (LPENTITY ent)
	{
		const entt::entity gM = m_pkGM ? m_pkGM->GetEntityHandle() : entt::null;
		if (!ent->IsType(ENTITY_CHARACTER))
			return;

		LPCHARACTER pkChr = (LPCHARACTER) ent;
		const entt::entity chr = pkChr ? pkChr->GetEntityHandle() : entt::null;


		int iDist = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(chr) - ecs::PlayerRuntime::GetX(gM), ecs::PlayerRuntime::GetY(chr) - ecs::PlayerRuntime::GetY(gM));

		if (!m_bAll && iDist >= 1000)	// 10 ̻ ִ ͵ purge  ʴ´.
			return;

		LOG_INFO("PURGE: {} {}", ecs::PlayerRuntime::GetName(chr).data(), iDist);

#ifdef __NEWPET_SYSTEM__
		if (ecs::PlayerRuntime::IsNPC(chr) && !pkChr->IsPet() && !pkChr->IsNewPet() && !pkChr->IsMount() && pkChr->GetRider() == nullptr
#else
		if (ecs::PlayerRuntime::IsNPC(chr) && !pkChr->IsPet() && pkChr->GetRider() == NULL
#endif
		)
		{
			M2_DESTROY_CHARACTER(pkChr);
		}
	}
};

ACMD(do_purge)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	FuncPurge func(ch);

	if (*arg1 && !strcmp(arg1, "all"))
		func.m_bAll = true;

	LPSECTREE sectree = ecs::PlayerRuntime::GetSectree(character);
	if (sectree) // #431
		sectree->ForEachAround(func);
	else
		LOG_ERROR("PURGE_ERROR.NULL_SECTREE(mapIndex={}, pos=({}, {})", ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character));
}

#define ENABLE_CMD_IPURGE_EX
ACMD(do_item_purge)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
#ifdef ENABLE_CMD_IPURGE_EX
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: ipurge <window>");
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "List of the available windows:");
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, " all");
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, " inventory or inv");
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, " equipment or equip");
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, " dragonsoul or ds");
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, " belt");
		return;
	}

	int i;
	const entt::entity owner = character;

#ifdef __NEWPET_SYSTEM__
	CNewPetSystem* petSystem = ch->GetNewPetSystem();

	if(petSystem->CountSummoned() > 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 807, "");
#endif
		return;
	}
#endif
	std::string strArg(arg1);
	if (!strArg.compare(0, 3, "all"))
	{
		for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
		{
			const entt::entity item = ItemSystem::GetInventoryItem(owner, i);
			if (ItemSystem::IsValidItem(item))
			{
				ItemSystem::DestroyItemEntityEcs(item, "PURGE");
				ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, i, 255);
			}
		}
		for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
		{
			const entt::entity item = ItemSystem::GetItem(
				owner, TItemPos(DRAGON_SOUL_INVENTORY, i));
			if (ItemSystem::IsValidItem(item))
				ItemSystem::DestroyItemEntityEcs(item, "PURGE");
		}
#ifdef ENABLE_EXTRA_INVENTORY
		for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
		{
			const entt::entity item = ItemSystem::GetExtraInventoryItem(owner, i);
			if (ItemSystem::IsValidItem(item)) {
				ItemSystem::DestroyItemEntityEcs(item, "PURGE");
				ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, i, 255);
			}
		}
#endif
	}
	else if (!strArg.compare(0, 3, "inv"))
	{
		for (i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			const entt::entity item = ItemSystem::GetInventoryItem(owner, i);
			if (ItemSystem::IsValidItem(item))
			{
				ItemSystem::DestroyItemEntityEcs(item, "PURGE");
				ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, i, 255);
			}
		}
	}
	else if (!strArg.compare(0, 5, "equip"))
	{
		for (i = 0; i < WEAR_MAX_NUM; ++i)
		{
			const entt::entity item = ItemSystem::GetWearItem(owner, i);
			if (ItemSystem::IsValidItem(item))
			{
				ItemSystem::DestroyItemEntityEcs(item, "PURGE");
				ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, INVENTORY_MAX_NUM + i, 255);
			}
		}
	}
	else if (!strArg.compare(0, 6, "dragon") || !strArg.compare(0, 2, "ds"))
	{
		for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
		{
			const entt::entity item = ItemSystem::GetItem(
				owner, TItemPos(DRAGON_SOUL_INVENTORY, i));
			if (ItemSystem::IsValidItem(item))
				ItemSystem::DestroyItemEntityEcs(item, "PURGE");
		}
	}
	else if (!strArg.compare(0, 4, "belt"))
	{
		for (i = 0; i < BELT_INVENTORY_SLOT_COUNT; ++i)
		{
			const entt::entity item = ItemSystem::GetInventoryItem(
				owner, BELT_INVENTORY_SLOT_START + i);
			if (ItemSystem::IsValidItem(item))
			{
				ItemSystem::DestroyItemEntityEcs(item, "PURGE");
				ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, BELT_INVENTORY_SLOT_START + i, 255);
			}
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (!strArg.compare(0, 5, "extra"))
	{
		for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
		{
			const entt::entity item = ItemSystem::GetExtraInventoryItem(owner, i);
			if (ItemSystem::IsValidItem(item)) {
				ItemSystem::DestroyItemEntityEcs(item, "PURGE");
				ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, i, 255);
			}
		}
	}
#endif
#else
	int i;
	const entt::entity owner = character;

	for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		const entt::entity item = ItemSystem::GetInventoryItem(owner, i);
		if (ItemSystem::IsValidItem(item))
		{
			ItemSystem::DestroyItemEntityEcs(item, "PURGE");
			ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, i, 255);
		}
	}
	for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
	{
		const entt::entity item = ItemSystem::GetItem(
			owner, TItemPos(DRAGON_SOUL_INVENTORY, i));
		if (ItemSystem::IsValidItem(item))
			ItemSystem::DestroyItemEntityEcs(item, "PURGE");
	}
#ifdef ENABLE_EXTRA_INVENTORY
	for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
	{
		const entt::entity item = ItemSystem::GetExtraInventoryItem(owner, i);
		if (ItemSystem::IsValidItem(item)) {
			ItemSystem::DestroyItemEntityEcs(item, "PURGE");
			ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, i, 255);
		}
	}
#endif
#endif
}

ACMD(do_state)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	LPCHARACTER tch;

	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		if (arg1[0] == '#')
		{
			tch = CHARACTER_MANAGER::instance().Find(strtoul(arg1+1, nullptr, 10));
		}
		else
		{
			LPDESC d = DESC_MANAGER::instance().FindByCharacterName(arg1);

			if (!d)
				tch = nullptr;
			else
				tch = d->GetCharacter();
		}
	}
	else
		tch = ch;

	if (!tch)
		return;

	char buf[256];

	snprintf(buf, sizeof(buf), "%s's State: ", ecs::PlayerRuntime::GetName(((tch) ? (tch)->GetEntityHandle() : entt::null)).data());

	if (tch->IsPosition(POS_FIGHTING))
		strlcat(buf, "Battle", sizeof(buf));
	else if (tch->IsPosition(POS_DEAD))
		strlcat(buf, "Dead", sizeof(buf));
	else
		strlcat(buf, "Standing", sizeof(buf));

	if (ch->GetShop())
		strlcat(buf, ", Shop", sizeof(buf));

	if (ecs::SocialSystem::GetExchange(character))
		strlcat(buf, ", Exchange", sizeof(buf));

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s", buf);

	int len;
	len = snprintf(buf, sizeof(buf), "Coordinate %ldx%ld (%ldx%ld)",
			ecs::PlayerRuntime::GetX(((tch) ? (tch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY(((tch) ? (tch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetX(((tch) ? (tch)->GetEntityHandle() : entt::null)) / 100, ecs::PlayerRuntime::GetY(((tch) ? (tch)->GetEntityHandle() : entt::null)) / 100);

	if (len < 0 || len >= (int) sizeof(buf))
		len = sizeof(buf) - 1;

	LPSECTREE pSec = SECTREE_MANAGER::instance().Get(ecs::PlayerRuntime::GetMapIndex(((tch) ? (tch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetX(((tch) ? (tch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY(((tch) ? (tch)->GetEntityHandle() : entt::null)));

	if (pSec)
	{
		TMapSetting& map_setting = SECTREE_MANAGER::instance().GetMap(ecs::PlayerRuntime::GetMapIndex(((tch) ? (tch)->GetEntityHandle() : entt::null)))->m_setting;
		snprintf(buf + len, sizeof(buf) - len, " MapIndex %ld Attribute %08X Local Position (%ld x %ld)",
			ecs::PlayerRuntime::GetMapIndex(((tch) ? (tch)->GetEntityHandle() : entt::null)), pSec->GetAttribute(ecs::PlayerRuntime::GetX(((tch) ? (tch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY(((tch) ? (tch)->GetEntityHandle() : entt::null))), (ecs::PlayerRuntime::GetX(((tch) ? (tch)->GetEntityHandle() : entt::null)) - map_setting.iBaseX)/100, (ecs::PlayerRuntime::GetY(((tch) ? (tch)->GetEntityHandle() : entt::null)) - map_setting.iBaseY)/100);
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s", buf);

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "LEV %d", (ecs::PointSystem::GetLevel(((tch) ? (tch)->GetEntityHandle() : entt::null))));
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "HP %d/%d", tch->GetHP(), ecs::PointSystem::GetMaxHP(((tch) ? (tch)->GetEntityHandle() : entt::null)));
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "SP %d/%d", tch->GetSP(), ecs::PointSystem::GetMaxSP(((tch) ? (tch)->GetEntityHandle() : entt::null)));
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "ATT %d MAGIC_ATT %d SPD %d CRIT %d%% PENE %d%% ATT_BONUS %d%%",
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATT_GRADE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_MAGIC_ATT_GRADE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATT_SPEED),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_CRITICAL_PCT),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_PENETRATE_PCT),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATT_BONUS));
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "DEF %d MAGIC_DEF %d BLOCK %d%% DODGE %d%% DEF_BONUS %d%%",
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_DEF_GRADE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_MAGIC_DEF_GRADE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_BLOCK),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_DODGE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_DEF_BONUS));
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "RESISTANCES:");
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   WARR:%3d%% ASAS:%3d%% SURA:%3d%% SHAM:%3d%%"
#ifdef ENABLE_WOLFMAN_CHARACTER
			" WOLF:%3d%%"
#endif
			,
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_WARRIOR),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_ASSASSIN),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_SURA),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_SHAMAN)
#ifdef ENABLE_WOLFMAN_CHARACTER
			,ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_WOLFMAN)
#endif
	);
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   SWORD:%3d%% THSWORD:%3d%% DAGGER:%3d%% BELL:%3d%% FAN:%3d%% BOW:%3d%%"
#ifdef ENABLE_WOLFMAN_CHARACTER
			" CLAW:%3d%%"
#endif
			,
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_SWORD),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_TWOHAND),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_DAGGER),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_BELL),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_FAN),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_BOW)
#ifdef ENABLE_WOLFMAN_CHARACTER
			,ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_CLAW)
#endif
	);
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   FIRE:%3d%% ELEC:%3d%% MAGIC:%3d%% WIND:%3d%% CRIT:%3d%% PENE:%3d%%",
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_FIRE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_ELEC),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_MAGIC),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_WIND),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_CRITICAL),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_PENETRATE));
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   ICE:%3d%% EARTH:%3d%% DARK:%3d%%",
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_ICE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_EARTH),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_DARK));


#ifdef ENABLE_NEW_BONUS_TALISMAN
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "IRR_SPA:%3d%% IRR_SPAD:%3d%% IRR_PUG:%3d%% IRR_FRE:%3d%% IRR_VEN:%3d%% IRR_CAMP:%3d%%"
									"RES_MEZ:%3d%% DEF_TAL:%3d%% FORT_DES:%3d%% FORT_INS:%3d%% FORT_ZOD:%3d%% ",
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_SPADA),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_SPADONE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_PUGNALE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_FRECCIA),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_VENTAGLIO),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_CAMPANA),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_MEZZIUOMINI),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_DEF_TALISMAN),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_FORT_ZODIAC));
#endif


#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   MAGICREDUCT:%3d%%", ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_MAGIC_REDUCTION));
#endif

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "MALL:");
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   ATT:%3d%% DEF:%3d%% EXP:%3d%% ITEMx%d GOLDx%d",
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_MALL_ATTBONUS),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_MALL_DEFBONUS),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_MALL_EXPBONUS),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_MALL_ITEMBONUS) / 10,
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_MALL_GOLDBONUS) / 10);

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "BONUS:");
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   SKILL:%3d%% NORMAL:%3d%% SKILL_DEF:%3d%% NORMAL_DEF:%3d%%",
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_SKILL_DAMAGE_BONUS),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DAMAGE_BONUS),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_SKILL_DEFEND_BONUS),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DEFEND_BONUS));

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   HUMAN:%3d%% ANIMAL:%3d%% ORC:%3d%% MILGYO:%3d%% UNDEAD:%3d%%",
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_HUMAN),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_ANIMAL),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_ORC),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_MILGYO),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_UNDEAD));

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   DEVIL:%3d%% INSECT:%3d%% FIRE:%3d%% ICE:%3d%% DESERT:%3d%%",
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_DEVIL),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_INSECT),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_FIRE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_ICE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_DESERT));

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   TREE:%3d%% MONSTER:%3d%%"
#ifdef ENABLE_STRONG_METIN
			"METIN:%3d%%"
#endif

#ifdef ENABLE_STRONG_BOSS
			"BOSS:%3d%%"
#endif

#ifdef ENABLE_MEDI_PVM
			"Medi_Pvm:%3d%%"
#endif


#ifdef ENABLE_RESIST_MONSTER
			"MONSTER_RES:%3d%%"
#endif
			,
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_TREE),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_MONSTER)
#ifdef ENABLE_STRONG_METIN
			,ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_METIN)
#endif

#ifdef ENABLE_STRONG_BOSS
			,ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_BOSS)
#endif
#ifdef ENABLE_MEDI_PVM
			,ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_MEDI_PVM)
#endif
#ifdef ENABLE_RESIST_MONSTER
			,ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_RESIST_MONSTER)
#endif
			);

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   WARR:%3d%% ASAS:%3d%% SURA:%3d%% SHAM:%3d%%"
#ifdef ENABLE_WOLFMAN_CHARACTER
			" WOLF:%3d%%"
#endif
			,
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_WARRIOR),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_ASSASSIN),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_SURA),
			ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_SHAMAN)
#ifdef ENABLE_WOLFMAN_CHARACTER
			,ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_ATTBONUS_WOLFMAN)
#endif
	);
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "IMMUNE:");
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "   STUN:%d SLOW:%d FALL:%d",
		ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_IMMUNE_STUN),
		ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_IMMUNE_SLOW),
		ecs::PointSystem::Get(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_IMMUNE_FALL));

	for (int i = 0; i < MAX_PRIV_NUM; ++i) {
		if (CPrivManager::instance().GetPriv(tch, i))
		{
			int iByEmpire = CPrivManager::instance().GetPrivByEmpire((ecs::PlayerRuntime::GetEmpire(((tch) ? (tch)->GetEntityHandle() : entt::null))), i);
			int iByGuild = 0;

			if (ecs::SocialSystem::GetGuild(((tch) ? (tch)->GetEntityHandle() : entt::null)))
				iByGuild = CPrivManager::instance().GetPrivByGuild(ecs::SocialSystem::GetGuild(((tch) ? (tch)->GetEntityHandle() : entt::null))->GetID(), i);

			int iByPlayer = CPrivManager::instance().GetPrivByCharacter((ecs::PlayerRuntime::GetPlayerID(((tch) ? (tch)->GetEntityHandle() : entt::null))), i);

#ifdef TEXTS_IMPROVEMENT
			if (iByEmpire) {
				ecs::ChatSystem::SendNew(((tch) ? (tch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 698, "%s#%d", c_apszPrivNames[i], iByEmpire);
			}
			if (iByGuild) {
				ecs::ChatSystem::SendNew(((tch) ? (tch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 699, "%s#%d", c_apszPrivNames[i], iByGuild);
			}
			if (iByPlayer) {
				ecs::ChatSystem::SendNew(((tch) ? (tch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 700, "%s#%d", c_apszPrivNames[i], iByPlayer);
			}
#endif
		}
	}
}

struct notice_packet_func
{
	const char * m_str;
#ifdef ENABLE_FULL_NOTICE
	bool m_bBigFont;
	notice_packet_func(const char * str, bool bBigFont=false) : m_str(str), m_bBigFont(bBigFont)
#else
	notice_packet_func(const char * str) : m_str(str)
#endif
	{
	}

	void operator () (LPDESC d)
	{
		if (!d->GetCharacter())
			return;
#ifdef ENABLE_FULL_NOTICE
		ecs::ChatSystem::Send(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null), (m_bBigFont)?CHAT_TYPE_BIG_NOTICE:CHAT_TYPE_NOTICE, "%s", m_str);
#else
		ecs::ChatSystem::Send(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null), CHAT_TYPE_NOTICE, "%s", m_str);
#endif
	}
};

#ifdef ENABLE_FULL_NOTICE
void SendNotice(const char * c_pszBuf, bool bBigFont)
#else
void SendNotice(const char * c_pszBuf)
#endif
{
	const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();
#ifdef ENABLE_FULL_NOTICE
	std::for_each(c_ref_set.begin(), c_ref_set.end(), notice_packet_func(c_pszBuf, bBigFont));
#else
	std::for_each(c_ref_set.begin(), c_ref_set.end(), notice_packet_func(c_pszBuf));
#endif
}

struct notice_map_packet_func
{
	const char* m_str;
	int32_t m_mapIndex;
	bool m_bBigFont;

	notice_map_packet_func(const char* str, int idx, bool bBigFont) : m_str(str), m_mapIndex(idx), m_bBigFont(bBigFont)
	{
	}

	void operator() (LPDESC d)
	{
		if (d->GetCharacter() == nullptr) return;
		if (ecs::PlayerRuntime::GetMapIndex(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null)) != m_mapIndex) return;

		ecs::ChatSystem::Send(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null), m_bBigFont == true ? CHAT_TYPE_BIG_NOTICE : CHAT_TYPE_NOTICE, "%s", m_str);
	}
};

void SendNoticeMap(const char* c_pszBuf, int32_t nMapIndex, bool bBigFont)
{
	const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();
	std::for_each(c_ref_set.begin(), c_ref_set.end(), notice_map_packet_func(c_pszBuf, nMapIndex, bBigFont));
}

struct log_packet_func
{
	const char * m_str;

	log_packet_func(const char * str) : m_str(str)
	{
	}

	void operator () (LPDESC d)
	{
		if (!d->GetCharacter())
			return;

		if (ecs::PlayerRuntime::GetGMLevel(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null)) > GM_PLAYER)
			ecs::ChatSystem::Send(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null), CHAT_TYPE_NOTICE, "%s", m_str);
	}
};


void SendLog(const char * c_pszBuf)
{
	const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();
	std::for_each(c_ref_set.begin(), c_ref_set.end(), log_packet_func(c_pszBuf));
}

#ifdef ENABLE_FULL_NOTICE
void BroadcastNotice(const char * c_pszBuf, bool bBigFont)
#else
void BroadcastNotice(const char * c_pszBuf)
#endif
{
	TPacketGGNotice p;
#ifdef ENABLE_FULL_NOTICE
	p.bHeader = (bBigFont)?HEADER_GG_BIG_NOTICE:HEADER_GG_NOTICE;
#else
	p.bHeader = HEADER_GG_NOTICE;
#endif
	p.lSize = strlen(c_pszBuf) + 1;

	TEMP_BUFFER buf;
	buf.write(&p, sizeof(p));
	buf.write(c_pszBuf, p.lSize);

	P2P_MANAGER::instance().Send(buf.read_peek(), buf.size()); // HEADER_GG_NOTICE

#ifdef ENABLE_FULL_NOTICE
	SendNotice(c_pszBuf, bBigFont);
#else
	SendNotice(c_pszBuf);
#endif
}

#ifdef TEXTS_IMPROVEMENT
struct noticenew_packet_func {
	uint8_t m_type;
	uint8_t m_empire;
	int32_t m_mapidx;
	uint32_t m_idx;
	const char * m_str;
	noticenew_packet_func(uint8_t type, uint8_t empire, int32_t mapidx, uint32_t idx, const char * format) : m_type(type), m_empire(empire), m_mapidx(mapidx), m_idx(idx), m_str(format) {}

	void operator () (LPDESC d) {
		if (!d->GetCharacter())
			return;

		if (m_empire == 0) {
			if (m_mapidx == 0) {
				ecs::ChatSystem::SendNew(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null), m_type, m_idx, m_str);
			} else if (ecs::PlayerRuntime::GetMapIndex(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null)) == m_mapidx) {
				ecs::ChatSystem::SendNew(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null), m_type, m_idx, m_str);
			}
		}
		else if (ecs::PlayerRuntime::GetEmpire(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null)) == m_empire) {
			if (m_mapidx == 0) {
				ecs::ChatSystem::SendNew(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null), m_type, m_idx, m_str);
			} else if (ecs::PlayerRuntime::GetMapIndex(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null)) == m_mapidx) {
				ecs::ChatSystem::SendNew(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null), m_type, m_idx, m_str);
			}
		}
	}
};

void SendNoticeNew(uint8_t type, uint8_t empire, int32_t mapidx, uint32_t idx, const char * format, ...) {
	char chatbuf[256];
	va_list args;
	va_start(args, format);
	vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();
	std::for_each(c_ref_set.begin(), c_ref_set.end(), noticenew_packet_func(type, empire, mapidx, idx, chatbuf));
}

void BroadcastNoticeNew(uint8_t type, uint8_t empire, int32_t mapidx, uint32_t idx, const char * format, ...) {
	char chatbuf[256];
	va_list args;
	va_start(args, format);
	int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	TPacketGGChatNew p;
	p.header = HEADER_GG_CHAT_NEW;
	p.type = type;
	p.empire = empire;
	p.mapidx = mapidx;
	p.idx = idx;
	p.size = len;

	TEMP_BUFFER buf;
	buf.write(&p, sizeof(p));
	if (len > 0) {
		buf.write(chatbuf, len);
	}

	P2P_MANAGER::instance().Send(buf.read_peek(), buf.size());
	SendNoticeNew(type, empire, mapidx, idx, chatbuf);
}
#endif

ACMD(do_notice)
{
	BroadcastNotice(argument);
}

ACMD(do_map_notice)
{
	SendNoticeMap(argument, ecs::PlayerRuntime::GetMapIndex(character), false);
}

ACMD(do_big_notice)
{
#ifdef ENABLE_FULL_NOTICE
	BroadcastNotice(argument, true);
#else
	ecs::ChatSystem::Send(character, CHAT_TYPE_BIG_NOTICE, "%s", argument);
#endif
}

#ifdef ENABLE_FULL_NOTICE
ACMD(do_map_big_notice)
{
	SendNoticeMap(argument, ecs::PlayerRuntime::GetMapIndex(character), true);
}

ACMD(do_notice_test)
{
	ecs::ChatSystem::Send(character, CHAT_TYPE_NOTICE, "%s", argument);
}

ACMD(do_big_notice_test)
{
	ecs::ChatSystem::Send(character, CHAT_TYPE_BIG_NOTICE, "%s", argument);
}
#endif

ACMD(do_who)
{
	int iTotal;
	int * paiEmpireUserCount;
	int iLocal;
	DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 808, "%d#%d#%d#%d#%d",  iTotal, paiEmpireUserCount[1], paiEmpireUserCount[2], paiEmpireUserCount[3], iLocal);
#endif
}

class user_func
{
	public:
		LPCHARACTER	m_ch;
		static int count;
		static char str[128];
		static int str_len;

		user_func()
			: m_ch(nullptr)
		{}

		void initialize(LPCHARACTER ch)
		{
			m_ch = ch;
			str_len = 0;
			count = 0;
			str[0] = '\0';
		}

		void operator () (LPDESC d)
		{
			if (!d->GetCharacter())
				return;

			int len = snprintf(str + str_len, sizeof(str) - str_len, "%-16s ", ecs::PlayerRuntime::GetName(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null)).data());

			if (len < 0 || len >= (int) sizeof(str) - str_len)
				len = (sizeof(str) - str_len) - 1;

			str_len += len;
			++count;

			if (!(count % 4))
			{
				ecs::ChatSystem::Send(((m_ch) ? (m_ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, str);

				str[0] = '\0';
				str_len = 0;
			}
		}
};

int	user_func::count = 0;
char user_func::str[128] = { 0, };
int	user_func::str_len = 0;

ACMD(do_user)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();
	user_func func;

	func.initialize(ch);
	std::for_each(c_ref_set.begin(), c_ref_set.end(), func);

	if (func.count % 4)
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, func.str);

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Total %d", func.count);
}

ACMD(do_disconnect)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "usage: /dc <player name>");
		return;
	}

	LPDESC d = DESC_MANAGER::instance().FindByCharacterName(arg1);
	LPCHARACTER	tch = d ? d->GetCharacter() : nullptr;

	if (!tch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s: no such a player.", arg1);
		return;
	}

	if (tch == ch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "cannot disconnect myself");
		return;
	}

	DESC_MANAGER::instance().DestroyDesc(d);
}

ACMD(do_kill)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "usage: /kill <player name>");
		return;
	}

	LPDESC	d = DESC_MANAGER::instance().FindByCharacterName(arg1);
	LPCHARACTER tch = d ? d->GetCharacter() : nullptr;

	if (!tch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s: no such a player", arg1);
		return;
	}

	tch->Dead();
}

#ifdef ENABLE_NEWSTUFF
ACMD(do_poison)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "ex) /poison <player name>");
		return;
	}

	LPDESC	d = DESC_MANAGER::instance().FindByCharacterName(arg1);
	LPCHARACTER tch = d ? d->GetCharacter() : nullptr;

	if (!tch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s: no such a player", arg1);
		return;
	}

	tch->AttackedByPoison(entt::null);
}
#endif
#ifdef ENABLE_WOLFMAN_CHARACTER
ACMD(do_bleeding)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "ex) /bleeding <player name>");
		return;
	}

	LPDESC	d = DESC_MANAGER::instance().FindByCharacterName(arg1);
	LPCHARACTER tch = d ? d->GetCharacter() : NULL;

	if (!tch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s: no such a player", arg1);
		return;
	}

	tch->AttackedByBleeding(entt::null);
}
#endif

#define MISC    0
#define BINARY  1
#define NUMBER  2

namespace DoSetTypes{
#ifdef ENABLE_GAYA_SYSTEM
typedef enum do_set_types_s {GOLD, RACE, SEX, JOB, EXP, MAX_HP, MAX_SP, SKILL, ALIGNMENT, ALIGN, GAYA} do_set_types_t;
#else
typedef enum do_set_types_s {GOLD, RACE, SEX, JOB, EXP, MAX_HP, MAX_SP, SKILL, ALIGNMENT, ALIGN} do_set_types_t;
#endif
}

const struct set_struct
{
	const char *cmd;
	const char type;
	const char * help;
} set_fields[] = {
	{ "gold",		NUMBER, nullptr},
#ifdef ENABLE_WOLFMAN_CHARACTER
	{ "race",		NUMBER,	"0. Warrior, 1. Ninja, 2. Sura, 3. Shaman, 4. Lycan"		},
#else
	{ "race",		NUMBER,	"0. Warrior, 1. Ninja, 2. Sura, 3. Shaman"		},
#endif
	{ "sex",		NUMBER,	"0. Male, 1. Female"	},
	{ "job",		NUMBER,	"0. None, 1. First, 2. Second"	},
	{ "exp",		NUMBER, nullptr},
	{ "max_hp",		NUMBER, nullptr},
	{ "max_sp",		NUMBER, nullptr},
	{ "skill",		NUMBER, nullptr},
	{ "alignment",	NUMBER, nullptr},
	{ "align",		NUMBER, nullptr},
	#ifdef ENABLE_GAYA_SYSTEM
	{ "gaya",		NUMBER, nullptr},
	#endif
	{ "\n",			MISC, nullptr}
};

ACMD(do_set)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256], arg3[256];

	LPCHARACTER tch = nullptr;

	int i;
	const char* line;

	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));

	if (!*arg1 || !*arg2 || !*arg3)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: set <name> <field> <value>");
#ifdef ENABLE_NEWSTUFF
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "List of the fields available:");
		for (i = 0; *(set_fields[i].cmd) != '\n'; i++)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, " %d. %s", i+1, set_fields[i].cmd);
			if (set_fields[i].help != nullptr)
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "  Help: %s", set_fields[i].help);
		}
#endif
		return;
	}

	tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (!tch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s not exist", arg1);
		return;
	}

	const auto len = strlen(arg2);

	for (i = 0; *(set_fields[i].cmd) != '\n'; i++)
		if (!strncmp(arg2, set_fields[i].cmd, len))
			break;

	switch (i)
	{
		case DoSetTypes::GOLD:	// gold
			{
				int64_t gold = 0;
				str_to_number(gold, arg3);
				DBManager::instance().SendMoneyLog(MONEY_LOG_MISC, 3, gold);
				int64_t before_gold = ecs::PointSystem::GetGold(((tch) ? (tch)->GetEntityHandle() : entt::null));
				ecs::PointSystem::Change(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_GOLD, gold, true);
				int64_t after_gold = ecs::PointSystem::GetGold(((tch) ? (tch)->GetEntityHandle() : entt::null));
				if (after_gold < 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 809, "");
#endif
					tch->SetGold(0);
				}
				if (0 == after_gold && 0 != before_gold)
				{
					LogManager::instance().CharLog(tch, gold, "ZERO_GOLD", "GM");
				}
			}
			break;

		case DoSetTypes::RACE: // race
#ifdef ENABLE_NEWSTUFF
			{
				int amount = 0;
				str_to_number(amount, arg3);
				amount = MINMAX(0, amount, JOB_MAX_NUM);
				ESex mySex = GET_SEX(tch);
				uint32_t dwRace = MAIN_RACE_WARRIOR_M;
				switch (amount)
				{
					case JOB_WARRIOR:
						dwRace = (mySex==SEX_MALE)?MAIN_RACE_WARRIOR_M:MAIN_RACE_WARRIOR_W;
						break;
					case JOB_ASSASSIN:
						dwRace = (mySex==SEX_MALE)?MAIN_RACE_ASSASSIN_M:MAIN_RACE_ASSASSIN_W;
						break;
					case JOB_SURA:
						dwRace = (mySex==SEX_MALE)?MAIN_RACE_SURA_M:MAIN_RACE_SURA_W;
						break;
					case JOB_SHAMAN:
						dwRace = (mySex==SEX_MALE)?MAIN_RACE_SHAMAN_M:MAIN_RACE_SHAMAN_W;
						break;
#ifdef ENABLE_WOLFMAN_CHARACTER
					case JOB_WOLFMAN:
						dwRace = (mySex==SEX_MALE)?MAIN_RACE_WOLFMAN_M:MAIN_RACE_WOLFMAN_M;
						break;
#endif
				}
				if (dwRace!=ecs::PlayerRuntime::GetRaceNum(((tch) ? (tch)->GetEntityHandle() : entt::null)))
				{
					tch->SetRace(dwRace);
					tch->ClearSkill();
					tch->SetSkillGroup(0);
					// quick mesh change workaround begin
					tch->SetPolymorph(101);
					tch->SetPolymorph(0);
					// quick mesh change workaround end
				}
			}
#endif
			break;

		case DoSetTypes::SEX: // sex
#ifdef ENABLE_NEWSTUFF
			{
				int amount = 0;
				str_to_number(amount, arg3);
				amount = MINMAX(SEX_MALE, amount, SEX_FEMALE);
				if (amount != GET_SEX(tch))
				{
					tch->ChangeSex();
					// quick mesh change workaround begin
					tch->SetPolymorph(101);
					tch->SetPolymorph(0);
					// quick mesh change workaround end
				}
			}
#endif
			break;

		case DoSetTypes::JOB: // job
#ifdef ENABLE_NEWSTUFF
			{
				int amount = 0;
				str_to_number(amount, arg3);
				amount = MINMAX(0, amount, 2);
				if (amount != tch->GetSkillGroup())
				{
					tch->ClearSkill();
					tch->SetSkillGroup(amount);
				}
			}
#endif
			break;

		case DoSetTypes::EXP: // exp
			{
				int amount = 0;
				str_to_number(amount, arg3);
				ecs::PointSystem::Change(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_EXP, amount, true);
			}
			break;

		case DoSetTypes::MAX_HP: // max_hp
			{
				int amount = 0;
				str_to_number(amount, arg3);
				ecs::PointSystem::Change(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_MAX_HP, amount, true);
			}
			break;

		case DoSetTypes::MAX_SP: // max_sp
			{
				int amount = 0;
				str_to_number(amount, arg3);
				ecs::PointSystem::Change(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_MAX_SP, amount, true);
			}
			break;

		case DoSetTypes::SKILL: // active skill point
			{
				int amount = 0;
				str_to_number(amount, arg3);
				ecs::PointSystem::Change(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_SKILL, amount, true);
			}
			break;

		case DoSetTypes::ALIGN: // alignment
		case DoSetTypes::ALIGNMENT: // alignment
			{
				uint32_t	amount = 0;
				str_to_number(amount, arg3);
				tch->UpdateAlignment(amount - ch->GetRealAlignment());
			}
			break;

#ifdef ENABLE_GAYA_SYSTEM
		case DoSetTypes::GAYA: //gaya
		{
			int gaya = 0;
			str_to_number(gaya, arg3);
			int before_gaya = tch->GetGaya();
			ecs::PointSystem::Change(((tch) ? (tch)->GetEntityHandle() : entt::null), POINT_GAYA, gaya, true);
			int after_gaya = tch->GetGaya();
			if (0 == after_gaya && 0 != before_gaya)
			{
				LogManager::instance().CharLog(tch, gaya, "ZERO_GAYA", "GM");
			}
		}
		break;
#endif

	}

	if (set_fields[i].type == NUMBER)
	{
		int64_t	amount = 0;
		str_to_number(amount, arg3);
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s's %s set to [%lld]", ecs::PlayerRuntime::GetName(((tch) ? (tch)->GetEntityHandle() : entt::null)).data(), set_fields[i].cmd, amount);

	}
}

ACMD(do_reset)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ecs::PointSystem::Change(character, POINT_HP, ecs::PointSystem::GetMaxHP(character) - ch->GetHP());
	ecs::PointSystem::Change(character, POINT_SP, ecs::PointSystem::GetMaxSP(character) - ch->GetSP());
	ch->Save();
}

ACMD(do_advance)
{
	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: advance <name> <level>");
		return;
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (!tch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s not exist", arg1);
		return;
	}

	int level = 0;
	str_to_number(level, arg2);

	tch->ResetPoint(MINMAX(0, level, gPlayerMaxLevel));
}

ACMD(do_respawn)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1 && !strcasecmp(arg1, "all"))
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Respaw everywhere");
		regen_reset(0, 0);
	}
	else
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Respaw around");
		regen_reset(ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character));
	}
}

ACMD(do_safebox_size)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	int size = 0;

	if (*arg1)
		str_to_number(size, arg1);

	if (size > 3 || size < 0)
		size = 0;

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Safebox size set to %d", size);
	ch->ChangeSafeboxSize(size);
}

ACMD(do_makeguild)
{
	if (ecs::SocialSystem::GetGuild(character))
		return;

	CGuildManager& gm = CGuildManager::instance();

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	TGuildCreateParameter cp;
	memset(&cp, 0, sizeof(cp));

	cp.master = character;
	strlcpy(cp.name, arg1, sizeof(cp.name));

	if (!check_name(cp.name))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 455, "");
#endif
		return;
	}

	gm.CreateGuild(cp);
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 119, "%s", cp.name);
#endif
}

ACMD(do_deleteguild)
{
	if (ecs::SocialSystem::GetGuild(character))
		ecs::SocialSystem::GetGuild(character)->RequestDisband((ecs::PlayerRuntime::GetPlayerID(character)));
}

ACMD(do_greset)
{
	if (ecs::SocialSystem::GetGuild(character))
		ecs::SocialSystem::GetGuild(character)->Reset();
}

// REFINE_ROD_HACK_BUG_FIX
ACMD(do_refine_rod)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	uint8_t cell = 0;
	str_to_number(cell, arg1);
	const entt::entity owner = character;
	const entt::entity item = ItemSystem::GetInventoryItem(owner, cell);
	if (ItemSystem::IsValidItem(item))
		ActivitySystem::RefineFishingRod(owner, item);
}
// END_OF_REFINE_ROD_HACK_BUG_FIX

// REFINE_PICK
ACMD(do_refine_pick)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	uint8_t cell = 0;
	str_to_number(cell, arg1);
	const entt::entity item = ItemSystem::GetInventoryItem(character, cell);
	if (ItemSystem::IsValidItem(item))
	{
		mining::CHEAT_MAX_PICK(character, item);
		mining::RealRefinePick(character, item);
	}
}

ACMD(do_max_pick)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	uint8_t cell = 0;
	str_to_number(cell, arg1);
	const entt::entity item = ItemSystem::GetInventoryItem(character, cell);
	if (ItemSystem::IsValidItem(item))
	{
		mining::CHEAT_MAX_PICK(character, item);
	}
}
// END_OF_REFINE_PICK


ACMD(do_fishing_simul)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	char arg2[256];
	char arg3[256];
	argument = one_argument(argument, arg1, sizeof(arg1));
	two_arguments(argument, arg2, sizeof(arg2), arg3, sizeof(arg3));

	int count = 1000;
	int prob_idx = 0;
	int level = 100;

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: fishing_simul <level> <prob index> <count>");

	if (*arg1)
		str_to_number(level, arg1);

	if (*arg2)
		str_to_number(prob_idx, arg2);

	if (*arg3)
		str_to_number(count, arg3);

	fishing::Simulation(level, count, prob_idx, ch);
}

ACMD(do_invisibility)
{
	if (AffectSystem::IsAffectFlag(character, AFF_INVISIBILITY))
	{
		AffectSystem::RemoveAffect(character, AFFECT_INVISIBILITY);
	}
	else
	{
		AffectSystem::AddAffect(character, AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, INFINITE_AFFECT_DURATION, 0, true);
	}
}

ACMD(do_event_flag)
{
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!(*arg1) || !(*arg2))
		return;

	int value = 0;
	str_to_number(value, arg2);

	if (!strcmp(arg1, "mob_item") ||
			!strcmp(arg1, "mob_exp") ||
			!strcmp(arg1, "mob_gold") ||
			!strcmp(arg1, "mob_dam") ||
			!strcmp(arg1, "mob_gold_pct") ||
			!strcmp(arg1, "mob_item_buyer") ||
			!strcmp(arg1, "mob_exp_buyer") ||
			!strcmp(arg1, "mob_gold_buyer") ||
			!strcmp(arg1, "mob_gold_pct_buyer")
	   )
		value = MINMAX(0, value, 1000);

	//quest::CQuestManager::instance().SetEventFlag(arg1, atoi(arg2));
	quest::CQuestManager::instance().RequestSetEventFlag(arg1, value);
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "RequestSetEventFlag %s %d", arg1, value);
	LOG_INFO("RequestSetEventFlag {} {}", arg1, value);
}

ACMD(do_get_event_flag)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	quest::CQuestManager::instance().SendEventFlagList(ch);
}

ACMD(do_private)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: private <map index>");
		return;
	}

	int32_t lMapIndex;
	int32_t map_index = 0;
	str_to_number(map_index, arg1);
	if ((lMapIndex = SECTREE_MANAGER::instance().CreatePrivateMap(map_index)))
	{
		ch->SaveExitLocation();

		LPSECTREE_MAP pkSectreeMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
		ecs::MovementSystem::WarpSet(character, pkSectreeMap->m_setting.posSpawn.x, pkSectreeMap->m_setting.posSpawn.y, lMapIndex);
	}
	else
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Can't find map by index %d", map_index);
}

ACMD(do_qf)
{
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	quest::PC* pPC = quest::CQuestManager::instance().GetPCForce((ecs::PlayerRuntime::GetPlayerID(character)));
	std::string questname = pPC->GetCurrentQuestName();

	if (!questname.empty())
	{
		int value = quest::CQuestManager::Instance().GetQuestStateIndex(questname, arg1);

		pPC->SetFlag(questname + ".__status", value);
		pPC->ClearTimer();

		quest::PC::QuestInfoIterator it = pPC->quest_begin();
		unsigned int questindex = quest::CQuestManager::instance().GetQuestIndexByName(questname);

		while (it!= pPC->quest_end())
		{
			if (it->first == questindex)
			{
				it->second.st = value;
				break;
			}

			++it;
		}

		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "setting quest state flag %s %s %d", questname.c_str(), arg1, value);
	}
	else
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "setting quest state flag failed");
	}
}

ACMD(do_book)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	CSkillProto * pkProto;

	if (isnhdigit(*arg1))
	{
		uint32_t vnum = 0;
		str_to_number(vnum, arg1);
		pkProto = CSkillManager::instance().Get(vnum);
	}
	else
		pkProto = CSkillManager::instance().Get(arg1);

	if (!pkProto)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "There is no such a skill.");
		return;
	}

	LPITEM item = ch->AutoGiveItem(50300);
	ItemSystem::SetItemSocket((item ? item->GetEntityHandle() : entt::null), 0, pkProto->dwVnum);
}

ACMD(do_setskillother)
{
	char arg1[256], arg2[256], arg3[256];
	argument = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(argument, arg3, sizeof(arg3));

	if (!*arg1 || !*arg2 || !*arg3 || !isdigit(*arg3))
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: setskillother <target> <skillname> <lev>");
		return;
	}

	LPCHARACTER tch;

	tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (!tch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "There is no such character.");
		return;
	}

	CSkillProto * pk;

	if (isdigit(*arg2))
	{
		uint32_t vnum = 0;
		str_to_number(vnum, arg2);
		pk = CSkillManager::instance().Get(vnum);
	}
	else
		pk = CSkillManager::instance().Get(arg2);

	if (!pk)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "No such a skill by that name.");
		return;
	}

	uint8_t level = 0;
	str_to_number(level, arg3);
	tch->SetSkillLevel(pk->dwVnum, level);
	tch->ComputePoints();
	tch->SkillLevelPacket();
}

ACMD(do_setskill)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2 || !isdigit(*arg2))
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: setskill <name> <lev>");
		return;
	}

	CSkillProto * pk;

	if (isdigit(*arg1))
	{
		uint32_t vnum = 0;
		str_to_number(vnum, arg1);
		pk = CSkillManager::instance().Get(vnum);
	}

	else
		pk = CSkillManager::instance().Get(arg1);

	if (!pk)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "No such a skill by that name.");
		return;
	}

	uint8_t level = 0;
	str_to_number(level, arg2);
	ch->SetSkillLevel(pk->dwVnum, level);
	ch->ComputePoints();
	ch->SkillLevelPacket();
}

ACMD(do_set_skill_point)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	int skill_point = 0;
	if (*arg1)
		str_to_number(skill_point, arg1);

	ch->SetRealPoint(POINT_SKILL, skill_point);
	ch->SetPoint(POINT_SKILL, ecs::PointSystem::GetReal(character, POINT_SKILL));
	ecs::PointSystem::Change(character, POINT_SKILL, 0);
}

ACMD(do_set_skill_group)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	int skill_group = 0;
	if (*arg1)
		str_to_number(skill_group, arg1);

	ch->SetSkillGroup(skill_group);

	ch->ClearSkill();
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "skill group to %d.", skill_group);
}

ACMD(do_reload)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		switch (LOWER(*arg1))
		{
			case 'u':
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Reloading state_user_count.");
				LoadStateUserCount();
				break;

#ifdef ENABLE_ITEMSHOP
			case 'z':
			{
				uint8_t subIndex = ITEMSHOP_RELOAD;
				db_clientdesc->DBPacketHeader(HEADER_GD_ITEMSHOP, 0, sizeof(uint8_t));
				db_clientdesc->Packet(&subIndex, sizeof(uint8_t));
			}
			break;
#endif


			case 'p':
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Reloading prototype tables,");
				db_clientdesc->DBPacket(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);
				break;

			case 's':
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Reloading notice string.");
				DBManager::instance().LoadDBString();
				break;

			case 'q':
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Reloading quest.");
				quest::CQuestManager::instance().Reload();
				break;

			case 'f':
				fishing::Initialize();
				break;

				//RELOAD_ADMIN
			case 'a':
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Reloading Admin infomation.");
				db_clientdesc->DBPacket(HEADER_GD_RELOAD_ADMIN, 0, nullptr, 0);
				LOG_INFO("Reloading admin infomation.");
				break;
				//END_RELOAD_ADMIN
			case 'c':	// cube
				//  μ Ѵ.
				Cube_init ();
				break;
		}
	}
	else
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Reloading state_user_count.");
		LoadStateUserCount();

		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Reloading prototype tables,");
		db_clientdesc->DBPacket(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);

		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Reloading notice string.");
		DBManager::instance().LoadDBString();
	}
}

ACMD(do_cooltime)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->DisableCooltime();
}

ACMD(do_level)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg2[256];
	one_argument(argument, arg2, sizeof(arg2));

	if (!*arg2)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: level <level>");
		return;
	}

	int	level = 0;
	str_to_number(level, arg2);

	ch->ResetPoint(MINMAX(1, level, gPlayerMaxLevel));

	ch->ClearSkill();
	ch->ClearSubSkill();
}

ACMD(do_gwlist)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(character, CHAT_TYPE_NOTICE, 490, "");
#endif
	CGuildManager::instance().ShowGuildWarList(ch);
}

ACMD(do_stop_guild_war)
{
	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	int id1 = 0, id2 = 0;

	str_to_number(id1, arg1);
	str_to_number(id2, arg2);

	if (!id1 || !id2)
		return;

	if (id1 > id2)
	{
		std::swap(id1, id2);
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_TALKING, "%d %d", id1, id2);
	CGuildManager::instance().RequestEndWar(id1, id2);
}

ACMD(do_cancel_guild_war)
{
	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	int id1 = 0, id2 = 0;
	str_to_number(id1, arg1);
	str_to_number(id2, arg2);

	if (id1 > id2)
		std::swap(id1, id2);

	CGuildManager::instance().RequestCancelWar(id1, id2);
}

ACMD(do_guild_state)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	CGuild* pGuild = CGuildManager::instance().FindGuildByName(arg1);
	if (pGuild != nullptr)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "GuildID: %d", pGuild->GetID());
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "GuildMasterPID: %d", pGuild->GetMasterPID());
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "IsInWar: %d", pGuild->UnderAnyWar());
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 114, "%s", arg1);
	}
#endif
}

struct FuncWeaken
{
	LPCHARACTER m_pkGM;
	bool	m_bAll;

	FuncWeaken(LPCHARACTER ch) : m_pkGM(ch), m_bAll(false)
	{
	}

	void operator () (LPENTITY ent)
	{
		const entt::entity gM = m_pkGM ? m_pkGM->GetEntityHandle() : entt::null;
		if (!ent->IsType(ENTITY_CHARACTER))
			return;

		LPCHARACTER pkChr = (LPCHARACTER) ent;
		const entt::entity chr = pkChr ? pkChr->GetEntityHandle() : entt::null;


		int iDist = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(chr) - ecs::PlayerRuntime::GetX(gM), ecs::PlayerRuntime::GetY(chr) - ecs::PlayerRuntime::GetY(gM));

		if (!m_bAll && iDist >= 1000)	// 10 ̻ ִ ͵ purge  ʴ´.
			return;

		if (ecs::PlayerRuntime::IsNPC(chr))
			ecs::PointSystem::Change(chr, POINT_HP, (10 - pkChr->GetHP()));
	}
};

ACMD(do_weaken)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	FuncWeaken func(ch);

	if (*arg1 && !strcmp(arg1, "all"))
		func.m_bAll = true;

	ecs::PlayerRuntime::GetSectree(character)->ForEachAround(func);
}

ACMD(do_getqf)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	LPCHARACTER tch;

	if (!*arg1)
		tch = ch;
	else
	{
		tch = CHARACTER_MANAGER::instance().FindPC(arg1);

		if (!tch)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "There is no such character.");
			return;
		}
	}

	quest::PC* pPC = quest::CQuestManager::instance().GetPC(ecs::PlayerRuntime::GetPlayerID(((tch) ? (tch)->GetEntityHandle() : entt::null)));

	if (pPC)
		pPC->SendFlagList(ch);
}

#define ENABLE_SET_STATE_WITH_TARGET
ACMD(do_set_state)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	char arg2[256];

	argument = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO,
			"Syntax: set_state <questname> <statename>"
#ifdef ENABLE_SET_STATE_WITH_TARGET
			" [<character name>]"
#endif
		);
		return;
	}

#ifdef ENABLE_SET_STATE_WITH_TARGET
	LPCHARACTER tch = ch;
	char arg3[256];
	argument = one_argument(argument, arg3, sizeof(arg3));
	if (*arg3)
	{
		tch = CHARACTER_MANAGER::instance().FindPC(arg3);
		if (!tch)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "There is no such character.");
			return;
		}
	}
	quest::PC* pPC = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(((tch) ? (tch)->GetEntityHandle() : entt::null)));
#else
	quest::PC* pPC = quest::CQuestManager::instance().GetPCForce((ecs::PlayerRuntime::GetPlayerID(character)));
#endif
	std::string questname = arg1;
	std::string statename = arg2;

	if (!questname.empty())
	{
		int value = quest::CQuestManager::Instance().GetQuestStateIndex(questname, statename);

		pPC->SetFlag(questname + ".__status", value);
		pPC->ClearTimer();

		quest::PC::QuestInfoIterator it = pPC->quest_begin();
		unsigned int questindex = quest::CQuestManager::instance().GetQuestIndexByName(questname);

		while (it!= pPC->quest_end())
		{
			if (it->first == questindex)
			{
				it->second.st = value;
				break;
			}

			++it;
		}

		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "setting quest state flag %s %s %d", questname.c_str(), arg1, value);
	}
	else
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "setting quest state flag failed");
	}
}

ACMD(do_setqf)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	char arg2[256];
	char arg3[256];

	one_argument(two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: setqf <flagname> <value> [<character name>]");
		return;
	}

	LPCHARACTER tch = ch;

	if (*arg3)
		tch = CHARACTER_MANAGER::instance().FindPC(arg3);

	if (!tch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "There is no such character.");
		return;
	}

	quest::PC* pPC = quest::CQuestManager::instance().GetPC(ecs::PlayerRuntime::GetPlayerID(((tch) ? (tch)->GetEntityHandle() : entt::null)));

	if (pPC)
	{
		int value = 0;
		str_to_number(value, arg2);
		pPC->SetFlag(arg1, value);
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Quest flag set: %s %d", arg1, value);
	}
}

ACMD(do_delqf)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: delqf <flagname> [<character name>]");
		return;
	}

	LPCHARACTER tch = ch;

	if (*arg2)
		tch = CHARACTER_MANAGER::instance().FindPC(arg2);

	if (!tch)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "There is no such character.");
		return;
	}

	quest::PC* pPC = quest::CQuestManager::instance().GetPC(ecs::PlayerRuntime::GetPlayerID(((tch) ? (tch)->GetEntityHandle() : entt::null)));

	if (pPC)
	{
		if (pPC->DeleteFlag(arg1))
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Delete success.");
		else
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Delete failed. Quest flag does not exist.");
	}
}

ACMD(do_forgetme)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->ForgetMyAttacker();
}

ACMD(do_aggregate)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->AggregateMonster();
}

ACMD(do_attract_ranger)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->AttractRanger();
}

ACMD(do_pull_monster)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->PullMonster();
}

ACMD(do_polymorph)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	if (*arg1)
	{
		uint32_t dwVnum = 0;
		str_to_number(dwVnum, arg1);
		bool bMaintainStat = false;
		if (*arg2)
		{
			int value = 0;
			str_to_number(value, arg2);
			bMaintainStat = (value>0);
		}

		ch->SetPolymorph(dwVnum, bMaintainStat);
	}
}

ACMD(do_polymorph_item)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		uint32_t dwVnum = 0;
		str_to_number(dwVnum, arg1);

		LPITEM item = ITEM_MANAGER::instance().CreateItem(70104, 1, 0, true);
		if (item)
		{
			ItemSystem::SetItemSocket((item ? item->GetEntityHandle() : entt::null), 0, dwVnum);
			int iEmptyPos = ch->GetEmptyInventory(item->GetSize());

			if (iEmptyPos != -1)
			{
				InventorySystem::AddToCharacter(item->GetEntityHandle(), ch->GetEntityHandle(), TItemPos(INVENTORY, iEmptyPos));
				LogManager::instance().ItemLog(ch, item, "GM", item->GetName());
			}
			else
			{
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 366, "");
#endif
			}
		}
		else
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "#%d item not exist by that vnum.", 70103);
		}
		//ch->SetPolymorph(dwVnum, bMaintainStat);
	}
}

ACMD(do_priv_empire)
{
	char arg1[256] = {0};
	char arg2[256] = {0};
	char arg3[256] = {0};
	char arg4[256] = {0};
	int empire = 0;
	int type = 0;
	int value = 0;
	int duration = 0;

	const char* line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		goto USAGE;

	if (!line)
		goto USAGE;

	two_arguments(line, arg3, sizeof(arg3), arg4, sizeof(arg4));

	if (!*arg3 || !*arg4)
		goto USAGE;

	str_to_number(empire, arg1);
	str_to_number(type,	arg2);
	str_to_number(value,	arg3);
	value = MINMAX(0, value, 1000);
	str_to_number(duration, arg4);

	if (empire < 0 || 3 < empire)
		goto USAGE;

	if (type < 1 || 4 < type)
		goto USAGE;

	if (value < 0)
		goto USAGE;

	if (duration < 0)
		goto USAGE;

	// ð
	duration = duration * (60*60);

	LOG_INFO("_give_empire_privileage(empire={}, type={}, value={}, duration={}) by command", empire, type, value, duration);
	CPrivManager::instance().RequestGiveEmpirePriv(empire, type, value, duration);
	return;

USAGE:
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "usage : priv_empire <empire> <type> <value> <duration>");
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "  <empire>    0 - 3 (0==all)");
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "  <type>      1:item_drop, 2:gold_drop, 3:gold10_drop, 4:exp");
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "  <value>     percent");
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "  <duration>  hour");
}

/**
 * @version 05/06/08	Bang2ni -  ʽ Ʈ  ȵǴ  .(ũƮ ۼȵ.)
 * 			          quest/priv_guild.quest   ũƮ о
 */
ACMD(do_priv_guild)
{
	static const char msg[] = { '\0' };

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		CGuild * g = CGuildManager::instance().FindGuildByName(arg1);

		if (!g)
		{
			uint32_t guild_id = 0;
			str_to_number(guild_id, arg1);
			g = CGuildManager::instance().FindGuild(guild_id);
		}

		if (g) {
			char buf[1024+1];
			snprintf(buf, sizeof(buf), msg, g->GetID());

			using namespace quest;
			PC * pc = CQuestManager::instance().GetPC((ecs::PlayerRuntime::GetPlayerID(character)));
			QuestState qs = CQuestManager::instance().OpenState("ADMIN_QUEST", QUEST_FISH_REFINE_STATE_INDEX);
			luaL_loadbuffer(qs.co, buf, strlen(buf), "ADMIN_QUEST");
			pc->SetQuest("ADMIN_QUEST", qs);

			QuestState & rqs = *pc->GetRunningQuestState();

			if (!CQuestManager::instance().RunState(rqs)) {
				CQuestManager::instance().CloseState(rqs);
				pc->EndRunning();
				return;
			}
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 268, "");
		}
#endif
	}
}

ACMD(do_mount_test)
{
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		uint32_t vnum = 0;
		str_to_number(vnum, arg1);
		MountSystem::SetMountVnum(character, vnum);
	}
}

ACMD(do_observer)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->SetObserverMode(!ecs::PlayerRuntime::IsObserverMode(character));
}

ACMD(do_socket_item)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (*arg1)
	{
		uint32_t dwVnum = 0;
		str_to_number(dwVnum, arg1);

		int iSocketCount = 0;
		str_to_number(iSocketCount, arg2);

		if (!iSocketCount || iSocketCount >= ITEM_SOCKET_MAX_NUM)
			iSocketCount = 3;

		if (!dwVnum)
		{
			if (!ITEM_MANAGER::instance().GetVnum(arg1, dwVnum))
			{
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "#%d item not exist by that vnum.", dwVnum);
				return;
			}
		}

		LPITEM item = ch->AutoGiveItem(dwVnum);

		if (item)
		{
			for (int i = 0; i < iSocketCount; ++i)
				ItemSystem::SetItemSocket((item ? item->GetEntityHandle() : entt::null), i, 1);
		}
		else
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "#%d cannot create item.", dwVnum);
		}
	}
}

// BLOCK_CHAT
ACMD(do_block_chat_list)
{
	// GM ƴϰų block_chat_privilege   ɾ  Ұ
	if (!ecs::PlayerRuntime::IsValid(character) || ((ecs::PlayerRuntime::GetGMLevel(character)) < GM_HIGH_WIZARD && ecs::QuestSystem::GetFlag(character, "chat_privilege.block") <= 0))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 266, "");
#endif
		return;
	}

	DBManager::instance().ReturnQuery(QID_BLOCK_CHAT_LIST, (ecs::PlayerRuntime::GetPlayerID(character)), nullptr,
			"SELECT p.name, a.lDuration FROM affect%s as a, player%s as p WHERE a.bType = %d AND a.dwPID = p.id",
			get_table_postfix(), get_table_postfix(), AFFECT_BLOCK_CHAT);
}

ACMD(do_vote_block_chat)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	return;

	char arg1[256];
	argument = one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: vote_block_chat <name>");
		return;
	}

	const char* name = arg1;
	int32_t lBlockDuration = 10;
	LOG_INFO("vote_block_chat {} {}", name, lBlockDuration);

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(name);

	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(name);

		if (pkCCI)
		{
			TPacketGGBlockChat p;

			p.bHeader = HEADER_GG_BLOCK_CHAT;
			strlcpy(p.szName, name, sizeof(p.szName));
			p.lBlockDuration = lBlockDuration;
			P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGBlockChat));
		}
		else
		{
			TPacketBlockChat p;

			strlcpy(p.szName, name, sizeof(p.szName));
			p.lDuration = lBlockDuration;
			db_clientdesc->DBPacket(HEADER_GD_BLOCK_CHAT, ch ? ecs::PlayerRuntime::GetDesc(character)->GetHandle() : 0, &p, sizeof(p));

		}

		if (ch)
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Chat block requested.");

		return;
	}

	if (tch && ch != tch)
		AffectSystem::AddAffect(((tch) ? (tch)->GetEntityHandle() : entt::null), AFFECT_BLOCK_CHAT, POINT_NONE, 0, AFF_NONE, lBlockDuration, 0, true);
}

ACMD(do_block_chat)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	// GM ƴϰų block_chat_privilege   ɾ  Ұ
	if (ch && (ecs::PlayerRuntime::GetGMLevel(character) < GM_HIGH_WIZARD && ecs::QuestSystem::GetFlag(character, "chat_privilege.block") <= 0))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 266, "");
#endif
		return;
	}

	char arg1[256];
	argument = one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		if (ch)
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: block_chat <name> <time> (0 to off)");

		return;
	}

	const char* name = arg1;
	int32_t lBlockDuration = parse_time_str(argument);

	if (lBlockDuration < 0) {
		return;
	}

	LOG_INFO("BLOCK CHAT {} {}", name, lBlockDuration);

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(name);

	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(name);

		if (pkCCI)
		{
			TPacketGGBlockChat p;

			p.bHeader = HEADER_GG_BLOCK_CHAT;
			strlcpy(p.szName, name, sizeof(p.szName));
			p.lBlockDuration = lBlockDuration;
			P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGBlockChat));
		}
		else
		{
			TPacketBlockChat p;

			strlcpy(p.szName, name, sizeof(p.szName));
			p.lDuration = lBlockDuration;
			db_clientdesc->DBPacket(HEADER_GD_BLOCK_CHAT, ch ? ecs::PlayerRuntime::GetDesc(character)->GetHandle() : 0, &p, sizeof(p));
		}

#ifdef TEXTS_IMPROVEMENT
		if (ch) {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 810, "");
		}
#endif
		return;
	}

	if (tch && ch != tch)
		AffectSystem::AddAffect(((tch) ? (tch)->GetEntityHandle() : entt::null), AFFECT_BLOCK_CHAT, POINT_NONE, 0, AFF_NONE, lBlockDuration, 0, true);
}

void block_chat(entt::entity executor, std::string_view arguments)
{
	std::string ownedArguments(arguments);
	do_block_chat(executor, ownedArguments.data(), 0, 0);
}
// END_OF_BLOCK_CHAT

// BUILD_BUILDING
ACMD(do_build)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	using namespace building;

	char arg1[256], arg2[256], arg3[256], arg4[256];
	const char * line = one_argument(argument, arg1, sizeof(arg1));
	uint8_t GMLevel = (ecs::PlayerRuntime::GetGMLevel(character));

	CLand * pkLand = CManager::instance().FindLand(ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character));

	// NOTE:  üũ Ŭ̾Ʈ  Բ ϱ
	//       ޼  ʰ  Ѵ.
	if (!pkLand)
	{
		LOG_ERROR("{} trying to build on not buildable area.", ecs::PlayerRuntime::GetName(character).data());
		return;
	}

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Invalid syntax: no command");
		return;
	}

	// Ǽ  üũ
	if (GMLevel == GM_PLAYER)
	{
		// ÷̾      Ȯؾ Ѵ.
		if ((!ecs::SocialSystem::GetGuild(character) || ecs::SocialSystem::GetGuild(character)->GetID() != pkLand->GetOwner()))
		{
			LOG_ERROR("{} trying to build on not owned land.", ecs::PlayerRuntime::GetName(character).data());
			return;
		}

		//  渶ΰ?
		if (ecs::SocialSystem::GetGuild(character)->GetMasterPID() != (ecs::PlayerRuntime::GetPlayerID(character)))
		{
			LOG_ERROR("{} trying to build while not the guild master.", ecs::PlayerRuntime::GetName(character).data());
			return;
		}
	}

	switch (LOWER(*arg1))
	{
		case 'c':
			{
				// /build c vnum x y x_rot y_rot z_rot
				char arg5[256], arg6[256];
				line = one_argument(two_arguments(line, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3)); // vnum x y
				one_argument(two_arguments(line, arg4, sizeof(arg4), arg5, sizeof(arg5)), arg6, sizeof(arg6)); // x_rot y_rot z_rot

				if (!*arg1 || !*arg2 || !*arg3 || !*arg4 || !*arg5 || !*arg6)
				{
					ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Invalid syntax");
					return;
				}

				uint32_t dwVnum = 0;
				str_to_number(dwVnum,  arg1);

				using namespace building;

				const TObjectProto * t = CManager::instance().GetObjectProto(dwVnum);
				if (!t)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 462, "");
#endif
					return;
				}

				const uint32_t BUILDING_MAX_PRICE = 100000000;

				if (t->dwGroupVnum)
				{
					if (pkLand->FindObjectByGroup(t->dwGroupVnum))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 231, "");
#endif
						return;
					}
				}

				// ǹ Ӽ üũ ( ǹ  ־)
				if (t->dwDependOnGroupVnum)
				{
					//		const TObjectProto * dependent = CManager::instance().GetObjectProto(dwVnum);
					//		if (dependent)
					{
						// ִ°?
						if (!pkLand->FindObjectByGroup(t->dwDependOnGroupVnum))
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 239, "");
#endif
							return;
						}
					}
				}

				if (test_server || GMLevel == GM_PLAYER)
				{
					// GM ƴҰ츸 (׼ GM Ҹ)
					// Ǽ  üũ
					if (t->dwPrice > BUILDING_MAX_PRICE)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 238, "");
#endif
						return;
					}

					if (ecs::PointSystem::GetGold(character) < (int64_t)t->dwPrice)

					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 232, "");
#endif
						return;
					}

					//    üũ

					int i;
					for (i = 0; i < OBJECT_MATERIAL_MAX_NUM; ++i)
					{
						uint32_t dwItemVnum = t->kMaterials[i].dwItemVnum;
						uint32_t dwItemCount = t->kMaterials[i].dwCount;

						if (dwItemVnum == 0)
							break;

						if ((int) dwItemCount > ch->CountSpecifyItem(dwItemVnum))
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 449, "");
#endif
							return;
						}
					}
				}

				float x_rot = atof(arg4);
				float y_rot = atof(arg5);
				float z_rot = atof(arg6);
				int32_t map_x = 0;
				str_to_number(map_x, arg2);
				int32_t map_y = 0;
				str_to_number(map_y, arg3);

				bool isSuccess = pkLand->RequestCreateObject(dwVnum,
						ecs::PlayerRuntime::GetMapIndex(character),
						map_x,
						map_y,
						x_rot,
						y_rot,
						z_rot, true);

				if (!isSuccess) {
					return;
				}

				if (test_server || GMLevel == GM_PLAYER)
				{
					ecs::PointSystem::Change(character, POINT_GOLD, -static_cast<int64_t>(t->dwPrice));

					{
						for (int i = 0; i < OBJECT_MATERIAL_MAX_NUM; ++i)
						{
							uint32_t dwItemVnum = t->kMaterials[i].dwItemVnum;
							uint32_t dwItemCount = t->kMaterials[i].dwCount;

							if (dwItemVnum == 0)
								break;

							LOG_INFO("BUILD: material {} {} {}", i, dwItemVnum, dwItemCount);
							ch->RemoveSpecifyItem(dwItemVnum, dwItemCount);
						}
					}
				}
			}
			break;

		case 'd' :
			// build (d)elete ObjectID
			{
				one_argument(line, arg1, sizeof(arg1));

				if (!*arg1)
				{
					ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Invalid syntax");
					return;
				}

				uint32_t vid = 0;
				str_to_number(vid, arg1);
				pkLand->RequestDeleteObjectByVID(vid);
			}
			break;

			// BUILD_WALL

			// build w n/e/w/s
		case 'w' :
			if (GMLevel > GM_PLAYER)
			{
				int mapIndex = ecs::PlayerRuntime::GetMapIndex(character);

				one_argument(line, arg1, sizeof(arg1));

				LOG_INFO("guild.wall.build map[{}] direction[{}]", mapIndex, arg1);

				switch (arg1[0])
				{
					case 's':
						pkLand->RequestCreateWall(mapIndex,   0.0f);
						break;
					case 'n':
						pkLand->RequestCreateWall(mapIndex, 180.0f);
						break;
					case 'e':
						pkLand->RequestCreateWall(mapIndex,  90.0f);
						break;
					case 'w':
						pkLand->RequestCreateWall(mapIndex, 270.0f);
						break;
					default:
						ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "guild.wall.build unknown_direction[%s]", arg1);
						LOG_ERROR("guild.wall.build unknown_direction[{}]", arg1);
						break;
				}

			}
			break;

		case 'e':
			if (GMLevel > GM_PLAYER)
			{
				pkLand->RequestDeleteWall();
			}
			break;

		case 'W' :
			//
			// build (w)all ȣ ũ 빮 빮 빮 빮

			if (GMLevel >  GM_PLAYER)
			{
				int setID = 0, wallSize = 0;
				char arg5[256], arg6[256];
				line = two_arguments(line, arg1, sizeof(arg1), arg2, sizeof(arg2));
				line = two_arguments(line, arg3, sizeof(arg3), arg4, sizeof(arg4));
				two_arguments(line, arg5, sizeof(arg5), arg6, sizeof(arg6));

				str_to_number(setID, arg1);
				str_to_number(wallSize, arg2);

				if (setID != 14105 && setID != 14115 && setID != 14125)
				{
					LOG_INFO("BUILD_WALL: wrong wall set id {}", setID);
					break;
				}
				else
				{
					bool door_east = false;
					str_to_number(door_east, arg3);
					bool door_west = false;
					str_to_number(door_west, arg4);
					bool door_south = false;
					str_to_number(door_south, arg5);
					bool door_north = false;
					str_to_number(door_north, arg6);
					pkLand->RequestCreateWallBlocks(setID, ecs::PlayerRuntime::GetMapIndex(character), wallSize, door_east, door_west, door_south, door_north);
				}
			}
			break;

		case 'E' :
			//
			// build (e)rase ID
			if (GMLevel > GM_PLAYER)
			{
				one_argument(line, arg1, sizeof(arg1));
				uint32_t id = 0;
				str_to_number(id, arg1);
				pkLand->RequestDeleteWallBlocks(id);
			}
			break;

		default:
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Invalid command %s", arg1);
			break;
	}
}
// END_OF_BUILD_BUILDING

ACMD(do_clear_quest)
{
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	quest::PC* pPC = quest::CQuestManager::instance().GetPCForce((ecs::PlayerRuntime::GetPlayerID(character)));
	pPC->ClearQuest(arg1);
}

ACMD(do_horse_state)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Horse Information:");
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "    Level  %d", ch->GetHorseLevel());
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "    Health %d/%d (%d%%)", ch->GetHorseHealth(), ch->GetHorseMaxHealth(), ch->GetHorseHealth() * 100 / ch->GetHorseMaxHealth());
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "    Stam   %d/%d (%d%%)", ch->GetHorseStamina(), ch->GetHorseMaxStamina(), ch->GetHorseStamina() * 100 / ch->GetHorseMaxStamina());
}

ACMD(do_horse_level)
{
	char arg1[256] = {0};
	char arg2[256] = {0};
	LPCHARACTER victim;
	int	level = 0;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "usage : /horse_level <name> <level>");
		return;
	}

	victim = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (nullptr == victim)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 463, "");
#endif
		return;
	}

	str_to_number(level, arg2);
	level = MINMAX(0, level, HORSE_MAX_LEVEL);

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "horse level set (%s: %d)", ecs::PlayerRuntime::GetName(((victim) ? (victim)->GetEntityHandle() : entt::null)).data(), level);

	victim->SetHorseLevel(level);
	victim->ComputePoints();
	victim->SkillLevelPacket();
	return;
}

ACMD(do_horse_ride)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ch->IsHorseRiding())
		ch->StopRiding();
	else
		ch->StartRiding();
}

ACMD(do_horse_summon)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (ch->IsRidingMount())
		return;
#endif

	ch->HorseSummon(true, true);
}

ACMD(do_horse_unsummon)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->HorseSummon(false, true);
}

ACMD(do_horse_set_stat)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (*arg1 && *arg2)
	{
		int hp = 0;
		str_to_number(hp, arg1);
		int stam = 0;
		str_to_number(stam, arg2);
		ch->UpdateHorseHealth(hp - ch->GetHorseHealth());
		ch->UpdateHorseStamina(stam - ch->GetHorseStamina());
	}
	else
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage : /horse_set_stat <hp> <stamina>");
	}
}

ACMD(do_save_attribute_to_image) // command "/saveati" for alias
{
	char szFileName[256];
	char szMapIndex[256];

	two_arguments(argument, szMapIndex, sizeof(szMapIndex), szFileName, sizeof(szFileName));

	if (!*szMapIndex || !*szFileName)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: /saveati <map_index> <filename>");
		return;
	}

	int32_t lMapIndex = 0;
	str_to_number(lMapIndex, szMapIndex);

	if (SECTREE_MANAGER::instance().SaveAttributeToImage(lMapIndex, szFileName))
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Save done.");
	else
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Save failed.");
}

ACMD(do_affect_remove)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: /affect_remove <player name>");
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Syntax: /affect_remove <type> <point>");

		LPCHARACTER tch = ch;

		if (*arg1)
			if (!(tch = CHARACTER_MANAGER::instance().FindPC(arg1)))
				tch = ch;

		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "-- Affect List of %s -------------------------------", ecs::PlayerRuntime::GetName(((tch) ? (tch)->GetEntityHandle() : entt::null)).data());
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Type Point Modif Duration Flag");

		const std::list<CAffect *> & cont = tch->GetAffectContainer();

		auto it = cont.begin();

		while (it != cont.end())
		{
			CAffect * pkAff = *it++;

			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%4d %5d %5d %8d %u",
					pkAff->dwType, pkAff->bApplyOn, pkAff->lApplyValue, pkAff->lDuration, pkAff->dwFlag);
		}
		return;
	}

	bool removed = false;

	CAffect * af;

	uint32_t	type = 0;
	str_to_number(type, arg1);
	uint8_t	point = 0;
	str_to_number(point, arg2);
	while ((af = AffectSystem::FindAffect(character, type, point)))
	{
		AffectSystem::RemoveAffect(character, af);
		removed = true;
	}

	if (removed)
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Affect successfully removed.");
	else
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Not affected by that type and point.");
}

ACMD(do_change_attr)
{
	const entt::entity weapon = ItemSystem::GetWearItem(
		character, WEAR_WEAPON);
	if (ItemSystem::IsValidItem(weapon))
		ItemSystem::ChangeItemAttributeEcs(weapon);
}

ACMD(do_add_attr)
{
	const entt::entity weapon = ItemSystem::GetWearItem(
		character, WEAR_WEAPON);
	if (ItemSystem::IsValidItem(weapon))
		ItemSystem::AddItemAttributeEcs(weapon);
}

ACMD(do_add_socket)
{
	const entt::entity weapon = ItemSystem::GetWearItem(
		character, WEAR_WEAPON);
	if (!ItemSystem::IsValidItem(weapon))
		return;
	for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket) {
		if (ItemSystem::GetItemSocket(weapon, socket) == 0) {
			ItemSystem::SetItemSocketEcs(weapon, socket, 1);
			break;
		}
	}
}

#ifdef ENABLE_NEWSTUFF
ACMD(do_change_rare_attr)
{
	const entt::entity weapon = ItemSystem::GetWearItem(
		character, WEAR_WEAPON);
	if (ItemSystem::IsValidItem(weapon))
		ItemSystem::ChangeItemRareAttributeEcs(weapon);
}

ACMD(do_add_rare_attr)
{
	const entt::entity weapon = ItemSystem::GetWearItem(
		character, WEAR_WEAPON);
	if (ItemSystem::IsValidItem(weapon))
		ItemSystem::AddItemRareAttributeEcs(weapon);
}
#endif

ACMD(do_show_arena_list)
{
	CArenaManager::instance().SendArenaMapListTo(character);
}

ACMD(do_end_all_duel)
{
	CArenaManager::instance().EndAllDuel();
}

ACMD(do_end_duel)
{
	char szName[256];

	one_argument(argument, szName, sizeof(szName));

	const entt::entity pChar = CHARACTER_MANAGER::instance().FindPCEntity(szName);
	if (pChar == entt::null)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 463, "");
#endif
		return;
	}

	CArenaManager::instance().EndDuel(ecs::PlayerRuntime::GetPlayerID(pChar));
}

ACMD(do_duel)
{
	char szName1[256];
	char szName2[256];
	char szSet[256];
	char szMinute[256];
	int set = 0;
	int minute = 0;

	argument = two_arguments(argument, szName1, sizeof(szName1), szName2, sizeof(szName2));
	two_arguments(argument, szSet, sizeof(szSet), szMinute, sizeof(szMinute));

	str_to_number(set, szSet);

	if (set < 0) set = 1;
	if (set > 5) set = 5;

	if (!str_to_number(minute, szMinute))
		minute = 5;

	if (minute < 5)
		minute = 5;

	LPCHARACTER pChar1 = CHARACTER_MANAGER::instance().FindPC(szName1);
	const entt::entity char1 = pChar1 ? pChar1->GetEntityHandle() : entt::null;

	LPCHARACTER pChar2 = CHARACTER_MANAGER::instance().FindPC(szName2);
	const entt::entity char2 = pChar2 ? pChar2->GetEntityHandle() : entt::null;


	if (pChar1 != nullptr && pChar2 != nullptr)
	{
		pChar1->RemoveGoodAffect();
		pChar2->RemoveGoodAffect();

		pChar1->RemoveBadAffect();
		pChar2->RemoveBadAffect();

		LPPARTY pParty = ecs::SocialSystem::GetParty(char1);
		if (pParty != nullptr)
		{
			if (pParty->GetMemberCount() == 2)
			{
				CPartyManager::instance().DeleteParty(pParty);
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(char1, CHAT_TYPE_INFO, 215, "");
#endif
				pParty->Quit(ecs::PlayerRuntime::GetPlayerID(char1));
			}
		}

		pParty = ecs::SocialSystem::GetParty(char2);
		if (pParty != nullptr)
		{
			if (pParty->GetMemberCount() == 2)
			{
				CPartyManager::instance().DeleteParty(pParty);
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(char2, CHAT_TYPE_INFO, 215, "");
#endif
				pParty->Quit(ecs::PlayerRuntime::GetPlayerID(char2));
			}
		}

		if (CArenaManager::instance().StartDuel(pChar1->GetEntityHandle(), pChar2->GetEntityHandle(), set, minute) == true) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 301, "");
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 300, "");
		}
#endif
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 302, "");
	}
#endif
}

#define ENABLE_STATPLUS_NOLIMIT
ACMD(do_stat_plus_amount)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char szPoint[256];

	one_argument(argument, szPoint, sizeof(szPoint));

	if (*szPoint == '\0')
		return;

	if (ch->IsPolymorphed())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 314, "");
#endif
		return;
	}

	int nRemainPoint = ecs::PointSystem::Get(character, POINT_STAT);

	if (nRemainPoint <= 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 285, "");
#endif
		return;
	}

	int nPoint = 0;
	str_to_number(nPoint, szPoint);

	if (nRemainPoint < nPoint)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 286, "");
#endif
		return;
	}

	if (nPoint < 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 228, "");
#endif
		return;
	}

#ifndef ENABLE_STATPLUS_NOLIMIT
	switch (subcmd)
	{
		case POINT_HT : // ü
			if (nPoint + ecs::PointSystem::Get(character, POINT_HT) > 90)
			{
				nPoint = 90 - ecs::PointSystem::Get(character, POINT_HT);
			}
			break;

		case POINT_IQ : //
			if (nPoint + ecs::PointSystem::Get(character, POINT_IQ) > 90)
			{
				nPoint = 90 - ecs::PointSystem::Get(character, POINT_IQ);
			}
			break;

		case POINT_ST : // ٷ
			if (nPoint + ecs::PointSystem::Get(character, POINT_ST) > 90)
			{
				nPoint = 90 - ecs::PointSystem::Get(character, POINT_ST);
			}
			break;

		case POINT_DX : // ø
			if (nPoint + ecs::PointSystem::Get(character, POINT_DX) > 90)
			{
				nPoint = 90 - ecs::PointSystem::Get(character, POINT_DX);
			}
			break;

		default :
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 343, "");
#endif
			return;
			break;
	}
#endif

	if (nPoint != 0)
	{
		ch->SetRealPoint(subcmd, ecs::PointSystem::GetReal(character, subcmd) + nPoint);
		ch->SetPoint(subcmd, ecs::PointSystem::Get(character, subcmd) + nPoint);
		ch->ComputePoints();
		ecs::PointSystem::Change(character, subcmd, 0);

		ecs::PointSystem::Change(character, POINT_STAT, -nPoint);
		ch->ComputePoints();
	}
}

struct tTwoPID
{
	int pid1;
	int pid2;
};

ACMD(do_break_marriage)
{
	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	tTwoPID pids = { 0, 0 };
	str_to_number(pids.pid1, arg1);
	str_to_number(pids.pid2, arg2);

	db_clientdesc->DBPacket(HEADER_GD_BREAK_MARRIAGE, 0, &pids, sizeof(pids));
}

ACMD(do_effect)
{
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	uint8_t	effect_type = 0;
	str_to_number(effect_type, arg1);
	NetworkSyncSystem::BroadcastEffect(g_registry, character, effect_type);
}


struct FCountInMap
{
	int m_Count[4];
	FCountInMap() { memset(m_Count, 0, sizeof(int) * 4); }
	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;
			const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;

			if (ch && (ecs::PlayerRuntime::IsPC(chEntity)))
				++m_Count[(ecs::PlayerRuntime::GetEmpire(chEntity))];
		}
	}
	int GetCount(uint8_t bEmpire) { return m_Count[bEmpire]; }
};

ACMD(do_reset_subskill)
{
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: reset_subskill <name>");
		return;
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (tch == nullptr)
		return;

	tch->ClearSubSkill();
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Subskill of [%s] was reset", ecs::PlayerRuntime::GetName(((tch) ? (tch)->GetEntityHandle() : entt::null)).data());
}

ACMD(do_flush)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (0 == arg1[0])
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "usage : /flush player_id");
		return;
	}

	uint32_t pid = (uint32_t) strtoul(arg1, nullptr, 10);

	db_clientdesc->DBPacketHeader(HEADER_GD_FLUSH_CACHE, 0, sizeof(uint32_t));
	db_clientdesc->Packet(&pid, sizeof(uint32_t));
}

ACMD(do_eclipse)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (strtol(arg1, nullptr, 10) == 1)
	{
		quest::CQuestManager::instance().RequestSetEventFlag("eclipse", 1);
	}
	else
	{
		quest::CQuestManager::instance().RequestSetEventFlag("eclipse", 0);
	}
}

struct FMobCounter
{
	int nCount;

	void operator () (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER pChar = static_cast<LPCHARACTER>(ent);

			if (pChar->IsMonster() == true || ecs::PlayerRuntime::IsStone(((pChar) ? (pChar)->GetEntityHandle() : entt::null)))
			{
				nCount++;
			}
		}
	}
};

ACMD(do_get_mob_count)
{
	LPSECTREE_MAP pSectree = SECTREE_MANAGER::instance().GetMap(ecs::PlayerRuntime::GetMapIndex(character));

	if (pSectree == nullptr)
		return;

	FMobCounter f;
	f.nCount = 0;

	pSectree->for_each(f);

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "MapIndex: %d MobCount %d", ecs::PlayerRuntime::GetMapIndex(character), f.nCount);
}

ACMD(do_clear_land)
{
	const building::CLand* pLand = building::CManager::instance().FindLand(ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character));

	if(nullptr == pLand )
	{
		return;
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Guild Land(%d) Cleared", pLand->GetID());

	building::CManager::instance().ClearLand(pLand->GetID());
}

ACMD(do_special_item)
{
    ITEM_MANAGER::instance().ConvSpecialDropItemFile();
}

ACMD(do_set_stat)
{
	char szName [256];
	char szChangeAmount[256];

	two_arguments (argument, szName, sizeof (szName), szChangeAmount, sizeof(szChangeAmount));

	if (*szName == '\0' || *szChangeAmount == '\0')
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Invalid argument.");
		return;
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(szName);
	const entt::entity tchEntity = tch ? tch->GetEntityHandle() : entt::null;


	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(szName);

		if (pkCCI)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Cannot find player(%s). %s is not in your game server.", szName, szName);
			return;
		}
		else
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Cannot find player(%s). Perhaps %s doesn't login or exist.", szName, szName);
			return;
		}
	}
	else
	{
		if (tch->IsPolymorphed())
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 314, "");
#endif
			return;
		}

		if (subcmd != POINT_HT && subcmd != POINT_IQ && subcmd != POINT_ST && subcmd != POINT_DX)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 343, "");
#endif
			return;
		}
		int nRemainPoint = ecs::PointSystem::Get(tchEntity, POINT_STAT);
		int nCurPoint = ecs::PointSystem::GetReal(tchEntity, subcmd);
		int nChangeAmount = 0;
		str_to_number(nChangeAmount, szChangeAmount);
		int nPoint = nCurPoint + nChangeAmount;

		int n = -1;
		switch (subcmd)
		{
		case POINT_HT:
			if (nPoint < JobInitialPoints[tch->GetJob()].ht)
			{
				return;
			}
			n = 0;
			break;
		case POINT_IQ:
			if (nPoint < JobInitialPoints[tch->GetJob()].iq)
			{
				return;
			}
			n = 1;
			break;
		case POINT_ST:
			if (nPoint < JobInitialPoints[tch->GetJob()].st)
			{
				return;
			}
			n = 2;
			break;
		case POINT_DX:
			if (nPoint < JobInitialPoints[tch->GetJob()].dx)
			{
				return;
			}
			n = 3;
			break;
		}

		if (nPoint > 90)
		{
			nChangeAmount -= nPoint - 90;
			nPoint = 90;
		}

		if (nRemainPoint < nChangeAmount)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 286, "");
#endif
			return;
		}

		tch->SetRealPoint(subcmd, nPoint);
		tch->SetPoint(subcmd, ecs::PointSystem::Get(tchEntity, subcmd) + nChangeAmount);
		tch->ComputePoints();
		ecs::PointSystem::Change(tchEntity, subcmd, 0);

		ecs::PointSystem::Change(tchEntity, POINT_STAT, -nChangeAmount);
		tch->ComputePoints();

		const char* stat_name[4] = {"con", "int", "str", "dex"};
		if (-1 == n)
			return;
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s's %s change %d to %d", szName, stat_name[n], nCurPoint, nPoint);
	}
}

ACMD(do_get_item_id_list)
{
	const entt::entity owner = character;
	for (int i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; i++)
	{
		const entt::entity item = ItemSystem::GetInventoryItem(owner, i);
		if (item != entt::null)
			ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "cell : %d, name : %s, id : %d", ItemSystem::GetItemCell(item), ItemSystem::GetItemName(item), ItemSystem::GetItemID(item));
	}
}

ACMD(do_set_socket)
{
	char arg1 [256];
	char arg2 [256];
	char arg3 [256];

	one_argument (two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof(arg2)), arg3, sizeof (arg3));

	int item_id, socket_num, value;
	if (!str_to_number (item_id, arg1) || !str_to_number (socket_num, arg2) || !str_to_number (value, arg3))
		return;

	LPITEM item = ITEM_MANAGER::instance().Find (item_id);
	if (item)
		ItemSystem::SetItemSocket((item ? item->GetEntityHandle() : entt::null), socket_num, value);
}

ACMD (do_can_dead)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (subcmd)
		ch->SetArmada();
	else
		ch->ResetArmada();
}

ACMD (do_full_set)
{
	extern ACMD(do_all_skill_master);
	do_all_skill_master(character, nullptr, 0, 0);
	extern ACMD(do_item_full_set);
	do_item_full_set(character, nullptr, 0, 0);
	extern ACMD(do_attr_full_set);
	do_attr_full_set(character, nullptr, 0, 0);

}

ACMD (do_all_skill_master)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->SetHorseLevel(SKILL_MAX_LEVEL);
	for (int i = 0; i < SKILL_MAX_NUM; i++)
	{
		if (true == SkillSystem::CanUseSkill(character, i))
		{
			ch->SetSkillLevel(i, SKILL_MAX_LEVEL);
		}
		else
		{
			switch(i)
			{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:
				ch->SetSkillLevel(i, SKILL_MAX_LEVEL);
				break;
			}
		}
	}
	ch->ComputePoints();
	ch->SkillLevelPacket();
}

ACMD (do_item_full_set)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	uint8_t job = ch->GetJob();
	const entt::entity owner = character;
	for (int i = 0; i < 6; i++)
	{
		const entt::entity equippedItem = ItemSystem::GetWearItem(owner, i);
		if (equippedItem != entt::null)
			ItemSystem::UnequipItemEcs(owner, equippedItem);
	}
	const entt::entity shield = ItemSystem::GetWearItem(owner, WEAR_SHIELD);
	if (shield != entt::null)
		ItemSystem::UnequipItemEcs(owner, shield);

	LPITEM item;
	switch (job)
	{
	case JOB_SURA:
		{

			item = ITEM_MANAGER::instance().CreateItem(19712);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1733);//pajzs
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1773 );//cipi
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(399 );//kard
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1713 );//sisak
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1753 );//karos
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1813 );//fli
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1793 );//nyaki
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");


			item = ITEM_MANAGER::instance().CreateItem(85153);//SZARNY
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");

			item = ITEM_MANAGER::instance().CreateItem(88211);//EFFECT FEGYO
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(88213);//EFFETC VERT
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");

			item = ITEM_MANAGER::instance().CreateItem(10810);//tali
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
		}
		break;
	case JOB_WARRIOR:
		{

			item = ITEM_MANAGER::instance().CreateItem(19312);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1733);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1773 );
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(3269);//2kezes
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1713);//sisak
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1753 );
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1813 );
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1793 );
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(399);//kard
			item = ITEM_MANAGER::instance().CreateItem(85153);//SZARNY
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");

			item = ITEM_MANAGER::instance().CreateItem(88211);//EFFECT FEGYO
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(88213);//EFFETC VERT
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");

			item = ITEM_MANAGER::instance().CreateItem(10810);//tali
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
		}
		break;
	case JOB_SHAMAN:
		{

			item = ITEM_MANAGER::instance().CreateItem(19912);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1733);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1773 );
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(7349);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1713);//sisak
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1753 );
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1813 );
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1793 );
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(5209);

			item = ITEM_MANAGER::instance().CreateItem(85153);//SZARNY
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");

			item = ITEM_MANAGER::instance().CreateItem(88211);//EFFECT FEGYO
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(88213);//EFFETC VERT
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");

			item = ITEM_MANAGER::instance().CreateItem(10810);//tali
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
		}
		break;
	case JOB_ASSASSIN:
		{

			item = ITEM_MANAGER::instance().CreateItem(19512);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1733);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1773 );
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1229);//tr
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1713);//sisak
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1753);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1813);//fli
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1793);//nyaki
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");

			item = ITEM_MANAGER::instance().CreateItem(2249);//j

			item = ITEM_MANAGER::instance().CreateItem(85153);//SZARNY
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");

			item = ITEM_MANAGER::instance().CreateItem(88211);//EFFECT FEGYO
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(88213);//EFFETC VERT
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");

			item = ITEM_MANAGER::instance().CreateItem(10810);//tali
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
		}
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
	case JOB_WOLFMAN:
		{

			item = ITEM_MANAGER::instance().CreateItem(21049);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(13049);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1773);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(6049);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(21559);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1753);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1813);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
			item = ITEM_MANAGER::instance().CreateItem(1793);
			if (!item || !InventorySystem::EquipTo(item->GetEntityHandle(), ch->GetEntityHandle(), ItemSystem::FindEquipCell(ch->GetEntityHandle(), item->GetEntityHandle())))
				ItemSystem::DestroyItemEntityEcs(
			(item ? item->GetEntityHandle() : entt::null),
			"GM_CMD_DESTROY");
		}
		break;
#endif
	}
}

namespace {
struct FullAttributeSpec {
	uint8_t type;
	int16_t value;
};

void ApplyFullAttributes(entt::entity owner, uint8_t wearSlot,
	std::initializer_list<FullAttributeSpec> attributes)
{
	const entt::entity item = ItemSystem::GetWearItem(owner, wearSlot);
	if (!ItemSystem::IsValidItem(item))
		return;

	ItemSystem::ModifyItemPointsEcs(item, false);
	ItemSystem::ClearItemAttributesEcs(item);
	int index = 0;
	for (const FullAttributeSpec attribute : attributes)
		ItemSystem::SetItemForceAttributeEcs(
			item, index++, attribute.type, attribute.value);
	ItemSystem::ModifyItemPointsEcs(item, true);
}
}

ACMD (do_attr_full_set)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	uint8_t job = ch->GetJob();
	const entt::entity owner = character;
	switch (job)
	{
		case JOB_WARRIOR:
		case JOB_ASSASSIN:
		case JOB_SURA:
		case JOB_SHAMAN:
	#ifdef ENABLE_WOLFMAN_CHARACTER
		case JOB_WOLFMAN:
	#endif
		{

			ApplyFullAttributes(owner, WEAR_HEAD, {
				{APPLY_HP_REGEN, 25}, {APPLY_POISON_REDUCE, 10},
				{APPLY_RESIST_MAGIC, 15}, {APPLY_RESIST_BOW, 60},
				{APPLY_ATTBONUS_HUMAN, 10}, {APPLY_ATTBONUS_HUMAN, 10},
				{APPLY_MAX_HP, 4000}});
			ApplyFullAttributes(owner, WEAR_WEAPON, {
				{APPLY_NORMAL_HIT_DAMAGE_BONUS, 53}, {APPLY_SKILL_DAMAGE_BONUS, 10},
				{APPLY_ATTBONUS_HUMAN, 10}, {APPLY_ATTBONUS_DEVIL, 20},
				{APPLY_ATTBONUS_ORC, 20}, {APPLY_ATTBONUS_HUMAN, 10},
				{APPLY_MAX_HP, 4000}});
			ApplyFullAttributes(owner, WEAR_SHIELD, {
				{APPLY_ATTBONUS_HUMAN, 10}, {APPLY_BLOCK, 15},
				{APPLY_REFLECT_MELEE, 10}, {APPLY_IMMUNE_STUN, 1},
				{APPLY_IMMUNE_SLOW, 1}, {APPLY_ATTBONUS_HUMAN, 50},
				{APPLY_MAX_HP, 4000}});
			for (const uint8_t slot : {WEAR_BODY, WEAR_FOOTS, WEAR_NECK, WEAR_EAR})
				ApplyFullAttributes(owner, slot, {
					{APPLY_RESIST_SWORD, 15}, {APPLY_RESIST_TWOHAND, 15},
					{APPLY_RESIST_DAGGER, 15}, {APPLY_RESIST_BELL, 15},
					{APPLY_RESIST_FAN, 15}, {APPLY_ATTBONUS_HUMAN, 10},
					{APPLY_MAX_HP, 4000}});
			ApplyFullAttributes(owner, WEAR_WRIST, {
				{APPLY_MAX_HP, 2500}, {APPLY_ATTBONUS_HUMAN, 10},
				{APPLY_PENETRATE_PCT, 12}, {APPLY_STEAL_HP, 10},
				{APPLY_RESIST_MAGIC, 15}, {APPLY_ATTBONUS_HUMAN, 10},
				{APPLY_MAX_HP, 4000}});
			ApplyFullAttributes(owner, WEAR_PENDANT, {
				{APPLY_RESIST_MEZZIUOMINI, 10}, {APPLY_ATTBONUS_HUMAN, 10},
				{APPLY_CRITICAL_PCT, 15}});
		}
		break;
	}
}

ACMD (do_use_item)
{
	char arg1 [256];

	one_argument (argument, arg1, sizeof (arg1));

	int cell = 0;
	str_to_number(cell, arg1);

	const entt::entity owner = character;
	const entt::entity item = ItemSystem::GetInventoryItem(owner, cell);
	if (ItemSystem::IsValidItem(item))
	{
		ItemSystem::UseItemEcs(owner, item);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 811, "");
	}
#endif
}

ACMD (do_clear_affect)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->ClearAffect(true);
}

ACMD (do_dragon_soul)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[512];
	const char* rest = one_argument (argument, arg1, sizeof(arg1));
	switch (arg1[0])
	{
	case 'a':
		{
			one_argument (rest, arg1, sizeof(arg1));
			int deck_idx;
			if (str_to_number(deck_idx, arg1) == false)
			{
				return;
			}
			ch->DragonSoul_ActivateDeck(deck_idx);
		}
		break;
	case 'd':
		{
			ch->DragonSoul_DeactivateAll();
		}
		break;
	}
}

#ifdef __ENABLE_NEW_OFFLINESHOP__
std::string GetNewShopName(std::string_view oldname, std::string_view newname) {
	auto nameindex = oldname.find('@');
	if (nameindex == std::string_view::npos)
		return std::string(newname);

	else {
		std::string playername(oldname.substr(0, nameindex));
		playername += '@';
		playername.append(newname);
		return playername;
	}
}


ACMD(do_offshop_change_shop_name) {
	char arg1[50]; char arg2[256];
	argument = one_argument(argument, arg1, sizeof(arg1));
	argument = one_argument(argument, arg2, sizeof(arg2));

	if (arg1[0] != 0 && isdigit(arg1[0]) && arg2[0] != 0) {
		uint32_t id = 0;
		str_to_number(id, arg1);

		if (id == 0) {
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "syntax : offshop_change_shop_name  <player-id>  <new-name> ");
			return;
		} else {
			offlineshop::CShop* pkShop = offlineshop::GetManager().GetShopByOwnerID(id);
			if (!pkShop) {
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Cannot find shop by id %u ", id);
				return;
			} else {
				std::string oldname = pkShop->GetName();
				offlineshop::GetManager().SendShopChangeNameDBPacket(id, GetNewShopName(oldname, arg2).c_str());
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "shop's name changed.");
			}
		}

	} else {
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO , "syntax : offshop_change_shop_name  <player-id>  <new-name> ");
		return;
	}
}



ACMD(do_offshop_force_close_shop) {
	char arg1[50];
	argument = one_argument(argument, arg1, sizeof(arg1));
	if (arg1[0] != 0 && isdigit(arg1[0])) {

		uint32_t id = 0;
		str_to_number(id, arg1);

		if (id == 0) {
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "syntax : offshop_force_close_shop  <player-id>  ");
			return;
		}
		else {
			offlineshop::CShop* pkShop = offlineshop::GetManager().GetShopByOwnerID(id);
			if (!pkShop) {
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Cannot find shop by id %u ", id);
				return;
			}
			else {
				offlineshop::GetManager().SendShopForceCloseDBPacket(id);
				ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "shop closed successfully.");
			}
		}

	} else {
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "syntax : offshop_force_close_shop  <player-id>  ");
		return;
	}
}
#endif

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
ACMD(do_open_whispersys) {
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "recv_whispersys");
}
#endif
