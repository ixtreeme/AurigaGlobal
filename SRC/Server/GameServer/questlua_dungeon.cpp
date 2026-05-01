#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "constants.h"
#include "questmanager.h"
#include "questlua.h"
#include "dungeon.h"
#include "char_interface.hpp"
#include "party.h"
#include "buffer_manager.h"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "packet.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "db.h"
#include "config.h"
#include "battle_pass.h"
#include "ecs/quest_helpers.hpp"
#ifdef ENABLE_MELEY_LAIR
#include "guild.h"
#endif

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif
#ifndef ENABLE_DUNGEON_SHARED_DROP_HWID


template <class Func> Func CDungeon::ForEachMember(Func f)
{
	for (auto it = m_set_pkCharacter.begin(); it != m_set_pkCharacter.end(); ++it)
	{
		LOG_INFO("Dungeon ForEachMember {}", (*it)->GetName());
		f(*it);
	}
	return f;
}
#endif //ENABLE_DUNGEON_SHARED_DROP_HWID


namespace quest
{
#if defined(__DUNGEON_INFO_SYSTEM__)
	ALUA(dungeon_update_rank)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		// DungeonMapIndex, FinishTime, HighestDamage
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("Invalid argument");
			return 0;
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch) {
			return 0;
		}

		uint32_t pid = (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));

		int map_index = int(lua_tonumber(L, 1));
		int time = int(lua_tonumber(L, 2));
		int damage = int(lua_tonumber(L, 3));

		if (map_index <= 0) {
			return 0;
		}

		if (time < 0) {
			time = 0;
		}

		if (damage < 0) {
			damage = 0;
		}

		std::unique_ptr<SQLMsg> msgcheck(DBManager::instance().DirectQuery("SELECT time, damage FROM dungeon_ranking WHERE pid=%u AND dungeon_index=%d", pid, map_index));
		if (msgcheck->Get()->uiNumRows > 0)
		{
			MYSQL_ROW row = mysql_fetch_row(msgcheck->Get()->pSQLResult);
			int32_t lasttime = 0, lastdamage = 0;
			str_to_number(lasttime, row[0]);
			str_to_number(lastdamage, row[1]);

			lasttime = time > lasttime ? lasttime : time;
			lastdamage = damage > lastdamage ? damage : lastdamage;

			std::unique_ptr<SQLMsg> msgupdate(DBManager::instance().DirectQuery("UPDATE dungeon_ranking SET completed=completed+1, time=%d, damage=%d WHERE pid=%u AND dungeon_index=%d", lasttime, lastdamage, pid, map_index));
		}
		else
		{
			std::unique_ptr<SQLMsg> msgadd(DBManager::instance().DirectQuery("INSERT INTO dungeon_ranking (acc_id, pid, dungeon_index, completed, time, damage) VALUES ('%u', '%d', '%d', '%d', '%d', '%d')", ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetAccountTable().id : 0, pid, map_index, 1, time, damage));
		}

		return 1;
	}

	ALUA(dungeon_get_rank)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		if (!lua_tonumber(L, 1) || !lua_tonumber(L, 2)) {
			sys_err("Invalid argument");
			return 0;
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		int map_index = int(lua_tonumber(L, 1));
		uint8_t rank_type = int(lua_tonumber(L, 2));
		if (map_index <= 0 || rank_type <= 0) {
			sys_err("Invalid rank arguments");
			return 0;
		}

		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "CleanDungeonRanking");

		std::string szRankType;
		if (rank_type == 1)
			szRankType = "completed DESC";
		else if (rank_type == 2)
			szRankType = "time ASC";
		else if (rank_type == 3)
			szRankType = "damage DESC";

		{
			char szQuery[1024] = {0};
			snprintf(szQuery, sizeof(szQuery), "SELECT d.acc_id AS acc_id, d.pid AS pid, p.name AS name, p.level AS level, d.completed AS completed, d.time AS time, "
				"d.damage AS damage FROM player.dungeon_ranking%s AS d INNER JOIN player.player%s AS p ON d.pid = p.id "
				"WHERE dungeon_index = '%d' AND account_id=(SELECT id FROM account.account%s WHERE status='OK' AND id=d.acc_id) AND p.name not in(SELECT mName FROM common.gmlist%s) AND pid!='%d' ORDER BY d.%s LIMIT 100;", get_table_postfix(), get_table_postfix(), map_index, get_table_postfix(), get_table_postfix(), ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER ? (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))) : 0, szRankType.c_str());
			std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(szQuery));

			if (pMsg->Get()->uiNumRows > 0) {
				MYSQL_ROW row;
				for (unsigned int i = 0; i < pMsg->Get()->uiNumRows; ++i) {
					row = mysql_fetch_row(pMsg->Get()->pSQLResult);
					int c = 1;
					uint32_t pid;
					str_to_number(pid, row[c++]);
					const char * name = row[c++];
					int level;
					str_to_number(level, row[c++]);
					int points = 0;
					if (rank_type == 1) {
						str_to_number(points, row[c]);
					} else if (rank_type == 2) {
						c++;
						str_to_number(points, row[c]);
					} else if (rank_type == 3) {
						c++;
						c++;
						str_to_number(points, row[c]);
					}

					ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "UpdateDungeonRanking %s %d %d", name, level, points);
				}
			}
		}

		{
			char szQuery[1024] = {0};
			if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > GM_PLAYER) {
				snprintf(szQuery, sizeof(szQuery),
				"SELECT * FROM (SELECT @position:=0) AS a, (SELECT @position:=@position+1 AS r, d.acc_id AS acc_id, d.pid AS pid, p.name AS name, p.level AS level, d.completed AS completed, d.time AS time, "
				"d.damage AS damage FROM player.dungeon_ranking%s AS d INNER JOIN player.player%s AS p ON d.pid = p.id "
				"WHERE dungeon_index = '%d' ORDER BY d.%s) AS b WHERE b.pid = '%d';", get_table_postfix(), get_table_postfix(), map_index, szRankType.c_str(), (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));
			} else {
				snprintf(szQuery, sizeof(szQuery),
				"SELECT * FROM (SELECT @position:=0) AS a, (SELECT @position:=@position+1 AS r, d.acc_id AS acc_id, d.pid AS pid, p.name AS name, p.level AS level, d.completed AS completed, d.time AS time, "
				"d.damage AS damage FROM player.dungeon_ranking%s AS d INNER JOIN player.player%s AS p ON d.pid = p.id "
				"WHERE dungeon_index = '%d' AND p.name not in(SELECT mName FROM common.gmlist%s) ORDER BY d.%s) AS b WHERE b.pid = '%d';", get_table_postfix(), get_table_postfix(), map_index, get_table_postfix(), szRankType.c_str(), (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));
			}
			std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(szQuery));

			if (pMsg->Get()->uiNumRows > 0) {
				MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
				int c = 1;
				int p;
				str_to_number(p, row[c++]);
				c++;
				c++;
				const char * name = row[c++];
				int level;
				str_to_number(level, row[c++]);
				int points = 0;
				if (rank_type == 1) {
					str_to_number(points, row[c]);
				} else if (rank_type == 2) {
					c++;
					str_to_number(points, row[c]);
				} else if (rank_type == 3) {
					c++;
					c++;
					str_to_number(points, row[c]);
				}

				ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "UpdateMyDungeonRanking %d %s %d %d", p, name, level, points);
			}
		}

		return 0;
	}

	ALUA(dungeon_get_my_rank)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		// DungeonMapIndex, RankingType (Completed|FinishTime|HighestDamage)
		if (!lua_tonumber(L, 1) || !lua_tonumber(L, 2))
		{
			sys_err("Invalid argument");
			return 0;
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		uint32_t pid = (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));

		int map_index = int(lua_tonumber(L, 1));
		uint8_t rank_type = int(lua_tonumber(L, 2));

		if (map_index <= 0 || rank_type <= 0)
		{
			sys_err("Invalid rank arguments");
			return 0;
		}

		char szQuery[1024];
		snprintf(szQuery, sizeof(szQuery), "SELECT completed, time, damage FROM dungeon_ranking%s WHERE pid = '%u' AND dungeon_index = '%d'", get_table_postfix(), pid, map_index);
		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(szQuery));
		if (pMsg->Get()->uiNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

			int points = 0;
			if (rank_type == 1)
				str_to_number(points, row[0]); // completed
			else if (rank_type == 2)
				str_to_number(points, row[1]); // time
			else if (rank_type == 3)
				str_to_number(points, row[2]); // damage

			lua_pushnumber(L, points);
		}
		else
			lua_pushnumber(L, 0);

		return 1;
	}
