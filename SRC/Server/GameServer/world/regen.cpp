#include "stdafx.h"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "config.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "regen.h"
#include "JsonFile.h"
#include "mob_manager.h"

#include <nlohmann/json.hpp>
#include "dungeon.h"


#include "desc.h"

LPREGEN	regen_list = nullptr;
LPREGEN_EXCEPTION regen_exception_list = nullptr;

bool is_regen_exception(int32_t x, int32_t y)
{
	LPREGEN_EXCEPTION exc;

	for (exc = regen_exception_list; exc; exc = exc->next)
	{
		if (exc->sx <= x && exc->sy <= y)
			if (exc->ex >= x && exc->ey >= y)
				return true;
	}

	return false;
}

static void regen_spawn_dungeon(LPREGEN regen, entt::entity pDungeon, bool bOnce)
{
	uint32_t	num;
	uint32_t	i;

	num = (regen->max_count - regen->count);

	if (!num)
		return;

	for (i = 0; i < num; ++i)
	{
		entt::entity mob = entt::null;

		if (regen->type == REGEN_TYPE_ANYWHERE)
		{
			mob = CHARACTER_MANAGER::instance().SpawnMobRandomPosition(regen->vnum, regen->lMapIndex);

			if (mob != entt::null)
			{
				++regen->count;
				ecs::SocialSystem::SetDungeon(mob, pDungeon);
			}
		}
		else if (regen->sx == regen->ex && regen->sy == regen->ey)
		{
			mob = CHARACTER_MANAGER::instance().SpawnMobEntity(regen->vnum,
					regen->lMapIndex,
					regen->sx,
					regen->sy,
					regen->z_section,
					false,
					regen->direction == 0 ? number(0, 7) * 45 : (regen->direction - 1) * 45);

			if (mob != entt::null)
			{
				++regen->count;
				ecs::SocialSystem::SetDungeon(mob, pDungeon);
			}
		}
		else
		{
			if (regen->type == REGEN_TYPE_MOB)
			{
				mob = CHARACTER_MANAGER::Instance().SpawnMobRange(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, true);

				if (mob != entt::null)
				{
					++regen->count;
					ecs::SocialSystem::SetDungeon(mob, pDungeon);
				}
			}
			else if (regen->type == REGEN_TYPE_GROUP)
			{
				if (CHARACTER_MANAGER::Instance().SpawnGroup(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? nullptr : regen, regen->is_aggressive, pDungeon) != entt::null)
					++regen->count;
			}
			else if (regen->type == REGEN_TYPE_GROUP_GROUP)
			{
				if (CHARACTER_MANAGER::Instance().SpawnGroupGroup(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? nullptr : regen, regen->is_aggressive, pDungeon))
					++regen->count;
			}
		}

		if (mob != entt::null && !bOnce)
			ecs::PlayerRuntime::SetRegen(mob, regen);
		
#ifdef __DEFENSE_WAVE__
		if (pDungeon != entt::null)
		{
			const entt::entity mast = DungeonSystem::GetMast(pDungeon);
			// A group regen never sets mob, and a spawn that failed leaves it null.
			if (mob != entt::null && ecs::PlayerRuntime::IsValid(mast))
			{
				CombatSystem::SetVictim(mob, mast);
			}
		}
#endif
	}
}

static void regen_spawn(LPREGEN regen, bool bOnce)
{
	uint32_t	num;
	uint32_t	i;

	num = (regen->max_count - regen->count);

	if (!num)
		return;

	for (i = 0; i < num; ++i)
	{
		entt::entity mob = entt::null;

		if (regen->type == REGEN_TYPE_ANYWHERE)
		{
			mob = CHARACTER_MANAGER::instance().SpawnMobRandomPosition(regen->vnum, regen->lMapIndex);

			if (mob != entt::null)
				++regen->count;
		}
		else if (regen->sx == regen->ex && regen->sy == regen->ey)
		{
			mob = CHARACTER_MANAGER::instance().SpawnMobEntity(regen->vnum,
					regen->lMapIndex,
					regen->sx,
					regen->sy,
					regen->z_section,
					false,
					regen->direction == 0 ? number(0, 7) * 45 : (regen->direction - 1) * 45);

			if (mob != entt::null)
				++regen->count;
		}
		else
		{
			if (regen->type == REGEN_TYPE_MOB)
			{
				mob = CHARACTER_MANAGER::Instance().SpawnMobRange(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, true, regen->is_aggressive, regen->is_aggressive );

				if (mob != entt::null)
					++regen->count;
			}
			else if (regen->type == REGEN_TYPE_GROUP)
			{
				if (CHARACTER_MANAGER::Instance().SpawnGroup(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? nullptr : regen, regen->is_aggressive) != entt::null)
					++regen->count;
			}
			else if (regen->type == REGEN_TYPE_GROUP_GROUP)
			{
				if (CHARACTER_MANAGER::Instance().SpawnGroupGroup(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? nullptr : regen, regen->is_aggressive))
					++regen->count;
			}
		}

		if (mob != entt::null && !bOnce)
			ecs::PlayerRuntime::SetRegen(mob, regen);
	}
}

