#include "../../stdafx.h"

#include "PointSystem.hpp"
#include "PointRouter.hpp"

#include <array>
#include <common/VnumHelper.h>

#include "../CharacterAccessors.hpp"
#include "../components/character_stats_components.hpp"
#include "../components/inventory_components.hpp"
#include "../components/vital_components.hpp"
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

void SyncInstantPointMirror(LPCHARACTER ch, uint8_t type, int64_t val)
{
	if (!ch || type >= POINT_MAX_NUM || !ecs::IsStatArrayPoint(type))
		return;

	const entt::entity e = ch->GetEntityHandle();
	if (!IsReadableEntity(e))
		return;

	auto& stats = g_registry.get_or_emplace<ecs::CharacterStatsComponent>(e);
	stats.points[type] = val;

	auto& points = g_registry.get_or_emplace<ecs::CharacterPoints>(e);
	points.instant.points[type] = val;
}

void SyncRealPointMirror(LPCHARACTER ch, uint8_t type, int64_t val)
{
	if (!ch || type >= POINT_MAX_NUM)
		return;

	const entt::entity e = ch->GetEntityHandle();
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

void Change(entt::entity e, uint8_t type, int64_t amount, bool bAmount, bool bBroadcast
#ifdef __ENABLE_BLOCK_EXP__
    , bool bForceExp
#endif
)
{
    auto* ch = ecs::LegacyCharOf(e);
    if (!ch)
        return;

    ch->PointChange(type, amount, bAmount, bBroadcast
#ifdef __ENABLE_BLOCK_EXP__
        , bForceExp
#endif
    );
}

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

void CHARACTER::SetRealPoint(uint8_t type, int64_t val)
{
	m_points.points[type] = val;
	ecs::PointSystem::SyncRealPointMirror(this, type, val);
#ifdef ENABLE_RANKING
	if (type == POINT_PLAYTIME)
		SetRankPoints(15, val);
#endif
}

int CHARACTER::GetPolymorphPoint(uint8_t type) const
{
	if (IsPolymorphed() && !IsPolyMaintainStat())
	{
		uint32_t dwMobVnum = GetPolymorphVnum();
		const CMob* pMob = CMobManager::instance().Get(dwMobVnum);
		int iPower = GetPolymorphPower();

		if (pMob)
		{
			switch (type)
			{
			case POINT_ST:
				if ((GetJob() == JOB_SHAMAN) || ((GetJob() == JOB_SURA) && (GetSkillGroup() == 2)))
					return pMob->m_table.bStr * iPower / 100 + GetPoint(POINT_IQ);
				return pMob->m_table.bStr * iPower / 100 + GetPoint(POINT_ST);

			case POINT_HT:
				return pMob->m_table.bCon * iPower / 100 + GetPoint(POINT_HT);

			case POINT_IQ:
				return pMob->m_table.bInt * iPower / 100 + GetPoint(POINT_IQ);

			case POINT_DX:
				return pMob->m_table.bDex * iPower / 100 + GetPoint(POINT_DX);
			}
		}
	}

	return GetPoint(type);
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
	ecs::PointSystem::SyncInstantPointMirror(this, type, val);


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


void CHARACTER::PointChange(uint8_t type, int64_t amount, bool bAmount, bool bBroadcast
#ifdef __ENABLE_BLOCK_EXP__
	, bool bForceExp
#endif
)
{
	int64_t val = 0;


	//LOG_TRACE("PointChange {} {} | {} -> {} cHP {} mHP {}", type, amount, GetPoint(type), GetPoint(type)+amount, GetHP(), GetMaxHP());

	switch (type)
	{
	case POINT_NONE:

#ifdef ENABLE_BATTLE_PASS
	case POINT_BATTLE_PASS_ID:
#endif		

		return;

	case POINT_LEVEL:
		if ((GetLevel() + amount) > gPlayerMaxLevel)
			return;

		SetLevel(GetLevel() + amount);
		val = GetLevel();

		LOG_INFO("LEVELUP: {} {} NEXT EXP {}", GetName(), GetLevel(), GetNextExp());
#ifdef ENABLE_WOLFMAN_CHARACTER
		if (GetJob() == JOB_WOLFMAN)
		{
			if ((5 <= val) && (GetSkillGroup() != 1))
			{
				ClearSkill();
				// set skill group
				SetSkillGroup(1);
				// set skill points
				SetRealPoint(POINT_SKILL, GetLevel() - 1);
				SetPoint(POINT_SKILL, GetRealPoint(POINT_SKILL));
				PointChange(POINT_SKILL, 0);
				// update points (not required)
				// ComputePoints();
				// PointsPacket();
			}
		}
#endif
		PointChange(POINT_NEXT_EXP, GetNextExp(), false);
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
			BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 546, "%s#%d", GetName(), val);
			break;
		default:
			break;
		}
#endif
#endif
		if (amount)
		{
			quest::CQuestManager::instance().LevelUp(GetPlayerID());

			LogManager::instance().LevelLog(this, val, GetRealPoint(POINT_PLAYTIME) + (get_dword_time() - m_dwPlayStartTime) / 60000);

			if (GetGuild())
			{
				GetGuild()->LevelChange(GetPlayerID(), GetLevel());
			}

			if (GetParty())
			{
				GetParty()->RequestSetMemberLevel(GetPlayerID(), GetLevel());
			}
		}
		break;

	case POINT_NEXT_EXP:
		val = GetNextExp();
		bAmount = false;	// 1����� bAmount�� false ?�3� �N�U.
		break;

	case POINT_EXP:
	{
		uint32_t exp = GetExp();
		uint32_t next_exp = GetNextExp();

		// exp�! 0 AI�I�� �!�� 3E���I �N�U
		if ((amount < 0) && (exp < (uint32_t)(-amount)))
		{
			LOG_TRACE("{} AMOUNT < 0 {}, CUR EXP: {}", GetName(), -amount, exp);
			amount = exp; // -exp

			SetExp(exp + amount);
			val = GetExp();
		}
		else
		{
			if (gPlayerMaxLevel <= GetLevel())
				return;

#ifdef __ENABLE_BLOCK_EXP__
			if (Block_Exp && !bForceExp)
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
			//						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), 
			//#ifdef ENABLE_NEW_CHAT
			//						CHAT_TYPE_INFO_EXP
			//#else
			//						CHAT_TYPE_INFO
			//#endif
			//						, 2, "%s", s.c_str());
			//					}
			//#endif
			uint32_t iExpBalance = 0;

			// �1o� 3�!
			if (exp + amount >= next_exp)
			{
				iExpBalance = (exp + amount) - next_exp;
				amount = next_exp - exp;

				SetExp(0);
				exp = next_exp;
			}
			else
			{
				SetExp(exp + amount);
				exp = GetExp();
			}

			uint32_t q = uint32_t(next_exp / 4.0f);
			int iLevStep = GetRealPoint(POINT_LEVEL_STEP);

			// iLevStepAI 4 AI��AI�� �1o�AI ?A��3�3� �I1Ƿ� ?���?! ?A 1� 3o�� �aAI�U.
			if (iLevStep >= 4)
			{
				LOG_ERROR("{} LEVEL_STEP bigger than 4! ({})", GetName(), iLevStep);
				iLevStep = 4;
			}

			if (exp >= next_exp && iLevStep < 4)
			{
				for (int i = 0; i < 4 - iLevStep; ++i)
					PointChange(POINT_LEVEL_STEP, 1, false, true);
			}
			else if (exp >= q * 3 && iLevStep < 3)
			{
				for (int i = 0; i < 3 - iLevStep; ++i)
					PointChange(POINT_LEVEL_STEP, 1, false, true);
			}
			else if (exp >= q * 2 && iLevStep < 2)
			{
				for (int i = 0; i < 2 - iLevStep; ++i)
					PointChange(POINT_LEVEL_STEP, 1, false, true);
			}
			else if (exp >= q && iLevStep < 1)
				PointChange(POINT_LEVEL_STEP, 1);

			if (iExpBalance)
			{
				PointChange(POINT_EXP, iExpBalance);
			}

			val = GetExp();
		}
	}
	break;

	case POINT_LEVEL_STEP:
		if (amount > 0)
		{
			val = GetPoint(POINT_LEVEL_STEP) + amount;

			switch (val)
			{
			case 1:
			case 2:
			case 3:
			{
				int iLvl = GetLevel();
#ifdef ENABLE_STATUS_MAX_344_POINTS
				if (iLvl > 115)
					break;
				else if ((iLvl == 115) && (val == 3))
					break;

				PointChange(POINT_STAT, 1);
#else
				if ((iLvl <= g_iStatusPointGetLevelLimit) && (iLvl <= gPlayerMaxLevel))
					PointChange(POINT_STAT, 1);
#endif
			}
			break;

			case 4:
			{
				int iHP = number(JobInitialPoints[GetJob()].hp_per_lv_begin, JobInitialPoints[GetJob()].hp_per_lv_end);
				int iSP = number(JobInitialPoints[GetJob()].sp_per_lv_begin, JobInitialPoints[GetJob()].sp_per_lv_end);

				m_points.iRandomHP += iHP;
				m_points.iRandomSP += iSP;

				if (GetSkillGroup())
				{
					if (GetLevel() >= 5)
						PointChange(POINT_SKILL, 1);

					if (GetLevel() >= 9)
						PointChange(POINT_SUB_SKILL, 1);
				}

				PointChange(POINT_MAX_HP, iHP);
				PointChange(POINT_MAX_SP, iSP);
				PointChange(POINT_LEVEL, 1, false, true);

				val = 0;
			}
			break;
			}

			PointChange(POINT_HP, GetMaxHP() - GetHP());
			PointChange(POINT_SP, GetMaxSP() - GetSP());
			PointChange(POINT_STAMINA, GetMaxStamina() - GetStamina());

			SetPoint(POINT_LEVEL_STEP, val);
			SetRealPoint(POINT_LEVEL_STEP, val);

			Save();
		}
		else
			val = GetPoint(POINT_LEVEL_STEP);

		break;

	case POINT_HP:
	{
		if (IsDead() || IsStun())
			return;

		int64_t prev_hp = GetHP();

		amount = std::min(GetMaxHP() - GetHP(), amount);
		SetHP(GetHP() + amount);
		val = GetHP();

		BroadcastTargetPacket();

		if (GetParty() && IsPC() && val != prev_hp)
			GetParty()->SendPartyInfoOneToAll(this);
	}
	break;

	case POINT_SP:
	{
		if (IsDead() || IsStun())
			return;

		amount = std::min(GetMaxSP() - GetSP(), amount);
		SetSP(GetSP() + amount);
		val = GetSP();
	}
	break;

	case POINT_STAMINA:
	{
		if (IsDead() || IsStun())
			return;

		int prev_val = GetStamina();
		amount = std::min(GetMaxStamina() - GetStamina(), amount);
		SetStamina(GetStamina() + amount);
		val = GetStamina();

		if (val == 0)
		{
			// Stamina�! 3oA��I �EA�!
			SetNowWalking(true);
		}
		else if (prev_val == 0)
		{
			// 3o�o 1oA�1I3a�! ���aA��I AIA� �?�a o1��
			ResetWalking();
		}

		if (amount < 0 && val != 0) // ��1O�� o�3���3E�´U.
			return;
	}
	break;

	case POINT_MAX_HP:
	{
		SetPoint(type, GetPoint(type) + amount);

		const int64_t base = GetRealPoint(POINT_MAX_HP);              // 20-30k
		const int64_t flat = GetPoint(POINT_MAX_HP);                  // �kszerek stb. fix +HP (ett�l lesz 350k)
		const int64_t party = GetPoint(POINT_PARTY_TANKER_BONUS);
		const int64_t pct = GetPoint(POINT_MAX_HP_PCT);              // +20

		const int64_t totalNoPct = base + flat + party;                // pl 350k
		int64_t newMax = totalNoPct + (totalNoPct * pct) / 100;        // 350k + 20% = 420k

		if (newMax < 1)
			newMax = 1;

		SetMaxHP(newMax);
		val = GetMaxHP();
	}
	break;

	case POINT_MAX_SP:
	{
		SetPoint(type, GetPoint(type) + amount);

		//SetMaxSP(GetMaxSP() + amount);
		// Aִ� ��1A�� = (��o� Aִ� ��1A�� + A߰!) * Aִ���1A��%
		int64_t sp = GetRealPoint(POINT_MAX_SP);
		int64_t add_sp = std::min((int64_t)800, sp * GetPoint(POINT_MAX_SP_PCT) / 100);
		add_sp += GetPoint(POINT_MAX_SP);
		add_sp += GetPoint(POINT_PARTY_SKILL_MASTER_BONUS);

		SetMaxSP(sp + add_sp);

		val = GetMaxSP();
	}
	break;
	case POINT_MAX_HP_PCT:
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
		PointChange(POINT_MAX_HP, 0);
		break;

	case POINT_MAX_SP_PCT:
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
		PointChange(POINT_MAX_SP, 0);
		break;

	case POINT_MAX_STAMINA:
		SetMaxStamina(GetMaxStamina() + amount);
		val = GetMaxStamina();
		break;


#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	case POINT_INVEN:
	{
		const int64_t Envantertoplami = static_cast<int64_t>(Inven_Point()) + amount;
		if (Envantertoplami > 18)
		{
			LOG_ERROR("[ENVANTER ERROR!]");
			return;
		}
		Set_Inventory_Point(Inven_Point() + amount);
		val = Inven_Point();
	}
	break;
#endif

	case POINT_GOLD:
	{
		const int64_t nTotalMoney = GetGold() + amount;

		if (GOLD_MAX <= nTotalMoney)
		{
			LOG_ERROR("[OVERFLOW_GOLD] OriGold {} AddedGold {} id {} Name {} ", GetGold(), amount, GetPlayerID(), GetName());

			LogManager::instance().CharLog(this, GetGold() + amount, "OVERFLOW_GOLD", "");
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
		//					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), 
		//#ifdef ENABLE_NEW_CHAT
		//					CHAT_TYPE_INFO_VALUE
		//#else
		//					CHAT_TYPE_INFO
		//#endif
		//					, 3, "%s", s.c_str());
		//				}
		//#endif
		SetGold(GetGold() + amount);
		val = GetGold();
	}
	break;

#ifdef ENABLE_GAYA_SYSTEM
	case POINT_GAYA:
	{
		const int64_t nTotalGaya = static_cast<int64_t>(GetGaya()) + static_cast<int64_t>(amount);

		if (GAYA_MAX <= nTotalGaya)
		{
			LOG_ERROR("[OVERFLOW_GAYA] Gaya max seviyede {} Name {} ", GetGaya(), GetName());
			return;
		}

		if (nTotalGaya < 0)
		{
			LOG_ERROR("Gaya eksiye dusecekti. PID::[{}]", GetPlayerID());
			return;
		}

		SetGaya(GetGaya() + amount);
		val = GetGaya();
	}
	break;
#endif


	case POINT_SKILL:
	case POINT_STAT:
	case POINT_SUB_SKILL:
	case POINT_STAT_RESET_COUNT:
	case POINT_HORSE_SKILL:
	{
		int32_t total = GetPoint(type) + amount;
#ifdef ENABLE_STATUS_MAX_344_POINTS
		if (type == POINT_STAT)
			total = total > 344 ? 344 : total;
#endif

		SetPoint(type, total);
		val = GetPoint(type);

		SetRealPoint(type, val);
	}
	break;
	case POINT_DEF_GRADE:
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);

		PointChange(POINT_CLIENT_DEF_GRADE, amount);
		break;

	case POINT_CLIENT_DEF_GRADE:
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
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

	case POINT_ATTBONUS_HUMAN:	// 42 AΰL?!�� ����
	case POINT_ATTBONUS_ANIMAL:	// 43 �?1�?!�� �Y1I�� % �o�!
	case POINT_ATTBONUS_ORC:		// 44 ?o��?!�� �Y1I�� % �o�!
	case POINT_ATTBONUS_MILGYO:	// 45 1?�3?!�� �Y1I�� % �o�!
	case POINT_ATTBONUS_UNDEAD:	// 46 1AA1?!�� �Y1I�� % �o�!
	case POINT_ATTBONUS_DEVIL:	// 47 ����(3Ǹ�)?!�� �Y1I�� % �o�!

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

	case POINT_STEAL_HP:		// 48 ������ E�1�
	case POINT_STEAL_SP:		// 49 ��1A�� E�1�

	case POINT_MANA_BURN_PCT:	// 50 ��3a 1o
	case POINT_DAMAGE_SP_RECOVER:	// 51 �o�ݴ��O 1A ��1A�� E�o1 E���
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
	case POINT_REFLECT_MELEE:	// 67 �o�� 1ݻ�
	case POINT_REFLECT_CURSE:	// 68 A��� 1ݻ�
	case POINT_POISON_REDUCE:	// 69 ���Y1I�� ��1O
#ifdef ENABLE_WOLFMAN_CHARACTER
	case POINT_BLEEDING_REDUCE:
#endif
	case POINT_KILL_SP_RECOVER:	// 70 Au 1O�e1A MP E�o1
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
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
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
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
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
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
		break;
	case POINT_MALL_EXPBONUS:
	case POINT_MALL_ITEMBONUS:
	case POINT_MALL_GOLDBONUS:
	case POINT_MALL_ATTBONUS:
	case POINT_MALL_DEFBONUS:
	case POINT_MELEE_MAGIC_ATT_BONUS_PER:
		if (GetPoint(type) + amount > 100)
		{
			if (type != POINT_MALL_EXPBONUS && type != POINT_MALL_ITEMBONUS) {
				LOG_ERROR("MALL_BONUS exceeded over 100!! point type: {} name: {} amount {}", type, GetName(), amount);
			}

			amount = 100 - GetPoint(type);
		}

		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);

		if (type == POINT_MALL_DEFBONUS)
			ComputeBattlePoints();

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
		SetPoint(type, amount);
		val = GetPoint(type);
		break;
		// END_PC_BANG_ITEM_ADD

	case POINT_EXP_DOUBLE_BONUS:	// 71
	case POINT_GOLD_DOUBLE_BONUS:	// 72
	case POINT_ITEM_DROP_BONUS:	// 73
	case POINT_POTION_BONUS:	// 74
		if (GetPoint(type) + amount > 254)
		{
			if (type != POINT_EXP_DOUBLE_BONUS && type != POINT_GOLD_DOUBLE_BONUS) {
				LOG_ERROR("BONUS exceeded over 100!! point type: {} name: {} amount {}", type, GetName(), amount);
			}

			amount = 254 - GetPoint(type);
		}

		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
		break;

	case POINT_IMMUNE_STUN:		// 76
	{
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
		uint32_t immuneFlag = GetImmuneFlag();
		if (val)
		{
			SET_BIT(immuneFlag, IMMUNE_STUN);
		}
		else
		{
			REMOVE_BIT(immuneFlag, IMMUNE_STUN);
		}
		SetImmuneFlag(immuneFlag);
		break;
	}

	case POINT_IMMUNE_SLOW:		// 77
	{
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
		uint32_t immuneFlag = GetImmuneFlag();
		if (val)
		{
			SET_BIT(immuneFlag, IMMUNE_SLOW);
		}
		else
		{
			REMOVE_BIT(immuneFlag, IMMUNE_SLOW);
		}
		SetImmuneFlag(immuneFlag);
		break;
	}

	case POINT_IMMUNE_FALL:	// 78
	{
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
		uint32_t immuneFlag = GetImmuneFlag();
		if (val)
		{
			SET_BIT(immuneFlag, IMMUNE_FALL);
		}
		else
		{
			REMOVE_BIT(immuneFlag, IMMUNE_FALL);
		}
		SetImmuneFlag(immuneFlag);
		break;
	}

	case POINT_ATT_GRADE_BONUS:
		SetPoint(type, GetPoint(type) + amount);
		PointChange(POINT_ATT_GRADE, amount);
		val = GetPoint(type);
		break;

	case POINT_DEF_GRADE_BONUS:
		SetPoint(type, GetPoint(type) + amount);
		PointChange(POINT_DEF_GRADE, amount);
		val = GetPoint(type);
		break;

	case POINT_MAGIC_ATT_GRADE_BONUS:
		SetPoint(type, GetPoint(type) + amount);
		PointChange(POINT_MAGIC_ATT_GRADE, amount);
		val = GetPoint(type);
		break;

	case POINT_MAGIC_DEF_GRADE_BONUS:
		SetPoint(type, GetPoint(type) + amount);
		PointChange(POINT_MAGIC_DEF_GRADE, amount);
		val = GetPoint(type);
		break;

	case POINT_VOICE:
	case POINT_EMPIRE_POINT:
		//"CHARACTER::PointChange: %s: point cannot be changed. use SetPoint instead (type: %d)", GetName(), type);
		val = GetRealPoint(type);
		break;

	case POINT_POLYMORPH:
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
		SetPolymorph(val);
		break;

	case POINT_MOUNT:
		SetPoint(type, GetPoint(type) + amount);
		val = GetPoint(type);
		MountVnum(val);
		break;

	case POINT_ENERGY:
	case POINT_COSTUME_ATTR_BONUS:
	{
		int old_val = GetPoint(type);
		SetPoint(type, old_val + amount);
		val = GetPoint(type);
		BuffOnAttr_ValueChange(type, old_val, val);
	}
	break;

	default:
		LOG_ERROR("CHARACTER::PointChange: {}: unknown point change type {}", GetName(), type);
		return;
	}

	switch (type)
	{
	case POINT_LEVEL:
	case POINT_ST:
	case POINT_DX:
	case POINT_IQ:
	case POINT_HT:
		ComputeBattlePoints();
		break;
	case POINT_MAX_HP:
	case POINT_MAX_SP:
	case POINT_MAX_STAMINA:
		break;
	}

	if (type == POINT_HP && amount == 0)
		return;

	if (GetDesc())
	{
		struct packet_point_change pack;

		pack.header = HEADER_GC_CHARACTER_POINT_CHANGE;
		pack.dwVID = GetPacketVID();
		pack.type = type;
		pack.value = val;

		if (bAmount)
			pack.amount = amount;
		else
			pack.amount = 0;

		if (!bBroadcast)
			GetDesc()->Packet(&pack, sizeof(struct packet_point_change));
		else
			PacketAround(&pack, sizeof(pack));
	}
}