#endif

	ALUA(dungeon_join_coords)
	{
		// migrated from CHARACTER::GetDungeon()
		// DUAL-PATH: ECS update + legacy call during migration window
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* dm = ECS_TryGet<ecs::DungeonMembership>(e);
		int32_t mapidx = (int32_t)lua_tonumber(L, 1);
		LPDUNGEON pDungeon = CDungeonManager::instance().Create(mapidx);
		if (!pDungeon) {
			sys_err("dungeon {} cannot be started.", mapidx);
			return 0;
		}

		if (dm)
		{
			dm->dungeon = pDungeon;
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		int32_t index = ch ? ((ch)->GetMapIndex()) : -1;
		if (index != -1) {
			LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
			if (!party)
			{
				pDungeon->Join_Coords(ch, (int32_t)lua_tonumber(L, 2), (int32_t)lua_tonumber(L, 3), index);
			}
			else if (party->GetLeaderPID() == (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))))
			{
				pDungeon->JoinParty_Coords(party, (int32_t)lua_tonumber(L, 2), (int32_t)lua_tonumber(L, 3), index);
			}
		}

		return 0;
	}

	ALUA(dungeon_find)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy fallback during migration window
		//if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			lua_pushboolean(L, 0);
			return 1;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		lua_pushboolean(L, 1);
		return 1;
	}

	ALUA(dungeon_jump_all)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 3 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->JumpAll(mapidx, (int32_t)lua_tonumber(L, 2), (int32_t)lua_tonumber(L, 3));
		return 0;
	}

	ALUA(dungeon_set_unique)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 3 || !lua_isnumber(L, 1) || !lua_isstring(L, 2) || !lua_isnumber(L, 3))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->SetUnique(lua_tostring(L, 2), (int32_t)lua_tonumber(L, 3));
		return 0;
	}

	ALUA(dungeon_is_unique_dead)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy fallback during migration window
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isstring(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
		{
			sys_err("not enough arguments.");
			lua_pushboolean(L, 0);
			return 1;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			lua_pushboolean(L, 0);
			return 1;
		}

		lua_pushboolean(L, dungeon->IsUniqueDead(lua_tostring(L, 1)) ? 1 : 0);
		return 1;
	}

	ALUA(dungeon_get_unique_vid)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy fallback during migration window
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isstring(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
		{
			sys_err("not enough arguments.");
			lua_pushnumber(L, 0);
			return 1;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, dungeon->GetUniqueVid(lua_tostring(L,1)));
		return 1;
	}

	ALUA(dungeon_kill_unique)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isstring(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->KillUnique(lua_tostring(L, 2));
		return 0;
	}

	ALUA(dungeon_set_flag)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 3 || !lua_isnumber(L, 1) || !lua_isstring(L, 2) || !lua_isnumber(L, 3))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->SetFlag(lua_tostring(L, 2), (int32_t)lua_tonumber(L, 3));
		return 0;
	}

	ALUA(dungeon_get_flag)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy fallback during migration window
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isstring(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
		{
			sys_err("not enough arguments.");
			lua_pushnumber(L, 0);
			return 1;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, dungeon->GetFlag(lua_tostring(L, 2)));
		return 1;
	}

	ALUA(dungeon_notice)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