EVENTFUNC(dungeon_regen_event)
{
	dungeon_regen_event_info* info = dynamic_cast<dungeon_regen_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("dungeon_regen_event> <Factor> Null pointer");
		return 0;
	}

	const entt::entity pDungeon = info->dungeon;
	if (!DungeonSystem::IsValid(pDungeon) || info->regen == nullptr) {
		return 0;
	}

	LPREGEN	regen = info->regen;
	if (regen->time == 0)
	{
		regen->event = nullptr;
	}

	regen_spawn_dungeon(regen, pDungeon, false);
	return PASSES_PER_SEC(regen->time);
}

namespace
{
	bool RegenTypeFromToken(const std::string& token, REGEN& regen)
	{
		if (token == "mob")
		{
			regen.type = REGEN_TYPE_MOB;
			return true;
		}

		if (token == "group")
		{
			regen.type = REGEN_TYPE_GROUP;
			return true;
		}

		if (token == "group_aggressive")
		{
			regen.type = REGEN_TYPE_GROUP;
			regen.is_aggressive = true;
			return true;
		}

		if (token == "exception")
		{
			regen.type = REGEN_TYPE_EXCEPTION;
			return true;
		}

		if (token == "group_group")
		{
			regen.type = REGEN_TYPE_GROUP_GROUP;
			return true;
		}

		if (token == "anywhere")
		{
			regen.type = REGEN_TYPE_ANYWHERE;
			return true;
		}

		LOG_ERROR("RegenFromJson: unknown regen type '{}'", token);
		exit(1);
	}

	bool RegenFromJson(const nlohmann::json& entry, REGEN& regen)
	{
		if (!RegenTypeFromToken(entry.value("type", std::string()), regen))
			return false;

		const int x = entry.value("x", 0);
		const int y = entry.value("y", 0);
		const int range_x = entry.value("range_x", 0);
		const int range_y = entry.value("range_y", 0);

		regen.sx = x;
		regen.sy = y;

		// Same maths as the old MODE_EX / MODE_EY.
		regen.sx -= range_x;
		regen.ex = regen.sx + range_x * 2;

		regen.sy -= range_y;
		regen.ey = regen.sy + range_y * 2;

		regen.sx *= 100;
		regen.ex *= 100;

		regen.sy *= 100;
		regen.ey *= 100;

		regen.z_section = static_cast<uint8_t>(entry.value("z", 0));
		regen.direction = static_cast<uint8_t>(entry.value("dir", 0));
		// "time" is already in seconds; "percent" is kept in the JSON only
		// (the old parser read but never used it).
		regen.time = entry.value("time", 0u);
		regen.max_count = entry.value("count", 0);
		regen.vnum = entry.value("vnum", 0);
		return true;
	}
}

