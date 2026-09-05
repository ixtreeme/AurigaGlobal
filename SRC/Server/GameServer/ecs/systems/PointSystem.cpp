#include "../../stdafx.h"
#include "ViewSystem.hpp"

#include "PointSystem.hpp"
#include "PointRouter.hpp"
#include "QuestSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "SkillSystem.hpp"
#include "CombatSystem.hpp"
#include "SocialSystem.hpp"
#include "../components/skill_components.hpp"
#include "NetworkSyncSystem.hpp"
#include "MountSystem.hpp"
#include "AffectSystem.hpp"
#include "ItemSystem.hpp"

#include <array>
#include <common/VnumHelper.h>

#include "../CharacterAccessors.hpp"
#include "../Registry.hpp"
#include "../components/character_stats_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/inventory_components.hpp"
#include "../components/status_components.hpp"
#include "../components/vital_components.hpp"
#include "../EventDispatcher.hpp"
#include "../events.hpp"
#include "../../char.h"


#include "../../config.h"
#include "../../utils.h"
#include "../../crc32.h"
#include "../../char_manager.h"
#include "../../desc_client.h"
#include "../../desc_manager.h"
#include "../../buffer_manager.h"
#include "../../item_manager.h"
#include "../../motion.h"
#include "../../vector.h"
#include "../../packet.h"
#include "../../cmd.h"
#include "../../fishing.h"
#include "../../exchange.h"
#include "../../battle.h"
#include "../../affect.h"
#include "../../shop.h"
#include "../../shop_manager.h"
#include "../../safebox.h"
#include "../../MountInventory.h"
#include "../../regen.h"
#include "../../pvp.h"
#include "../../party.h"
#include "../../start_position.h"
#include "../../questmanager.h"
#include "../../log.h"
#include "../../p2p.h"
#include "../../guild.h"
#include "../../guild_manager.h"
#include "../../dungeon.h"
#include "../../messenger_manager.h"
#include "../../unique_item.h"
#include "../../priv_manager.h"
#include "../../war_map.h"
#include "../../banword.h"
#include "../../target.h"
#include "../../wedding.h"
#include "../../mob_manager.h"
#include "../../mining.h"
#include "../../arena.h"
#include "../../dev_log.h"
#include "../../horsename_manager.h"
#include "../../pcbang.h"
#include "../../gm.h"
#include "../../map_location.h"
#include "../../skill_power.h"
#include "../../buff_on_attributes.h"
#include "../../constants.h"
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "../../new_offlineshop.h"
#include "../../new_offlineshop_manager.h"
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
#include "../../MountSystem.h"
#endif

#ifdef ENABLE_BATTLE_PASS
#include "../../battle_pass.h"
#endif

#ifdef __PET_SYSTEM__
#include "../../PetSystem.h"
#endif
#ifdef __NEWPET_SYSTEM__
#include "../../New_PetSystem.h"
#endif
#include <boost/algorithm/string/find.hpp>

#include "../../DragonSoul.h"
#include <Core/Logging.hpp>
#include <common/CommonDefines.h>

namespace ecs::PointSystem {

namespace {

bool IsReadableEntity(entt::entity e)
{
	return e != entt::null && g_registry.valid(e);
}

void WarnLegacyOnlyPointOnce(uint8_t type, entt::entity e)
{
	static std::array<bool, POINT_MAX_NUM> warned {};
	if (type >= POINT_MAX_NUM || warned[type])
		return;

	warned[type] = true;
	LOG_WARN("[POINT_LEGACY_ONLY] type={} entity={} no ECS source yet (one-time warning)",
		static_cast<int>(type), static_cast<uint32_t>(e));
}

int64_t GetNextExpFromEcs(entt::entity e)
{
	if (!IsReadableEntity(e))
		return 0;

	if (const auto* exp = g_registry.try_get<ecs::Experience>(e); exp && exp->next > 0)
		return exp->next;

	const auto* level = g_registry.try_get<ecs::LevelComponent>(e);
	if (!level)
		return 0;

	if (PLAYER_MAX_LEVEL_CONST < level->value)
		return 2500000000LL;

	return exp_table[level->value];
}

int64_t ReadInstantArray(entt::entity e, uint8_t type)
{
	if (!IsReadableEntity(e))
		return 0;

	const auto* stats = g_registry.try_get<ecs::CharacterStatsComponent>(e);
	if (!stats)
		return 0;

	return stats->points[type];
}

int64_t ReadRealArray(entt::entity e, uint8_t type)
{
	if (!IsReadableEntity(e))
		return 0;

	const auto* points = g_registry.try_get<ecs::CharacterPoints>(e);
	if (!points)
		return 0;

	return points->base.points[type];
}

void SyncInstantPointMirror(entt::entity ch, uint8_t type, int64_t val)
{
	if (!ecs::PlayerRuntime::IsValid(ch) || type >= POINT_MAX_NUM || !ecs::IsStatArrayPoint(type))
		return;

	const entt::entity e = ch;
	if (!IsReadableEntity(e))
		return;

	auto& stats = g_registry.get_or_emplace<ecs::CharacterStatsComponent>(e);
	stats.points[type] = val;

	auto& points = g_registry.get_or_emplace<ecs::CharacterPoints>(e);
	points.instant.points[type] = val;
}

void SyncRealPointMirror(entt::entity ch, uint8_t type, int64_t val)
{
	if (!ecs::PlayerRuntime::IsValid(ch) || type >= POINT_MAX_NUM)
		return;

	const entt::entity e = ch;
	if (!IsReadableEntity(e))
		return;

	auto& points = g_registry.get_or_emplace<ecs::CharacterPoints>(e);
	points.base.points[type] = val;
}

} // namespace

int64_t Get(entt::entity e, uint8_t type)
{
	if (type >= POINT_MAX_NUM || !IsReadableEntity(e))
		return 0;

	const auto& mapping = ecs::PointRouter::g_pointMap[type];

	switch (mapping.source) {
	case ecs::PointRouter::PointSource::SRC_NONE:
		return 0;
	case ecs::PointRouter::PointSource::SRC_GOLD:
		if (const auto* gold = g_registry.try_get<ecs::GoldAmount>(e))
			return gold->amount;
		return 0;
	case ecs::PointRouter::PointSource::SRC_HP_CURRENT:
		if (const auto* health = g_registry.try_get<ecs::Health>(e))
			return health->current;
		return 0;
	case ecs::PointRouter::PointSource::SRC_HP_MAX:
		if (const auto* health = g_registry.try_get<ecs::Health>(e))
			return health->max;
		return 0;
	case ecs::PointRouter::PointSource::SRC_SP_CURRENT:
		if (const auto* mana = g_registry.try_get<ecs::Mana>(e))
			return mana->current;
		return 0;
	case ecs::PointRouter::PointSource::SRC_SP_MAX:
		if (const auto* mana = g_registry.try_get<ecs::Mana>(e))
			return mana->max;
		return 0;
	case ecs::PointRouter::PointSource::SRC_STAMINA_CURRENT:
		if (const auto* stamina = g_registry.try_get<ecs::Stamina>(e))
			return stamina->current;
		return 0;
	case ecs::PointRouter::PointSource::SRC_STAMINA_MAX:
		if (const auto* stamina = g_registry.try_get<ecs::Stamina>(e))
			return stamina->max;
		return 0;
	case ecs::PointRouter::PointSource::SRC_LEVEL:
		if (const auto* level = g_registry.try_get<ecs::LevelComponent>(e))
			return level->value;
		return 0;
	case ecs::PointRouter::PointSource::SRC_EXP:
		if (const auto* exp = g_registry.try_get<ecs::Experience>(e))
			return exp->current;
		return 0;
	case ecs::PointRouter::PointSource::SRC_NEXT_EXP_COMPUTED:
		return GetNextExpFromEcs(e);
	case ecs::PointRouter::PointSource::SRC_INSTANT_ARRAY:
		return ReadInstantArray(e, mapping.index);
	case ecs::PointRouter::PointSource::SRC_REAL_ARRAY:
		return ReadRealArray(e, mapping.index);
	case ecs::PointRouter::PointSource::SRC_LEGACY_ONLY:
		WarnLegacyOnlyPointOnce(type, e);
		if (auto* ch = ecs::LegacyCharOf(e))
			return ch->GetPoint(type);
		return 0;
	default:
		return 0;
	}
}

int64_t GetReal(entt::entity e, uint8_t type)
{
	if (type >= POINT_MAX_NUM || !IsReadableEntity(e))
		return 0;

	const auto& mapping = ecs::PointRouter::g_pointMap[type];

	switch (mapping.source) {
	case ecs::PointRouter::PointSource::SRC_NONE:
		return 0;
	case ecs::PointRouter::PointSource::SRC_GOLD:
	case ecs::PointRouter::PointSource::SRC_HP_CURRENT:
	case ecs::PointRouter::PointSource::SRC_HP_MAX:
	case ecs::PointRouter::PointSource::SRC_SP_CURRENT:
	case ecs::PointRouter::PointSource::SRC_SP_MAX:
	case ecs::PointRouter::PointSource::SRC_STAMINA_CURRENT:
	case ecs::PointRouter::PointSource::SRC_STAMINA_MAX:
	case ecs::PointRouter::PointSource::SRC_EXP:
	case ecs::PointRouter::PointSource::SRC_NEXT_EXP_COMPUTED:
		return Get(e, type);
	case ecs::PointRouter::PointSource::SRC_LEVEL:
		if (mapping.readable_in_real)
			return Get(e, type);
		return ReadRealArray(e, type);
	case ecs::PointRouter::PointSource::SRC_INSTANT_ARRAY:
		return ReadRealArray(e, mapping.index);
	case ecs::PointRouter::PointSource::SRC_REAL_ARRAY:
		return ReadRealArray(e, mapping.index);
	case ecs::PointRouter::PointSource::SRC_LEGACY_ONLY:
		WarnLegacyOnlyPointOnce(type, e);
		if (auto* ch = ecs::LegacyCharOf(e))
			return ch->GetRealPoint(type);
		return 0;
	default:
		return 0;
	}
}

int64_t GetGold(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* gold = g_registry.try_get<ecs::GoldAmount>(e))
			return gold->amount;
	}

	return 0;
}