#ifdef TEXTS_IMPROVEMENT
		//if (lua_gettop(L) < 3 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isstring(L, 3))
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isstring(L, 3))
#else
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isstring(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
#endif
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

#ifdef TEXTS_IMPROVEMENT
		if (lua_isboolean(L, 4))
		{
			dungeon->Notice((int32_t)lua_tonumber(L, 2), lua_tostring(L, 3), lua_toboolean(L, 4));
		}
		else
		{
			dungeon->Notice((int32_t)lua_tonumber(L, 2), lua_tostring(L, 3));
		}
#else
		dungeon->Notice(lua_tostring(L, 2));
#endif
		return 0;
	}

	ALUA(dungeon_spawn_mob)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 4 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		{
			sys_err("not enough arguments.");
			lua_pushnumber(L, 0);
			return 1;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			lua_pushnumber(L, 0);
			return 1;
		}

		int32_t vnum = (int32_t)lua_tonumber(L, 2);

		LPCHARACTER ch;
		if (lua_isnumber(L, 5))
		{
			ch = dungeon->SpawnMob(vnum, (int32_t)lua_tonumber(L, 3), (int32_t)lua_tonumber(L, 4), (int32_t)lua_tonumber(L, 5));
		}
		else
		{
			ch = dungeon->SpawnMob(vnum, (int32_t)lua_tonumber(L, 3), (int32_t)lua_tonumber(L, 4));
		}

		if (!ch)
		{
			lua_pushnumber(L, 0);
			return 1;
		}

#ifdef __DEFENSE_WAVE__
		if (vnum == 20434)
		{
			dungeon->SetMast(ch);
		}
		else if (vnum == 3956)
		{
			LPCHARACTER mast = dungeon->GetMast();
			if (mast)
			{
				ch->SetVictim(mast);
			}
		}
#endif

#ifdef ENABLE_MELEY_LAIR
		if (vnum == 6118)
		{
			ch->SetRotationToXY(320200, 1518100);
		}
#endif

		lua_pushnumber(L, ((ch)->GetLegacyVID()));
		return 1;
	}

	ALUA(dungeon_set_regen_file)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isstring(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->SpawnRegen(lua_tostring(L, 2), false);
		return 0;
	}

	ALUA(dungeon_regen_file)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isstring(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->SpawnRegen(lua_tostring(L, 2));
		return 0;
	}

	ALUA(dungeon_clear_regen)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->ClearRegen();
		return 0;
	}

	ALUA(dungeon_set_vid_invincible)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isboolean(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isboolean(L, 2))
		{
			sys_err("not enough arguments.");
			lua_pushboolean(L, 0);
			return 1;
		}

		int32_t vid = (int32_t)lua_tonumber(L, 1);

		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
		if (!ch) {
			sys_err("The vid {} not exist.", vid);
			lua_pushboolean(L, 0);
			return 1;
		}


#ifdef ENABLE_MELEY_LAIR
		bool lastmeley = lua_isboolean(L, 3);
		bool what = lua_toboolean(L, 2);
		if (what == false && !lastmeley)
		{
			if ((ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(ch))) == 6118 && ch->FindAffect(AFFECT_STATUE))
			{
				AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_STATUE);
			}
		}

		if (!lastmeley)
		{
			lua_pushboolean(L, ch->SetInvincible(what));
		}
		else
		{
			if ((ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(ch))) == 6118 && !ch->FindAffect(AFFECT_STATUE))
			{
				lua_pushboolean(L, ch->SetInvincible(what));
			}
			else
			{
				lua_pushboolean(L, true);
			}
		}
#else
		lua_pushboolean(L, ch->SetInvincible(lua_toboolean(L, 2)));
#endif
		return 1;
	}

	ALUA(dungeon_exit_all_lobby)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->ExitAllLobby((uint8_t)lua_tonumber(L, 2));
		return 0;
	}

	ALUA(dungeon_count_monster)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy fallback during migration window
		//if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			lua_pushnumber(L, 0);
			return 1;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, dungeon->CountMonster());
		return 1;
	}

	ALUA(dungeon_kill_all)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->KillAll();
		return 0;
	}

	ALUA(dungeon_kill_all_monsters)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->KillAllMonsters();
		return 0;
	}

	ALUA(dungeon_cmdchat)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isstring(L, 2))
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->CmdChat(lua_tostring(L, 2));
		return 0;
	}

	ALUA(dungeon_purge_vid)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t vid = (int32_t)lua_tonumber(L, 1);

		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
		if (ch)
		{
			M2_DESTROY_CHARACTER(ch);
		}

		return 0;
	}

#ifdef __DEFENSE_WAVE__
	ALUA(dungeon_kill_all_monstershydra)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->KillAllMonstersHydra();
		return 0;
	}

	ALUA(dungeon_restore_mast_partial_life)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		//if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		dungeon->RestoreMastPartialHP();
		return 0;
	}