bool regen_do(const char* filename, int32_t lMapIndex, int base_x, int base_y, entt::entity pDungeon, bool bOnce)
{
	if (g_bNoRegen)
		return true;

	if ( lMapIndex >= 114 && lMapIndex <= 117 )
		return true;

	LPREGEN regen = nullptr;

	nlohmann::json doc;

	if (!JsonFile::Load(filename, doc) || !doc.contains("regens") || !doc.at("regens").is_array())
	{
		LOG_ERROR("SYSTEM: regen_do: {}: file not found", filename);
		return false;
	}

	for (const auto& entry : doc.at("regens"))
	{
		REGEN tmp{};

		if (!RegenFromJson(entry, tmp))
			continue;

		if (tmp.type == REGEN_TYPE_MOB ||
			tmp.type == REGEN_TYPE_GROUP ||
			tmp.type == REGEN_TYPE_GROUP_GROUP ||
			tmp.type == REGEN_TYPE_ANYWHERE)
		{
			if (!bOnce)
			{
				regen = new REGEN;
				*regen = tmp;
			}
			else
				regen = &tmp;

			if (pDungeon != entt::null)
				regen->is_aggressive = true;

			regen->lMapIndex = lMapIndex;
			regen->count = 0;

			regen->sx += base_x;
			regen->ex += base_x;

			regen->sy += base_y;
			regen->ey += base_y;

			if (regen->sx > regen->ex)
			{
				regen->sx ^= regen->ex;
				regen->ex ^= regen->sx;
				regen->sx ^= regen->ex;
			}

			if (regen->sy > regen->ey)
			{
				regen->sy ^= regen->ey;
				regen->ey ^= regen->sy;
				regen->sy ^= regen->ey;
			}

			if (regen->type == REGEN_TYPE_MOB)
			{
				const CMob * p = CMobManager::instance().Get(regen->vnum);

				if (!p)
				{
					LOG_ERROR("In {}, No mob data by vnum {}", filename, regen->vnum);
					if (!bOnce) {
						delete regen;
					}
					continue;
				}
			}

			if (!bOnce && pDungeon != entt::null)
			{
				dungeon_regen_event_info* info = AllocEventInfo<dungeon_regen_event_info>();

				info->regen = regen;
				info->dungeon = pDungeon;

				regen->event = event_create(dungeon_regen_event, info, PASSES_PER_SEC(number(0, 16)) + PASSES_PER_SEC(regen->time));

				DungeonSystem::AddRegen(pDungeon, regen);
				// regen_id should be determined at this point,
				// before the call to ecs::PlayerRuntime::SetRegen()
			}

			regen_spawn_dungeon(regen, pDungeon, bOnce);

		}
	}

	return true;
}

bool regen_load_in_file(const char* filename, int32_t lMapIndex, int base_x, int base_y)
{
	if (g_bNoRegen)
		return true;

	LPREGEN regen = nullptr;

	nlohmann::json doc;

	if (!JsonFile::Load(filename, doc) || !doc.contains("regens") || !doc.at("regens").is_array())
	{
		LOG_ERROR("SYSTEM: regen_do: {}: file not found", filename);
		return false;
	}

	for (const auto& entry : doc.at("regens"))
	{
		REGEN tmp{};

		if (!RegenFromJson(entry, tmp))
			continue;

		if (tmp.type == REGEN_TYPE_MOB ||
			tmp.type == REGEN_TYPE_GROUP ||
			tmp.type == REGEN_TYPE_GROUP_GROUP ||
			tmp.type == REGEN_TYPE_ANYWHERE)
		{
			regen = &tmp;

			regen->is_aggressive = true;

			regen->lMapIndex = lMapIndex;
			regen->count = 0;

			regen->sx += base_x;
			regen->ex += base_x;

			regen->sy += base_y;
			regen->ey += base_y;

			if (regen->sx > regen->ex)
			{
				regen->sx ^= regen->ex;
				regen->ex ^= regen->sx;
				regen->sx ^= regen->ex;
			}

			if (regen->sy > regen->ey)
			{
				regen->sy ^= regen->ey;
				regen->ey ^= regen->sy;
				regen->sy ^= regen->ey;
			}

			if (regen->type == REGEN_TYPE_MOB)
			{
				const CMob * p = CMobManager::instance().Get(regen->vnum);

				if (!p)
				{
					LOG_ERROR("In {}, No mob data by vnum {}", filename, regen->vnum);
					continue;
				}
			}

			regen_spawn(regen, true);
		}
	}

	return true;
}

EVENTFUNC(regen_event)
{
	regen_event_info* info = dynamic_cast<regen_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("regen_event> <Factor> Null pointer");
		return 0;
	}

	LPREGEN	regen = info->regen;

	if (regen->time == 0)
		regen->event = nullptr;

	regen_spawn(regen, false);
	return PASSES_PER_SEC(regen->time);
}