int32_t GetMaxHP(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* health = g_registry.try_get<ecs::Health>(e))
			return health->max;
	}

	return 0;
}

int32_t GetMaxSP(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* mana = g_registry.try_get<ecs::Mana>(e))
			return mana->max;
	}

	return 0;
}

int32_t GetLevel(entt::entity e)
{
	if (e != entt::null && g_registry.valid(e)) {
		if (const auto* level = g_registry.try_get<ecs::LevelComponent>(e))
			return level->value;
	}

	return 0;
}

bool Set(entt::entity e, uint8_t type, int64_t value)
{
	if (type >= POINT_MAX_NUM || !IsReadableEntity(e))
		return false;

	// The legacy method mirrors the write back into CharacterStatsComponent.
	if (auto* character = ecs::LegacyCharOf(e))
	{
		character->SetPoint(type, value);
		return true;
	}

	auto& stats = g_registry.get_or_emplace<ecs::CharacterStatsComponent>(e);
	stats.points[type] = value;
	auto& points = g_registry.get_or_emplace<ecs::CharacterPoints>(e);
	points.instant.points[type] = value;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	return true;
}

bool SetReal(entt::entity e, uint8_t type, int64_t value)
{
	if (type >= POINT_MAX_NUM || !IsReadableEntity(e))
		return false;

	// Compatibility boundary until CHARACTER's point storage is removed.
	if (auto* character = ecs::LegacyCharOf(e))
	{
		character->SetRealPoint(type, value);
		return true;
	}

	auto& points = g_registry.get_or_emplace<ecs::CharacterPoints>(e);
	points.base.points[type] = value;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	return true;
}