#endif

	struct FPartyPIDCollectorDungeon
	{
		std::vector <uint32_t> vecPIDs;
		FPartyPIDCollectorDungeon()
		{
		}

		void operator () (LPCHARACTER ch)
		{
			if (ch && (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))))
			{
				vecPIDs.push_back((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));
			}
		}
	};

	ALUA(dungeon_check_entrance)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		std::string r = "";
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5) || !lua_isstring(L, 6))
		{
			sys_err("not enough arguments.");
			lua_pushnumber(L, 0);
			lua_pushnumber(L, 0);
			lua_pushstring(L, r.c_str());
			return 3;
		}

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
		{
			lua_pushnumber(L, 0);
			lua_pushnumber(L, 0);
			lua_pushstring(L, r.c_str());
			return 3;
		}

		int32_t m_minlvl = (int32_t)lua_tonumber(L, 1);
		int32_t m_maxlvl = (int32_t)lua_tonumber(L, 2);
		int32_t m_itemvnum1 = (int32_t)lua_tonumber(L, 3);
		int32_t m_itemvnum2 = (int32_t)lua_tonumber(L, 4);
		int32_t m_itemcount = (int32_t)lua_tonumber(L, 5);
		std::string m_questname = lua_tostring(L, 6);
		int32_t m_result = 1;
		int32_t m_resulttime = 0;
		std::string m_resultname = "";

		LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		if (party)
		{
			if (party->GetLeaderPID() != (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))))
			{
				lua_pushnumber(L, 2);
				lua_pushnumber(L, 0);
				lua_pushstring(L, r.c_str());
				return 3;
			}

			FPartyPIDCollectorDungeon f;
			party->ForEachOnMapMember(f, ((ch)->GetMapIndex()));

			for (auto it = f.vecPIDs.begin(); it != f.vecPIDs.end(); ++it)
			{
				LPCHARACTER tch = CHARACTER_MANAGER::instance().FindByPID(*it);
				if (tch && (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(tch))))
				{
					if (!tch->CanWarp())
					{
						m_result = 3;
						m_resultname = ((tch)->GetName());
					}

					int32_t lvl = ((tch)->GetLevel());
					if (lvl < m_minlvl)
					{
						m_result = 4;
						m_resultname = ((tch)->GetName());
						break;
					}
					else if (lvl > m_maxlvl)
					{
						m_result = 5;
						m_resultname = ((tch)->GetName());
						break;
					}
					else if (m_itemvnum2 == 0 && m_itemvnum1 > 0 && tch->CountSpecifyItem(m_itemvnum1) < m_itemcount)
					{
						m_result = 6;
						m_resultname = ((tch)->GetName());
						break;
					}
					else if (m_itemvnum2 > 0)
					{
						if (tch->CountSpecifyItem(m_itemvnum2) < m_itemcount && tch->CountSpecifyItem(m_itemvnum1) < m_itemcount)
						{
							m_result = 7;
							m_resultname = ((tch)->GetName());
							break;
						}
					}

					m_resulttime = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(tch), m_questname.c_str()) - get_global_time();
					if (m_resulttime > 0)
					{
						m_result = 8;
						m_resultname = ((tch)->GetName());
						break;
					}
				}
			}

			if (!q.GetPC((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))))
			{
				sys_err("cannot return to leader.");
			}
		}
		else
		{
			if (!ch->CanWarp())
			{
				m_result = 3;
				m_resultname = ((ch)->GetName());
			}

			int32_t lvl = ((ch)->GetLevel());
			m_resulttime = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), m_questname.c_str()) - get_global_time();
			if (lvl < m_minlvl)
			{
				m_result = 4;
				m_resultname = ((ch)->GetName());
			}
			else if (lvl > m_maxlvl)
			{
				m_result = 5;
				m_resultname = ((ch)->GetName());
			}
			else if (m_itemvnum2 == 0 && m_itemvnum1 > 0 && ch->CountSpecifyItem(m_itemvnum1) < m_itemcount)
			{
				m_result = 6;
				m_resultname = ((ch)->GetName());
			}
			else if (m_itemvnum2 > 0)
			{
				if (ch->CountSpecifyItem(m_itemvnum2) < m_itemcount && ch->CountSpecifyItem(m_itemvnum1) < m_itemcount)
				{
					m_result = 7;
					m_resultname = ((ch)->GetName());
				}
			}

			if (m_result == 1 && m_resulttime > 0)
			{
				m_result = 8;
				m_resultname = ((ch)->GetName());
			}
		}

		lua_pushnumber(L, m_result);
		lua_pushnumber(L, m_resulttime);
		lua_pushstring(L, m_resultname.c_str());
		return 3;
	}

	ALUA(dungeon_remove_item)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		std::string r = "";
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5) || !lua_isnumber(L, 6) || !lua_isnumber(L, 7) || !lua_isnumber(L, 8) || !lua_isnumber(L, 9) || !lua_isnumber(L, 10) || !lua_isnumber(L, 11) || !lua_isnumber(L, 12) || !lua_isnumber(L, 13) || !lua_isnumber(L, 14) || !lua_isnumber(L, 15) || !lua_isnumber(L, 16) || !lua_isnumber(L, 17) || !lua_isstring(L, 18))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
		{
			sys_err("no current character.");
			return 0;
		}

		int32_t vnum1 = (int32_t)lua_tonumber(L, 1), count1 = (int32_t)lua_tonumber(L, 2);
		int32_t vnum2 = (int32_t)lua_tonumber(L, 3), count2 = (int32_t)lua_tonumber(L, 4);
		int32_t vnum3 = (int32_t)lua_tonumber(L, 5), count3 = (int32_t)lua_tonumber(L, 6);
		int32_t vnum4 = (int32_t)lua_tonumber(L, 7), count4 = (int32_t)lua_tonumber(L, 8);
		int32_t vnum5 = (int32_t)lua_tonumber(L, 9), count5 = (int32_t)lua_tonumber(L, 10);
		int32_t vnum6 = (int32_t)lua_tonumber(L, 11), count6 = (int32_t)lua_tonumber(L, 12);
		int32_t vnum7 = (int32_t)lua_tonumber(L, 13), count7 = (int32_t)lua_tonumber(L, 14);
		int32_t vnum8 = (int32_t)lua_tonumber(L, 15), count8 = (int32_t)lua_tonumber(L, 16);
		int32_t cooldown = (int32_t)lua_tonumber(L, 17);
		std::string m_questname = lua_tostring(L, 18);

		LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		if (party)
		{
			FPartyPIDCollectorDungeon f;
			party->ForEachOnMapMember(f, ((ch)->GetMapIndex()));

			for (auto it = f.vecPIDs.begin(); it != f.vecPIDs.end(); ++it)
			{
				LPCHARACTER tch = CHARACTER_MANAGER::instance().FindByPID(*it);
				if (tch && (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(tch))))
				{
					bool ticket = false;
					if (vnum2 > 0 && count2 > 0)
					{
						if (tch->CountSpecifyItem(vnum2) >= count2)
						{
							ticket = true;
							tch->RemoveSpecifyItem(vnum2, count2);
						}
					}

					if (ticket == false && vnum1 > 0 && count1 > 0)
					{
						tch->RemoveSpecifyItem(vnum1, count1);
					}

					if (vnum3 > 0 && count3 > 0)
					{
						if (count3 == 255)
						{
							count3 = tch->CountSpecifyItem(vnum3);
						}
						
						tch->RemoveSpecifyItem(vnum3, count3);
					}

					if (vnum4 > 0 && count4 > 0)
					{
						if (count4 == 255)
						{
							count4 = tch->CountSpecifyItem(vnum4);
						}
						
						tch->RemoveSpecifyItem(vnum4, count4);
					}

					if (vnum5 > 0 && count5 > 0)
					{
						if (count5 == 255)
						{
							count5 = tch->CountSpecifyItem(vnum5);
						}
						
						tch->RemoveSpecifyItem(vnum5, count5);
					}

					if (vnum6 > 0 && count6 > 0)
					{
						if (count6 == 255)
						{
							count6 = tch->CountSpecifyItem(vnum6);
						}
						
						tch->RemoveSpecifyItem(vnum6, count6);
					}

					if (vnum7 > 0 && count7 > 0)
					{
						if (count7 == 255)
						{
							count7 = tch->CountSpecifyItem(vnum7);
						}
						
						tch->RemoveSpecifyItem(vnum7, count7);
					}

					if (vnum8 > 0 && count8 > 0)
					{
						if (count8 == 255)
						{
							count8 = tch->CountSpecifyItem(vnum8);
						}
						
						tch->RemoveSpecifyItem(vnum8, count8);
					}

					ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(tch), m_questname + ".enter_time", get_global_time());
					ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(tch), m_questname + ".cooldown", get_global_time() + cooldown);
				}
			}

			if (!q.GetPC((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))))
			{
				sys_err("cannot return to leader.");
			}
		}
		else
		{
			bool ticket = false;
			if (vnum2 > 0 && count2 > 0)
			{
				if (ch->CountSpecifyItem(vnum2) >= count2)
				{
					ticket = true;
					ch->RemoveSpecifyItem(vnum2, count2);
				}
			}

			if (ticket == false && vnum1 > 0 && count1 > 0)
			{
				ch->RemoveSpecifyItem(vnum1, count1);
			}

			if (vnum3 > 0 && count3 > 0)
			{
				if (count3 == 255)
				{
					count3 = ch->CountSpecifyItem(vnum3);
				}
				
				ch->RemoveSpecifyItem(vnum3, count3);
			}

			if (vnum4 > 0 && count4 > 0)
			{
				if (count4 == 255)
				{
					count4 = ch->CountSpecifyItem(vnum4);
				}
				
				ch->RemoveSpecifyItem(vnum4, count4);
			}

			if (vnum5 > 0 && count5 > 0)
			{
				if (count5 == 255)
				{
					count5 = ch->CountSpecifyItem(vnum5);
				}
				
				ch->RemoveSpecifyItem(vnum5, count5);
			}

			if (vnum6 > 0 && count6 > 0)
			{
				if (count6 == 255)
				{
					count6 = ch->CountSpecifyItem(vnum6);
				}
				
				ch->RemoveSpecifyItem(vnum6, count6);
			}

			if (vnum7 > 0 && count7 > 0)
			{
				if (count7 == 255)
				{
					count7 = ch->CountSpecifyItem(vnum7);
				}
				
				ch->RemoveSpecifyItem(vnum7, count7);
			}

			if (vnum8 > 0 && count8 > 0)
			{
				if (count8 == 255)
				{
					count8 = ch->CountSpecifyItem(vnum8);
				}
				
				ch->RemoveSpecifyItem(vnum8, count8);
			}

			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), m_questname + ".enter_time", get_global_time());
			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), m_questname + ".cooldown", get_global_time() + cooldown);
		}

		return 0;
	}

	ALUA(dungeon_complete)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		std::string r;
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isstring(L, 4))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
		{
			return 0;
		}

		const int32_t race = (int32_t)lua_tonumber(L, 1);
		const int32_t cooldown = (int32_t)lua_tonumber(L, 2);
		const int32_t idx = (int32_t)lua_tonumber(L, 3);
		const std::string questname = lua_tostring(L, 4);

		LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		if (party)
		{
			LPDUNGEON currentDungeon = ch->GetDungeon();
			FPartyPIDCollectorDungeon f;
			party->ForEachOnlineMember(f);
			//party->ForEachOnMapMember(f, ((ch)->GetMapIndex()));

			for (auto it = f.vecPIDs.begin(); it != f.vecPIDs.end(); ++it)
			{
				LPCHARACTER tch = CHARACTER_MANAGER::instance().FindByPID(*it);
				if (tch && (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(tch))))
				{

					if (currentDungeon)
					{
						LPDUNGEON memberDungeon = tch->GetDungeon();
						if (memberDungeon && memberDungeon != currentDungeon && ((tch)->GetMapIndex()) != ((ch)->GetMapIndex()))
						{
							continue;
						}
					}
					else if (((tch)->GetMapIndex()) != ((ch)->GetMapIndex()))
					{
						continue;
					}

					tch->SetRankPoints(16, tch->GetRankPoints(16) + 1);
#ifdef ENABLE_BATTLE_PASS
					const uint8_t battlepassid = tch->GetBattlePassId();
					if(battlepassid)
					{
						uint32_t id, count;
						if (CBattlePass::instance().BattlePassMissionGetInfo(battlepassid, COMPLETE_DUNGEON, &id, &count))
						{
							if (id == 1 && tch->GetMissionProgress(COMPLETE_DUNGEON, battlepassid) < count)
							{
								tch->UpdateMissionProgress(COMPLETE_DUNGEON, battlepassid, 1, count);
							}
						}
					}
#endif
					int32_t enter_time = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(tch), questname + ".enter_time");
					ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(tch), questname + ".enter_time", 0);
					ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(tch), questname + ".ch", 0);
					ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(tch), questname + ".cooldown", get_global_time() + cooldown);
					int32_t pid = (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(tch)));
					int32_t time = get_global_time() - enter_time;
					int32_t damage = tch->GetQuestDamage(race);

					if (time < 0) {
						time = 0;
					}

					if (damage < 0) {
						damage = 0;
					}

					std::unique_ptr<SQLMsg> msgcheck(DBManager::instance().DirectQuery("SELECT time, damage FROM dungeon_ranking WHERE pid=%u AND dungeon_index=%d", pid, idx));
					if (msgcheck->Get()->uiNumRows > 0)
					{
						MYSQL_ROW row = mysql_fetch_row(msgcheck->Get()->pSQLResult);
						int32_t lasttime = 0, lastdamage = 0;
						str_to_number(lasttime, row[0]);
						str_to_number(lastdamage, row[1]);

						lasttime = time > lasttime ? lasttime : time;
						lastdamage = damage > lastdamage ? damage : lastdamage;

						std::unique_ptr<SQLMsg> msgupdate(DBManager::instance().DirectQuery("UPDATE dungeon_ranking SET completed=completed+1, time=%d, damage=%d WHERE pid=%u AND dungeon_index=%d", lasttime, lastdamage, pid, idx));
					}
					else
					{
						LPDESC desc = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(tch));
						std::unique_ptr<SQLMsg> msgadd(DBManager::instance().DirectQuery("INSERT INTO dungeon_ranking (acc_id, pid, dungeon_index, completed, time, damage) VALUES ('%u', '%d', '%d', '%d', '%d', '%d')", desc ? desc->GetAccountTable().id : 0, pid, idx, 1, time, damage));
					}
				}
			}

			if (!q.GetPC((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))))
			{
				sys_err("cannot return to main.");
			}
		}
		else
		{
			ch->SetRankPoints(16, ch->GetRankPoints(16) + 1);
#ifdef ENABLE_BATTLE_PASS
			uint8_t battlepassid = ch->GetBattlePassId();
			if(battlepassid)
			{
				uint32_t id, count;
				if (CBattlePass::instance().BattlePassMissionGetInfo(battlepassid, COMPLETE_DUNGEON, &id, &count))
				{
					if (id == 1 && ch->GetMissionProgress(COMPLETE_DUNGEON, battlepassid) < count)
					{
						ch->UpdateMissionProgress(COMPLETE_DUNGEON, battlepassid, 1, count);
					}
				}
			}
#endif
			int32_t enter_time = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), questname + ".enter_time");
			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), questname + ".enter_time", 0);
			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), questname + ".ch", 0);
			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), questname + ".cooldown", get_global_time() + cooldown);
			int32_t pid = (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
			int32_t time = get_global_time() - enter_time;
			int32_t damage = ch->GetQuestDamage(race);

			if (time < 0) {
				time = 0;
			}

			if (damage < 0) {
				damage = 0;
			}

			std::unique_ptr<SQLMsg> msgcheck(DBManager::instance().DirectQuery("SELECT time, damage FROM dungeon_ranking WHERE pid=%u AND dungeon_index=%d", pid, idx));
			if (msgcheck->Get()->uiNumRows > 0)
			{
				MYSQL_ROW row = mysql_fetch_row(msgcheck->Get()->pSQLResult);
				int32_t lasttime = 0, lastdamage = 0;
				str_to_number(lasttime, row[0]);
				str_to_number(lastdamage, row[1]);

				lasttime = time > lasttime ? lasttime : time;
				lastdamage = damage > lastdamage ? damage : lastdamage;

				std::unique_ptr<SQLMsg> msgupdate(DBManager::instance().DirectQuery("UPDATE dungeon_ranking SET completed=completed+1, time=%d, damage=%d WHERE pid=%u AND dungeon_index=%d", lasttime, lastdamage, pid, idx));
			}
			else
			{
				LPDESC desc = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));
				std::unique_ptr<SQLMsg> msgadd(DBManager::instance().DirectQuery("INSERT INTO dungeon_ranking (acc_id, pid, dungeon_index, completed, time, damage) VALUES ('%u', '%d', '%d', '%d', '%d', '%d')", desc ? desc->GetAccountTable().id : 0, pid, idx, 1, time, damage));
			}
		}

		return 0;
	}