#ifdef ENABLE_ATLAS_BOSS
bool regen_load(const char* filename, int32_t lMapIndex, int base_x, int base_y, bool bossFile)
#else
bool regen_load(const char* filename, int32_t lMapIndex, int base_x, int base_y)
#endif
{
	if (g_bNoRegen)
		return true;

	LPREGEN regen = nullptr;

	nlohmann::json doc;

	if (!JsonFile::Load(filename, doc) || !doc.contains("regens") || !doc.at("regens").is_array())
	{
		LOG_INFO("SYSTEM: regen_load: {}: file not found", filename);
		return false;
	}

	for (const auto& entry : doc.at("regens"))
	{
		REGEN tmp{};

		if (!RegenFromJson(entry, tmp))
			continue;

		if (tmp.type == REGEN_TYPE_MOB ||
			tmp.type == REGEN_TYPE_GROUP ||
			tmp.type == REGEN_TYPE_GROUP_GROUP ||
			tmp.type == REGEN_TYPE_ANYWHERE)
		{
			if (test_server)
			{
				CMobManager::instance().IncRegenCount(tmp.type, tmp.vnum, tmp.max_count, tmp.time);
			}

			regen = new REGEN;
			*regen = tmp;
			INSERT_TO_TW_LIST(regen, regen_list, prev, next);

			regen->lMapIndex = lMapIndex;
			regen->count = 0;

			regen->sx += base_x;
			regen->ex += base_x;

			regen->sy += base_y;
			regen->ey += base_y;

			if (regen->sx > regen->ex)
			{
				regen->sx ^= regen->ex;
				regen->ex ^= regen->sx;
				regen->sx ^= regen->ex;
			}

			if (regen->sy > regen->ey)
			{
				regen->sy ^= regen->ey;
				regen->ey ^= regen->sy;
				regen->sy ^= regen->ey;
			}
			
			if (regen->type == REGEN_TYPE_MOB)
			{
				const CMob * p = CMobManager::instance().Get(regen->vnum);

				if (!p)
				{
					LOG_ERROR("In {}, No mob data by vnum {}", filename, regen->vnum);
				}
				else if (p->m_table.bType == CHAR_TYPE_NPC || p->m_table.bType == CHAR_TYPE_WARP || p->m_table.bType == CHAR_TYPE_GOTO)
				{
					SECTREE_MANAGER::instance().InsertNPCPosition(lMapIndex,
							p->m_table.bType,
#ifdef ENABLE_MULTI_NAMES
							regen->vnum
#else
							p->m_table.szLocaleName
#endif
							, (regen->sx+regen->ex) / 2 - base_x,
							(regen->sy+regen->ey) / 2 - base_y);
				}
			}
			
#ifdef ENABLE_ATLAS_BOSS
			if ((bossFile) && (tmp.type == REGEN_TYPE_MOB)) {
				const CMob * p = CMobManager::instance().Get(regen->vnum);
				if (!p)
					LOG_ERROR("In {}, No mob data by vnum {}", filename, regen->vnum);
				else {
					SECTREE_MANAGER::instance().InsertBossPosition(lMapIndex, p->m_table.bType, 
#ifdef ENABLE_MULTI_NAMES
					regen->vnum
#else
					p->m_table.szLocaleName
#endif
					, (regen->sx+regen->ex) / 2 - base_x, (regen->sy+regen->ey) / 2 - base_y, tmp.time);
				}
			}
#endif
			
			//NO_REGEN
			if (regen->time != 0)
			{
				regen_spawn(regen, false);

				regen_event_info* info = AllocEventInfo<regen_event_info>();

				info->regen = regen;

				regen->event = event_create(regen_event, info, PASSES_PER_SEC(number(0, 16)) + PASSES_PER_SEC(regen->time));
			}
			//END_NO_REGEN
		}
		else if (tmp.type == REGEN_TYPE_EXCEPTION)
		{
			LPREGEN_EXCEPTION exc;

			exc = new REGEN_EXCEPTION;

			exc->sx = tmp.sx;
			exc->sy = tmp.sy;
			exc->ex = tmp.ex;
			exc->ey = tmp.ey;
			exc->z_section = tmp.z_section;

			INSERT_TO_TW_LIST(exc, regen_exception_list, prev, next);
		}
	}

	return true;
}

void regen_free(void)
{
	LPREGEN		regen, next_regen;
	LPREGEN_EXCEPTION	exc, next_exc;

	for (regen = regen_list; regen; regen = next_regen)
	{
		next_regen = regen->next;

		event_cancel(&regen->event);
		delete regen;
	}

	regen_list = nullptr;

	for (exc = regen_exception_list; exc; exc = next_exc)
	{
		next_exc = exc->next;

		delete exc;
	}

	regen_exception_list = nullptr;
}

void regen_reset(int x, int y)
{
	LPREGEN regen;

	for (regen = regen_list; regen; regen = regen->next)
	{
		if (!regen->event)
			continue;

		if (x != 0 || y != 0)
		{
			if (x >= regen->sx && x <= regen->ex)
				if (y >= regen->sy && y <= regen->ey)
					event_reset_time(regen->event, 1);
		}
		else
			event_reset_time(regen->event, 1);
	}
}