#ifdef __NEWPET_SYSTEM__
void CHARACTER::SendPetLevelUpEffect(int vid, int type, int value, int amount) {
	struct packet_point_change pack;

	pack.header = HEADER_GC_CHARACTER_POINT_CHANGE;
	pack.dwVID = vid;
	pack.type = type;
	pack.value = value;
	pack.amount = amount;
	PacketAround(&pack, sizeof(pack));
}
#endif

void CHARACTER::ApplyPoint(uint8_t bApplyType, int iVal)
{
	switch (bApplyType)
	{
	case APPLY_NONE:			// 0
		break;;

	case APPLY_CON:
		PointChange(POINT_HT, iVal);
		PointChange(POINT_MAX_HP, (iVal * JobInitialPoints[GetJob()].hp_per_ht));
		PointChange(POINT_MAX_STAMINA, (iVal * JobInitialPoints[GetJob()].stamina_per_con));
		break;

	case APPLY_INT:
		PointChange(POINT_IQ, iVal);
		PointChange(POINT_MAX_SP, (iVal * JobInitialPoints[GetJob()].sp_per_iq));
		break;

	case APPLY_SKILL:
		// SKILL_DAMAGE_BONUS
	{
		// Aֻ�A� onA� ���OA��� 8onA� vnum, 9onA� add, 15onA� change
		// 00000000 00000000 00000000 00000000
		// ^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^
		// vnum     ^ add       change
		uint8_t bSkillVnum = (uint8_t)(((uint32_t)iVal) >> 24);
		int iAdd = iVal & 0x00800000;
		int iChange = iVal & 0x007fffff;

		LOG_TRACE("APPLY_SKILL skill {} add? {} change {}", static_cast<int>(bSkillVnum), iAdd ? 1 : 0, iChange);

		if (0 == iAdd)
			iChange = -iChange;

		const auto iter = m_SkillDamageBonus.find(bSkillVnum);

		if (iter == m_SkillDamageBonus.end())
			m_SkillDamageBonus.insert(std::make_pair(bSkillVnum, iChange));
		else
			iter->second += iChange;
	}
	// END_OF_SKILL_DAMAGE_BONUS
	break;

	// NOTE: 3AAIAU?! A��N Aִ�HP o�3E1o3a �u1oA� o��� o�3E1o�! �E��Ao 1a1�A� ��?��I1Ƿ�
	// ��3� MAX_HP�� �e�e�I�� �u1oA� o���A� �a?i 1����! ���e. ��1� ?o�! AI�EAI �O��AuAI�⵵ �I��..
	// 1U2U �o1�Ao ��A� Aִ� hp?� o�A� hpA� onA2A� ���N �� 1U2? Aִ� hp�� ���OA��� hp�� o����N�U.
	// ?o�! PointChange?!1� �I�°� ��A��� ��Ao�Y 13�e 1����� 3��?�1� skip..
	// SP�� �E��AI �e�e�N�U.
	// Mantis : 101460			~ ity ~
	case APPLY_MAX_HP:
	case APPLY_MAX_HP_PCT:
	{
		int i = GetMaxHP();
		if (i == 0) {
			break;
		}

		PointChange(aApplyInfo[bApplyType].bPointType, iVal);
		float fRatio = (float)GetMaxHP() / (float)i;
		PointChange(POINT_HP, GetHP() * fRatio - GetHP());
	}
	break;
	case APPLY_MAX_SP:
	case APPLY_MAX_SP_PCT:
	{
		int i = GetMaxSP();
		if (i == 0) {
			break;
		}

		PointChange(aApplyInfo[bApplyType].bPointType, iVal);
		float fRatio = (float)GetMaxSP() / (float)i;
		PointChange(POINT_SP, GetSP() * fRatio - GetSP());
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
	case APPLY_ENERGY:					// 82 ���
	case APPLY_DEF_GRADE:				// 83 1a3��. DEF_GRADE_BONUS�� A���?!1� ��1e�� o�?����� Aǵ��E 1���(...)�! AִU.
	case APPLY_COSTUME_ATTR_BONUS:		// 84 ��1oA� 3AAIAU?! oUAo 1�1o�! o�3E1o
	case APPLY_MAGIC_ATTBONUS_PER:		// 85 ��1� �o�ݷ� +x%
	case APPLY_MELEE_MAGIC_ATTBONUS_PER:			// 86 ��1� + 1?�� �o�ݷ� +x%
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
		PointChange(aApplyInfo[bApplyType].bPointType, iVal);
		break;

	default:
		LOG_ERROR("Unknown apply type {} name {}", bApplyType, GetName());
		break;

	case APPLY_ATTBONUS_IRR_SPADA:
	case APPLY_ATTBONUS_IRR_SPADONE:
	case APPLY_ATTBONUS_IRR_PUGNALE:
	case APPLY_ATTBONUS_IRR_FRECCIA:
	case APPLY_ATTBONUS_IRR_VENTAGLIO:
	case APPLY_ATTBONUS_IRR_CAMPANA:
	{
		int v = iVal / 5; // 5 -> 1, 10 -> 2, ...
		PointChange(aApplyInfo[bApplyType].bPointType, v);
		break;
	}
	}
}