void SetRandomHP(entt::entity e, int value)
{
	if (!IsReadableEntity(e))
		return;
	g_registry.get_or_emplace<ecs::CharacterPoints>(e).base.iRandomHP = value;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void SetRandomSP(entt::entity e, int value)
{
	if (!IsReadableEntity(e))
		return;
	g_registry.get_or_emplace<ecs::CharacterPoints>(e).base.iRandomSP = value;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

int GetRandomHP(entt::entity e)
{
	if (!IsReadableEntity(e))
		return 0;
	const auto* points = g_registry.try_get<ecs::CharacterPoints>(e);
	return points ? points->base.iRandomHP : 0;
}

int GetRandomSP(entt::entity e)
{
	if (!IsReadableEntity(e))
		return 0;
	const auto* points = g_registry.try_get<ecs::CharacterPoints>(e);
	return points ? points->base.iRandomSP : 0;
}

bool SetLevelFromQuest(entt::entity e, int newLevel)
{
	if (!IsReadableEntity(e))
		return false;
	const int oldLevel = GetLevel(e);
	Change(e, POINT_SKILL, newLevel - oldLevel);
	Change(e, POINT_SUB_SKILL, newLevel < 10 ? 0 : newLevel - MAX(oldLevel, 9));
	Change(e, POINT_STAT,
		(MINMAX(1, newLevel, gPlayerMaxLevel) - oldLevel) * 3 + Get(e, POINT_LEVEL_STEP));
	Change(e, POINT_LEVEL, newLevel - oldLevel);

	const uint8_t job = ecs::PlayerRuntime::GetJob(e);
	SetRandomHP(e, (newLevel - 1) * number(
		JobInitialPoints[job].hp_per_lv_begin, JobInitialPoints[job].hp_per_lv_end));
	SetRandomSP(e, (newLevel - 1) * number(
		JobInitialPoints[job].sp_per_lv_begin, JobInitialPoints[job].sp_per_lv_end));
	Compute(e);
	Change(e, POINT_HP, GetMaxHP(e) - Get(e, POINT_HP));
	Change(e, POINT_SP, GetMaxSP(e) - Get(e, POINT_SP));
	NetworkSyncSystem::PointsPacket(e);
	SkillSystem::SendSkillLevelPacket(e);
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	g_dispatcher.trigger(ecs::EvLevelUp { e, GetLevel(e) });
	return true;
}

bool ResetStat(entt::entity e, int statIndex)
{
	if (!IsReadableEntity(e) || statIndex < 0 || statIndex > 3)
		return false;
	static constexpr uint8_t points[] { POINT_HT, POINT_IQ, POINT_ST, POINT_DX };
	static constexpr const char* names[] { "ht", "iq", "st", "dx" };
	const uint8_t point = points[statIndex];
	const int64_t oldValue = GetReal(e, point);
	const int64_t oldStat = GetReal(e, POINT_STAT);
	SetReal(e, point, 1);
	Set(e, point, 1);
	Change(e, POINT_STAT, oldValue - 1);

	const uint8_t job = ecs::PlayerRuntime::GetJob(e);
	if (point == POINT_HT)
		SetRandomHP(e, (GetLevel(e) - 1) * number(
			JobInitialPoints[job].hp_per_lv_begin, JobInitialPoints[job].hp_per_lv_end));
	else if (point == POINT_IQ)
		SetRandomSP(e, (GetLevel(e) - 1) * number(
			JobInitialPoints[job].sp_per_lv_begin, JobInitialPoints[job].sp_per_lv_end));

	Compute(e);
	NetworkSyncSystem::PointsPacket(e);
	if (point == POINT_HT)
		Change(e, POINT_HP, GetMaxHP(e) - Get(e, POINT_HP));
	else if (point == POINT_IQ)
		Change(e, POINT_SP, GetMaxSP(e) - Get(e, POINT_SP));

	char detail[128];
	snprintf(detail, sizeof(detail), "reset %s(%lld)->1 stat_point(%lld)->(%lld)",
		names[statIndex], oldValue, oldStat, GetReal(e, POINT_STAT));
	LogManager::instance().CharLog(e, 0, "RESET_ONE_STATUS", detail);
	return true;
}

bool ResetAllPoints(entt::entity e, int level)
{
	if (!IsReadableEntity(e))
		return false;
	const uint8_t job = ecs::PlayerRuntime::GetJob(e);
	Change(e, POINT_LEVEL, level - GetLevel(e));
	for (const auto [point, value] : {
		std::pair<uint8_t, int> { POINT_ST, JobInitialPoints[job].st },
		std::pair<uint8_t, int> { POINT_HT, JobInitialPoints[job].ht },
		std::pair<uint8_t, int> { POINT_DX, JobInitialPoints[job].dx },
		std::pair<uint8_t, int> { POINT_IQ, JobInitialPoints[job].iq } })
	{
		SetReal(e, point, value);
		Set(e, point, value);
	}
	SetRandomHP(e, (level - 1) * number(
		JobInitialPoints[job].hp_per_lv_begin, JobInitialPoints[job].hp_per_lv_end));
	SetRandomSP(e, (level - 1) * number(
		JobInitialPoints[job].sp_per_lv_begin, JobInitialPoints[job].sp_per_lv_end));
	int statusLevel = level;
#ifdef ENABLE_STATUS_MAX_344_POINTS
	if (statusLevel > 0)
		--statusLevel;
#endif
	Change(e, POINT_STAT,
		MINMAX(1, statusLevel, g_iStatusPointGetLevelLimit) * 3 +
		Get(e, POINT_LEVEL_STEP) - Get(e, POINT_STAT));
	Compute(e);
	Change(e, POINT_HP, GetMaxHP(e) - Get(e, POINT_HP));
	Change(e, POINT_SP, GetMaxSP(e) - Get(e, POINT_SP));
	NetworkSyncSystem::PointsPacket(e);
	LogManager::instance().CharLog(e, 0, "RESET_POINT", "");
	return true;
}

void Compute(entt::entity e)
{
	// Compatibility boundary until point calculation is component-native.
	if (auto* character = ecs::LegacyCharOf(e))
		character->ComputePoints();
}

#ifdef __ENABLE_BLOCK_EXP__
bool IsExperienceBlocked(entt::entity e)
{
	if (!IsReadableEntity(e))
		return false;

	const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
	return (status && status->blockExp) ||
		ecs::QuestSystem::GetFlag(e, "exp.stat") == 1;
}

bool SetExperienceBlocked(entt::entity e, bool blocked)
{
	if (!IsReadableEntity(e))
		return false;

	auto& status = g_registry.get_or_emplace<ecs::StatusFlags>(e);
	status.blockExp = blocked;
	ecs::QuestSystem::SetFlag(e, "exp.stat", blocked ? 1 : 0);
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	return true;
}
#endif


} // namespace ecs::PointSystem

#include "../../../Poly/Constants.h"
#ifdef __SEND_TARGET_INFO__
#include <algorithm>
#include <iterator>
#endif
#ifdef ENABLE_SWITCHBOT
#include "../../new_switchbot.h"
#endif
#ifdef ENABLE_RUNE_SYSTEM
#include <common/rune_length.h>
#endif
#ifdef ENABLE_STOLE_COSTUME
#include <common/stole_length.h>
#endif
#include "../../mount_inventory_helper.h"
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "../../OrcsDungeon.h"
#include "../../TritonTempleDungeon.h"
#include "../../ValentineDungeon.h"
#include "../../RuneDungeon.h"
#include "../../PyramidDungeonRazor93.h"
#include "../../NightmareDungeonRazor93.h"
#include "../../Halloween2022Dungeon.h"
#include "../../VikingDungeon.h"
#include "../../EasterDungeon.h"
#endif


int64_t CHARACTER::GetRealPoint(uint8_t type) const
{
	return m_points.points[type];
}

void CHARACTER::SetRandomHP(int value)
{
	m_points.iRandomHP = value;
	ecs::PointSystem::SetRandomHP(GetEntityHandle(), value);
}

void CHARACTER::SetRandomSP(int value)
{
	m_points.iRandomSP = value;
	ecs::PointSystem::SetRandomSP(GetEntityHandle(), value);
}

void CHARACTER::SetRealPoint(uint8_t type, int64_t val)
{
	m_points.points[type] = val;
	ecs::PointSystem::SyncRealPointMirror(GetEntityHandle(), type, val);
#ifdef ENABLE_RANKING
	if (type == POINT_PLAYTIME)
		SetRankPoints(15, val);
#endif
}

int CHARACTER::GetPolymorphPoint(uint8_t type) const
{
	return ecs::PointSystem::GetPolymorphPoint(GetEntityHandle(), type);
}

int64_t CHARACTER::GetPoint(uint8_t type) const
{
	if (type >= POINT_MAX_NUM)
	{
		LOG_ERROR("Point type overflow (type {})", type);
		return 0;
	}

	int64_t val = m_pointsInstant.points[type];
	int64_t max_val = INT_MAX;

	switch (type)
	{
	case POINT_STEAL_HP:
	case POINT_STEAL_SP:
		max_val = 50;
		break;
	case POINT_GOLD:
		max_val = GOLD_MAX;
		break;
	}

	if (val > max_val)
		LOG_ERROR("POINT_ERROR: {} type {} val {} (max: {})", GetName(), type, val, max_val);

	return (val);
}

int CHARACTER::GetLimitPoint(uint8_t type) const
{
	if (type >= POINT_MAX_NUM)
	{
		LOG_ERROR("Point type overflow (type {})", type);
		return 0;
	}

	int val = m_pointsInstant.points[type];
	int max_val = INT_MAX;
	int limit = INT_MAX;
	int min_limit = -INT_MAX;

	switch (type)
	{
	case POINT_ATT_SPEED:
		min_limit = 0;

		if (IsPC())
			limit = 170;
		else
			limit = 250;
		break;

	case POINT_MOV_SPEED:
		min_limit = 0;
		limit = 350;
		break;

	case POINT_STEAL_HP:
	case POINT_STEAL_SP:
		limit = 50;
		max_val = 50;
		break;

	case POINT_MALL_ATTBONUS:
	case POINT_MALL_DEFBONUS:
		limit = 20;
		max_val = 50;
		break;
	}

	if (val > max_val)
		LOG_ERROR("POINT_ERROR: {} type {} val {} (max: {})", GetName(), type, val, max_val);

	if (val > limit)
		val = limit;

	if (val < min_limit)
		val = min_limit;

	return (val);
}

void CHARACTER::SetPoint(uint8_t type, int64_t val)
{
	if (type >= POINT_MAX_NUM)
	{
		LOG_ERROR("Point type overflow (type {})", type);
		return;
	}


	m_pointsInstant.points[type] = val;
	ecs::PointSystem::SyncInstantPointMirror(GetEntityHandle(), type, val);


	// B.1.2: read timing via getters (ECS MovementState).
	if (type == POINT_MOV_SPEED && get_dword_time() < GetCurrentMoveStartTime() + GetCurrentMoveDuration())
	{
		CalculateMoveDuration();
	}
}

int64_t CHARACTER::GetAllowedGold() const
{
	if (GetLevel() <= 10)
		return 100000;
	else if (GetLevel() <= 20)
		return 500000;
	else
		return 50000000;
}

void CHARACTER::CheckMaximumPoints()
{
	if (GetMaxHP() < GetHP())
		PointChange(POINT_HP, GetMaxHP() - GetHP());

	if (GetMaxSP() < GetSP())
		PointChange(POINT_SP, GetMaxSP() - GetSP());
}


namespace ecs::PointSystem {

void Change(entt::entity e, uint8_t type, int64_t amount, bool bAmount, bool bBroadcast
#ifdef __ENABLE_BLOCK_EXP__
	, bool bForceExp
#endif
)
{
	// Resolved once for the leaves below that still own legacy state -
	// ComputePoints, Save, the packet senders, gaya and the inventory point.
	// Every point read and write in this body goes through components.
	LPCHARACTER ch = ecs::LegacyCharOf(e);
	int64_t val = 0;


	//LOG_TRACE("PointChange {} {} | {} -> {} cHP {} mHP {}", type, amount, Get(e, type), Get(e, type)+amount, Get(e, POINT_HP), GetMaxHP(e));

	switch (type)
	{
	case POINT_NONE:

#ifdef ENABLE_BATTLE_PASS
	case POINT_BATTLE_PASS_ID:
#endif		

		return;

	case POINT_LEVEL:
		if ((GetLevel(e) + amount) > gPlayerMaxLevel)
			return;

		ecs::PlayerRuntime::SetLevel(e, GetLevel(e) + amount);
		val = GetLevel(e);

		LOG_INFO("LEVELUP: {} {} NEXT EXP {}", ecs::PlayerRuntime::GetName(e).data(), GetLevel(e), ecs::PlayerRuntime::GetNextExp(e));
#ifdef ENABLE_WOLFMAN_CHARACTER
		if (ecs::PlayerRuntime::GetJob(e) == JOB_WOLFMAN)
		{
			if ((5 <= val) && (SkillSystem::GetSkillGroup(e) != 1))
			{
				ClearSkill();
				// set skill group
				SetSkillGroup(1);
				// set skill points
				SetReal(e, POINT_SKILL, GetLevel(e) - 1);
				Set(e, POINT_SKILL, GetReal(e, POINT_SKILL));
				Change(e, POINT_SKILL, 0);
				// update points (not required)
				// if (ch) ch->ComputePoints();
				// PointsPacket();
			}
		}
#endif
		Change(e, POINT_NEXT_EXP, ecs::PlayerRuntime::GetNextExp(e), false);
#ifdef ENABLE_ANNOUNCEMENT_LEVELUP
#ifdef TEXTS_IMPROVEMENT
		switch (val) {
		case 30:
		case 40:
		case 50:
		case 60:
		case 70:
		case 80:
		case 85:
		case 90:
		case 95:
		case 100:
		case 105:
		case 110:
		case 115:
		case 120:
		case 125:
		case 130:
		case 135:
		case 136:
		case 137:
		case 138:
		case 139:
			BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 546, "%s#%d", ecs::PlayerRuntime::GetName(e).data(), val);
			break;
		default:
			break;
		}
#endif
#endif
		if (amount)
		{
			quest::CQuestManager::instance().LevelUp(ecs::PlayerRuntime::GetPlayerID(e));

			LogManager::instance().LevelLog(e, val, GetReal(e, POINT_PLAYTIME) + (get_dword_time() - (ch ? ch->GetPlayStartTime() : 0u)) / 60000);

			if ((ch ? ch->GetGuild() : nullptr))
			{
				(ch ? ch->GetGuild() : nullptr)->LevelChange(ecs::PlayerRuntime::GetPlayerID(e), GetLevel(e));
			}

			if (::ecs::SocialSystem::GetParty(e))
			{
				::ecs::SocialSystem::GetParty(e)->RequestSetMemberLevel(ecs::PlayerRuntime::GetPlayerID(e), GetLevel(e));
			}
		}
		break;

	case POINT_NEXT_EXP:
		val = ecs::PlayerRuntime::GetNextExp(e);
		bAmount = false;	// 1ï¿½ï¿½ï¿½ï¿½ï¿½ bAmountï¿½ï¿½ false ?ï¿½3ï¿½ ï¿½Nï¿½U.
		break;

	case POINT_EXP:
	{
		uint32_t exp = ecs::PlayerRuntime::GetExp(e);
		uint32_t next_exp = ecs::PlayerRuntime::GetNextExp(e);

		// expï¿½! 0 AIï¿½Iï¿½ï¿½ ï¿½!ï¿½ï¿½ 3Eï¿½ï¿½ï¿½I ï¿½Nï¿½U
		if ((amount < 0) && (exp < (uint32_t)(-amount)))
		{
			LOG_TRACE("{} AMOUNT < 0 {}, CUR EXP: {}", ecs::PlayerRuntime::GetName(e).data(), -amount, exp);
			amount = exp; // -exp

			ecs::PlayerRuntime::SetExp(e, exp + amount);
			val = ecs::PlayerRuntime::GetExp(e);
		}
		else
		{
			if (gPlayerMaxLevel <= GetLevel(e))
				return;

#ifdef __ENABLE_BLOCK_EXP__
			if (ecs::PointSystem::IsExperienceBlocked(e) && !bForceExp)
			{
				return;
			}
#endif

			//#ifdef TEXTS_IMPROVEMENT
			//
			//					if (amount > 0) {
			//						auto s = std::to_string(amount);
			//						int n = s.length() - 3;
			//						while (n > 0) {
			//							s.insert(n, ".");
			//							n -= 3;
			//						}
			//
			//						ecs::ChatSystem::SendNew(e,
			//#ifdef ENABLE_NEW_CHAT
			//						CHAT_TYPE_INFO_EXP
			//#else
			//						CHAT_TYPE_INFO
			//#endif
			//						, 2, "%s", s.c_str());
			//					}
			//#endif
			uint32_t iExpBalance = 0;

			// ï¿½1oï¿½ 3ï¿½!
			if (exp + amount >= next_exp)
			{
				iExpBalance = (exp + amount) - next_exp;
				amount = next_exp - exp;

				ecs::PlayerRuntime::SetExp(e, 0);
				exp = next_exp;
			}
			else
			{
				ecs::PlayerRuntime::SetExp(e, exp + amount);
				exp = ecs::PlayerRuntime::GetExp(e);
			}

			uint32_t q = uint32_t(next_exp / 4.0f);
			int iLevStep = GetReal(e, POINT_LEVEL_STEP);

			// iLevStepAI 4 AIï¿½ï¿½AIï¿½ï¿½ ï¿½1oï¿½AI ?Aï¿½ï¿½3ï¿½3ï¿½ ï¿½I1Ç·ï¿½ ?ï¿½ï¿½ï¿½?! ?A 1ï¿½ 3oï¿½ï¿½ ï¿½aAIï¿½U.
			if (iLevStep >= 4)
			{
				LOG_ERROR("{} LEVEL_STEP bigger than 4! ({})", ecs::PlayerRuntime::GetName(e).data(), iLevStep);
				iLevStep = 4;
			}

			if (exp >= next_exp && iLevStep < 4)
			{
				for (int i = 0; i < 4 - iLevStep; ++i)
					Change(e, POINT_LEVEL_STEP, 1, false, true);
			}
			else if (exp >= q * 3 && iLevStep < 3)
			{
				for (int i = 0; i < 3 - iLevStep; ++i)
					Change(e, POINT_LEVEL_STEP, 1, false, true);
			}
			else if (exp >= q * 2 && iLevStep < 2)
			{
				for (int i = 0; i < 2 - iLevStep; ++i)
					Change(e, POINT_LEVEL_STEP, 1, false, true);
			}
			else if (exp >= q && iLevStep < 1)
				Change(e, POINT_LEVEL_STEP, 1);

			if (iExpBalance)
			{
				Change(e, POINT_EXP, iExpBalance);
			}

			val = ecs::PlayerRuntime::GetExp(e);
		}
	}
	break;

	case POINT_LEVEL_STEP:
		if (amount > 0)
		{
			val = Get(e, POINT_LEVEL_STEP) + amount;

			switch (val)
			{
			case 1:
			case 2:
			case 3:
			{
				int iLvl = GetLevel(e);
#ifdef ENABLE_STATUS_MAX_344_POINTS
				if (iLvl > 115)
					break;
				else if ((iLvl == 115) && (val == 3))
					break;

				Change(e, POINT_STAT, 1);
#else
				if ((iLvl <= g_iStatusPointGetLevelLimit) && (iLvl <= gPlayerMaxLevel))
					Change(e, POINT_STAT, 1);
#endif
			}
			break;

			case 4:
			{
				int iHP = number(JobInitialPoints[ecs::PlayerRuntime::GetJob(e)].hp_per_lv_begin, JobInitialPoints[ecs::PlayerRuntime::GetJob(e)].hp_per_lv_end);
				int iSP = number(JobInitialPoints[ecs::PlayerRuntime::GetJob(e)].sp_per_lv_begin, JobInitialPoints[ecs::PlayerRuntime::GetJob(e)].sp_per_lv_end);

				SetRandomHP(e, GetRandomHP(e) + iHP);
				SetRandomSP(e, GetRandomSP(e) + iSP);

				if (SkillSystem::GetSkillGroup(e))
				{
					if (GetLevel(e) >= 5)
						Change(e, POINT_SKILL, 1);

					if (GetLevel(e) >= 9)
						Change(e, POINT_SUB_SKILL, 1);
				}

				Change(e, POINT_MAX_HP, iHP);
				Change(e, POINT_MAX_SP, iSP);
				Change(e, POINT_LEVEL, 1, false, true);

				val = 0;
			}
			break;
			}

			Change(e, POINT_HP, GetMaxHP(e) - Get(e, POINT_HP));
			Change(e, POINT_SP, GetMaxSP(e) - Get(e, POINT_SP));
			Change(e, POINT_STAMINA, ecs::PlayerRuntime::GetMaxStamina(e) - ecs::PlayerRuntime::GetStamina(e));

			Set(e, POINT_LEVEL_STEP, val);
			SetReal(e, POINT_LEVEL_STEP, val);

			if (ch) ch->Save();
		}
		else
			val = Get(e, POINT_LEVEL_STEP);

		break;

	case POINT_HP:
	{
		if (::CombatSystem::IsDead(e) || ::CombatSystem::IsStun(e))
			return;

		int64_t prev_hp = Get(e, POINT_HP);

		amount = std::min(GetMaxHP(e) - Get(e, POINT_HP), amount);
		ecs::PlayerRuntime::SetHP(e, Get(e, POINT_HP) + amount);
		val = Get(e, POINT_HP);

		if (ch) ch->BroadcastTargetPacket();

		if (::ecs::SocialSystem::GetParty(e) && (ecs::PlayerRuntime::GetDesc(e) != nullptr) && val != prev_hp)
			::ecs::SocialSystem::GetParty(e)->SendPartyInfoOneToAll(e);
	}
	break;

	case POINT_SP:
	{
		if (::CombatSystem::IsDead(e) || ::CombatSystem::IsStun(e))
			return;

		amount = std::min(GetMaxSP(e) - Get(e, POINT_SP), amount);
		ecs::PlayerRuntime::SetSP(e, Get(e, POINT_SP) + amount);
		val = Get(e, POINT_SP);
	}
	break;

	case POINT_STAMINA:
	{
		if (::CombatSystem::IsDead(e) || ::CombatSystem::IsStun(e))
			return;

		int prev_val = ecs::PlayerRuntime::GetStamina(e);
		amount = std::min(ecs::PlayerRuntime::GetMaxStamina(e) - ecs::PlayerRuntime::GetStamina(e), amount);
		ecs::PlayerRuntime::SetStamina(e, ecs::PlayerRuntime::GetStamina(e) + amount);
		val = ecs::PlayerRuntime::GetStamina(e);

		if (val == 0)
		{
			// Staminaï¿½! 3oAï¿½ï¿½I ï¿½EAï¿½!
			if (ch) ch->SetNowWalking(true);
		}
		else if (prev_val == 0)
		{
			// 3oï¿½o 1oAï¿½1I3aï¿½! ï¿½ï¿½ï¿½aAï¿½ï¿½I AIAï¿½ ï¿½?ï¿½a o1ï¿½ï¿½
			if (ch) ch->ResetWalking();
		}

		if (amount < 0 && val != 0) // ï¿½ï¿½1Oï¿½ï¿½ oï¿½3ï¿½ï¿½ï¿½3Eï¿½Â´U.
			return;
	}
	break;

	case POINT_MAX_HP:
	{
		Set(e, type, Get(e, type) + amount);

		const int64_t base = GetReal(e, POINT_MAX_HP);              // 20-30k
		const int64_t flat = Get(e, POINT_MAX_HP);                  // ï¿½kszerek stb. fix +HP (ettï¿½l lesz 350k)
		const int64_t party = Get(e, POINT_PARTY_TANKER_BONUS);
		const int64_t pct = Get(e, POINT_MAX_HP_PCT);              // +20

		const int64_t totalNoPct = base + flat + party;                // pl 350k
		int64_t newMax = totalNoPct + (totalNoPct * pct) / 100;        // 350k + 20% = 420k

		if (newMax < 1)
			newMax = 1;

		ecs::PlayerRuntime::SetMaxHP(e, newMax);
		val = GetMaxHP(e);
	}
	break;

	case POINT_MAX_SP:
	{
		Set(e, type, Get(e, type) + amount);

		//ecs::PlayerRuntime::SetMaxSP(e, GetMaxSP(e) + amount);
		// AÖ´ï¿½ ï¿½ï¿½1Aï¿½ï¿½ = (ï¿½ï¿½oï¿½ AÖ´ï¿½ ï¿½ï¿½1Aï¿½ï¿½ + Aß°!) * AÖ´ï¿½ï¿½ï¿½1Aï¿½ï¿½%
		int64_t sp = GetReal(e, POINT_MAX_SP);
		int64_t add_sp = std::min((int64_t)800, sp * Get(e, POINT_MAX_SP_PCT) / 100);
		add_sp += Get(e, POINT_MAX_SP);
		add_sp += Get(e, POINT_PARTY_SKILL_MASTER_BONUS);

		ecs::PlayerRuntime::SetMaxSP(e, sp + add_sp);

		val = GetMaxSP(e);
	}
	break;
	case POINT_MAX_HP_PCT:
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		Change(e, POINT_MAX_HP, 0);
		break;

	case POINT_MAX_SP_PCT:
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		Change(e, POINT_MAX_SP, 0);
		break;

	case POINT_MAX_STAMINA:
		ecs::PlayerRuntime::SetMaxStamina(e, ecs::PlayerRuntime::GetMaxStamina(e) + amount);
		val = ecs::PlayerRuntime::GetMaxStamina(e);
		break;


#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	case POINT_INVEN:
	{
		const int64_t Envantertoplami = static_cast<int64_t>((ch ? ch->Inven_Point() : 0)) + amount;
		if (Envantertoplami > 18)
		{
			LOG_ERROR("[ENVANTER ERROR!]");
			return;
		}
		if (ch) ch->Set_Inventory_Point((ch ? ch->Inven_Point() : 0) + amount);
		val = (ch ? ch->Inven_Point() : 0);
	}
	break;
#endif

	case POINT_GOLD:
	{
		const int64_t nTotalMoney = GetGold(e) + amount;

		if (GOLD_MAX <= nTotalMoney)
		{
			LOG_ERROR("[OVERFLOW_GOLD] OriGold {} AddedGold {} id {} Name {} ", GetGold(e), amount, ecs::PlayerRuntime::GetPlayerID(e), ecs::PlayerRuntime::GetName(e).data());

			LogManager::instance().CharLog(e, GetGold(e) + amount, "OVERFLOW_GOLD", "");
			return;
		}

		//#ifdef TEXTS_IMPROVEMENT
		//				if (amount > 0) {
		//					auto s = std::to_string(amount);
		//					int n = s.length() - 3;
		//					while (n > 0) {
		//						s.insert(n, ".");
		//						n -= 3;
		//					}
		//
		//					ecs::ChatSystem::SendNew(e,
		//#ifdef ENABLE_NEW_CHAT
		//					CHAT_TYPE_INFO_VALUE
		//#else
		//					CHAT_TYPE_INFO
		//#endif
		//					, 3, "%s", s.c_str());
		//				}
		//#endif
		ecs::PlayerRuntime::SetGold(e, GetGold(e) + amount);
		val = GetGold(e);
	}
	break;

#ifdef ENABLE_GAYA_SYSTEM
	case POINT_GAYA:
	{
		const int64_t nTotalGaya = static_cast<int64_t>((ch ? ch->GetGaya() : 0)) + static_cast<int64_t>(amount);

		if (GAYA_MAX <= nTotalGaya)
		{
			LOG_ERROR("[OVERFLOW_GAYA] Gaya max seviyede {} Name {} ", (ch ? ch->GetGaya() : 0), ecs::PlayerRuntime::GetName(e).data());
			return;
		}

		if (nTotalGaya < 0)
		{
			LOG_ERROR("Gaya eksiye dusecekti. PID::[{}]", ecs::PlayerRuntime::GetPlayerID(e));
			return;
		}

		if (ch) ch->SetGaya((ch ? ch->GetGaya() : 0) + amount);
		val = (ch ? ch->GetGaya() : 0);
	}
	break;
#endif


	case POINT_SKILL:
	case POINT_STAT:
	case POINT_SUB_SKILL:
	case POINT_STAT_RESET_COUNT:
	case POINT_HORSE_SKILL:
	{
		int32_t total = Get(e, type) + amount;
#ifdef ENABLE_STATUS_MAX_344_POINTS
		if (type == POINT_STAT)
			total = total > 344 ? 344 : total;
#endif

		Set(e, type, total);
		val = Get(e, type);

		SetReal(e, type, val);
	}
	break;
	case POINT_DEF_GRADE:
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);

		Change(e, POINT_CLIENT_DEF_GRADE, amount);
		break;

	case POINT_CLIENT_DEF_GRADE:
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		break;

	case POINT_ST:
	case POINT_HT:
	case POINT_DX:
	case POINT_IQ:
	case POINT_HP_REGEN:
	case POINT_SP_REGEN:
	case POINT_ATT_SPEED:
	case POINT_ATT_GRADE:
	case POINT_MOV_SPEED:
	case POINT_CASTING_SPEED:
	case POINT_MAGIC_ATT_GRADE:
	case POINT_MAGIC_DEF_GRADE:
	case POINT_BOW_DISTANCE:
	case POINT_HP_RECOVERY:
	case POINT_SP_RECOVERY:

	case POINT_ATTBONUS_HUMAN:	// 42 AÎ°L?!ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½
	case POINT_ATTBONUS_ANIMAL:	// 43 ï¿½?1ï¿½?!ï¿½ï¿½ ï¿½Y1Iï¿½ï¿½ % ï¿½oï¿½!
	case POINT_ATTBONUS_ORC:		// 44 ?oï¿½ï¿½?!ï¿½ï¿½ ï¿½Y1Iï¿½ï¿½ % ï¿½oï¿½!
	case POINT_ATTBONUS_MILGYO:	// 45 1?ï¿½3?!ï¿½ï¿½ ï¿½Y1Iï¿½ï¿½ % ï¿½oï¿½!
	case POINT_ATTBONUS_UNDEAD:	// 46 1AA1?!ï¿½ï¿½ ï¿½Y1Iï¿½ï¿½ % ï¿½oï¿½!
	case POINT_ATTBONUS_DEVIL:	// 47 ï¿½ï¿½ï¿½ï¿½(3Ç¸ï¿½)?!ï¿½ï¿½ ï¿½Y1Iï¿½ï¿½ % ï¿½oï¿½!

	case POINT_ATTBONUS_MONSTER:
	case POINT_ATTBONUS_SURA:
	case POINT_ATTBONUS_ASSASSIN:
	case POINT_ATTBONUS_WARRIOR:
	case POINT_ATTBONUS_SHAMAN:
#ifdef ENABLE_WOLFMAN_CHARACTER
	case POINT_ATTBONUS_WOLFMAN:
#endif

	case POINT_POISON_PCT:
#ifdef ENABLE_WOLFMAN_CHARACTER
	case POINT_BLEEDING_PCT:
#endif
	case POINT_STUN_PCT:
	case POINT_SLOW_PCT:

	case POINT_BLOCK:
	case POINT_DODGE:

	case POINT_CRITICAL_PCT:
	case POINT_PVM_CRITICAL_PCT:
	case POINT_RESIST_CRITICAL:
	case POINT_PENETRATE_PCT:
	case POINT_RESIST_PENETRATE:
	case POINT_CURSE_PCT:

	case POINT_STEAL_HP:		// 48 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ Eï¿½1ï¿½
	case POINT_STEAL_SP:		// 49 ï¿½ï¿½1Aï¿½ï¿½ Eï¿½1ï¿½

	case POINT_MANA_BURN_PCT:	// 50 ï¿½ï¿½3a 1o
	case POINT_DAMAGE_SP_RECOVER:	// 51 ï¿½oï¿½Ý´ï¿½ï¿½O 1A ï¿½ï¿½1Aï¿½ï¿½ Eï¿½o1 Eï¿½ï¿½ï¿½
	case POINT_RESIST_NORMAL_DAMAGE:
	case POINT_RESIST_SWORD:
	case POINT_RESIST_TWOHAND:
	case POINT_RESIST_DAGGER:
	case POINT_RESIST_BELL:
	case POINT_RESIST_FAN:
	case POINT_RESIST_BOW:
#ifdef ENABLE_WOLFMAN_CHARACTER
	case POINT_RESIST_CLAW:
#endif
	case POINT_RESIST_FIRE:
	case POINT_RESIST_ELEC:
	case POINT_RESIST_MAGIC:
#ifdef ENABLE_ACCE_SYSTEM
	case POINT_ACCEDRAIN_RATE:
#endif
#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
	case POINT_RESIST_MAGIC_REDUCTION:
#endif
	case POINT_RESIST_WIND:
	case POINT_RESIST_ICE:
	case POINT_RESIST_EARTH:
	case POINT_RESIST_DARK:
	case POINT_REFLECT_MELEE:	// 67 ï¿½oï¿½ï¿½ 1Ý»ï¿½
	case POINT_REFLECT_CURSE:	// 68 Aï¿½ï¿½ï¿½ 1Ý»ï¿½
	case POINT_POISON_REDUCE:	// 69 ï¿½ï¿½ï¿½Y1Iï¿½ï¿½ ï¿½ï¿½1O
#ifdef ENABLE_WOLFMAN_CHARACTER
	case POINT_BLEEDING_REDUCE:
#endif
	case POINT_KILL_SP_RECOVER:	// 70 Au 1Oï¿½e1A MP Eï¿½o1
	case POINT_KILL_HP_RECOVERY:	// 75
	case POINT_HIT_HP_RECOVERY:
	case POINT_HIT_SP_RECOVERY:
	case POINT_MANASHIELD:
	case POINT_ATT_BONUS:
	case POINT_DEF_BONUS:
	case POINT_SKILL_DAMAGE_BONUS:
	case POINT_NORMAL_HIT_DAMAGE_BONUS:
	case POINT_SKILL_DEFEND_BONUS:
	case POINT_NORMAL_HIT_DEFEND_BONUS:
#ifdef ENABLE_DS_RUNE
	case POINT_RUNE_MONSTERS:
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
	case POINT_DOUBLE_DROP_ITEM:
	case POINT_IRR_WEAPON_DEFENSE:
#endif
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		break;
#ifdef ELEMENT_NEW_BONUSES
	case POINT_ATTBONUS_ELEC:
	case POINT_ATTBONUS_FIRE:
	case POINT_ATTBONUS_ICE:
	case POINT_ATTBONUS_WIND:
	case POINT_ATTBONUS_EARTH:
	case POINT_ATTBONUS_DARK:
#ifdef ENABLE_NEW_BONUS_TALISMAN
	case POINT_ATTBONUS_IRR_SPADA:
	case POINT_ATTBONUS_IRR_SPADONE:
	case POINT_ATTBONUS_IRR_PUGNALE:
	case POINT_ATTBONUS_IRR_FRECCIA:
	case POINT_ATTBONUS_IRR_VENTAGLIO:
	case POINT_ATTBONUS_IRR_CAMPANA:
	case POINT_RESIST_MEZZIUOMINI:
	case POINT_DEF_TALISMAN:
	case POINT_ATTBONUS_INSECT:
	case POINT_ATTBONUS_DESERT:
	case POINT_ATTBONUS_FORT_ZODIAC:
#endif
	case POINT_FISHING_RARE:
#ifdef ENABLE_NEW_USE_POTION
	case POINT_PARTY_DROPEXP:
#endif
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		break;

#endif
#ifdef ENABLE_STRONG_METIN
	case POINT_ATTBONUS_METIN:
#endif
#ifdef ENABLE_STRONG_BOSS
	case POINT_ATTBONUS_BOSS:
#endif
#ifdef ENABLE_RESIST_MONSTER
	case POINT_RESIST_MONSTER:
#endif
#ifdef ENABLE_MEDI_PVM
	case POINT_ATTBONUS_MEDI_PVM:
#endif

	case POINT_PARTY_ATTACKER_BONUS:
	case POINT_PARTY_TANKER_BONUS:
	case POINT_PARTY_BUFFER_BONUS:
	case POINT_PARTY_SKILL_MASTER_BONUS:
	case POINT_PARTY_HASTE_BONUS:
	case POINT_PARTY_DEFENDER_BONUS:

	case POINT_RESIST_WARRIOR:
	case POINT_RESIST_ASSASSIN:
	case POINT_RESIST_SURA:
	case POINT_RESIST_SHAMAN:
#ifdef ENABLE_WOLFMAN_CHARACTER
	case POINT_RESIST_WOLFMAN:
#endif
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		break;
	case POINT_MALL_EXPBONUS:
	case POINT_MALL_ITEMBONUS:
	case POINT_MALL_GOLDBONUS:
	case POINT_MALL_ATTBONUS:
	case POINT_MALL_DEFBONUS:
	case POINT_MELEE_MAGIC_ATT_BONUS_PER:
		if (Get(e, type) + amount > 100)
		{
			if (type != POINT_MALL_EXPBONUS && type != POINT_MALL_ITEMBONUS) {
				LOG_ERROR("MALL_BONUS exceeded over 100!! point type: {} name: {} amount {}", type, ecs::PlayerRuntime::GetName(e).data(), amount);
			}

			amount = 100 - Get(e, type);
		}

		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);

		if (type == POINT_MALL_DEFBONUS)
			if (ch) ch->ComputeBattlePoints();

		break;

		// PC_BANG_ITEM_ADD
	case POINT_PC_BANG_EXP_BONUS:
	case POINT_PC_BANG_DROP_BONUS:
	case POINT_RAMADAN_CANDY_BONUS_EXP:
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	case POINT_EXTRA_INVENTORY1:
	case POINT_EXTRA_INVENTORY2:
	case POINT_EXTRA_INVENTORY3:
	case POINT_EXTRA_INVENTORY4:
	case POINT_EXTRA_INVENTORY5:
	case POINT_EXTRA_INVENTORY6:
#endif
		Set(e, type, amount);
		val = Get(e, type);
		break;
		// END_PC_BANG_ITEM_ADD

	case POINT_EXP_DOUBLE_BONUS:	// 71
	case POINT_GOLD_DOUBLE_BONUS:	// 72
	case POINT_ITEM_DROP_BONUS:	// 73
	case POINT_POTION_BONUS:	// 74
		if (Get(e, type) + amount > 254)
		{
			if (type != POINT_EXP_DOUBLE_BONUS && type != POINT_GOLD_DOUBLE_BONUS) {
				LOG_ERROR("BONUS exceeded over 100!! point type: {} name: {} amount {}", type, ecs::PlayerRuntime::GetName(e).data(), amount);
			}

			amount = 254 - Get(e, type);
		}

		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		break;

	case POINT_IMMUNE_STUN:		// 76
	{
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		uint32_t immuneFlag = ecs::PlayerRuntime::GetImmuneFlag(e);
		if (val)
		{
			SET_BIT(immuneFlag, IMMUNE_STUN);
		}
		else
		{
			REMOVE_BIT(immuneFlag, IMMUNE_STUN);
		}
		ecs::PlayerRuntime::SetImmuneFlag(e, immuneFlag);
		break;
	}

	case POINT_IMMUNE_SLOW:		// 77
	{
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		uint32_t immuneFlag = ecs::PlayerRuntime::GetImmuneFlag(e);
		if (val)
		{
			SET_BIT(immuneFlag, IMMUNE_SLOW);
		}
		else
		{
			REMOVE_BIT(immuneFlag, IMMUNE_SLOW);
		}
		ecs::PlayerRuntime::SetImmuneFlag(e, immuneFlag);
		break;
	}

	case POINT_IMMUNE_FALL:	// 78
	{
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		uint32_t immuneFlag = ecs::PlayerRuntime::GetImmuneFlag(e);
		if (val)
		{
			SET_BIT(immuneFlag, IMMUNE_FALL);
		}
		else
		{
			REMOVE_BIT(immuneFlag, IMMUNE_FALL);
		}
		ecs::PlayerRuntime::SetImmuneFlag(e, immuneFlag);
		break;
	}

	case POINT_ATT_GRADE_BONUS:
		Set(e, type, Get(e, type) + amount);
		Change(e, POINT_ATT_GRADE, amount);
		val = Get(e, type);
		break;

	case POINT_DEF_GRADE_BONUS:
		Set(e, type, Get(e, type) + amount);
		Change(e, POINT_DEF_GRADE, amount);
		val = Get(e, type);
		break;

	case POINT_MAGIC_ATT_GRADE_BONUS:
		Set(e, type, Get(e, type) + amount);
		Change(e, POINT_MAGIC_ATT_GRADE, amount);
		val = Get(e, type);
		break;

	case POINT_MAGIC_DEF_GRADE_BONUS:
		Set(e, type, Get(e, type) + amount);
		Change(e, POINT_MAGIC_DEF_GRADE, amount);
		val = Get(e, type);
		break;

	case POINT_VOICE:
	case POINT_EMPIRE_POINT:
		//"CHARACTER::PointChange: %s: point cannot be changed. use SetPoint instead (type: %d)", ecs::PlayerRuntime::GetName(e).data(), type);
		val = GetReal(e, type);
		break;

	case POINT_POLYMORPH:
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		if (ch) ch->SetPolymorph(val);
		break;

	case POINT_MOUNT:
		Set(e, type, Get(e, type) + amount);
		val = Get(e, type);
		if (ch) ch->MountVnum(val);
		break;

	case POINT_ENERGY:
	case POINT_COSTUME_ATTR_BONUS:
	{
		int old_val = Get(e, type);
		Set(e, type, old_val + amount);
		val = Get(e, type);
		if (ch) ch->BuffOnAttr_ValueChange(type, old_val, val);
	}
	break;

	default:
		LOG_ERROR("CHARACTER::PointChange: {}: unknown point change type {}", ecs::PlayerRuntime::GetName(e).data(), type);
		return;
	}

	switch (type)
	{
	case POINT_LEVEL:
	case POINT_ST:
	case POINT_DX:
	case POINT_IQ:
	case POINT_HT:
		if (ch) ch->ComputeBattlePoints();
		break;
	case POINT_MAX_HP:
	case POINT_MAX_SP:
	case POINT_MAX_STAMINA:
		break;
	}

	if (type == POINT_HP && amount == 0)
		return;

	if (ecs::PlayerRuntime::GetDesc(e))
	{
		struct packet_point_change pack;

		pack.header = HEADER_GC_CHARACTER_POINT_CHANGE;
		pack.dwVID = ecs::PlayerRuntime::GetPacketVID(e);
		pack.type = type;
		pack.value = val;

		if (bAmount)
			pack.amount = amount;
		else
			pack.amount = 0;

		if (!bBroadcast)
			ecs::PlayerRuntime::GetDesc(e)->Packet(&pack, sizeof(struct packet_point_change));
		else
			if (ch) ecs::ViewSystem::PacketView(ch->GetEntityHandle(), &pack, sizeof(pack));
	}
}

} // namespace ecs::PointSystem