#ifdef ENABLE_MELEY_LAIR
	struct FPartyPIDCollectorDungeonGuild
	{
		std::string name;
		int32_t guildid;
		std::vector <uint32_t> vecPIDs;
		FPartyPIDCollectorDungeonGuild()
		{
		}

		void operator () (LPCHARACTER ch)
		{
			if (ch && (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))))
			{
				if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)))
				{
					if (guildid == ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->GetID())
					{
						vecPIDs.push_back((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));
					}
					else
					{
						name = ((ch)->GetName());
					}
				}
				else
				{
					name = ((ch)->GetName());
				}
			}
		}
	};

	ALUA(dungeon_check_entrance_meley)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		std::string r = "";
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isstring(L, 5))
		{
			sys_err("not enough arguments.");
			lua_pushnumber(L, 0);
			lua_pushnumber(L, 0);
			lua_pushstring(L, r.c_str());
			return 3;
		}

		CQuestManager & q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
		{
			lua_pushnumber(L, 0);
			lua_pushnumber(L, 0);
			lua_pushstring(L, r.c_str());
			return 3;
		}

		CGuild * guild = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch));
		if (!guild)
		{
			lua_pushnumber(L, 2);
			lua_pushnumber(L, 0);
			lua_pushstring(L, r.c_str());
			return 3;
		}

		int32_t m_minlvl = (int32_t)lua_tonumber(L, 1);
		int32_t m_maxlvl = (int32_t)lua_tonumber(L, 2);
		int32_t m_minguildlvl = (int32_t)lua_tonumber(L, 3);
		int32_t m_partycount = (int32_t)lua_tonumber(L, 4);
		std::string m_questname = lua_tostring(L, 5);
		int32_t m_result = 1;
		int32_t m_resulttime = 0;

		if (guild->GetLevel() < m_minguildlvl)
		{
			lua_pushnumber(L, 4);
			lua_pushnumber(L, m_minguildlvl);
			lua_pushstring(L, r.c_str());
			return 3;
		}

		LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		if (party)
		{
			if (party->GetLeaderPID() != (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))))
			{
				lua_pushnumber(L, 6);
				lua_pushnumber(L, 0);
				lua_pushstring(L, r.c_str());
				return 3;
			}

			FPartyPIDCollectorDungeonGuild f;
			f.guildid = guild->GetID();
			f.name = "";
			party->ForEachOnMapMember(f, ((ch)->GetMapIndex()));

			if (f.vecPIDs.size() < m_partycount)
			{
				lua_pushnumber(L, 7);
				lua_pushnumber(L, m_partycount);
				lua_pushstring(L, r.c_str());
				return 3;
			}

			if (!f.name.empty())
			{
				lua_pushnumber(L, 5);
				lua_pushnumber(L, 0);
				lua_pushstring(L, f.name.c_str());
				return 3;
			}

			for (auto it = f.vecPIDs.begin(); it != f.vecPIDs.end(); ++it)
			{
				LPCHARACTER tch = CHARACTER_MANAGER::instance().FindByPID(*it);
				if (tch && (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(tch))))
				{
					if (!tch->CanWarp())
					{
						m_result = 8;
						r = ((tch)->GetName());
					}

					int32_t lvl = ((tch)->GetLevel());
					if (lvl < m_minlvl)
					{
						m_result = 9;
						r = ((tch)->GetName());
						break;
					}
					else if (lvl > m_maxlvl)
					{
						m_result = 10;
						r = ((tch)->GetName());
						break;
					}

					m_resulttime = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(tch), m_questname.c_str()) - get_global_time();
					if (m_resulttime > 0)
					{
						m_result = 11;
						r = ((tch)->GetName());
						break;
					}
				}
			}

			if (!q.GetPC((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))))
			{
				sys_err("cannot return to leader.");
			}
		}
		else
		{
			lua_pushnumber(L, 3);
			lua_pushnumber(L, 0);
			lua_pushstring(L, r.c_str());
			return 3;
		}

		lua_pushnumber(L, m_result);
		lua_pushnumber(L, m_resulttime);
		lua_pushstring(L, r.c_str());
		return 3;
	}

	ALUA(dungeon_remove_item_meley)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		std::string r = "";
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isstring(L, 3))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
		{
			sys_err("no current character.");
			return 0;
		}

		int32_t vnum = (int32_t)lua_tonumber(L, 1);
		int32_t cooldown = (int32_t)lua_tonumber(L, 2);
		std::string m_questname = lua_tostring(L, 3);

		// LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		// if (party)
		// {
			// FPartyPIDCollectorDungeon f;
			// party->ForEachOnMapMember(f, ((ch)->GetMapIndex()));

			// for (auto it = f.vecPIDs.begin(); it != f.vecPIDs.end(); ++it)
			// {
				// LPCHARACTER tch = CHARACTER_MANAGER::instance().FindByPID(*it);
				// if (tch && (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(tch))))
				// {
					// if (vnum > 0)
					// {
						// int32_t count = tch->CountSpecifyItem(vnum);
						// if (count > 0)
						// {
							// tch->RemoveSpecifyItem(vnum, count);
						// }
					// }

					// ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(tch), m_questname + ".enter_time", get_global_time());
					// ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(tch), m_questname + ".cooldown", get_global_time() + cooldown);
				// }
			// }

			// if (!q.GetPC((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))))
			// {
				// "cannot return to leader.");
			// }
		// }
		// else
		{
			if (vnum > 0)
			{
				int32_t count = ch->CountSpecifyItem(vnum);
				if (count > 0)
				{
					ch->RemoveSpecifyItem(vnum, count);
				}
			}

			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), m_questname + ".enter_time", get_global_time());
			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), m_questname + ".cooldown", get_global_time() + cooldown);
		}

		return 0;
	}

	ALUA(dungeon_attack_meley)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		int32_t statue_vid1 = dungeon->GetFlag("statue_vid1"), statue_vid2 = dungeon->GetFlag("statue_vid2"), statue_vid3 = dungeon->GetFlag("statue_vid3"), statue_vid4 = dungeon->GetFlag("statue_vid4");

		LPCHARACTER statue1 = CHARACTER_MANAGER::instance().Find(statue_vid1);
		if (!statue1)
		{
			sys_err("The vid {} not exist.", statue_vid1);
			return 0;
		}

		LPCHARACTER statue2 = CHARACTER_MANAGER::instance().Find(statue_vid2);
		if (!statue2)
		{
			sys_err("The vid {} not exist.", statue_vid2);
			return 0;
		}

		LPCHARACTER statue3 = CHARACTER_MANAGER::instance().Find(statue_vid3);
		if (!statue3)
		{
			sys_err("The vid {} not exist.", statue_vid3);
			return 0;
		}

		LPCHARACTER statue4 = CHARACTER_MANAGER::instance().Find(statue_vid4);
		if (!statue4)
		{
			sys_err("The vid {} not exist.", statue_vid4);
			return 0;
		}

		statue1->SetRotationToXY(320200, 1518100);
		statue2->SetRotationToXY(320200, 1518100);
		statue3->SetRotationToXY(320200, 1518100);
		statue4->SetRotationToXY(320200, 1518100);

		int32_t time = get_dword_time();

		statue1->SendMovePacket(FUNC_MOB_SKILL, 0, statue1->GetX(), statue1->GetY(), 0, time);
		statue2->SendMovePacket(FUNC_MOB_SKILL, 0, statue2->GetX(), statue2->GetY(), 0, time);
		statue3->SendMovePacket(FUNC_MOB_SKILL, 0, statue3->GetX(), statue3->GetY(), 0, time);
		statue4->SendMovePacket(FUNC_MOB_SKILL, 0, statue4->GetX(), statue4->GetY(), 0, time);

		return 0;
	}

	ALUA(dungeon_set_meley_last_statue)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		int32_t statue_vid = (int32_t)lua_tonumber(L, 2);

		LPCHARACTER statue = CHARACTER_MANAGER::instance().Find(statue_vid);
		if (!statue)
		{
			sys_err("The vid {} not exist.", statue_vid);
			return 0;
		}

		if (ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(statue)) == 6118)
		{
			if (statue->FindAffect(AFFECT_STATUE))
			{
				AffectSystem::RemoveAffect(AIHelpers::EcsOf(statue), AFFECT_STATUE);
			}

			AffectSystem::AddAffect(AIHelpers::EcsOf(statue), AFFECT_STATUE, POINT_NONE, 0, AFF_STATUE4, 3600, 0, true);
		}

		return 0;
	}

	ALUA(dungeon_kill_meley)
	{
		// migrated from CHARACTER::GetDungeon
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t mapidx = (int32_t)lua_tonumber(L, 1);

		LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
		if (!dungeon)
		{
			sys_err("dungeon {} doesn't exist.", mapidx);
			return 0;
		}

		int32_t bossvid = dungeon->GetFlag("boss");

		LPCHARACTER boss = CHARACTER_MANAGER::instance().Find(bossvid);
		if (!boss)
		{
			sys_err("The vid {} not exist.", bossvid);
			return 0;
		}

		int32_t statue_vid1 = dungeon->GetFlag("statue_vid1"), statue_vid2 = dungeon->GetFlag("statue_vid2"), statue_vid3 = dungeon->GetFlag("statue_vid3"), statue_vid4 = dungeon->GetFlag("statue_vid4");

		LPCHARACTER statue1 = CHARACTER_MANAGER::instance().Find(statue_vid1);
		if (!statue1)
		{
			sys_err("The vid {} not exist.", statue_vid1);
			return 0;
		}

		LPCHARACTER statue2 = CHARACTER_MANAGER::instance().Find(statue_vid2);
		if (!statue2)
		{
			sys_err("The vid {} not exist.", statue_vid2);
			return 0;
		}

		LPCHARACTER statue3 = CHARACTER_MANAGER::instance().Find(statue_vid3);
		if (!statue3)
		{
			sys_err("The vid {} not exist.", statue_vid3);
			return 0;
		}

		LPCHARACTER statue4 = CHARACTER_MANAGER::instance().Find(statue_vid4);
		if (!statue4)
		{
			sys_err("The vid {} not exist.", statue_vid4);
			return 0;
		}

		boss->SetInvincible(false);
		boss->Dead();

		statue1->SetInvincible(false);
		statue2->SetInvincible(false);
		statue3->SetInvincible(false);
		statue4->SetInvincible(false);
		statue1->Dead();
		statue2->Dead();
		statue3->Dead();
		statue4->Dead();

		return 0;
	}