void CHARACTER::PointChange(uint8_t type, int64_t amount, bool bAmount, bool bBroadcast
#ifdef __ENABLE_BLOCK_EXP__
	, bool bForceExp
#endif
)
{
	ecs::PointSystem::Change(GetEntityHandle(), type, amount, bAmount, bBroadcast
#ifdef __ENABLE_BLOCK_EXP__
		, bForceExp
#endif
	);
}

#ifdef __NEWPET_SYSTEM__
void CHARACTER::SendPetLevelUpEffect(int vid, int type, int value, int amount) {
	struct packet_point_change pack;

	pack.header = HEADER_GC_CHARACTER_POINT_CHANGE;
	pack.dwVID = vid;
	pack.type = type;
	pack.value = value;
	pack.amount = amount;
	ecs::ViewSystem::PacketView(GetEntityHandle(), &pack, sizeof(pack));
}
#endif

namespace ecs::PointSystem {

int GetPolymorphPoint(entt::entity e, uint8_t type)
{
	if (AffectSystem::IsPolymorphed(e) && !AffectSystem::IsPolyMaintainStat(e))
	{
		uint32_t dwMobVnum = AffectSystem::GetPolymorphVnum(e);
		const CMob* pMob = CMobManager::instance().Get(dwMobVnum);
		int iPower = AffectSystem::GetPolymorphPower(e);

		if (pMob)
		{
			switch (type)
			{
			case POINT_ST:
				if ((ecs::PlayerRuntime::GetJob(e) == JOB_SHAMAN) || ((ecs::PlayerRuntime::GetJob(e) == JOB_SURA) && (SkillSystem::GetSkillGroup(e) == 2)))
					return pMob->m_table.bStr * iPower / 100 + Get(e, POINT_IQ);
				return pMob->m_table.bStr * iPower / 100 + Get(e, POINT_ST);

			case POINT_HT:
				return pMob->m_table.bCon * iPower / 100 + Get(e, POINT_HT);

			case POINT_IQ:
				return pMob->m_table.bInt * iPower / 100 + Get(e, POINT_IQ);

			case POINT_DX:
				return pMob->m_table.bDex * iPower / 100 + Get(e, POINT_DX);
			}
		}
	}

	return Get(e, type);
}

void ComputeBattlePoints(entt::entity e)
{
	if (AffectSystem::IsPolymorphed(e))
	{
		uint32_t dwMobVnum = AffectSystem::GetPolymorphVnum(e);
		const CMob* pMob = CMobManager::instance().Get(dwMobVnum);
		int iAtt = 0;
		int iDef = 0;

		if (pMob)
		{
			iAtt = GetLevel(e) * 2 + GetPolymorphPoint(e, POINT_ST) * 2;
			// lev + con
			iDef = GetLevel(e) + GetPolymorphPoint(e, POINT_HT) + pMob->m_table.wDef;
		}

		Set(e, POINT_ATT_GRADE, iAtt);
		Set(e, POINT_DEF_GRADE, iDef);
		Set(e, POINT_MAGIC_ATT_GRADE, Get(e, POINT_ATT_GRADE));
		Set(e, POINT_MAGIC_DEF_GRADE, Get(e, POINT_DEF_GRADE));
	}
	else if (ecs::PlayerRuntime::GetDesc(e) != nullptr)
	{
		Set(e, POINT_ATT_GRADE, 0);
		Set(e, POINT_DEF_GRADE, 0);
		Set(e, POINT_CLIENT_DEF_GRADE, 0);
		Set(e, POINT_MAGIC_ATT_GRADE, Get(e, POINT_ATT_GRADE));
		Set(e, POINT_MAGIC_DEF_GRADE, Get(e, POINT_DEF_GRADE));

		//
		// ±âo» ATK = 2lev + 2str, Á÷3÷?! ¸¶´U 2strAo 1U2? 1ö AÖA1
		//
		int iAtk = GetLevel(e) * 2;
		int iStatAtk = 0;

		switch (ecs::PlayerRuntime::GetJob(e))
		{
		case JOB_WARRIOR:
		case JOB_SURA:
			iStatAtk = (2 * Get(e, POINT_ST));
			break;

		case JOB_ASSASSIN:
			iStatAtk = (4 * Get(e, POINT_ST) + 2 * Get(e, POINT_DX)) / 3;
			break;

		case JOB_SHAMAN:
			iStatAtk = (4 * Get(e, POINT_ST) + 2 * Get(e, POINT_IQ)) / 3;
			break;
#ifdef ENABLE_WOLFMAN_CHARACTER
		case JOB_WOLFMAN:
			// TODO: 1öAÎÁ· °o°Ý·Â °o1Ä ±âE1AÚ?!°Ô ?äA»
			iStatAtk = (2 * Get(e, POINT_ST));
			break;
#endif
		default:
			LOG_ERROR("invalid job {}", ecs::PlayerRuntime::GetJob(e));
			iStatAtk = (2 * Get(e, POINT_ST));
			break;
		}

		// ¸»A» A¸°í AÖ°í, 1oAEA¸·Î AÎÇN °o°Ý·ÂAI ST*2 o¸´U 3·A¸¸é ST*2·Î ÇN´U.
		// 1oAEA» Aß¸o ÂiAo »ç¶÷ °o°Ý·ÂAI ´o 3·Áö 3E°Ô ÇI±â A§ÇO1­´U.
		if (MountSystem::GetMountVnum(e) && iStatAtk < 2 * Get(e, POINT_ST))
			iStatAtk = (2 * Get(e, POINT_ST));

		iAtk += iStatAtk;

		// 1Â¸¶(¸») : °Ë1ö¶ó µY1IÁö °¨1O
		if (MountSystem::GetMountVnum(e))
		{
			if (ecs::PlayerRuntime::GetJob(e) == JOB_SURA && SkillSystem::GetSkillGroup(e) == 1)
			{
				iAtk += (iAtk * MountSystem::GetHorseLevel(e)) / 60;
			}
			else
			{
				iAtk += (iAtk * MountSystem::GetHorseLevel(e)) / 30;
			}
		}

		//
		// ATK Setting
		//
		iAtk += Get(e, POINT_ATT_GRADE_BONUS);

		Change(e, POINT_ATT_GRADE, iAtk);

		// DEF = LEV + CON + ARMOR
		int iShowDef = GetLevel(e) + Get(e, POINT_HT); // For Ymir(Aµ¸¶)
		int iDef = GetLevel(e) + (int)(Get(e, POINT_HT) / 1.25); // For Other
		int iArmor = 0;

		for (int i = 0; i < WEAR_MAX_NUM; ++i)
		{
			const entt::entity worn = ItemSystem::GetWearItem(e, i);
			if (ItemSystem::GetItemType(worn) == ITEM_ARMOR)
			{
#ifdef ENABLE_PENDANT
				if (ItemSystem::GetItemSubType(worn) == ARMOR_BODY || ItemSystem::GetItemSubType(worn) == ARMOR_HEAD || ItemSystem::GetItemSubType(worn) == ARMOR_FOOTS || ItemSystem::GetItemSubType(worn) == ARMOR_SHIELD || ItemSystem::GetItemSubType(worn) == ARMOR_PENDANT)
#else
				if (ItemSystem::GetItemSubType(worn) == ARMOR_BODY || ItemSystem::GetItemSubType(worn) == ARMOR_HEAD || ItemSystem::GetItemSubType(worn) == ARMOR_FOOTS || ItemSystem::GetItemSubType(worn) == ARMOR_SHIELD)
#endif
				{
					iArmor += ItemSystem::GetItemValue(worn, 1);
					iArmor += (2 * ItemSystem::GetItemValue(worn, 5));
				}
			}
		}

		// ¸» A¸°í AÖA» ¶§ 1a3î·ÂAI ¸»AÇ ±âÁO 1a3î·Âo¸´U 3·A¸¸é ±âÁO 1a3î·ÂA¸·Î 13Á¤
		if (true == MountSystem::IsHorseRiding(e))
		{
			if (iArmor < MountSystem::GetHorseArmor(e))
				iArmor = MountSystem::GetHorseArmor(e);

			const char* pHorseName = CHorseNameManager::instance().GetHorseName(ecs::PlayerRuntime::GetPlayerID(e));

			if (pHorseName != nullptr && strlen(pHorseName))
			{
				iArmor += 20;
			}
		}

		iArmor += Get(e, POINT_DEF_GRADE_BONUS);
		iArmor += Get(e, POINT_PARTY_DEFENDER_BONUS);

		int iServerDef = iDef + iArmor;
		int iClientDef = iShowDef + iArmor;

		if (Get(e, POINT_MALL_DEFBONUS) > 0)
		{
			const int iPct = Get(e, POINT_MALL_DEFBONUS);
			iServerDef += (iServerDef * iPct) / 100;
			iClientDef += (iClientDef * iPct) / 100;
		}

		Change(e, POINT_DEF_GRADE, iServerDef - Get(e, POINT_DEF_GRADE));
		Change(e, POINT_CLIENT_DEF_GRADE, iClientDef - Get(e, POINT_CLIENT_DEF_GRADE));

		Change(e, POINT_MAGIC_ATT_GRADE, GetLevel(e) * 2 + Get(e, POINT_IQ) * 2 + Get(e, POINT_MAGIC_ATT_GRADE_BONUS));
		Change(e, POINT_MAGIC_DEF_GRADE, GetLevel(e) + (Get(e, POINT_IQ) * 3 + Get(e, POINT_HT)) / 3 + iArmor / 2 + Get(e, POINT_MAGIC_DEF_GRADE_BONUS));
	}
	else
	{
		// 2lev + str * 2
		int iAtt = GetLevel(e) * 2 + Get(e, POINT_ST) * 2;
		// lev + con
		const TMobTable* pTable = ecs::PlayerRuntime::GetMobTable(e);
		int iDef = GetLevel(e) + Get(e, POINT_HT) + (pTable ? pTable->wDef : 0);

		Set(e, POINT_ATT_GRADE, iAtt);
		Set(e, POINT_DEF_GRADE, iDef);
		Set(e, POINT_MAGIC_ATT_GRADE, Get(e, POINT_ATT_GRADE));
		Set(e, POINT_MAGIC_DEF_GRADE, Get(e, POINT_DEF_GRADE));
	}
}


void ApplyPoint(entt::entity e, uint8_t bApplyType, int iVal)
{
	// The skill damage bonus map is a component; the legacy m_SkillDamageBonus
	// was the same table on CHARACTER.
	auto& bonus = g_registry.get_or_emplace<ecs::SkillDamageBonus>(e);
	switch (bApplyType)
	{
	case APPLY_NONE:			// 0
		break;;

	case APPLY_CON:
		Change(e, POINT_HT, iVal);
		Change(e, POINT_MAX_HP, (iVal * JobInitialPoints[ecs::PlayerRuntime::GetJob(e)].hp_per_ht));
		Change(e, POINT_MAX_STAMINA, (iVal * JobInitialPoints[ecs::PlayerRuntime::GetJob(e)].stamina_per_con));
		break;

	case APPLY_INT:
		Change(e, POINT_IQ, iVal);
		Change(e, POINT_MAX_SP, (iVal * JobInitialPoints[ecs::PlayerRuntime::GetJob(e)].sp_per_iq));
		break;

	case APPLY_SKILL:
		// SKILL_DAMAGE_BONUS
	{
		// AÖ»ï¿½Aï¿½ onAï¿½ ï¿½ï¿½ï¿½OAï¿½ï¿½ï¿½ 8onAï¿½ vnum, 9onAï¿½ add, 15onAï¿½ change
		// 00000000 00000000 00000000 00000000
		// ^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^
		// vnum     ^ add       change
		uint8_t bSkillVnum = (uint8_t)(((uint32_t)iVal) >> 24);
		int iAdd = iVal & 0x00800000;
		int iChange = iVal & 0x007fffff;

		LOG_TRACE("APPLY_SKILL skill {} add? {} change {}", static_cast<int>(bSkillVnum), iAdd ? 1 : 0, iChange);

		if (0 == iAdd)
			iChange = -iChange;

		const auto iter = bonus.bySkill.find(bSkillVnum);

		if (iter == bonus.bySkill.end())
			bonus.bySkill.insert(std::make_pair(bSkillVnum, iChange));
		else
			iter->second += iChange;
	}
	// END_OF_SKILL_DAMAGE_BONUS
	break;

	// NOTE: 3AAIAU?! Aï¿½ï¿½N AÖ´ï¿½HP oï¿½3E1o3a ï¿½u1oAï¿½ oï¿½ï¿½ï¿½ oï¿½3E1oï¿½! ï¿½Eï¿½ï¿½Ao 1a1ï¿½Aï¿½ ï¿½ï¿½?ï¿½ï¿½I1Ç·ï¿½
	// ï¿½ï¿½3ï¿½ MAX_HPï¿½ï¿½ ï¿½eï¿½eï¿½Iï¿½ï¿½ ï¿½u1oAï¿½ oï¿½ï¿½ï¿½Aï¿½ ï¿½a?i 1ï¿½ï¿½ï¿½ï¿½! ï¿½ï¿½ï¿½e. ï¿½ï¿½1ï¿½ ?oï¿½! AIï¿½EAI ï¿½Oï¿½ï¿½AuAIï¿½âµµ ï¿½Iï¿½ï¿½..
	// 1U2U ï¿½o1ï¿½Ao ï¿½ï¿½Aï¿½ AÖ´ï¿½ hp?ï¿½ oï¿½Aï¿½ hpAï¿½ onA2Aï¿½ ï¿½ï¿½ï¿½N ï¿½ï¿½ 1U2? AÖ´ï¿½ hpï¿½ï¿½ ï¿½ï¿½ï¿½OAï¿½ï¿½ï¿½ hpï¿½ï¿½ oï¿½ï¿½ï¿½ï¿½Nï¿½U.
	// ?oï¿½! PointChange?!1ï¿½ ï¿½Iï¿½Â°ï¿½ ï¿½ï¿½Aï¿½ï¿½ï¿½ ï¿½ï¿½Aoï¿½Y 13ï¿½e 1ï¿½ï¿½ï¿½ï¿½ï¿½ 3ï¿½ï¿½?ï¿½1ï¿½ skip..
	// SPï¿½ï¿½ ï¿½Eï¿½ï¿½AI ï¿½eï¿½eï¿½Nï¿½U.
	// Mantis : 101460			~ ity ~
	case APPLY_MAX_HP:
	case APPLY_MAX_HP_PCT:
	{
		int i = GetMaxHP(e);
		if (i == 0) {
			break;
		}

		Change(e, aApplyInfo[bApplyType].bPointType, iVal);
		float fRatio = (float)GetMaxHP(e) / (float)i;
		Change(e, POINT_HP, Get(e, POINT_HP) * fRatio - Get(e, POINT_HP));
	}
	break;
	case APPLY_MAX_SP:
	case APPLY_MAX_SP_PCT:
	{
		int i = GetMaxSP(e);
		if (i == 0) {
			break;
		}

		Change(e, aApplyInfo[bApplyType].bPointType, iVal);
		float fRatio = (float)GetMaxSP(e) / (float)i;
		Change(e, POINT_SP, Get(e, POINT_SP) * fRatio - Get(e, POINT_SP));
	}
	break;
	case APPLY_STR:
	case APPLY_DEX:
	case APPLY_ATT_SPEED:
	case APPLY_MOV_SPEED:
	case APPLY_CAST_SPEED:
	case APPLY_HP_REGEN:
	case APPLY_SP_REGEN:
	case APPLY_POISON_PCT:
#ifdef ENABLE_WOLFMAN_CHARACTER
	case APPLY_BLEEDING_PCT:
#endif
	case APPLY_STUN_PCT:
	case APPLY_SLOW_PCT:
	case APPLY_CRITICAL_PCT:
	case APPLY_PENETRATE_PCT:
	case APPLY_ATTBONUS_HUMAN:
	case APPLY_ATTBONUS_ANIMAL:
	case APPLY_ATTBONUS_ORC:
	case APPLY_ATTBONUS_MILGYO:
	case APPLY_ATTBONUS_UNDEAD:
	case APPLY_ATTBONUS_DEVIL:
	case APPLY_ATTBONUS_WARRIOR:	// 59
	case APPLY_ATTBONUS_ASSASSIN:	// 60
	case APPLY_ATTBONUS_SURA:	// 61
	case APPLY_ATTBONUS_SHAMAN:	// 62
#ifdef ENABLE_WOLFMAN_CHARACTER
	case APPLY_ATTBONUS_WOLFMAN:
#endif
	case APPLY_ATTBONUS_MONSTER:	// 63
	case APPLY_STEAL_HP:
	case APPLY_STEAL_SP:
	case APPLY_MANA_BURN_PCT:
	case APPLY_DAMAGE_SP_RECOVER:
	case APPLY_BLOCK:
	case APPLY_DODGE:
	case APPLY_RESIST_SWORD:
	case APPLY_RESIST_TWOHAND:
	case APPLY_RESIST_DAGGER:
	case APPLY_RESIST_BELL:
	case APPLY_RESIST_FAN:
	case APPLY_RESIST_BOW:
#ifdef ENABLE_WOLFMAN_CHARACTER
	case APPLY_RESIST_CLAW:
#endif
	case APPLY_RESIST_FIRE:
	case APPLY_RESIST_ELEC:
	case APPLY_RESIST_MAGIC:
	case APPLY_RESIST_WIND:
	case APPLY_RESIST_ICE:
	case APPLY_RESIST_EARTH:
	case APPLY_RESIST_DARK:
	case APPLY_REFLECT_MELEE:
	case APPLY_REFLECT_CURSE:
	case APPLY_ANTI_CRITICAL_PCT:
	case APPLY_ANTI_PENETRATE_PCT:
	case APPLY_POISON_REDUCE:
#ifdef ENABLE_WOLFMAN_CHARACTER
	case APPLY_BLEEDING_REDUCE:
#endif
	case APPLY_KILL_SP_RECOVER:
	case APPLY_EXP_DOUBLE_BONUS:
	case APPLY_GOLD_DOUBLE_BONUS:
	case APPLY_ITEM_DROP_BONUS:
	case APPLY_POTION_BONUS:
	case APPLY_KILL_HP_RECOVER:
	case APPLY_IMMUNE_STUN:
	case APPLY_IMMUNE_SLOW:
	case APPLY_IMMUNE_FALL:
	case APPLY_BOW_DISTANCE:
	case APPLY_ATT_GRADE_BONUS:
	case APPLY_DEF_GRADE_BONUS:
	case APPLY_MAGIC_ATT_GRADE:
	case APPLY_MAGIC_DEF_GRADE:
	case APPLY_CURSE_PCT:
	case APPLY_MAX_STAMINA:
	case APPLY_MALL_ATTBONUS:
	case APPLY_MALL_DEFBONUS:
	case APPLY_MALL_EXPBONUS:
	case APPLY_MALL_ITEMBONUS:
	case APPLY_MALL_GOLDBONUS:
	case APPLY_SKILL_DAMAGE_BONUS:
	case APPLY_NORMAL_HIT_DAMAGE_BONUS:

		// DEPEND_BONUS_ATTRIBUTES
	case APPLY_SKILL_DEFEND_BONUS:
	case APPLY_NORMAL_HIT_DEFEND_BONUS:
		// END_OF_DEPEND_BONUS_ATTRIBUTES

	case APPLY_PC_BANG_EXP_BONUS:
	case APPLY_PC_BANG_DROP_BONUS:

	case APPLY_RESIST_WARRIOR:
	case APPLY_RESIST_ASSASSIN:
	case APPLY_RESIST_SURA:
	case APPLY_RESIST_SHAMAN:
#ifdef ENABLE_WOLFMAN_CHARACTER
	case APPLY_RESIST_WOLFMAN:
#endif
	case APPLY_ENERGY:					// 82 ï¿½ï¿½ï¿½
	case APPLY_DEF_GRADE:				// 83 1a3ï¿½ï¿½. DEF_GRADE_BONUSï¿½ï¿½ Aï¿½ï¿½ï¿½?!1ï¿½ ï¿½ï¿½1eï¿½ï¿½ oï¿½?ï¿½ï¿½ï¿½ï¿½ï¿½ AÇµï¿½ï¿½E 1ï¿½ï¿½ï¿½(...)ï¿½! AÖ´U.
	case APPLY_COSTUME_ATTR_BONUS:		// 84 ï¿½ï¿½1oAï¿½ 3AAIAU?! oUAo 1ï¿½1oï¿½! oï¿½3E1o
	case APPLY_MAGIC_ATTBONUS_PER:		// 85 ï¿½ï¿½1ï¿½ ï¿½oï¿½Ý·ï¿½ +x%
	case APPLY_MELEE_MAGIC_ATTBONUS_PER:			// 86 ï¿½ï¿½1ï¿½ + 1?ï¿½ï¿½ ï¿½oï¿½Ý·ï¿½ +x%
#ifdef ENABLE_ACCE_SYSTEM
	case APPLY_ACCEDRAIN_RATE:			//97
#endif
#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
	case APPLY_RESIST_MAGIC_REDUCTION:	//98
#endif
#ifdef ELEMENT_NEW_BONUSES
	case APPLY_ATTBONUS_ELEC:			//99
	case APPLY_ATTBONUS_FIRE:			//100
	case APPLY_ATTBONUS_ICE:			//101
	case APPLY_ATTBONUS_WIND:			//102
	case APPLY_ATTBONUS_EARTH:			//103
	case APPLY_ATTBONUS_DARK:			//104
#ifdef ENABLE_NEW_BONUS_TALISMAN
	case APPLY_RESIST_MEZZIUOMINI:
	case APPLY_DEF_TALISMAN:
	case APPLY_ATTBONUS_INSECT:
	case APPLY_ATTBONUS_DESERT:
	case APPLY_ATTBONUS_FORT_ZODIAC:
#endif		
#endif
#ifdef ENABLE_STRONG_METIN
	case APPLY_ATTBONUS_METIN:
#endif
#ifdef ENABLE_STRONG_BOSS
	case APPLY_ATTBONUS_BOSS:
#endif
#ifdef ENABLE_RESIST_MONSTER
	case APPLY_RESIST_MONSTER:
#endif
#ifdef ENABLE_MEDI_PVM
	case APPLY_ATTBONUS_MEDI_PVM:
#endif
	case APPLY_PVM_CRITICAL_PCT:
#ifdef ENABLE_DS_RUNE
	case APPLY_RUNE_MONSTERS:
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
	case APPLY_DOUBLE_DROP_ITEM:
	case APPLY_IRR_WEAPON_DEFENSE:
#endif
	case APPLY_FISHING_RARE:
#ifdef ENABLE_NEW_USE_POTION
	case APPLY_PARTY_DROPEXP:
#endif
		Change(e, aApplyInfo[bApplyType].bPointType, iVal);
		break;

	default:
		LOG_ERROR("Unknown apply type {} name {}", bApplyType, ecs::PlayerRuntime::GetName(e).data());
		break;

	case APPLY_ATTBONUS_IRR_SPADA:
	case APPLY_ATTBONUS_IRR_SPADONE:
	case APPLY_ATTBONUS_IRR_PUGNALE:
	case APPLY_ATTBONUS_IRR_FRECCIA:
	case APPLY_ATTBONUS_IRR_VENTAGLIO:
	case APPLY_ATTBONUS_IRR_CAMPANA:
	{
		int v = iVal / 5; // 5 -> 1, 10 -> 2, ...
		Change(e, aApplyInfo[bApplyType].bPointType, v);
		break;
	}
	}
}

} // namespace ecs::PointSystem

void CHARACTER::ApplyPoint(uint8_t bApplyType, int iVal)
{
	ecs::PointSystem::ApplyPoint(GetEntityHandle(), bApplyType, iVal);
}