#endif

	void RegisterDungeonFunctionTable()
	{
		luaL_reg dungeon_functions[] =
		{
#if defined(__DUNGEON_INFO_SYSTEM__)
			{"update_rank", dungeon_update_rank},
			{"get_rank", dungeon_get_rank},
			{"get_my_rank", dungeon_get_my_rank},
#endif
			{"join_cords", dungeon_join_coords},
			{"find", dungeon_find},
			{"jump_all", dungeon_jump_all},
			{"set_unique", dungeon_set_unique},
			{"is_unique_dead", dungeon_is_unique_dead},
			{"get_unique_vid", dungeon_get_unique_vid},
			{"kill_unique", dungeon_kill_unique},
			{"setf", dungeon_set_flag},
			{"getf", dungeon_get_flag},
			{"notice", dungeon_notice},
			{"spawn_mob", dungeon_spawn_mob},
			{"regen_file", dungeon_regen_file},
			{"set_regen_file", dungeon_set_regen_file},
			{"clear_regen", dungeon_clear_regen},
			{"set_vid_invincible", dungeon_set_vid_invincible},
			{"exit_all_lobby", dungeon_exit_all_lobby},
			{"count_monster", dungeon_count_monster},
			{"kill_all", dungeon_kill_all},
			{"kill_all_monsters", dungeon_kill_all_monsters},
			{"cmdchat", dungeon_cmdchat},
			{"purge_vid", dungeon_purge_vid},
#ifdef __DEFENSE_WAVE__
			{"kill_all_monstershydra", dungeon_kill_all_monstershydra},
			{"restore_mast_partial_hp", dungeon_restore_mast_partial_life},
#endif
			{"check_entrance", dungeon_check_entrance},
			{"remove_item", dungeon_remove_item},
#ifdef ENABLE_MELEY_LAIR
			{"check_entrance_meley", dungeon_check_entrance_meley},
			{"remove_item_meley", dungeon_remove_item_meley},
			{"attack_meley", dungeon_attack_meley},
			{"set_meley_last_statue", dungeon_set_meley_last_statue},
			{"kill_meley", dungeon_kill_meley},
#endif
			{"complete", dungeon_complete},
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("d", dungeon_functions);
	}
}




