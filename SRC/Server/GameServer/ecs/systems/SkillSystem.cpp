#include "../../stdafx.h"
#include <Core/Logging.hpp>
#include "PlayerRuntimeSystem.hpp"
#include "AffectSystem.hpp"

#include <sstream>

#include "SkillSystem.hpp"
#include "PointSystem.hpp"
#include "CombatSystem.hpp"
#include "SocialSystem.hpp"
#include "QuestSystem.hpp"
#include "MountSystem.hpp"
#include "NetworkSyncSystem.hpp"

#include "../../utils.h"
#include "../../vector.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../config.h"
#include "../../battle.h"
#include "../../desc.h"
#include "../../desc_client.h"
#include "../../desc_manager.h"
#include "../../constants.h"
#include "../../log.h"
#include "../../packet.h"
#include "../../questmanager.h"
#include "../../skill.h"
#include "../../affect.h"
#include "../../item.h"
#include "../../sectree_manager.h"
#include "../../mob_manager.h"
#include "../../start_position.h"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include "../../party.h"
#include "../../buffer_manager.h"
#include "../../guild.h"
#include "../../unique_item.h"
#include <common/CommonDefines.h>
#ifdef LEADERBOARD_RAZOR93
#include "../../db.h"
#endif

#define ENABLE_FORCE2MASTERSKILL

extern bool RaceToJob(unsigned race, unsigned* ret_job);

#include "../SpatialHelpers.hpp"
#include "../EntityFactory.hpp"
#include "../Registry.hpp"
#include "../VIDRegistry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/skill_components.hpp"
#include "../components/session_components.hpp"
#include "../components/vital_components.hpp"
#include "ItemSystem.hpp"
#include "../CharacterAccessors.hpp"

namespace
{

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);

LegacyCharHandle LegacyCharOf(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e);
    return legacy ? legacy->ptr : nullptr;
}

ecs::SkillLevels* TryGetSkillLevels(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::SkillLevels>(e);
}

const ecs::SkillLevels* TryGetSkillLevels(entt::entity e, int)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::SkillLevels>(e);
}

void MarkDirty(entt::entity e)
{
    if (e != entt::null && g_registry.valid(e))
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

bool ShouldCheckSkillBookExp(entt::entity character)
{
    return character != entt::null && g_registry.valid(character) &&
        ecs::PointSystem::GetLevel(character) < gPlayerMaxLevel;
}

static const uint32_t s_adwSubSkillVnums[] =
{
    SKILL_LEADERSHIP,
    SKILL_COMBO,
    SKILL_MINING,
    SKILL_LANGUAGE1,
    SKILL_LANGUAGE2,
    SKILL_LANGUAGE3,
    SKILL_POLYMORPH,
    SKILL_HORSE,
    SKILL_HORSE_SUMMON,
    SKILL_HORSE_WILDATTACK,
    SKILL_HORSE_CHARGE,
    SKILL_HORSE_ESCAPE,
    SKILL_HORSE_WILDATTACK_RANGE,
    SKILL_ADD_HP,
    SKILL_RESIST_PENETRATE
#ifdef ENABLE_NEW_SECONDARY_SKILLS
    , NEW_SUPPORT_SKILL_ATTACK,
    NEW_SUPPORT_SKILL_YANG,
    NEW_SUPPORT_SKILL_MONSTERS,
    NEW_SUPPORT_SKILL_HP,
#endif
};

const int SKILL_LIST_COUNT = 6;
static const uint32_t SkillListByJob[JOB_MAX_NUM][SKILL_GROUP_MAX_NUM][SKILL_LIST_COUNT] =
{
	{ {	1,	2,	3,	4,	5,	6	}, {	16,	17,	18,	19,	20,	21	} },
	{ {	31,	32,	33,	34,	35,	36	}, {	46,	47,	48,	49,	50,	51	} },
	{ {	61,	62,	63,	64,	65,	66	}, {	76,	77,	78,	79,	80,	81	} },
	{ {	91,	92,	93,	94,	95,	96	}, {	106,107,108,109,110,111	} },
#ifdef ENABLE_WOLFMAN_CHARACTER
	{ {	170,171,172,173,174,175	}, {	0,	0,	0,	0,	0,	0	} },
#endif
};

} // namespace

namespace SkillSystem {

time_t GetSkillNextReadTime(entt::entity e, uint32_t skillId)
{
    if (skillId >= SKILL_MAX_NUM)
        return 0;

    const auto* levels = TryGetSkillLevels(e, 0);
    return (levels && levels->levels) ? levels->levels[skillId].tNextRead : 0;
}

void SetSkillNextReadTime(entt::entity e, uint32_t skillId, time_t when)
{
    if (skillId >= SKILL_MAX_NUM)
        return;

    auto* levels = TryGetSkillLevels(e);
    if (!levels || !levels->levels)
        return;

    levels->levels[skillId].tNextRead = when;
    MarkDirty(e);
}

int GetSkillLevel(entt::entity e, uint32_t skillId)
{
    if (skillId >= SKILL_MAX_NUM)
        return 0;

    const auto* levels = TryGetSkillLevels(e, 0);
    return (levels && levels->levels) ? MIN(SKILL_MAX_LEVEL, levels->levels[skillId].bLevel) : 0;
}

void SendSkillLevelPacket(entt::entity e)
{
    const auto* levels = TryGetSkillLevels(e, 0);
    const auto* session = e != entt::null && g_registry.valid(e)
        ? g_registry.try_get<ecs::NetworkSession>(e)
        : nullptr;
    if (!levels || !levels->levels || !session || !session->desc)
        return;

    TPacketGCSkillLevel packet {};
    packet.bHeader = HEADER_GC_SKILL_LEVEL;
    std::copy_n(levels->levels, SKILL_MAX_NUM, packet.skills);
    session->desc->Packet(&packet, sizeof(packet));
}

uint8_t GetSkillGroup(entt::entity e)
{
    const auto* levels = TryGetSkillLevels(e, 0);
    if (levels && levels->group != 0)
        return levels->group;

    const auto* points = (e != entt::null && g_registry.valid(e))
        ? g_registry.try_get<ecs::CharacterPoints>(e)
        : nullptr;
    return points ? points->base.skill_group : 0;
}

void SetSkillGroup(entt::entity e, uint8_t skillGroup)
{
    auto* levels = TryGetSkillLevels(e);
    if (!levels)
        return;

    levels->group = skillGroup;
    MarkDirty(e);
}

void SetSkillLevel(entt::entity e, uint32_t skillId, uint8_t level)
{
    if (skillId >= SKILL_MAX_NUM)
        return;

    auto* levels = TryGetSkillLevels(e);
    if (!levels || !levels->levels)
        return;

    levels->levels[skillId].bLevel = MIN(40, level);

#ifdef ENABLE_NEW_SECONDARY_SKILLS
    if ((level > 10) &&
        ((skillId == NEW_SUPPORT_SKILL_ATTACK) || (skillId == NEW_SUPPORT_SKILL_YANG) ||
         (skillId == NEW_SUPPORT_SKILL_MONSTERS) || (skillId == NEW_SUPPORT_SKILL_HP))) {
        level = 10;
        levels->levels[skillId].bLevel = level;
    }
#endif

    if (level >= 40)
        levels->levels[skillId].bMasterType = SKILL_PERFECT_MASTER;
    else if (level >= 30)
        levels->levels[skillId].bMasterType = SKILL_GRAND_MASTER;
    else if (level >= 20)
        levels->levels[skillId].bMasterType = SKILL_MASTER;
    else
        levels->levels[skillId].bMasterType = SKILL_NORMAL;

    MarkDirty(e);
}

bool IsLearnableSkill(entt::entity e, uint32_t skillId)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->IsLearnableSkill(skillId) : false;
}

bool LearnGrandMasterSkill(entt::entity e, uint32_t skillId)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->LearnGrandMasterSkill(skillId) : false;
}

bool LearnSkillByBook(entt::entity e, uint32_t skillId, uint8_t prob)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->LearnSkillByBook(skillId, prob) : false;
}

bool CanUseMobSkill(entt::entity e, unsigned int idx)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->CanUseMobSkill(idx) : false;
}

bool CanUseSkill(entt::entity e, uint32_t skillId)
{
    if (e == entt::null || !g_registry.valid(e) || skillId == 0)
        return false;

    const uint8_t skillGroup = GetSkillGroup(e);
    if (skillGroup > 0) {
        unsigned job = JOB_WARRIOR;
        if (RaceToJob(ecs::PlayerRuntime::GetRaceNum(e), &job) && job < JOB_MAX_NUM) {
            const uint32_t* pSkill = SkillListByJob[job][skillGroup - 1];
            for (int i = 0; i < SKILL_LIST_COUNT; ++i) {
                if (pSkill[i] == skillId)
                    return true;
            }
        }
    }

    if (MountSystem::IsRiding(e)) {
        const uint32_t mountVnum = MountSystem::GetMountVnum(e);
#ifdef ENABLE_MOUNTSKILL_CHECK
        const eMountType mountType = GetMountLevelByVnum(mountVnum, false);
        if (mountType != MOUNT_TYPE_MILITARY) {
            if (test_server)
                LOG_INFO("CanUseSkill: Mount can't skill. vnum({}) type({})", mountVnum, static_cast<int>(mountType));
            return false;
        }
#endif
        switch (skillId) {
        case SKILL_HORSE_WILDATTACK:
        case SKILL_HORSE_CHARGE:
        case SKILL_HORSE_ESCAPE:
        case SKILL_HORSE_WILDATTACK_RANGE:
            return true;
        default:
            break;
        }
    }

    switch (skillId) {
    case 121: case 122: case 124: case 126: case 127: case 128: case 129: case 130:
    case 131:
    case 151: case 152: case 153: case 154: case 155: case 156: case 157: case 158: case 159:
        return true;
    default:
        return false;
    }
}

bool CheckSkillHit(entt::entity attacker, uint8_t skillId, entt::entity target)
{
    auto* ch = LegacyCharOf(attacker);
    return ch ? ch->CheckSkillHitCount(skillId, target) : false;
}

int ComputeCooltime(entt::entity e, int time)
{
    return e != entt::null && g_registry.valid(e)
        ? CalculateDuration(ecs::PointSystem::Get(e, POINT_CASTING_SPEED), time)
        : time;
}

void DisableCooltime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    auto& cooldowns = g_registry.get_or_emplace<ecs::SkillCooldowns>(e);
    cooldowns.disableCooltime = true;
    MarkDirty(e);
}

void ResetMobSkillCooltime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    auto& cooldowns = g_registry.get_or_emplace<ecs::SkillCooldowns>(e);
    cooldowns.mob.fill(0);
    MarkDirty(e);
}

void LearnSkill(entt::entity, uint32_t)
{
}

void SetSkillCooltime(entt::entity, uint32_t, uint32_t, uint32_t)
{
}

bool IsSkillCooltime(entt::entity, uint32_t, uint32_t)
{
    return false;
}

int GetSkillPoint(entt::entity)
{
    return 0;
}

void AddSkillPoint(entt::entity, int)
{
}

int GetSkillMasterType(entt::entity e, uint32_t skillId)
{
    if (skillId >= SKILL_MAX_NUM)
        return SKILL_NORMAL;

    const auto* levels = TryGetSkillLevels(e, 0);
    return (levels && levels->levels) ? levels->levels[skillId].bMasterType : SKILL_NORMAL;
}

int GetSkillPower(entt::entity, uint32_t, uint8_t)
{
    return 0;
}

void ComputeSkillPoints(entt::entity)
{
    if (g_bSkillDisable)
        return;
}

void ResetSkill(entt::entity e)
{
	auto* levels = TryGetSkillLevels(e);
	if (!levels || !levels->levels)
		return;

	std::vector<std::pair<uint32_t, TPlayerSkill>> preserved;
	for (const uint32_t skillId : s_adwSubSkillVnums)
	{
		if (skillId < SKILL_MAX_NUM)
			preserved.emplace_back(skillId, levels->levels[skillId]);
	}

	std::memset(levels->levels, 0, sizeof(TPlayerSkill) * SKILL_MAX_NUM);
	for (const auto& [skillId, value] : preserved)
		levels->levels[skillId] = value;

	ecs::PointSystem::Compute(e);
	SendSkillLevelPacket(e);
	MarkDirty(e);

#ifdef __SKILL_COLOR_SYSTEM__
	auto& colors = g_registry.get_or_emplace<ecs::SkillColor>(e);
	std::memset(colors.data, 0, sizeof(colors.data));
	TSkillColor packet {};
	std::memcpy(packet.dwSkillColor, colors.data, sizeof(colors.data));
	packet.player_id = ecs::PlayerRuntime::GetPlayerID(e);
	db_clientdesc->DBPacketHeader(HEADER_GD_SKILL_COLOR_SAVE, 0, sizeof(TSkillColor));
	db_clientdesc->Packet(&packet, sizeof(TSkillColor));
#endif
}

void ClearSkill(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	ecs::PointSystem::Change(e, POINT_SKILL,
		4 + (ecs::PointSystem::GetLevel(e) - 5) - ecs::PointSystem::Get(e, POINT_SKILL));
	ResetSkill(e);
}

void ClearSubSkill(entt::entity e)
{
	auto* levels = TryGetSkillLevels(e);
	if (!levels || !levels->levels)
		return;

	ecs::PointSystem::Change(e, POINT_SUB_SKILL,
		ecs::PointSystem::GetLevel(e) < 10 ? 0 :
		(ecs::PointSystem::GetLevel(e) - 9) - ecs::PointSystem::Get(e, POINT_SUB_SKILL));

	const TPlayerSkill clean {};
	for (const uint32_t skillId : s_adwSubSkillVnums)
	{
		if (skillId < SKILL_MAX_NUM)
			levels->levels[skillId] = clean;
	}
	ecs::PointSystem::Compute(e);
	SendSkillLevelPacket(e);
	MarkDirty(e);
}

bool ResetOneSkill(entt::entity e, uint32_t skillId)
{
	auto* levels = TryGetSkillLevels(e);
	if (!levels || !levels->levels || skillId >= SKILL_MAX_NUM)
		return false;

	uint8_t level = levels->levels[skillId].bLevel;
	levels->levels[skillId] = TPlayerSkill {};
	ecs::PointSystem::Change(e, POINT_SKILL, std::min<uint8_t>(level, 17));
	LogManager::instance().CharLog(e, skillId, "ONE_SKILL_RESET_BY_SCROLL", "");
	ecs::PointSystem::Compute(e);
	SendSkillLevelPacket(e);
	MarkDirty(e);
	return true;
}

} // namespace SkillSystem

int CHARACTER::ComputeCooltime(int time)
{
    return SkillSystem::ComputeCooltime(GetEntityHandle(), time);
}

void CHARACTER::SetSkillGroup(uint8_t bSkillGroup)
{
    if (bSkillGroup > 2)
        return;

    m_points.skill_group = bSkillGroup;
    SkillSystem::SetSkillGroup(GetEntityHandle(), bSkillGroup);

    TPacketGCChangeSkillGroup p;
    p.header = HEADER_GC_SKILL_GROUP;
    p.skill_group = m_points.skill_group;

    GetDesc()->Packet(&p, sizeof(TPacketGCChangeSkillGroup));
}

time_t CHARACTER::GetSkillNextReadTime(uint32_t dwVnum) const
{
    if (dwVnum >= SKILL_MAX_NUM)
    {
        LOG_ERROR("vnum overflow (vnum: {})", dwVnum);
        return 0;
    }

    const entt::entity e = GetEntityHandle();
    if (e != entt::null && g_registry.valid(e))
        return SkillSystem::GetSkillNextReadTime(e, dwVnum);

    return m_pSkillLevels ? m_pSkillLevels[dwVnum].tNextRead : 0;
}

void CHARACTER::SetSkillNextReadTime(uint32_t dwVnum, time_t time)
{
#ifdef ENABLE_NEW_PASSIVE_SKILLS
    if ((dwVnum >= SKILL_ANTI_PALBANG) && (dwVnum <= SKILL_ANTI_BYEURAK))
        time = uint32_t(get_global_time() + (3600 * 3));
    else if ((dwVnum >= SKILL_HELP_PALBANG) && (dwVnum <= SKILL_HELP_BYEURAK))
        time = uint32_t(get_global_time() + (3600 * 2));
#endif

    if ((GetSkillMasterType(dwVnum) == SKILL_MASTER) && (dwVnum >= SKILL_SAMYEON) && (dwVnum <= SKILL_JEUNGRYEOK))
        time = uint32_t(get_global_time() + 3600);

    if (m_pSkillLevels && dwVnum < SKILL_MAX_NUM)
        m_pSkillLevels[dwVnum].tNextRead = time;

    SkillSystem::SetSkillNextReadTime(GetEntityHandle(), dwVnum, time);
}

uint8_t CHARACTER::GetSkillGroup() const
{
	return SkillSystem::GetSkillGroup(GetEntityHandle());
}

void CHARACTER::SetSkillLevel(uint32_t dwVnum, uint8_t bLev)
{
    if (nullptr == m_pSkillLevels)
        return;

    if (dwVnum >= SKILL_MAX_NUM)
    {
        LOG_ERROR("vnum overflow (vnum {})", dwVnum);
        return;
    }

#ifdef ENABLE_NEW_PASSIVE_SKILLS
    if ((!SkillCanUp(dwVnum)) && (bLev != 0))
        return;

    if ((dwVnum >= SKILL_ANTI_PALBANG) && (dwVnum <= SKILL_ANTI_BYEURAK) && (bLev == 11))
        bLev = 20;
#endif

    m_pSkillLevels[dwVnum].bLevel = MIN(40, bLev);

#ifdef ENABLE_NEW_SECONDARY_SKILLS
    if ((bLev > 10) &&
        ((dwVnum == NEW_SUPPORT_SKILL_ATTACK) || (dwVnum == NEW_SUPPORT_SKILL_YANG) ||
         (dwVnum == NEW_SUPPORT_SKILL_MONSTERS) || (dwVnum == NEW_SUPPORT_SKILL_HP))) {
        bLev = 10;
        m_pSkillLevels[dwVnum].bLevel = bLev;
    }
#endif

    if (bLev >= 40)
        m_pSkillLevels[dwVnum].bMasterType = SKILL_PERFECT_MASTER;
    else if (bLev >= 30)
        m_pSkillLevels[dwVnum].bMasterType = SKILL_GRAND_MASTER;
    else if (bLev >= 20)
        m_pSkillLevels[dwVnum].bMasterType = SKILL_MASTER;
    else
        m_pSkillLevels[dwVnum].bMasterType = SKILL_NORMAL;

    SkillSystem::SetSkillLevel(GetEntityHandle(), dwVnum, bLev);
}

int CHARACTER::GetSkillLevel(uint32_t dwVnum) const
{
    if (dwVnum >= SKILL_MAX_NUM)
    {
        LOG_ERROR("{} skill vnum overflow {}", GetName(), dwVnum);
        LOG_INFO("{} skill vnum overflow {}", GetName(), dwVnum);
        return 0;
    }

    const entt::entity e = GetEntityHandle();
    if (e != entt::null && g_registry.valid(e))
        return SkillSystem::GetSkillLevel(e, dwVnum);

    return MIN(SKILL_MAX_LEVEL, m_pSkillLevels ? m_pSkillLevels[dwVnum].bLevel : 0);
}

void CHARACTER::DisableCooltime()
{
    m_bDisableCooltime = true;
    SkillSystem::DisableCooltime(GetEntityHandle());
}

void CHARACTER::ComputeSkillPoints()
{
    SkillSystem::ComputeSkillPoints(GetEntityHandle());
}

bool CHARACTER::IsLearnableSkill(uint32_t dwSkillVnum) const
{
	const CSkillProto * pkSkill = CSkillManager::instance().Get(dwSkillVnum);

	if (!pkSkill)
		return false;

	if (GetSkillLevel(dwSkillVnum) >= SKILL_MAX_LEVEL)
		return false;

	if (pkSkill->dwType == 0)
	{
		if (GetSkillLevel(dwSkillVnum) >= pkSkill->bMaxLevel)
			return false;

		return true;
	}

	if (pkSkill->dwType == 5)
	{
		if (dwSkillVnum == SKILL_HORSE_WILDATTACK_RANGE && GetJob() != JOB_ASSASSIN)
			return false;

		return true;
	}

	if (GetSkillGroup() == 0)
		return false;

	if (pkSkill->dwType - 1 == GetJob())
		return true;
#ifdef ENABLE_WOLFMAN_CHARACTER
	if (7 == pkSkill->dwType && JOB_WOLFMAN == GetJob())
		return true;
#endif
	if (6 == pkSkill->dwType)
	{
#ifdef ENABLE_NEW_PASSIVE_SKILLS
		return true;
#else
		if (SKILL_7_A_ANTI_TANHWAN <= dwSkillVnum && dwSkillVnum <= SKILL_7_D_ANTI_YONGBI)
		{
			for (int i=0 ; i < 4 ; i++)
			{
				if (unsigned(SKILL_7_A_ANTI_TANHWAN + i) != dwSkillVnum)
				{
					if (0 != GetSkillLevel(SKILL_7_A_ANTI_TANHWAN + i))
					{
						return false;
					}
				}
			}

			return true;
		}

		if (SKILL_8_A_ANTI_GIGONGCHAM <= dwSkillVnum && dwSkillVnum <= SKILL_8_D_ANTI_BYEURAK)
		{
			for (int i=0 ; i < 4 ; i++)
			{
				if (unsigned(SKILL_8_A_ANTI_GIGONGCHAM + i) != dwSkillVnum)
				{
					if (0 != GetSkillLevel(SKILL_8_A_ANTI_GIGONGCHAM + i))
						return false;
				}
			}

			return true;
		}
#endif
	}

	return false;
}

bool CHARACTER::LearnGrandMasterSkill(uint32_t dwSkillVnum)
{
	CSkillProto * pkSk = CSkillManager::instance().Get(dwSkillVnum);

	if (!pkSk)
		return false;

	if (!IsLearnableSkill(dwSkillVnum))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 398, "");
#endif
		return false;
	}

	LOG_INFO("learn grand master skill[{}] cur {}, next {}", dwSkillVnum, get_global_time(), GetSkillNextReadTime(dwSkillVnum));

	if (pkSk->dwType == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 265, "");
#endif
		return false;
	}

	if (GetSkillMasterType(dwSkillVnum) != SKILL_GRAND_MASTER) {
#ifdef TEXTS_IMPROVEMENT
		if (GetSkillMasterType(dwSkillVnum) > SKILL_GRAND_MASTER) {
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 422, "");
		} else {
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 421, "");
		}
#endif
		return false;
	}

	std::string strTrainSkill;
	{
		std::ostringstream os;
		os << "training_grandmaster_skill.skill" << dwSkillVnum;
		strTrainSkill = os.str();
	}

	uint8_t bLastLevel = GetSkillLevel(dwSkillVnum);
	int idx = MIN(9, GetSkillLevel(dwSkillVnum) - 30);

	LOG_INFO("LearnGrandMasterSkill {} table idx {} value {}", GetName(), idx, aiGrandMasterSkillBookCountForLevelUp[idx]);

	int iTotalReadCount = GetQuestFlag(strTrainSkill) + 1;
	SetQuestFlag(strTrainSkill, iTotalReadCount);

	int iMinReadCount = aiGrandMasterSkillBookMinCount[idx];
	int iMaxReadCount = aiGrandMasterSkillBookMaxCount[idx];

	int iBookCount = aiGrandMasterSkillBookCountForLevelUp[idx];

	if (FindAffect(AFFECT_SKILL_BOOK_BONUS))
	{
		if (iBookCount&1)
			iBookCount = iBookCount / 2 + 1;
		else
			iBookCount = iBookCount / 2;

		RemoveAffect(AFFECT_SKILL_BOOK_BONUS);
	}

	int n = number(1, iBookCount);
	LOG_INFO("Number({})", n);

	uint32_t nextTime = get_global_time() + number(g_dwSkillBookNextReadMin, g_dwSkillBookNextReadMax);

	LOG_INFO("GrandMaster SkillBookCount min {} cur {} max {} (next_time={})", iMinReadCount, iTotalReadCount, iMaxReadCount, nextTime);

	bool bSuccess = n == 2;

	if (iTotalReadCount < iMinReadCount)
		bSuccess = false;
	if (iTotalReadCount > iMaxReadCount)
		bSuccess = true;

	if (bSuccess)
	{
		SkillLevelUp(dwSkillVnum, SKILL_UP_BY_QUEST);
	}

	SetSkillNextReadTime(dwSkillVnum, nextTime);

	if (bLastLevel == GetSkillLevel(dwSkillVnum))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 397, "");
#endif
		LogManager::instance().CharLog(this, dwSkillVnum, "GM_READ_FAIL", "");
		return false;
	}

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 304, "");
#endif
	LogManager::instance().CharLog(this, dwSkillVnum, "GM_READ_SUCCESS", "");
	return true;
}

bool CHARACTER::LearnSkillByBook(uint32_t dwSkillVnum, uint8_t bProb)
{
	const CSkillProto* pkSk = CSkillManager::instance().Get(dwSkillVnum);

	if (!pkSk)
		return false;

	if (!IsLearnableSkill(dwSkillVnum))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 398, "");
#endif
		return false;
	}

#ifdef ENABLE_NEW_PASSIVE_SKILLS
	if (!SkillCanUp(dwSkillVnum, true))
		return false;
#endif

	int64_t need_exp = 0;
#ifndef DISABLE_SKILL_BOOK_NEED_EXP
	if (ShouldCheckSkillBookExp(GetEntityHandle()))
	{
		need_exp = 20000;
		if (GetExp() < need_exp)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 247, "");
#endif
			return false;
		}
	}
#endif
	if (pkSk->dwType != 0)
	{
#ifdef ENABLE_NEW_PASSIVE_SKILLS
		if ((GetSkillMasterType(dwSkillVnum) != SKILL_MASTER) && ((dwSkillVnum < SKILL_ANTI_PALBANG) || (dwSkillVnum > SKILL_ANTI_BYEURAK)))
#else
		if (GetSkillMasterType(dwSkillVnum) != SKILL_MASTER)
#endif
		{
#ifdef TEXTS_IMPROVEMENT
			if (GetSkillMasterType(dwSkillVnum) > SKILL_MASTER) {
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 423, "");
			}
			else {
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 424, "");
			}
#endif
			return false;
		}
	}

#ifdef ENABLE_NEW_SECONDARY_SKILLS
	if ((get_global_time() < GetSkillNextReadTime(dwSkillVnum)) && ((dwSkillVnum == NEW_SUPPORT_SKILL_ATTACK) || (dwSkillVnum == NEW_SUPPORT_SKILL_YANG) || (dwSkillVnum == NEW_SUPPORT_SKILL_MONSTERS) || (dwSkillVnum == NEW_SUPPORT_SKILL_HP))) {
		if (FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
		{
			RemoveAffect(AFFECT_SKILL_NO_BOOK_DELAY);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 465, "");
#endif
		}
		else
		{
			int iTime = GetSkillNextReadTime(dwSkillVnum) - get_global_time();
			int iHours = iTime / 3600;
			int iMinutes = (iTime - (iHours * 3600)) / 60;
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 91, "%d#%d", iHours, iMinutes);
#endif
			return false;
		}
	}

	if ((get_global_time() < GetSkillNextReadTime(dwSkillVnum)) && (dwSkillVnum != NEW_SUPPORT_SKILL_ATTACK) && (dwSkillVnum != NEW_SUPPORT_SKILL_YANG) && (dwSkillVnum != NEW_SUPPORT_SKILL_MONSTERS) && (dwSkillVnum != NEW_SUPPORT_SKILL_HP))
#else
	if (get_global_time() < GetSkillNextReadTime(dwSkillVnum))
#endif
	{
		if (!(test_server && quest::CQuestManager::instance().GetEventFlag("no_read_delay")))
		{
			if (FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
			{
				RemoveAffect(AFFECT_SKILL_NO_BOOK_DELAY);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 465, "");
#endif
			}
			else
			{
#ifdef ENABLE_NEW_PASSIVE_SKILLS
				int iTime = GetSkillNextReadTime(dwSkillVnum) - get_global_time();
				int iHours = iTime / 3600;
				int iMinutes = (iTime - (iHours * 3600)) / 60;
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 91, "%d#%d", iHours, iMinutes);
#endif
#else
				SkillLearnWaitMoreTimeMessage(GetSkillNextReadTime(dwSkillVnum) - get_global_time());
#endif
				return false;
			}
		}
	}

	uint8_t bLastLevel = GetSkillLevel(dwSkillVnum);

	if (bProb != 0)
	{
		if (FindAffect(AFFECT_SKILL_BOOK_BONUS))
		{
			bProb += bProb / 2;
			RemoveAffect(AFFECT_SKILL_BOOK_BONUS);
		}

		LOG_INFO("LearnSkillByBook Pct {} prob {}", dwSkillVnum, bProb);

		if (number(1, 100) <= bProb)
		{
			if (test_server)
				LOG_INFO("LearnSkillByBook {} SUCC", dwSkillVnum);

			SkillLevelUp(dwSkillVnum, SKILL_UP_BY_BOOK);
		}
		else
		{
			if (test_server)
				LOG_INFO("LearnSkillByBook {} FAIL", dwSkillVnum);
		}
	}

#ifdef ENABLE_NEW_PASSIVE_SKILLS
	else if ((dwSkillVnum >= SKILL_ANTI_PALBANG) && (dwSkillVnum <= SKILL_ANTI_BYEURAK) && (bLastLevel < 30)) {
		quest::CQuestManager& q = quest::CQuestManager::instance();
		quest::PC* pPC = q.GetPC(GetPlayerID());
		if (!pPC)
			return false;

		int aiSkillBookCount[30] = {
										1, 1, 1, 2, 2, 2, 3, 3, 3, 4,
										0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
										4, 5, 6, 7, 8, 9, 10, 13, 15, 19
		};

		int needBookCount = aiSkillBookCount[GetSkillLevel(dwSkillVnum)];

		char szFlag[128 + 1];
		memset(szFlag, 0, sizeof(szFlag));
		snprintf(szFlag, sizeof(szFlag), "training_%d.count", dwSkillVnum);

		int iReadCount = pPC->GetFlag(szFlag);
		int percent = 30;

		if (FindAffect(AFFECT_SKILL_BOOK_BONUS)) {
			percent = 20;
			RemoveAffect(AFFECT_SKILL_BOOK_BONUS);
		}
#ifndef DISABLE_SKILL_BOOK_NEED_EXP
		if (need_exp > 0) PointChange(POINT_EXP, -need_exp);
#endif
		if (number(1, 100) > percent) {
			if (iReadCount >= needBookCount) {
				SetSkillLevel(dwSkillVnum, bLastLevel + 1);

				ComputePoints();
				SkillLevelPacket();
				pPC->SetFlag(szFlag, 0);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 304, "");
#endif
				LogManager::instance().CharLog(this, dwSkillVnum, "READ_SUCCESS", "");
				return true;
			}
			else {
				pPC->SetFlag(szFlag, iReadCount + 1);
#ifdef TEXTS_IMPROVEMENT
				switch (number(1, 3)) {
				case 1:
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 319, "");
					break;
				case 2:
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 318, "");
					break;
				case 3:
				default:
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 320, "");
					break;
				}
#endif

#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 492, "%d", (needBookCount - iReadCount));
#endif
				return true;
			}
		}
	}
#endif

#ifdef ENABLE_NEW_SECONDARY_SKILLS
	else if ((dwSkillVnum == NEW_SUPPORT_SKILL_ATTACK) || (dwSkillVnum == NEW_SUPPORT_SKILL_YANG) || (dwSkillVnum == NEW_SUPPORT_SKILL_MONSTERS) || (dwSkillVnum == NEW_SUPPORT_SKILL_HP)) {
		quest::CQuestManager& q = quest::CQuestManager::instance();
		quest::PC* pPC = q.GetPC(GetPlayerID());
		if (!pPC)
			return false;

		int aiSkillBookCount[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, };
		int needBookCount = aiSkillBookCount[GetSkillLevel(dwSkillVnum)];

		char szFlag[128 + 1];
		memset(szFlag, 0, sizeof(szFlag));
		snprintf(szFlag, sizeof(szFlag), "training_%d.count", dwSkillVnum);

		int iReadCount = pPC->GetFlag(szFlag);
		int percent = 30;

		if (FindAffect(AFFECT_SKILL_BOOK_BONUS)) {
			percent = 10;
			RemoveAffect(AFFECT_SKILL_BOOK_BONUS);
		}

#ifndef DISABLE_SKILL_BOOK_NEED_EXP
		 if (need_exp > 0) PointChange(POINT_EXP, -need_exp);
#endif
		if (number(1, 100) > percent) {
			if (iReadCount >= needBookCount) {
				SkillLevelUp(dwSkillVnum, SKILL_UP_BY_BOOK);
				pPC->SetFlag(szFlag, 0);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 304, "");
#endif
				LogManager::instance().CharLog(this, dwSkillVnum, "READ_SUCCESS", "");
				return true;
			}
			else {
				pPC->SetFlag(szFlag, iReadCount + 1);
#ifdef TEXTS_IMPROVEMENT
				switch (number(1, 3)) {
				case 1:
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 319, "");
					break;
				case 2:
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 318, "");
					break;
				case 3:
				default:
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 320, "");
					break;
				}
#endif

#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 492, "%d", (needBookCount - iReadCount));
#endif
				return true;
			}
		}
	}
#endif

	else
	{
		int idx = MIN(9, GetSkillLevel(dwSkillVnum) - 20);

		LOG_INFO("LearnSkillByBook {} table idx {} value {}", GetName(), idx, aiSkillBookCountForLevelUp[idx]);

		{
			int need_bookcount = 0;

#ifndef DISABLE_SKILL_BOOK_NEED_EXP
			 if (need_exp > 0) PointChange(POINT_EXP, -need_exp);
#endif
			quest::CQuestManager& q = quest::CQuestManager::instance();
			quest::PC* pPC = q.GetPC(GetPlayerID());

			if (pPC)
			{
				char flag[128 + 1];
				memset(flag, 0, sizeof(flag));
				snprintf(flag, sizeof(flag), "traning_master_skill.%u.read_count", dwSkillVnum);

				int read_count = pPC->GetFlag(flag);
				int percent = 30;
				if (FindAffect(AFFECT_SKILL_BOOK_BONUS))
				{
					percent = 0;
					if ((dwSkillVnum >= SKILL_HELP_PALBANG) && (dwSkillVnum <= SKILL_HELP_BYEURAK))
						percent = 20;

					RemoveAffect(AFFECT_SKILL_BOOK_BONUS);
				}

				if (number(1, 100) > percent)
				{
#ifdef ENABLE_MASTER_SKILLBOOK_NO_STEPS
					if (true)
#else
					if (read_count >= need_bookcount)
#endif
					{
						SkillLevelUp(dwSkillVnum, SKILL_UP_BY_BOOK);
						pPC->SetFlag(flag, 0);

#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 304, "");
#endif
						LogManager::instance().CharLog(this, dwSkillVnum, "READ_SUCCESS", "");
						return true;
					}
					else
					{
						pPC->SetFlag(flag, read_count + 1);
#ifdef TEXTS_IMPROVEMENT
						switch (number(1, 3)) {
						case 1:
							ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 319, "");
							break;
						case 2:
							ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 318, "");
							break;
						case 3:
						default:
							ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 320, "");
							break;
						}
#endif
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 492, "%d", (need_bookcount - read_count));
#endif
						return true;
					}
				}
			}
		}
	}

	if (bLastLevel != GetSkillLevel(dwSkillVnum))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 304, "");
#endif
		LogManager::instance().CharLog(this, dwSkillVnum, "READ_SUCCESS", "");
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 397, "");
#endif
		LogManager::instance().CharLog(this, dwSkillVnum, "READ_FAIL", "");
	}

	return true;
}

bool CHARACTER::CanUseMobSkill(unsigned int idx) const
{
	const TMobSkillInfo* pInfo = GetMobSkill(idx);

	if (!pInfo)
		return false;

	if (m_adwMobSkillCooltime[idx] > get_dword_time())
		return false;

	if (number(0, 1))
		return false;

	return true;
}

bool CHARACTER::CanUseSkill(uint32_t dwSkillVnum) const
{
	if (0 == dwSkillVnum) return false;

	if (0 < GetSkillGroup())
	{
		const uint32_t* pSkill = SkillListByJob[ GetJob() ][ GetSkillGroup()-1 ];

		for (int i=0 ; i < SKILL_LIST_COUNT ; ++i)
		{
			if (pSkill[i] == dwSkillVnum) return true;
		}
	}

	if (true == IsRiding())
	{
#ifdef ENABLE_MOUNTSKILL_CHECK
		eMountType eIsMount = GetMountLevelByVnum(GetMountVnum(), false);
		if (eIsMount != MOUNT_TYPE_MILITARY)
		{
			if (test_server)
				LOG_INFO("CanUseSkill: Mount can't skill. vnum({}) type({})", GetMountVnum(), static_cast<int>(eIsMount));
			return false;
		}
#endif
		switch(dwSkillVnum)
		{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:
				return true;
		}
	}

	switch( dwSkillVnum )
	{
		case 121: case 122: case 124: case 126: case 127: case 128: case 129: case 130:
		case 131:
		case 151: case 152: case 153: case 154: case 155: case 156: case 157: case 158: case 159:
			return true;
	}

	return false;
}

bool CHARACTER::CheckSkillHitCount(const uint8_t SkillID, entt::entity TargetVID)
{
	std::map<int, TSkillUseInfo>::iterator iter = m_SkillUseInfo.find(SkillID);

	if (iter == m_SkillUseInfo.end())
	{
		LOG_INFO("SkillHack: Skill({}) is not in container", SkillID);
		return false;
	}

	TSkillUseInfo& rSkillUseInfo = iter->second;

	if (false == rSkillUseInfo.bUsed)
	{
		LOG_INFO("SkillHack: not used skill({})", SkillID);
		return false;
	}

	switch (SkillID)
	{
		case SKILL_YONGKWON:
		case SKILL_HWAYEOMPOK:
		case SKILL_DAEJINGAK:
		case SKILL_PAERYONG:
			LOG_INFO("SkillHack: cannot use attack packet for skill({})", SkillID);
			return false;
	}

	auto iterTargetMap = rSkillUseInfo.TargetVIDMap.find(TargetVID);

	if (rSkillUseInfo.TargetVIDMap.end() != iterTargetMap)
	{
		size_t MaxAttackCountPerTarget = 1;

		switch (SkillID)
		{
			case SKILL_SAMYEON:
			case SKILL_CHARYUN:
#ifdef ENABLE_WOLFMAN_CHARACTER
			case SKILL_CHAYEOL:
#endif
				MaxAttackCountPerTarget = 3;
				break;

			case SKILL_HORSE_WILDATTACK_RANGE:
				MaxAttackCountPerTarget = 5;
				break;

			case SKILL_YEONSA:
				MaxAttackCountPerTarget = 7;
				break;

			case SKILL_HORSE_ESCAPE:
				MaxAttackCountPerTarget = 10;
				break;
		}

		if (iterTargetMap->second >= MaxAttackCountPerTarget)
		{
			LOG_INFO("SkillHack: Too Many Hit count from SkillID({}) count({})", SkillID, iterTargetMap->second);
			return false;
		}

		iterTargetMap->second++;
	}
	else
	{
		rSkillUseInfo.TargetVIDMap.insert( std::make_pair(TargetVID, 1) );
	}

	return true;
}

void CHARACTER::ResetMobSkillCooltime()
{
    memset(m_adwMobSkillCooltime, 0, sizeof(m_adwMobSkillCooltime));
    SkillSystem::ResetMobSkillCooltime(GetEntityHandle());
}

void CHARACTER::ResetSkill()
{
	SkillSystem::ResetSkill(GetEntityHandle());
}

// char_skill.cpp slice E + remaining helpers migrated

bool TSkillUseInfo::HitOnce(uint32_t dwVnum)
{
	// ľ˛ÁöµµľĘľŇŔ¸¸é ¶§¸®Áöµµ ¸řÇŃ´Ů.
	if (!bUsed)
		return false;

	LOG_INFO("__HitOnce NextUse {} current {} count {} scount {}", dwNextSkillUsableTime, get_dword_time(), iHitCount, iSplashCount);

	if (dwNextSkillUsableTime && dwNextSkillUsableTime<get_dword_time() && dwVnum != SKILL_MUYEONG && dwVnum != SKILL_HORSE_WILDATTACK
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
	 && dwVnum != SKILL_GYEONGGONG
#endif
	)
	{
		LOG_INFO("__HitOnce can't hit");

		return false;
	}

	if (iHitCount == -1)
	{
		LOG_INFO("__HitOnce OK {} {} {}", dwNextSkillUsableTime, get_dword_time(), iHitCount);
		return true;
	}

	if (iHitCount)
	{
		LOG_INFO("__HitOnce OK {} {} {}", dwNextSkillUsableTime, get_dword_time(), iHitCount);
		iHitCount--;
		return true;
	}
	return false;
}



bool TSkillUseInfo::UseSkill(bool isGrandMaster, entt::entity vid, uint32_t dwCooltime, int splashcount, int hitcount, int range)
{
	this->isGrandMaster = isGrandMaster;
	uint32_t dwCur = get_dword_time();

	// ľĆÁ÷ ÄđĹ¸ŔÓŔĚ łˇłŞÁö ľĘľŇ´Ů.
	if (bUsed && dwNextSkillUsableTime > dwCur)
	{
		LOG_INFO("cooltime is not over delta {}", dwNextSkillUsableTime - dwCur);
		iHitCount = 0;
		return false;
	}

	bUsed = true;

	if (dwCooltime)
		dwNextSkillUsableTime = dwCur + dwCooltime;
	else
		dwNextSkillUsableTime = 0;

	iRange = range;
	iMaxHitCount = iHitCount = hitcount;

	if (test_server)
		LOG_INFO("UseSkill NextUse {}  current {} cooltime {} hitcount {}/{}", dwNextSkillUsableTime, dwCur, dwCooltime, iHitCount, iMaxHitCount);

	dwVID = vid;
	iSplashCount = splashcount;
	return true;
}

int CHARACTER::GetChainLightningMaxCount() const
{
	return aiChainLightningCountBySkillLevel[MIN(SKILL_MAX_LEVEL, GetSkillLevel(SKILL_CHAIN))];
}

void CHARACTER::SetAffectedEunhyung()
{
	m_dwAffectedEunhyungLevel = GetSkillPower(SKILL_EUNHYUNG);
}


void CHARACTER::SkillLevelPacket()
{
	if (!GetDesc())
		return;

	TPacketGCSkillLevel pack;

	pack.bHeader = HEADER_GC_SKILL_LEVEL;
	memcpy(&pack.skills, m_pSkillLevels, sizeof(TPlayerSkill) * SKILL_MAX_NUM);
	GetDesc()->Packet(&pack, sizeof(TPacketGCSkillLevel));
}



bool CHARACTER::SkillLevelDown(uint32_t dwVnum)
{
	if (nullptr == m_pSkillLevels)
		return false;

	if (g_bSkillDisable)
		return false;

	if (IsPolymorphed())
		return false;

	CSkillProto * pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
	{
		LOG_ERROR("There is no such skill by number {}", dwVnum);
		return false;
	}

	if (!IsLearnableSkill(dwVnum))
		return false;

	if (GetSkillMasterType(pkSk->dwVnum) != SKILL_NORMAL)
		return false;

	if (!GetSkillGroup())
		return false;

	if (pkSk->dwVnum >= SKILL_MAX_NUM)
		return false;

	if (m_pSkillLevels[pkSk->dwVnum].bLevel == 0)
		return false;

	int idx = POINT_SKILL;
	switch (pkSk->dwType)
	{
		case 0:
			idx = POINT_SUB_SKILL;
			break;
		case 1:
		case 2:
		case 3:
		case 4:
		case 6:
#ifdef ENABLE_WOLFMAN_CHARACTER
		case 7:
#endif
			idx = POINT_SKILL;
			break;
		case 5:
			idx = POINT_HORSE_SKILL;
			break;
		default:
			LOG_ERROR("Wrong skill type {} skill vnum {}", pkSk->dwType, pkSk->dwVnum);
			return false;

	}

	PointChange(idx, +1);
	SetSkillLevel(pkSk->dwVnum, m_pSkillLevels[pkSk->dwVnum].bLevel - 1);

	LOG_INFO("SkillDown: {} {} {} {} type {}", GetName(), pkSk->dwVnum, m_pSkillLevels[pkSk->dwVnum].bMasterType, m_pSkillLevels[pkSk->dwVnum].bLevel, pkSk->dwType);
	Save();

	ComputePoints();
	SkillLevelPacket();
	return true;
}

#ifdef ENABLE_NEW_PASSIVE_SKILLS
bool CHARACTER::SkillCanUp(uint32_t dwVnum, bool book)
{
	bool passive = false;
	bool bCan;
	switch (dwVnum) {
		case SKILL_HELP_PALBANG:
			bCan = GetSkillLevel(SKILL_PALBANG) < 40 ? false : true;
			break;
		case SKILL_HELP_AMSEOP:
			bCan = GetSkillLevel(SKILL_AMSEOP) < 40 ? false : true;
			break;
		case SKILL_HELP_SWAERYUNG:
			bCan = GetSkillLevel(SKILL_SWAERYUNG) < 40 ? false : true;
			break;
		case SKILL_HELP_YONGBI:
			bCan = GetSkillLevel(SKILL_YONGBI) < 40 ? false : true;
			break;
		case SKILL_HELP_GIGONGCHAM:
			bCan = GetSkillLevel(SKILL_GIGONGCHAM) < 40 ? false : true;
			break;
		case SKILL_HELP_HWAJO:
			bCan = GetSkillLevel(SKILL_HWAJO) < 40 ? false : true;
			break;
		case SKILL_HELP_MARYUNG:
			bCan = GetSkillLevel(SKILL_MARYUNG) < 40 ? false : true;
			break;
		case SKILL_HELP_BYEURAK:
			bCan = GetSkillLevel(SKILL_BYEURAK) < 40 ? false : true;
			break;
		case SKILL_ANTI_PALBANG:
		case SKILL_ANTI_AMSEOP:
		case SKILL_ANTI_SWAERYUNG:
		case SKILL_ANTI_YONGBI:
		case SKILL_ANTI_GIGONGCHAM:
		case SKILL_ANTI_HWAJO:
		case SKILL_ANTI_MARYUNG:
		case SKILL_ANTI_BYEURAK:
			{
				if (GetLevel() < 90) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 92, "");
#endif
					return false;
				}
				else {
					passive = true;
					if (book) {
						bCan = GetSkillLevel(dwVnum) < 30 ? true : false;
					} else {
						bCan = true;
					}
					break;
				}
			}
		default:
			bCan = true;
			break;
	}

	if (!bCan && !passive) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 93, "");
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 94, "");
#endif
	} else if (!bCan && passive) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 423, "");
#endif
	}

	return bCan;
}
#endif

void CHARACTER::SkillLevelUp(uint32_t dwVnum, uint8_t bMethod)
{
	if (nullptr == m_pSkillLevels)
		return;

	if (g_bSkillDisable)
		return;

	if (IsPolymorphed())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 313, "");
#endif
		return;
	}

#ifdef ENABLE_NEW_PASSIVE_SKILLS
	if (!SkillCanUp(dwVnum))
		return;
#endif

	if (SKILL_7_A_ANTI_TANHWAN <= dwVnum && dwVnum <= SKILL_8_D_ANTI_BYEURAK)
	{
		if (0 == GetSkillLevel(dwVnum))
			return;
	}

	const CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
	{
		LOG_ERROR("There is no such skill by number (vnum {})", dwVnum);
		return;
	}

	if (pkSk->dwVnum >= SKILL_MAX_NUM)
	{
		LOG_ERROR("Skill Vnum overflow (vnum {})", dwVnum);
		return;
	}

	if (!IsLearnableSkill(dwVnum))
		return;

	// ±×·Łµĺ ¸¶˝şĹÍ´Â Äů˝şĆ®·Î¸¸ ĽöÇŕ°ˇ´É
	if (pkSk->dwType != 0)
	{
		switch (GetSkillMasterType(pkSk->dwVnum))
		{
			case SKILL_GRAND_MASTER:
				if (bMethod != SKILL_UP_BY_QUEST)
					return;
				break;

			case SKILL_PERFECT_MASTER:
				return;
		}
	}

	if (bMethod == SKILL_UP_BY_POINT)
	{
		// ¸¶˝şĹÍ°ˇ ľĆ´Ń »óĹÂżˇĽ­¸¸ Ľö·Ă°ˇ´É
		if (GetSkillMasterType(pkSk->dwVnum) != SKILL_NORMAL)
			return;

		if (IS_SET(pkSk->dwFlag, SKILL_FLAG_DISABLE_BY_POINT_UP))
			return;
	}
	else if (bMethod == SKILL_UP_BY_BOOK)
	{
		if (pkSk->dwType != 0) // Á÷ľ÷żˇ ĽÓÇĎÁö ľĘľŇ°ĹłŞ Ć÷ŔÎĆ®·Î żĂ¸±Ľö ľř´Â ˝şĹłŔş ĂłŔ˝şÎĹÍ ĂĄŔ¸·Î ąčżď Ľö ŔÖ´Ů.
			if (GetSkillMasterType(pkSk->dwVnum) != SKILL_MASTER)
				return;
	}

	if (GetLevel() < pkSk->bLevelLimit)
		return;

	if (pkSk->preSkillVnum)
		if (GetSkillMasterType(pkSk->preSkillVnum) == SKILL_NORMAL &&
			GetSkillLevel(pkSk->preSkillVnum) < pkSk->preSkillLevel)
			return;

	if (!GetSkillGroup())
		return;

	if (bMethod == SKILL_UP_BY_POINT)
	{
		int idx;

		switch (pkSk->dwType)
		{
			case 0:
				idx = POINT_SUB_SKILL;
				break;

			case 1:
			case 2:
			case 3:
			case 4:
			case 6:
#ifdef ENABLE_WOLFMAN_CHARACTER
			case 7:
#endif
				idx = POINT_SKILL;
				break;

			case 5:
				idx = POINT_HORSE_SKILL;
				break;

			default:
				LOG_ERROR("Wrong skill type {} skill vnum {}", pkSk->dwType, pkSk->dwVnum);
				return;
		}

		if (GetPoint(idx) < 1)
			return;

		PointChange(idx, -1);
	}

	int SkillPointBefore = GetSkillLevel(pkSk->dwVnum);
	SetSkillLevel(pkSk->dwVnum, m_pSkillLevels[pkSk->dwVnum].bLevel + 1);

	if (pkSk->dwType != 0)
	{
		// °©ŔÚ±â ±×·ąŔĚµĺ ľ÷ÇĎ´Â ÄÚµů
		switch (GetSkillMasterType(pkSk->dwVnum))
		{
			case SKILL_NORMAL:
				// ąřĽ·Ŕş ˝şĹł ľ÷±×·ąŔĚµĺ 17~20 »çŔĚ ·Ł´ý ¸¶˝şĹÍ Ľö·Ă
				if (GetSkillLevel(pkSk->dwVnum) >= 17)
				{
#ifdef ENABLE_FORCE2MASTERSKILL
					SetSkillLevel(pkSk->dwVnum, 20);
#else
					if (GetQuestFlag("reset_scroll.force_to_master_skill") > 0)
					{
						SetSkillLevel(pkSk->dwVnum, 20);
						SetQuestFlag("reset_scroll.force_to_master_skill", 0);
					}
					else
					{
						if (number(1, 21 - MIN(20, GetSkillLevel(pkSk->dwVnum))) == 1)
							SetSkillLevel(pkSk->dwVnum, 20);
					}
#endif
				}
				break;

			case SKILL_MASTER:
				if (GetSkillLevel(pkSk->dwVnum) >= 30)
				{
					if (number(1, 31 - MIN(30, GetSkillLevel(pkSk->dwVnum))) == 1)
						SetSkillLevel(pkSk->dwVnum, 30);
				}
				break;

			case SKILL_GRAND_MASTER:
				if (GetSkillLevel(pkSk->dwVnum) >= 40)
				{
					SetSkillLevel(pkSk->dwVnum, 40);
				}
				break;
		}
	}

	char szSkillUp[1024];

	snprintf(szSkillUp, sizeof(szSkillUp), "SkillUp: %s %u %d %d[Before:%d] type %u",
			GetName(), pkSk->dwVnum, m_pSkillLevels[pkSk->dwVnum].bMasterType, m_pSkillLevels[pkSk->dwVnum].bLevel, SkillPointBefore, pkSk->dwType);

	LOG_INFO("{}", szSkillUp);

	LogManager::instance().CharLog(this, pkSk->dwVnum, "SKILLUP", szSkillUp);
	Save();

	ComputePoints();
	SkillLevelPacket();
}


void CHARACTER::ComputePassiveSkill(uint32_t dwVnum)
{
	if (g_bSkillDisable)
		return;

	if (GetSkillLevel(dwVnum) == 0)
		return;

	CSkillProto * pkSk = CSkillManager::instance().Get(dwVnum);
	pkSk->SetPointVar("k", GetSkillLevel(dwVnum));
	int iAmount = (int) pkSk->kPointPoly.Eval();

	LOG_INFO("{} passive #{} on {} amount {}", GetName(), dwVnum, pkSk->bPointOn, iAmount);
	PointChange(pkSk->bPointOn, iAmount);
}

struct FFindNearVictim
{
	FFindNearVictim(entt::entity center, entt::entity attacker,
		const CHARACTER::TChainLightningExceptContainer& excepts_set = empty_set_)
		: m_center(center),
	m_nextTarget(entt::null),
	m_attacker(attacker),
	m_count(0),
	m_excepts_set(excepts_set)
	{
	}

	void operator ()(LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER))
			return;

		auto* pkChr = static_cast<LegacyCharHandle>(ent);
		const entt::entity candidate = pkChr->GetEntityHandle();

		if (!m_excepts_set.empty()) {
			if (m_excepts_set.find(candidate) != m_excepts_set.end())
				return;
		}

		if (m_center == candidate)
			return;

		if (!battle_is_attackable(m_attacker, candidate))
		{
			return;
		}

		if (abs(ecs::PlayerRuntime::GetX(m_center) - ecs::PlayerRuntime::GetX(candidate)) > 1000 || abs(ecs::PlayerRuntime::GetY(m_center) - ecs::PlayerRuntime::GetY(candidate)) > 1000)
			return;

		float fDist = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(m_center) - ecs::PlayerRuntime::GetX(candidate), ecs::PlayerRuntime::GetY(m_center) - ecs::PlayerRuntime::GetY(candidate));

		if (fDist < 1000)
		{
			++m_count;

			if ((m_count == 1) || number(1, m_count) == 1)
				m_nextTarget = candidate;
		}
	}

	entt::entity GetVictim() const
	{
		return m_nextTarget;
	}

	entt::entity m_center;
	entt::entity m_nextTarget;
	entt::entity m_attacker;
	int		m_count;
	const CHARACTER::TChainLightningExceptContainer & m_excepts_set;
private:
	static CHARACTER::TChainLightningExceptContainer empty_set_;
};

CHARACTER::TChainLightningExceptContainer FFindNearVictim::empty_set_;

EVENTINFO(chain_lightning_event_info)
{
	entt::entity			dwVictim;
	entt::entity			dwChr;

	chain_lightning_event_info()
	: dwVictim(entt::null)
	, dwChr(entt::null)
	{
	}
};

EVENTFUNC(ChainLightningEvent)
{
	chain_lightning_event_info * info = dynamic_cast<chain_lightning_event_info *>( event->info );
	const entt::entity victimEntity = info->dwVictim;
	const entt::entity character = info->dwChr;

	auto* pkChrVictim = LegacyCharOf(victimEntity);
	auto* pkChr = LegacyCharOf(character);
	entt::entity target = entt::null;

	if (!pkChr || !pkChrVictim)
	{
		LOG_INFO("use chainlighting, but no character");
		return 0;
	}

	LOG_INFO("chainlighting event {}", ecs::PlayerRuntime::GetName(character).data());

	if (ecs::SocialSystem::GetParty(victimEntity)) // ĆÄĆĽ ¸ŐŔú
	{
		LPCHARACTER pkTarget = ecs::SocialSystem::GetParty(victimEntity)->GetNextOwnership(nullptr, ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity));
		target = pkTarget ? pkTarget->GetEntityHandle() : entt::null;
		if (target == victimEntity || !number(0, 2) || pkChr->GetChainLightingExcept().find(target) != pkChr->GetChainLightingExcept().end())
			target = entt::null;
	}

	if (target == entt::null)
	{
		// 1. Find Next victim
		FFindNearVictim f(victimEntity, character, pkChr->GetChainLightingExcept());

		if (ecs::PlayerRuntime::GetSectree(victimEntity))
		{
			ecs::PlayerRuntime::GetSectree(victimEntity)->ForEachAround(f);
			// 2. If exist, compute it again
			target = f.GetVictim();
		}
	}

	if (target != entt::null)
	{
		pkChrVictim->CreateFly(FLY_CHAIN_LIGHTNING, target);
		if (character != entt::null)
			g_dispatcher.trigger(ecs::EvSkillUsed { character, SKILL_CHAIN });
		pkChr->ComputeSkill(SKILL_CHAIN, target);
		pkChr->AddChainLightningExcept(target);
	}
	else
	{
		LOG_INFO("{} use chainlighting, but find victim failed near {}", ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetName(victimEntity).data());
	}

	return 0;
}

void SetPolyVarForAttack(entt::entity character, CSkillProto * pkSk, entt::entity pkWeapon)
{
	if (ecs::PlayerRuntime::IsPC(character))
	{
		if (ItemSystem::IsValidItem(pkWeapon) && ItemSystem::GetItemType(pkWeapon) == ITEM_WEAPON)
		{
			int iWep = number(ItemSystem::GetItemValue(pkWeapon, 3), ItemSystem::GetItemValue(pkWeapon, 4));
			iWep += ItemSystem::GetItemValue(pkWeapon, 5);

			int iMtk = number(ItemSystem::GetItemValue(pkWeapon, 1), ItemSystem::GetItemValue(pkWeapon, 2));
			iMtk += ItemSystem::GetItemValue(pkWeapon, 5);

			pkSk->SetPointVar("wep", iWep);
			pkSk->SetPointVar("mtk", iMtk);
			pkSk->SetPointVar("mwep", iMtk);
		}
		else
		{
			pkSk->SetPointVar("wep", 0);
			pkSk->SetPointVar("mtk", 0);
			pkSk->SetPointVar("mwep", 0);
		}
	}
	else
	{
		auto* legacyCharacter = LegacyCharOf(character);
		const int iWep = legacyCharacter
			? number(legacyCharacter->GetMobDamageMin(), legacyCharacter->GetMobDamageMax())
			: 0;
		pkSk->SetPointVar("wep", iWep);
		pkSk->SetPointVar("mwep", iWep);
		pkSk->SetPointVar("mtk", iWep);
	}
}

struct FuncSplashDamage
{
	FuncSplashDamage(int x, int y, CSkillProto * pkSk, LegacyCharHandle pkChr, int iAmount, int iAG, int iMaxHit, entt::entity pkWeapon, bool bDisableCooltime, TSkillUseInfo* pInfo, uint8_t bUseSkillPower)
		:
		m_x(x), m_y(y), m_pkSk(pkSk), m_pkChr(pkChr),
		m_character(pkChr ? pkChr->GetEntityHandle() : entt::null),
		m_iAmount(iAmount), m_iAG(iAG), m_iCount(0), m_iMaxHit(iMaxHit), m_pkWeapon(pkWeapon), m_bDisableCooltime(bDisableCooltime), m_pInfo(pInfo), m_bUseSkillPower(bUseSkillPower)
		{
		}

	void operator () (LPENTITY ent)
	{
		const entt::entity chr = m_pkChr ? m_pkChr->GetEntityHandle() : entt::null;
		if (!ent->IsType(ENTITY_CHARACTER))
		{
			//if (m_pkSk->dwVnum == SKILL_CHAIN) LOG_INFO(0, "CHAIN target not character %s", ecs::PlayerRuntime::GetName(m_character).data());
			return;
		}

		auto* pkChrVictim = static_cast<LegacyCharHandle>(ent);
		const entt::entity chrVictim = pkChrVictim ? pkChrVictim->GetEntityHandle() : entt::null;

		const entt::entity victimEntity = pkChrVictim->GetEntityHandle();

		if (DISTANCE_APPROX(m_x - ecs::PlayerRuntime::GetX(victimEntity), m_y - ecs::PlayerRuntime::GetY(victimEntity)) > m_pkSk->iSplashRange)
		{
			if(test_server)
				LOG_INFO("XXX target too far {}", ecs::PlayerRuntime::GetName(m_character).data());
			return;
		}

		if (!battle_is_attackable(chr, chrVictim))
		{
			if(test_server)
				LOG_INFO("XXX target not attackable {}", ecs::PlayerRuntime::GetName(m_character).data());
			return;
		}

		if (ecs::PlayerRuntime::IsPC(m_character))
			// ±ćµĺ ˝şĹłŔş ÄđĹ¸ŔÓ Ăł¸®¸¦ ÇĎÁö ľĘ´Â´Ů.
			if (!(m_pkSk->dwVnum >= GUILD_SKILL_START && m_pkSk->dwVnum <= GUILD_SKILL_END))
				if (!m_bDisableCooltime && m_pInfo && !m_pInfo->HitOnce(m_pkSk->dwVnum) && m_pkSk->dwVnum != SKILL_MUYEONG)
				{
					if(test_server)
						LOG_INFO("check guild skill {}", ecs::PlayerRuntime::GetName(m_character).data());
					return;
				}

		++m_iCount;

		int iDam;

		////////////////////////////////////////////////////////////////////////////////
		//float k = 1.0f * m_pkChr->GetSkillPower(m_pkSk->dwVnum) * m_pkSk->bMaxLevel / 100;
		//m_pkSk->kPointPoly2.SetVar("k", 1.0 * m_bUseSkillPower * m_pkSk->bMaxLevel / 100);
		m_pkSk->SetPointVar("k", 1.0 * m_bUseSkillPower * m_pkSk->bMaxLevel / 100);
		m_pkSk->SetPointVar("lv", ecs::PointSystem::GetLevel(m_character));
		m_pkSk->SetPointVar("iq", ecs::PointSystem::Get(m_character, POINT_IQ));
		m_pkSk->SetPointVar("str", ecs::PointSystem::Get(m_character, POINT_ST));
		m_pkSk->SetPointVar("dex", ecs::PointSystem::Get(m_character, POINT_DX));
		m_pkSk->SetPointVar("con", ecs::PointSystem::Get(m_character, POINT_HT));
		m_pkSk->SetPointVar("def", ecs::PointSystem::Get(m_character, POINT_DEF_GRADE));
		m_pkSk->SetPointVar("odef", ecs::PointSystem::Get(m_character, POINT_DEF_GRADE) - ecs::PointSystem::Get(m_character, POINT_DEF_GRADE_BONUS));
		m_pkSk->SetPointVar("horse_level", m_pkChr->GetHorseLevel());

		//int iPenetratePct = (int)(1 + k*4);
		bool bIgnoreDefense = false;

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_PENETRATE))
		{
			int iPenetratePct = (int) m_pkSk->kPointPoly2.Eval();

			if (number(1, 100) <= iPenetratePct)
				bIgnoreDefense = true;
		}

		bool bIgnoreTargetRating = false;

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_IGNORE_TARGET_RATING))
		{
			int iPct = (int) m_pkSk->kPointPoly2.Eval();

			if (number(1, 100) <= iPct)
				bIgnoreTargetRating = true;
		}

		m_pkSk->SetPointVar("ar", CalcAttackRating(chr, chrVictim, bIgnoreTargetRating));

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_USE_MELEE_DAMAGE))
			m_pkSk->SetPointVar("atk", CalcMeleeDamage(chr, chrVictim, true, bIgnoreTargetRating));
		else if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_USE_ARROW_DAMAGE))
		{
			entt::entity pkBow = entt::null, pkArrow = entt::null;

			if (1 == m_pkChr->GetArrowAndBow(&pkBow, &pkArrow, 1))
				m_pkSk->SetPointVar("atk", CalcArrowDamage(chr, chrVictim, pkBow, pkArrow, true));
			else
				m_pkSk->SetPointVar("atk", 0);
		}

		if (m_pkSk->bPointOn == POINT_MOV_SPEED)
			m_pkSk->kPointPoly.SetVar("maxv", pkChrVictim->GetLimitPoint(POINT_MOV_SPEED));

		m_pkSk->SetPointVar("maxhp", ecs::PointSystem::GetMaxHP(victimEntity));
		m_pkSk->SetPointVar("maxsp", ecs::PointSystem::GetMaxSP(victimEntity));

		m_pkSk->SetPointVar("chain", m_pkChr->GetChainLightningIndex());
		m_pkChr->IncChainLightningIndex();

		bool bUnderEunhyung = m_pkChr->GetAffectedEunhyung() > 0; // ŔĚ°Ç żÖ ż©±âĽ­ ÇĎÁö??

		m_pkSk->SetPointVar("ek", m_pkChr->GetAffectedEunhyung()*1./100);
		//m_pkChr->ClearAffectedEunhyung();
		SetPolyVarForAttack(m_character, m_pkSk, m_pkWeapon);

		int iAmount = 0;

		if (m_pkChr->GetUsedSkillMasterType(m_pkSk->dwVnum) >= SKILL_GRAND_MASTER)
		{
			iAmount =
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
			m_pkSk->dwVnum == SKILL_GYEONGGONG ? (int) m_pkSk->kPointPoly2.Eval() : (int) m_pkSk->kMasterBonusPoly.Eval();
#else
			(int) m_pkSk->kMasterBonusPoly.Eval();
#endif
			;
		}
		else
		{
			iAmount =
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
		m_pkSk->dwVnum == SKILL_GYEONGGONG ? (int) m_pkSk->kPointPoly2.Eval() : (int) m_pkSk->kPointPoly.Eval()
#else
		(int) m_pkSk->kPointPoly.Eval()
#endif
			;
		}
		////////////////////////////////////////////////////////////////////////////////
		iAmount =
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
		m_pkSk->dwVnum == SKILL_GYEONGGONG ? iAmount : -iAmount
#else
		-iAmount
#endif
		;

		const entt::entity equippedWeapon = ItemSystem::GetWearItem(
			m_character, WEAR_WEAPON);
		if (m_pkSk->dwVnum == SKILL_AMSEOP)
		{
			float fDelta = GetDegreeDelta(m_pkChr->GetRotation(), pkChrVictim->GetRotation());
			float adjust;

			if (fDelta < 35.0f)
			{
				adjust = 1.5f;

				if (bUnderEunhyung)
					adjust += 0.5f;

				if (ItemSystem::IsValidItem(equippedWeapon) &&
					ItemSystem::GetItemSubType(equippedWeapon) == WEAPON_DAGGER)
				{
					adjust += 0.5f;
				}
			}
			else
			{
				adjust = 1.0f;

				if (bUnderEunhyung)
					adjust += 0.5f;

				if (ItemSystem::IsValidItem(equippedWeapon) &&
					ItemSystem::GetItemSubType(equippedWeapon) == WEAPON_DAGGER)
					adjust += 0.5f;
			}

			iAmount = (int) (iAmount * adjust);
		}
		else if (m_pkSk->dwVnum == SKILL_GUNGSIN)
		{
			float adjust = 1.0;

			if (ItemSystem::IsValidItem(equippedWeapon) &&
				ItemSystem::GetItemSubType(equippedWeapon) == WEAPON_DAGGER)
			{
				adjust = 1.35f;
			}

			iAmount = (int) (iAmount * adjust);
		}
#ifdef ENABLE_WOLFMAN_CHARACTER
		else if (m_pkSk->dwVnum == SKILL_GONGDAB)
		{
			float adjust = 1.0;

			if (ItemSystem::IsValidItem(equippedWeapon) &&
				ItemSystem::GetItemSubType(equippedWeapon) == WEAPON_CLAW)
			{
				adjust = 1.35f;
			}

			iAmount = (int)(iAmount * adjust);
		}
#endif
		////////////////////////////////////////////////////////////////////////////////
		//LOG_INFO(0, "name: %s skill: %s amount %d to %s", ecs::PlayerRuntime::GetName(m_character).data(), m_pkSk->szName, iAmount, ecs::PlayerRuntime::GetName(victimEntity).data());
		iDam = CalcBattleDamage(iAmount, ecs::PointSystem::GetLevel(m_character), ecs::PointSystem::GetLevel(victimEntity));
		if (ecs::PlayerRuntime::IsPC(m_character) && m_pkChr->m_SkillUseInfo[m_pkSk->dwVnum].GetMainTargetVID() != victimEntity)
		{
			// µĄąĚÁö °¨ĽŇ
			iDam = (int) (iDam * m_pkSk->kSplashAroundDamageAdjustPoly.Eval());
		}

#ifdef ENABLE_NEW_GYEONGGONG_SKILL
		if (m_pkSk->dwVnum == SKILL_GYEONGGONG) {
			iDam = iAmount;
		}
#endif

		// TODO ˝şĹłżˇ µű¸Ą µĄąĚÁö Ĺ¸ŔÔ ±â·ĎÇŘľßÇŃ´Ů.
		EDamageType dt = DAMAGE_TYPE_NONE;

		switch (m_pkSk->bSkillAttrType)
		{
			case SKILL_ATTR_TYPE_NORMAL:
				break;

			case SKILL_ATTR_TYPE_MELEE:
				{
					dt = DAMAGE_TYPE_MELEE;

					if (ItemSystem::IsValidItem(equippedWeapon)) {
						switch (ItemSystem::GetItemSubType(equippedWeapon))
						{
							case WEAPON_SWORD:
							{
								int32_t lValue = ecs::PointSystem::Get(victimEntity, POINT_RESIST_SWORD);
#ifdef ENABLE_NEW_BONUS_TALISMAN
								lValue -= ecs::PointSystem::Get(m_character, POINT_ATTBONUS_IRR_SPADA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
								lValue -= ecs::PointSystem::Get(m_character, POINT_IRR_WEAPON_DEFENSE);
#endif
								lValue = lValue < 0 ? 0 :  lValue;
								iDam = iDam * (100 - lValue) / 100;
								break;
							}
							case WEAPON_TWO_HANDED:
							{
								int32_t lValue = ecs::PointSystem::Get(victimEntity, POINT_RESIST_TWOHAND);
#ifdef ENABLE_NEW_BONUS_TALISMAN
								lValue -= ecs::PointSystem::Get(m_character, POINT_ATTBONUS_IRR_SPADONE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
								lValue -= ecs::PointSystem::Get(m_character, POINT_IRR_WEAPON_DEFENSE);
#endif
								lValue = lValue < 0 ? 0 :  lValue;
								iDam = iDam * (100 - lValue) / 100;
								break;
							}
							case WEAPON_DAGGER:
							{
								int32_t lValue = ecs::PointSystem::Get(victimEntity, POINT_RESIST_DAGGER);
#ifdef ENABLE_NEW_BONUS_TALISMAN
								lValue -= ecs::PointSystem::Get(m_character, POINT_ATTBONUS_IRR_PUGNALE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
								lValue -= ecs::PointSystem::Get(m_character, POINT_IRR_WEAPON_DEFENSE);
#endif
								lValue = lValue < 0 ? 0 :  lValue;
								iDam = iDam * (100 - lValue) / 100;
								break;
							}
							case WEAPON_BELL:
							{
								int32_t lValue = ecs::PointSystem::Get(victimEntity, POINT_RESIST_BELL);
#ifdef ENABLE_NEW_BONUS_TALISMAN
								lValue -= ecs::PointSystem::Get(m_character, POINT_ATTBONUS_IRR_CAMPANA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
								lValue -= ecs::PointSystem::Get(m_character, POINT_IRR_WEAPON_DEFENSE);
#endif
								lValue = lValue < 0 ? 0 :  lValue;
								iDam = iDam * (100 - lValue) / 100;
								break;
							}
							case WEAPON_FAN:
							{
								int32_t lValue = ecs::PointSystem::Get(victimEntity, POINT_RESIST_FAN);
#ifdef ENABLE_NEW_BONUS_TALISMAN
								lValue -= ecs::PointSystem::Get(m_character, POINT_ATTBONUS_IRR_VENTAGLIO);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
								lValue -= ecs::PointSystem::Get(m_character, POINT_IRR_WEAPON_DEFENSE);
#endif
								lValue = lValue < 0 ? 0 :  lValue;
								iDam = iDam * (100 - lValue) / 100;
								break;
							}
							case WEAPON_BOW:
							{
								int32_t lValue = ecs::PointSystem::Get(victimEntity, POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
								lValue -= ecs::PointSystem::Get(m_character, POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
								lValue -= ecs::PointSystem::Get(m_character, POINT_IRR_WEAPON_DEFENSE);
#endif
								lValue = lValue < 0 ? 0 :  lValue;
								iDam = iDam * (100 - lValue) / 100;
								break;
							}
#ifdef ENABLE_WOLFMAN_CHARACTER
							case WEAPON_CLAW:
							{
								int32_t lValue = ecs::PointSystem::Get(victimEntity, POINT_RESIST_DAGGER);
#ifdef ENABLE_NEW_BONUS_TALISMAN
								lValue -= ecs::PointSystem::Get(m_character, POINT_ATTBONUS_IRR_PUGNALE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
								lValue -= ecs::PointSystem::Get(m_character, POINT_IRR_WEAPON_DEFENSE);
#endif
								lValue = lValue < 0 ? 0 :  lValue;
								iDam = iDam * (100 - lValue) / 100;
								break;
							}
#endif
							default:
								break;
						}
					}

					if (!bIgnoreDefense) {
						iDam -= ecs::PointSystem::Get(victimEntity, POINT_DEF_GRADE);
					}

					break;
				}
			case SKILL_ATTR_TYPE_RANGE: {
				dt = DAMAGE_TYPE_RANGE;
				int32_t lValue = ecs::PointSystem::Get(victimEntity, POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= ecs::PointSystem::Get(m_character, POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= ecs::PointSystem::Get(m_character, POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 : lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}

			case SKILL_ATTR_TYPE_MAGIC:
				dt = DAMAGE_TYPE_MAGIC;
				iDam = CalcAttBonus(chr, chrVictim, iDam);
				// Ŕ¸ľĆľĆľĆľÇ
				// żąŔüżˇ ŔűżëľČÇß´ř ąö±×°ˇ ŔÖľîĽ­ ąćľî·Â °č»ęŔ» ´Ů˝ĂÇĎ¸é ŔŻŔú°ˇ ł­¸®ł˛
				//iDam -= ecs::PointSystem::Get(victimEntity, POINT_MAGIC_DEF_GRADE);
//#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
//				{
//					const int resist_magic = MINMAX(0, ecs::PointSystem::Get(victimEntity, POINT_RESIST_MAGIC), 100);
//					const int resist_magic_reduction = MINMAX(0, (m_pkChr->GetJob()==JOB_SURA) ? ecs::PointSystem::Get(m_character, POINT_RESIST_MAGIC_REDUCTION)/2 : ecs::PointSystem::Get(m_character, POINT_RESIST_MAGIC_REDUCTION), 50);
//					const int total_res_magic = MINMAX(0, resist_magic - resist_magic_reduction, 100);
//					iDam = iDam * (100 - total_res_magic) / 100;
//				}
//#else
				iDam = iDam * (100 - (int)(ecs::PointSystem::Get(victimEntity, POINT_RESIST_MAGIC) / 2)) / 100;
//#endif
				break;

			default:
				LOG_ERROR("Unknown skill attr type {} vnum {}", m_pkSk->bSkillAttrType, m_pkSk->dwVnum);
				break;
		}

		//
		// 20091109 µ¶ŔĎ ˝şĹł ĽÓĽş żäĂ» ŔŰľ÷
		// ±âÁ¸ ˝şĹł Ĺ×ŔĚşíżˇ SKILL_FLAG_WIND, SKILL_FLAG_ELEC, SKILL_FLAG_FIRE¸¦ °ˇÁř ˝şĹłŔĚ
		// ŔüÇô ľřľúŔ¸ąÇ·Î ¸ó˝şĹÍŔÇ RESIST_WIND, RESIST_ELEC, RESIST_FIREµµ »çżëµÇÁö ľĘ°í ŔÖľú´Ů.
		//
		// PvPżÍ PvEąë·±˝ş şĐ¸®¸¦ Ŕ§ÇŘ ŔÇµµŔűŔ¸·Î NPC¸¸ ŔűżëÇĎµµ·Ď ÇßŔ¸¸ç ±âÁ¸ ąë·±˝şżÍ Â÷ŔĚÁˇŔ»
		// ´Ŕł˘Áö ¸řÇĎ±â Ŕ§ÇŘ mob_protoŔÇ RESIST_MAGICŔ» RESIST_WIND, RESIST_ELEC, RESIST_FIRE·Î
		// şą»çÇĎż´´Ů.
		//
		if (ecs::PlayerRuntime::IsNPC(victimEntity))
		{
			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_WIND))
			{
				iDam = iDam * (100 - ecs::PointSystem::Get(victimEntity, POINT_RESIST_WIND)) / 100;
			}

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_ELEC))
			{
				iDam = iDam * (100 - ecs::PointSystem::Get(victimEntity, POINT_RESIST_ELEC)) / 100;
			}

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_FIRE))
			{
				iDam = iDam * (100 - ecs::PointSystem::Get(victimEntity, POINT_RESIST_FIRE)) / 100;
			}
		}

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_COMPUTE_MAGIC_DAMAGE))
			dt = DAMAGE_TYPE_MAGIC;

		if (pkChrVictim->CanBeginFight())
			pkChrVictim->BeginFight((m_pkChr ? m_pkChr->GetEntityHandle() : entt::null));

		if (m_pkSk->dwVnum == SKILL_CHAIN)
			LOG_INFO("{} CHAIN INDEX {} DAM {} DT {}", ecs::PlayerRuntime::GetName(m_character).data(), m_pkChr->GetChainLightningIndex() - 1, iDam, static_cast<int>(dt));

#ifdef ENABLE_NEW_PASSIVE_SKILLS
		{
			uint8_t HELP_SKILL_ID = 0;
			switch (m_pkSk->dwVnum)
			{
				case SKILL_PALBANG:
					HELP_SKILL_ID = SKILL_HELP_PALBANG;
					break;
				case SKILL_AMSEOP:
					HELP_SKILL_ID = SKILL_HELP_AMSEOP;
					break;
				case SKILL_SWAERYUNG:
					HELP_SKILL_ID = SKILL_HELP_SWAERYUNG;
					break;
				case SKILL_YONGBI:
					HELP_SKILL_ID = SKILL_HELP_YONGBI;
					break;
				case SKILL_GIGONGCHAM:
					HELP_SKILL_ID = SKILL_HELP_GIGONGCHAM;
					break;
				case SKILL_HWAJO:
					HELP_SKILL_ID = SKILL_HELP_HWAJO;
					break;
				case SKILL_MARYUNG:
					HELP_SKILL_ID = SKILL_HELP_MARYUNG;
					break;
				case SKILL_BYEURAK:
					HELP_SKILL_ID = SKILL_HELP_BYEURAK;
					break;
				default:
					break;
			}

			if (HELP_SKILL_ID != 0)
			{
				uint8_t HELP_SKILL_LV = m_pkChr->GetSkillLevel(HELP_SKILL_ID);
				if (HELP_SKILL_LV != 0)
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(HELP_SKILL_ID);
					if (!pkSk)
						LOG_ERROR("Can't find {} skill in skill_proto.", HELP_SKILL_ID);
					else
					{
						pkSk->SetPointVar("k", 1.0f * m_pkChr->GetSkillPower(HELP_SKILL_ID) * pkSk->bMaxLevel / 100);

						double IncreaseAmount = pkSk->kPointPoly.Eval();
						LOG_INFO("HELP_SKILL: increase amount: {}, normal damage: {}, increased damage: {}.", IncreaseAmount, iDam, int(iDam * (IncreaseAmount / 100.0)));
						iDam += iDam * (IncreaseAmount / 100.0);
					}
				}
			}
		}

		{
			uint8_t ANTI_SKILL_ID = 0;
			switch (m_pkSk->dwVnum)
			{
				case SKILL_PALBANG:
					ANTI_SKILL_ID = SKILL_ANTI_PALBANG;
					break;
				case SKILL_AMSEOP:
					ANTI_SKILL_ID = SKILL_ANTI_AMSEOP;
					break;
				case SKILL_SWAERYUNG:
					ANTI_SKILL_ID = SKILL_ANTI_SWAERYUNG;
					break;
				case SKILL_YONGBI:
					ANTI_SKILL_ID = SKILL_ANTI_YONGBI;
					break;
				case SKILL_GIGONGCHAM:
					ANTI_SKILL_ID = SKILL_ANTI_GIGONGCHAM;
					break;
				case SKILL_HWAJO:
					ANTI_SKILL_ID = SKILL_ANTI_HWAJO;
					break;
				case SKILL_MARYUNG:
					ANTI_SKILL_ID = SKILL_ANTI_MARYUNG;
					break;
				case SKILL_BYEURAK:
					ANTI_SKILL_ID = SKILL_ANTI_BYEURAK;
					break;
				default:
					break;
			}

			if (ANTI_SKILL_ID != 0)
			{
				uint8_t ANTI_SKILL_LV = pkChrVictim->GetSkillLevel(ANTI_SKILL_ID);
				if (ANTI_SKILL_LV != 0)
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(ANTI_SKILL_ID);
					if (!pkSk)
						LOG_ERROR("Can't find {} skill in skill_proto.", ANTI_SKILL_ID);
					else
					{
						pkSk->SetPointVar("k", 1.0f * pkChrVictim->GetSkillPower(ANTI_SKILL_ID) * pkSk->bMaxLevel / 100);

						double ResistAmount = pkSk->kPointPoly.Eval();
						LOG_INFO("ANTI_SKILL: resist amount: {}, normal damage: {}, reduced damage: {}.", ResistAmount, iDam, int(iDam * (ResistAmount/100.0)));
						iDam -= iDam * (ResistAmount / 100.0);
					}
				}
			}
		}
#endif

		{
			uint8_t AntiSkillID = 0;

			switch (m_pkSk->dwVnum)
			{
				case SKILL_TANHWAN:		AntiSkillID = SKILL_7_A_ANTI_TANHWAN;		break;
				case SKILL_AMSEOP:		AntiSkillID = SKILL_7_B_ANTI_AMSEOP;		break;
				case SKILL_SWAERYUNG:	AntiSkillID = SKILL_7_C_ANTI_SWAERYUNG;		break;
				case SKILL_YONGBI:		AntiSkillID = SKILL_7_D_ANTI_YONGBI;		break;
				case SKILL_GIGONGCHAM:	AntiSkillID = SKILL_8_A_ANTI_GIGONGCHAM;	break;
				case SKILL_YEONSA:		AntiSkillID = SKILL_8_B_ANTI_YEONSA;		break;
				case SKILL_MAHWAN:		AntiSkillID = SKILL_8_C_ANTI_MAHWAN;		break;
				case SKILL_BYEURAK:		AntiSkillID = SKILL_8_D_ANTI_BYEURAK;		break;
			}

			if (0 != AntiSkillID)
			{
				uint8_t AntiSkillLevel = pkChrVictim->GetSkillLevel(AntiSkillID);

				if (0 != AntiSkillLevel)
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(AntiSkillID);
					if (!pkSk)
					{
						LOG_ERROR("There is no anti skill({}) in skill proto", AntiSkillID);
					}
					else
					{
						pkSk->SetPointVar("k", 1.0f * pkChrVictim->GetSkillPower(AntiSkillID) * pkSk->bMaxLevel / 100);

						double ResistAmount = pkSk->kPointPoly.Eval();

						LOG_INFO("ANTI_SKILL: Resist({}) Orig({}) Reduce({})", ResistAmount, iDam, int(iDam * (ResistAmount/100.0)));

						iDam -= iDam * (ResistAmount/100.0);
					}
				}
			}
		}

#ifdef ENABLE_SOUL_SYSTEM
		iDam += m_pkChr->GetSoulItemDamage((pkChrVictim ? pkChrVictim->GetEntityHandle() : entt::null), iDam, BLUE_SOUL);
#endif



		if (!pkChrVictim->Damage(m_character, iDam, dt) && !pkChrVictim->IsStun())
		{

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_REMOVE_GOOD_AFFECT))
			{
#ifdef ENABLE_NULLIFYAFFECT_LIMIT
				int iLevel = ecs::PointSystem::GetLevel(m_character);
				int yLevel = ecs::PointSystem::GetLevel(victimEntity);
				// const float k = 1.0 * m_pkChr->GetSkillPower(m_pkSk->dwVnum, bSkillLevel) * m_pkSk->bMaxLevel / 100;
				int iDifLev = 9;
				if ((iLevel-iDifLev <= yLevel) && (iLevel+iDifLev >= yLevel))
#endif
				{
					int iAmount2 = (int) m_pkSk->kPointPoly2.Eval();
					int iDur2 = (int) m_pkSk->kDurationPoly2.Eval();
					iDur2 += ecs::PointSystem::Get(m_character, POINT_PARTY_BUFFER_BONUS);

					if (number(1, 100) <= iAmount2)
					{
						pkChrVictim->RemoveGoodAffect();
						AffectSystem::AddAffect(victimEntity, m_pkSk->dwVnum, POINT_NONE, 0, AFF_PABEOP, iDur2, 0, true);
					}
				}
			}
#ifdef ENABLE_WOLFMAN_CHARACTER
			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_SLOW | SKILL_FLAG_STUN | SKILL_FLAG_FIRE_CONT | SKILL_FLAG_POISON | SKILL_FLAG_BLEEDING))
#else
			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_SLOW | SKILL_FLAG_STUN | SKILL_FLAG_FIRE_CONT | SKILL_FLAG_POISON))
#endif
			{
				int iPct = (int) m_pkSk->kPointPoly2.Eval();
				int iDur = (int) m_pkSk->kDurationPoly2.Eval();

				iDur += ecs::PointSystem::Get(m_character, POINT_PARTY_BUFFER_BONUS);

				if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_STUN))
				{
					SkillAttackAffect(chrVictim, iPct, IMMUNE_STUN, AFFECT_STUN, POINT_NONE, 0, AFF_STUN, iDur, m_pkSk->szName);
				}
				else if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_SLOW))
				{
					SkillAttackAffect(chrVictim, iPct, IMMUNE_SLOW, AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, iDur, m_pkSk->szName);
				}
				else if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_FIRE_CONT))
				{
					m_pkSk->SetDurationVar("k", 1.0 * m_bUseSkillPower * m_pkSk->bMaxLevel / 100);
					m_pkSk->SetDurationVar("iq", ecs::PointSystem::Get(m_character, POINT_IQ));

					iDur = (int)m_pkSk->kDurationPoly2.Eval();
					int bonus = ecs::PointSystem::Get(m_character, POINT_PARTY_BUFFER_BONUS);

					if (bonus != 0)
					{
						iDur += bonus / 2;
					}

					if (number(1, 100) <= iDur)
					{
						pkChrVictim->AttackedByFire((m_pkChr ? m_pkChr->GetEntityHandle() : entt::null), iPct, 5);
					}
				}
				else if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_POISON))
				{
					if (number(1, 100) <= iPct)
						pkChrVictim->AttackedByPoison((m_pkChr ? m_pkChr->GetEntityHandle() : entt::null));
				}
#ifdef ENABLE_WOLFMAN_CHARACTER
				else if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_BLEEDING))
				{
					if (number(1, 100) <= iPct)
						pkChrVictim->AttackedByBleeding((m_pkChr ? m_pkChr->GetEntityHandle() : entt::null));
				}
#endif
			}

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_CRUSH | SKILL_FLAG_CRUSH_LONG) &&
				!IS_SET(pkChrVictim->GetAIFlag(), AIFLAG_NOMOVE))
			{
				float fCrushSlidingLength = 200;

				if (ecs::PlayerRuntime::IsNPC(m_character))
					fCrushSlidingLength = 400;

				if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_CRUSH_LONG))
					fCrushSlidingLength *= 2;

				float fx, fy;
				float degree = GetDegreeFromPositionXY(ecs::PlayerRuntime::GetX(m_character), ecs::PlayerRuntime::GetY(m_character), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity));

				if (m_pkSk->dwVnum == SKILL_HORSE_WILDATTACK)
				{
					degree -= m_pkChr->GetRotation();
					degree = fmod(degree, 360.0f) - 180.0f;

					if (degree > 0)
						degree = m_pkChr->GetRotation() + 90.0f;
					else
						degree = m_pkChr->GetRotation() - 90.0f;
				}

				GetDeltaByDegree(degree, fCrushSlidingLength, &fx, &fy);
				LOG_INFO("CRUSH! {} -> {} ({} {}) -> ({} {})", ecs::PlayerRuntime::GetName(m_character).data(), ecs::PlayerRuntime::GetName(victimEntity).data(), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), ecs::PlayerRuntime::GetX(victimEntity) + static_cast<int32_t>(fx), ecs::PlayerRuntime::GetY(victimEntity) + static_cast<int32_t>(fy));
				int32_t tx = ecs::PlayerRuntime::GetX(victimEntity)+static_cast<int32_t>(fx);
				int32_t ty = ecs::PlayerRuntime::GetY(victimEntity)+static_cast<int32_t>(fy);

#ifdef ENABLE_BUG_FIXES
				while (ecs::PlayerRuntime::GetSectree(victimEntity)->GetAttribute(tx, ty) & (ATTR_BLOCK | ATTR_OBJECT) && fCrushSlidingLength > 0) {
					if (fCrushSlidingLength >= 10) {
						fCrushSlidingLength -= 10;
					} else {
						fCrushSlidingLength = 0;
					}

					GetDeltaByDegree(degree, fCrushSlidingLength, &fx, &fy);
					tx = ecs::PlayerRuntime::GetX(victimEntity) + static_cast<int32_t>(fx);
					ty = ecs::PlayerRuntime::GetY(victimEntity) + static_cast<int32_t>(fy);
				}
#endif

				pkChrVictim->Sync(tx, ty);
				pkChrVictim->Goto(tx, ty);
				pkChrVictim->CalculateMoveDuration();

				if (ecs::PlayerRuntime::IsPC(m_character) && m_pkChr->m_SkillUseInfo[m_pkSk->dwVnum].GetMainTargetVID() == victimEntity)
				{
					SkillAttackAffect(chrVictim, 1000, IMMUNE_STUN, m_pkSk->dwVnum, POINT_NONE, 0, AFF_STUN, 4, m_pkSk->szName);
				}
				else
				{
					NetworkSyncSystem::BroadcastSyncPacket(g_registry, victimEntity);
				}
			}
		}

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_HP_ABSORB))
		{
			int iPct = (int) m_pkSk->kPointPoly2.Eval();
			ecs::PointSystem::Change(m_character, POINT_HP, iDam * iPct / 100);
		}

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_SP_ABSORB))
		{
			int iPct = (int) m_pkSk->kPointPoly2.Eval();
			ecs::PointSystem::Change(m_character, POINT_SP, iDam * iPct / 100);
		}

		if (m_pkSk->dwVnum == SKILL_CHAIN && m_pkChr->GetChainLightningIndex() < m_pkChr->GetChainLightningMaxCount())
		{
			chain_lightning_event_info* info = AllocEventInfo<chain_lightning_event_info>();

			info->dwVictim = victimEntity;
			info->dwChr = m_character;

			event_create(ChainLightningEvent, info, passes_per_sec / 5);
		}
		if(test_server)
			LOG_INFO("FuncSplashDamage End :{} ", ecs::PlayerRuntime::GetName(m_character).data());
//#ifdef ENABLE_MAP1_SKILL_MOB
//		// csak PC -> 136-os mob esetén mentsünk
//		if (ecs::PlayerRuntime::IsPC(m_character) && pkChrVictim->IsMonster() && ecs::PlayerRuntime::GetRaceNum(victimEntity) == 136)
//		{
//
//			DBManager::instance().DirectQuery(
//				"UPDATE player.player "
//				"SET map1_skillmob = GREATEST(map1_skillmob, %d) "
//				"WHERE id=%u",
//				iAmount, ecs::PlayerRuntime::GetPlayerID(m_character));
//
//			// (opcionális) debug üzenet a játékosnak
//			ecs::ChatSystem::Send(m_character, CHAT_TYPE_INFO, "SkillMob DAMAGE: %d (max mentve).", iAmount);
//		}
//
//#endif
	}

	int		m_x;
	int		m_y;
	CSkillProto * m_pkSk;
	LegacyCharHandle	m_pkChr;
	entt::entity m_character;
	int		m_iAmount;
	int		m_iAG;
	int		m_iCount;
	int		m_iMaxHit;
	entt::entity	m_pkWeapon;
	bool m_bDisableCooltime;
	TSkillUseInfo* m_pInfo;
	uint8_t m_bUseSkillPower;


};

struct FuncSplashAffect
{
	FuncSplashAffect(entt::entity ch, int x, int y, int iDist, uint32_t dwVnum, uint8_t bPointOn, int iAmount, uint32_t dwAffectFlag, int iDuration, int iSPCost, bool bOverride, int iMaxHit)
	{
		m_x = x;
		m_y = y;
		m_iDist = iDist;
		m_dwVnum = dwVnum;
		m_bPointOn = bPointOn;
		m_iAmount = iAmount;
		m_dwAffectFlag = dwAffectFlag;
		m_iDuration = iDuration;
		m_iSPCost = iSPCost;
		m_bOverride = bOverride;
		m_attacker = ch;
		m_iMaxHit = iMaxHit;
		m_iCount = 0;
	}

	void operator () (LPENTITY ent)
	{
		if (m_iMaxHit && m_iMaxHit <= m_iCount)
			return;

		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* pkChr = static_cast<LegacyCharHandle>(ent);
			const entt::entity target = pkChr->GetEntityHandle();

			if (test_server)
				LOG_INFO("FuncSplashAffect step 1 : name:{} vnum:{} iDur:{}", ecs::PlayerRuntime::GetName(target).data(), m_dwVnum, m_iDuration);
			if (DISTANCE_APPROX(m_x - ecs::PlayerRuntime::GetX(target), m_y - ecs::PlayerRuntime::GetY(target)) < m_iDist)
			{
				if (test_server)
					LOG_INFO("FuncSplashAffect step 2 : name:{} vnum:{} iDur:{}", ecs::PlayerRuntime::GetName(target).data(), m_dwVnum, m_iDuration);
				if (m_dwVnum == SKILL_TUSOK)
					if (pkChr->CanBeginFight())
						pkChr->BeginFight(m_attacker);

				if (ecs::PlayerRuntime::IsPC(target) && m_dwVnum == SKILL_TUSOK)
					AffectSystem::AddAffect(target, m_dwVnum, m_bPointOn, m_iAmount, m_dwAffectFlag, m_iDuration/3, m_iSPCost, m_bOverride);
				else
					AffectSystem::AddAffect(target, m_dwVnum, m_bPointOn, m_iAmount, m_dwAffectFlag, m_iDuration, m_iSPCost, m_bOverride);

				m_iCount ++;
			}
		}
	}

	entt::entity m_attacker;
	int		m_x;
	int		m_y;
	int		m_iDist;
	uint32_t	m_dwVnum;
	uint8_t	m_bPointOn;
	int		m_iAmount;
	uint32_t	m_dwAffectFlag;
	int		m_iDuration;
	int		m_iSPCost;
	bool	m_bOverride;
	int         m_iMaxHit;
	int         m_iCount;
};

EVENTINFO(skill_gwihwan_info)
{
	uint32_t pid;
	uint8_t bsklv;

	skill_gwihwan_info()
	: pid( 0 )
	, bsklv( 0 )
	{
	}
};

EVENTFUNC(skill_gwihwan_event)
{
	skill_gwihwan_info* info = dynamic_cast<skill_gwihwan_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("skill_gwihwan_event> <Factor> Null pointer");
		return 0;
	}

	uint32_t pid = info->pid;
	uint8_t sklv= info->bsklv;
	auto* ch = CHARACTER_MANAGER::instance().FindByPID(pid);

	if (!ch)
		return 0;
	const entt::entity character = ch->GetEntityHandle();

	int percent = 20 * sklv - 1;

	if (number(1, 100) <= percent)
	{
		PIXEL_POSITION pos;

		// Ľş°ř
		if (ecs::GetRecallPosition(ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetEmpire(character), pos))
		{
			LOG_INFO("Recall: {} {} {} -> {} {}", ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), pos.x, pos.y);
			ch->WarpSet(pos.x, pos.y);
		}
		else
		{
			LOG_ERROR("CHARACTER::UseItem : cannot find spawn position (name {}, {} x {})", ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character));
			ch->WarpSet(EMPIRE_START_X(ecs::PlayerRuntime::GetEmpire(character)), EMPIRE_START_Y(ecs::PlayerRuntime::GetEmpire(character)));
		}
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 241, "");
	}
#endif
	return 0;
}

int CHARACTER::ComputeSkillAtPosition(uint32_t dwVnum, const PIXEL_POSITION& posTarget, uint8_t bSkillLevel)
{
	if (GetMountVnum())
		return BATTLE_NONE;

	if (IsPolymorphed())
		return BATTLE_NONE;

	if (g_bSkillDisable)
		return BATTLE_NONE;

	CSkillProto * pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
		return BATTLE_NONE;

	if (test_server)
	{
		LOG_INFO("ComputeSkillAtPosition {} vnum {} x {} y {} level {}", GetName(), dwVnum, posTarget.x, posTarget.y, bSkillLevel);
	}

	// łŞżˇ°Ô ľ˛´Â ˝şĹłŔş ł» Ŕ§Äˇ¸¦ ľ´´Ů.
	//if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
	//	posTarget = GetXYZ();

	// ˝şÇĂ·ˇ˝¬°ˇ ľĆ´Ń ˝şĹłŔş ÁÖŔ§ŔĚ¸é ŔĚ»óÇĎ´Ů
	if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
		return BATTLE_NONE;

	if (0 == bSkillLevel)
	{
		if ((bSkillLevel = GetSkillLevel(pkSk->dwVnum)) == 0)
		{
			return BATTLE_NONE;
		}
	}

	const float k = 1.0 * GetSkillPower(pkSk->dwVnum, bSkillLevel) * pkSk->bMaxLevel / 100;

	pkSk->SetPointVar("k", k);
	pkSk->kSplashAroundDamageAdjustPoly.SetVar("k", k);

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_MELEE_DAMAGE))
	{
		pkSk->SetPointVar("atk", CalcMeleeDamage(GetEntityHandle(), GetEntityHandle(), true, false));
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_MAGIC_DAMAGE))
	{
		pkSk->SetPointVar("atk", CalcMagicDamage(GetEntityHandle(), GetEntityHandle()));
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_ARROW_DAMAGE))
	{
		entt::entity pkBow = entt::null, pkArrow = entt::null;
		if (1 == GetArrowAndBow(&pkBow, &pkArrow, 1))
		{
			pkSk->SetPointVar("atk", CalcArrowDamage(GetEntityHandle(), GetEntityHandle(), pkBow, pkArrow, true));
		}
		else
		{
			pkSk->SetPointVar("atk", 0);
		}
	}

	if (pkSk->bPointOn == POINT_MOV_SPEED)
	{
		pkSk->SetPointVar("maxv", this->GetLimitPoint(POINT_MOV_SPEED));
	}

	pkSk->SetPointVar("lv", GetLevel());
	pkSk->SetPointVar("iq", GetPoint(POINT_IQ));
	pkSk->SetPointVar("str", GetPoint(POINT_ST));
	pkSk->SetPointVar("dex", GetPoint(POINT_DX));
	pkSk->SetPointVar("con", GetPoint(POINT_HT));
	pkSk->SetPointVar("maxhp", ecs::PointSystem::GetMaxHP(GetEntityHandle()));
	pkSk->SetPointVar("maxsp", ecs::PointSystem::GetMaxSP(GetEntityHandle()));
	pkSk->SetPointVar("chain", 0);
	pkSk->SetPointVar("ar", CalcAttackRating(GetEntityHandle(), GetEntityHandle()));
	pkSk->SetPointVar("def", GetPoint(POINT_DEF_GRADE));
	pkSk->SetPointVar("odef", GetPoint(POINT_DEF_GRADE) - GetPoint(POINT_DEF_GRADE_BONUS));
	pkSk->SetPointVar("horse_level", GetHorseLevel());

	if (pkSk->bSkillAttrType != SKILL_ATTR_TYPE_NORMAL)
		OnMove(true);

	entt::entity pkWeapon = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_WEAPON);

	SetPolyVarForAttack(GetEntityHandle(), pkSk, pkWeapon);

	pkSk->SetDurationVar("k", k/*bSkillLevel*/);

	int iAmount = (int) pkSk->kPointPoly.Eval();
	int iAmount2 = (int) pkSk->kPointPoly2.Eval();

	// ADD_GRANDMASTER_SKILL
	int iAmount3 = (int) pkSk->kPointPoly3.Eval();

	if (GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER)
	{
		/*
		   if (iAmount >= 0)
		   iAmount += (int) m_pkSk->kMasterBonusPoly.Eval();
		   else
		   iAmount -= (int) m_pkSk->kMasterBonusPoly.Eval();
		 */
		iAmount = (int) pkSk->kMasterBonusPoly.Eval();
	}

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_REMOVE_BAD_AFFECT))
	{
		if (number(1, 100) <= iAmount2)
		{
			RemoveBadAffect();
		}
	}
	// END_OF_ADD_GRANDMASTER_SKILL

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_ATTACK | SKILL_FLAG_USE_MELEE_DAMAGE | SKILL_FLAG_USE_MAGIC_DAMAGE))
	{
		//
		// °ř°Ý ˝şĹłŔĎ °ćżě
		//
		bool bAdded = false;

		if (pkSk->bPointOn == POINT_HP && iAmount < 0)
		{
			int iAG = 0;

			FuncSplashDamage f(posTarget.x, posTarget.y, pkSk, this, iAmount, iAG, pkSk->lMaxHit, pkWeapon, m_bDisableCooltime, IsPC()?&m_SkillUseInfo[dwVnum]: nullptr, GetSkillPower(dwVnum, bSkillLevel));

			if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
			{
				if (GetSectree())
					GetSectree()->ForEachAround(f);
			}
			else
			{
				//if (dwVnum == SKILL_CHAIN) LOG_INFO(0, "CHAIN skill call FuncSplashDamage %s", GetName());
				f(this);
			}
		}
		else
		{
			//if (dwVnum == SKILL_CHAIN) LOG_INFO(0, "CHAIN skill no damage %d %s", iAmount, GetName());
			int iDur = (int) pkSk->kDurationPoly.Eval();

			if (IsPC())
				if (!(dwVnum >= GUILD_SKILL_START && dwVnum <= GUILD_SKILL_END)) // ±ćµĺ ˝şĹłŔş ÄđĹ¸ŔÓ Ăł¸®¸¦ ÇĎÁö ľĘ´Â´Ů.
					if (!m_bDisableCooltime && !m_SkillUseInfo[dwVnum].HitOnce(dwVnum) && dwVnum != SKILL_MUYEONG)
					{
						//if (dwVnum == SKILL_CHAIN) LOG_INFO(0, "CHAIN skill cannot hit %s", GetName());
						return BATTLE_NONE;
					}


			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AddAffect(pkSk->dwVnum, pkSk->bPointOn, iAmount, pkSk->dwAffectFlag, iDur, 0, true);
				else
				{
					if (GetSectree())
					{
						FuncSplashAffect f(GetEntityHandle(), posTarget.x, posTarget.y, pkSk->iSplashRange, pkSk->dwVnum, pkSk->bPointOn, iAmount, pkSk->dwAffectFlag, iDur, 0, true, pkSk->lMaxHit);
						GetSectree()->ForEachAround(f);
					}
				}
				bAdded = true;
			}
		}

		if (pkSk->bPointOn2 != POINT_NONE)
		{
			int iDur = (int) pkSk->kDurationPoly2.Eval();

			LOG_INFO("try second {} {} {}", pkSk->dwVnum, pkSk->bPointOn2, iDur);

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AddAffect(pkSk->dwVnum, pkSk->bPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded);
				else
				{
					if (GetSectree())
					{
						FuncSplashAffect f(GetEntityHandle(), posTarget.x, posTarget.y, pkSk->iSplashRange, pkSk->dwVnum, pkSk->bPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded, pkSk->lMaxHit);
						GetSectree()->ForEachAround(f);
					}
				}
				bAdded = true;
			}
			else
			{
				PointChange(pkSk->bPointOn2, iAmount2);
			}
		}

		// ADD_GRANDMASTER_SKILL
		if (GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER && pkSk->bPointOn3 != POINT_NONE)
		{
			int iDur = (int) pkSk->kDurationPoly3.Eval();

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AddAffect(pkSk->dwVnum, pkSk->bPointOn3, iAmount3, 0 /*pkSk->dwAffectFlag3*/, iDur, 0, !bAdded);
				else
				{
					if (GetSectree())
					{
						FuncSplashAffect f(GetEntityHandle(), posTarget.x, posTarget.y, pkSk->iSplashRange, pkSk->dwVnum, pkSk->bPointOn3, iAmount3, 0 /*pkSk->dwAffectFlag3*/, iDur, 0, !bAdded, pkSk->lMaxHit);
						GetSectree()->ForEachAround(f);
					}
				}
			}
			else
			{
				PointChange(pkSk->bPointOn3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_DAMAGE;
	}
	else
	{
		bool bAdded = false;
		int iDur = (int) pkSk->kDurationPoly.Eval();

		if (iDur > 0)
		{
			iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);
			// AffectFlag°ˇ ľř°ĹłŞ, toggle ÇĎ´Â °ÍŔĚ ľĆ´Ď¶ó¸é..
			pkSk->kDurationSPCostPoly.SetVar("k", k/*bSkillLevel*/);

			AddAffect(pkSk->dwVnum,
					  pkSk->bPointOn,
					  iAmount,
					  pkSk->dwAffectFlag,
					  iDur,
					  (int32_t) pkSk->kDurationSPCostPoly.Eval(),
					  !bAdded);

			bAdded = true;
		}
		else
		{
			PointChange(pkSk->bPointOn, iAmount);
		}

		if (pkSk->bPointOn2 != POINT_NONE)
		{
			int iDur = (int) pkSk->kDurationPoly2.Eval();

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);
				AddAffect(pkSk->dwVnum, pkSk->bPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded);
				bAdded = true;
			}
			else
			{
				PointChange(pkSk->bPointOn2, iAmount2);
			}
		}

		// ADD_GRANDMASTER_SKILL
		if (GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER && pkSk->bPointOn3 != POINT_NONE)
		{
			int iDur = (int) pkSk->kDurationPoly3.Eval();

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);
				AddAffect(pkSk->dwVnum, pkSk->bPointOn3, iAmount3, 0 /*pkSk->dwAffectFlag3*/, iDur, 0, !bAdded);
			}
			else
			{
				PointChange(pkSk->bPointOn3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_NONE;
	}
}

#ifdef GROUP_BUFF
struct FComputeSkillParty
{
	FComputeSkillParty(uint32_t dwVnum, LegacyCharHandle pkAttacker, uint8_t bSkillLevel = 0)
		: m_dwVnum(dwVnum), m_pkAttacker(pkAttacker), m_bSkillLevel(bSkillLevel)
		{
		}

	void operator () (LegacyCharHandle ch)
	{
		m_pkAttacker->ComputeSkill(m_dwVnum, (ch ? ch->GetEntityHandle() : entt::null), m_bSkillLevel);
	}

	uint32_t m_dwVnum;
	LegacyCharHandle m_pkAttacker;
	uint8_t m_bSkillLevel;
};

int CHARACTER::ComputeSkillParty(uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel)
{
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	FComputeSkillParty f(dwVnum, pkVictim, bSkillLevel);
	if (GetParty() && GetParty()->GetNearMemberCount())
		GetParty()->ForEachNearMember(f);
	else
		f(this);

	return BATTLE_NONE;
}
#endif

#ifdef ENABLE_NEW_GYEONGGONG_SKILL
int CHARACTER::ComputeGyeongGongSkill(uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel)
{
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	if (IsPolymorphed())
		return BATTLE_NONE;

	if (g_bSkillDisable)
		return BATTLE_NONE;

	CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
		return BATTLE_NONE;

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
		pkVictim = this;

	if (!pkVictim)
	{
		if (test_server)
			LOG_INFO("ComputeGyeongGongSkill: {} Victim == null, skill {}", GetName(), dwVnum);

		return BATTLE_NONE;
	}
	const entt::entity victimEntity = pkVictim->GetEntityHandle();

	if (0 == bSkillLevel)
	{
		if ((bSkillLevel = GetSkillLevel(pkSk->dwVnum)) == 0)
		{
			if (test_server)
				LOG_INFO("ComputeGyeongGongSkill: name:{} vnum:{}  skillLevelBySkill : {} ", GetName(), pkSk->dwVnum, bSkillLevel);
			return BATTLE_NONE;
		}
	}

	const float k = 1.0 * GetSkillPower(pkSk->dwVnum, bSkillLevel) * pkSk->bMaxLevel / 100;
	pkSk->SetPointVar("k", k);
	pkSk->kSplashAroundDamageAdjustPoly.SetVar("k", k);
	entt::entity pkBow = entt::null, pkArrow = entt::null;

	if (1 == GetArrowAndBow(&pkBow, &pkArrow, 1)) {
		pkSk->SetPointVar("atk", CalcArrowDamage(GetEntityHandle(), victim, pkBow, pkArrow, true));
	} else {
		pkSk->SetPointVar("atk", CalcMeleeDamage(GetEntityHandle(), victim, true, false));
	}

	pkSk->SetPointVar("lv", GetLevel());
	pkSk->SetPointVar("iq", GetPoint(POINT_IQ));
	pkSk->SetPointVar("str", GetPoint(POINT_ST));
	pkSk->SetPointVar("dex", GetPoint(POINT_DX));
	pkSk->SetPointVar("con", GetPoint(POINT_HT));
	pkSk->SetPointVar("maxhp", ecs::PointSystem::GetMaxHP(victimEntity));
	pkSk->SetPointVar("maxsp", ecs::PointSystem::GetMaxSP(victimEntity));
	pkSk->SetPointVar("chain", 0);
	pkSk->SetPointVar("ar", CalcAttackRating(GetEntityHandle(), victim));
	pkSk->SetPointVar("def", GetPoint(POINT_DEF_GRADE));
	pkSk->SetPointVar("odef", GetPoint(POINT_DEF_GRADE) - GetPoint(POINT_DEF_GRADE_BONUS));
	pkSk->SetPointVar("horse_level", GetHorseLevel());

	if (pkSk->bSkillAttrType != SKILL_ATTR_TYPE_NORMAL)
		OnMove(true);

	entt::entity pkWeapon = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_WEAPON);

	SetPolyVarForAttack(GetEntityHandle(), pkSk, pkWeapon);
	int iAmount = (int) pkSk->kPointPoly2.Eval();

		// END_OF_ADD_GRANDMASTER_SKILL
	if (iAmount > 0 && dwVnum == SKILL_GYEONGGONG)
	{
		FuncSplashDamage f(ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), pkSk, this, -iAmount, 0, pkSk->lMaxHit, pkWeapon, m_bDisableCooltime, IsPC()?&m_SkillUseInfo[dwVnum]: nullptr, GetSkillPower(dwVnum, bSkillLevel));
		if (ecs::PlayerRuntime::GetSectree(victimEntity))
			ecs::PlayerRuntime::GetSectree(victimEntity)->ForEachAround(f);
		else
		{
			f(pkVictim);
		}
		return BATTLE_DAMAGE;
	}
	return BATTLE_NONE;
}
#endif

// bSkillLevel ŔÎŔÚ°ˇ 0ŔĚ ľĆ´Ň °ćżěżˇ´Â m_abSkillLevels¸¦ »çżëÇĎÁö ľĘ°í °­Á¦·Î
// bSkillLevel·Î °č»ęÇŃ´Ů.
int CHARACTER::ComputeSkill(uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel)
{
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);

	const bool bCanUseHorseSkill = CanUseHorseSkill();
#ifdef ENABLE_BUG_FIXES
	if(dwVnum != SKILL_MUYEONG) {
		if (false == bCanUseHorseSkill && true == IsRiding()) {
			return BATTLE_NONE;
		}
	}
#else
	// ¸»Ŕ» Ĺ¸°íŔÖÁö¸¸ ˝şĹłŔş »çżëÇŇ Ľö ľř´Â »óĹÂ¶ó¸é return
	if (false == bCanUseHorseSkill && true == IsRiding())
		return BATTLE_NONE;
#endif

	if (IsPolymorphed())
		return BATTLE_NONE;

	if (g_bSkillDisable)
		return BATTLE_NONE;

	CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
		return BATTLE_NONE;

#ifdef ENABLE_BUG_FIXES
	if(dwVnum != SKILL_MUYEONG) {
		if (bCanUseHorseSkill && pkSk->dwType != SKILL_TYPE_HORSE) {
			return BATTLE_NONE;
		}

		if (!bCanUseHorseSkill && pkSk->dwType == SKILL_TYPE_HORSE) {
			return BATTLE_NONE;
		}
	}
#else
	if (bCanUseHorseSkill && pkSk->dwType != SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	if (!bCanUseHorseSkill && pkSk->dwType == SKILL_TYPE_HORSE)
		return BATTLE_NONE;
#endif

	// »ó´ëąćżˇ°Ô ľ˛´Â °ÍŔĚ ľĆ´Ď¸é łŞżˇ°Ô ˝áľß ÇŃ´Ů.
	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
		pkVictim = this;
// #ifdef ENABLE_WOLFMAN_CHARACTER
	// else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_PARTY))
		// pkVictim = this;
// #endif

	if (!pkVictim)
	{
		if (test_server)
			LOG_INFO("ComputeSkill: {} Victim == null, skill {}", GetName(), dwVnum);

		return BATTLE_NONE;
	}
	const entt::entity victimEntity = pkVictim->GetEntityHandle();

	if (pkSk->dwTargetRange && DISTANCE_SQRT(GetX() - ecs::PlayerRuntime::GetX(victimEntity), GetY() - ecs::PlayerRuntime::GetY(victimEntity)) >= pkSk->dwTargetRange + 50)
	{
		if (test_server)
			LOG_INFO("ComputeSkill: Victim too far, skill {} : {} to {} (distance {} limit {})", dwVnum, GetName(), ecs::PlayerRuntime::GetName(victimEntity).data(), (int32_t)DISTANCE_SQRT(GetX() - ecs::PlayerRuntime::GetX(victimEntity), GetY() - ecs::PlayerRuntime::GetY(victimEntity)), pkSk->dwTargetRange);

		return BATTLE_NONE;
	}

	if (0 == bSkillLevel)
	{
		if ((bSkillLevel = GetSkillLevel(pkSk->dwVnum)) == 0)
		{
			if (test_server)
				LOG_INFO("ComputeSkill : name:{} vnum:{}  skillLevelBySkill : {} ", GetName(), pkSk->dwVnum, bSkillLevel);
			return BATTLE_NONE;
		}
	}

	if (AffectSystem::IsAffectFlag(victimEntity, AFF_PABEOP) && pkVictim->IsGoodAffect(dwVnum))
	{
		return BATTLE_NONE;
	}

	const float k = 1.0 * GetSkillPower(pkSk->dwVnum, bSkillLevel) * pkSk->bMaxLevel / 100;

	pkSk->SetPointVar("k", k);
	pkSk->kSplashAroundDamageAdjustPoly.SetVar("k", k);

	if (pkSk->dwType == SKILL_TYPE_HORSE)
	{
		entt::entity pkBow = entt::null, pkArrow = entt::null;
		if (1 == GetArrowAndBow(&pkBow, &pkArrow, 1))
		{
			pkSk->SetPointVar("atk", CalcArrowDamage(GetEntityHandle(), victim, pkBow, pkArrow, true));
		}
		else
		{
			pkSk->SetPointVar("atk", CalcMeleeDamage(GetEntityHandle(), victim, true, false));
		}
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_MELEE_DAMAGE))
	{
		pkSk->SetPointVar("atk", CalcMeleeDamage(GetEntityHandle(), victim, true, false));
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_MAGIC_DAMAGE))
	{
		pkSk->SetPointVar("atk", CalcMagicDamage(GetEntityHandle(), victim));
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_ARROW_DAMAGE))
	{
		entt::entity pkBow = entt::null, pkArrow = entt::null;
		if (1 == GetArrowAndBow(&pkBow, &pkArrow, 1))
		{
			pkSk->SetPointVar("atk", CalcArrowDamage(GetEntityHandle(), victim, pkBow, pkArrow, true));
		}
		else
		{
			pkSk->SetPointVar("atk", 0);
		}
	}

	if (pkSk->bPointOn == POINT_MOV_SPEED)
	{
		pkSk->SetPointVar("maxv", pkVictim->GetLimitPoint(POINT_MOV_SPEED));
	}

	pkSk->SetPointVar("lv", GetLevel());
	pkSk->SetPointVar("iq", GetPoint(POINT_IQ));
	pkSk->SetPointVar("str", GetPoint(POINT_ST));
	pkSk->SetPointVar("dex", GetPoint(POINT_DX));
	pkSk->SetPointVar("con", GetPoint(POINT_HT));
	pkSk->SetPointVar("maxhp", ecs::PointSystem::GetMaxHP(victimEntity));
	pkSk->SetPointVar("maxsp", ecs::PointSystem::GetMaxSP(victimEntity));
	pkSk->SetPointVar("chain", 0);
	pkSk->SetPointVar("ar", CalcAttackRating(GetEntityHandle(), victim));
	pkSk->SetPointVar("def", GetPoint(POINT_DEF_GRADE));
	pkSk->SetPointVar("odef", GetPoint(POINT_DEF_GRADE) - GetPoint(POINT_DEF_GRADE_BONUS));
	pkSk->SetPointVar("horse_level", GetHorseLevel());

	if (pkSk->bSkillAttrType != SKILL_ATTR_TYPE_NORMAL)
		OnMove(true);

	entt::entity pkWeapon = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_WEAPON);

	SetPolyVarForAttack(GetEntityHandle(), pkSk, pkWeapon);

	pkSk->kDurationPoly.SetVar("k", k/*bSkillLevel*/);
	pkSk->kDurationPoly2.SetVar("k", k/*bSkillLevel*/);

	int iAmount = (int) pkSk->kPointPoly.Eval();
	int iAmount2 = (int) pkSk->kPointPoly2.Eval();
	int iAmount3 = (int) pkSk->kPointPoly3.Eval();

	if (test_server && IsPC())
		LOG_INFO("iAmount: {} {} {} , atk:{} skLevel:{} k:{} GetSkillPower({}) MaxLevel:{} Per:{}", iAmount, iAmount2, iAmount3, pkSk->kPointPoly.GetVar("atk"), pkSk->kPointPoly.GetVar("k"), k, GetSkillPower(pkSk->dwVnum, bSkillLevel), pkSk->bMaxLevel, pkSk->bMaxLevel/100);

	// ADD_GRANDMASTER_SKILL
	if (GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER)
	{
		iAmount = (int) pkSk->kMasterBonusPoly.Eval();
	}
	// END_OF_ADD_GRANDMASTER_SKILL

	//LOG_INFO(0, "XXX SKILL Calc %d Amount %d", dwVnum, iAmount);

	// REMOVE_BAD_AFFECT_BUG_FIX
	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_REMOVE_BAD_AFFECT))
	{
		if (number(1, 100) <= iAmount2)
		{
			pkVictim->RemoveBadAffect();
		}
	}
	// END_OF_REMOVE_BAD_AFFECT_BUG_FIX

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_ATTACK | SKILL_FLAG_USE_MELEE_DAMAGE | SKILL_FLAG_USE_MAGIC_DAMAGE) &&
		!(pkSk->dwVnum == SKILL_MUYEONG && pkVictim == this) && !(pkSk->IsChargeSkill() && pkVictim == this))
	{
		bool bAdded = false;

		if (pkSk->bPointOn == POINT_HP && iAmount < 0)
		{
			int iAG = 0;
#ifdef LEADERBOARD_RAZOR93
			SetSkillHit(true);
#endif

			FuncSplashDamage f(ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), pkSk, this, iAmount, iAG, pkSk->lMaxHit, pkWeapon, m_bDisableCooltime, IsPC()?&m_SkillUseInfo[dwVnum]: nullptr, GetSkillPower(dwVnum, bSkillLevel));
			if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
			{
				if (ecs::PlayerRuntime::GetSectree(victimEntity))
					ecs::PlayerRuntime::GetSectree(victimEntity)->ForEachAround(f);
			}
			else
			{
				f(pkVictim);
			}
#ifdef LEADERBOARD_RAZOR93
			SetSkillHit(false);
#endif
		}
		else
		{
			pkSk->kDurationPoly.SetVar("k", k/*bSkillLevel*/);
			int iDur = (int) pkSk->kDurationPoly.Eval();


			if (IsPC())
				if (!(dwVnum >= GUILD_SKILL_START && dwVnum <= GUILD_SKILL_END)) // ±ćµĺ ˝şĹłŔş ÄđĹ¸ŔÓ Ăł¸®¸¦ ÇĎÁö ľĘ´Â´Ů.
					if (!m_bDisableCooltime && !m_SkillUseInfo[dwVnum].HitOnce(dwVnum) && dwVnum != SKILL_MUYEONG)
					{
						return BATTLE_NONE;
					}

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AffectSystem::AddAffect(victimEntity, pkSk->dwVnum, pkSk->bPointOn, iAmount, pkSk->dwAffectFlag, iDur, 0, true);
				else
				{
					if (ecs::PlayerRuntime::GetSectree(victimEntity))
					{
						FuncSplashAffect f(GetEntityHandle(), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), pkSk->iSplashRange, pkSk->dwVnum, pkSk->bPointOn, iAmount, pkSk->dwAffectFlag, iDur, 0, true, pkSk->lMaxHit);
						ecs::PlayerRuntime::GetSectree(victimEntity)->ForEachAround(f);
					}
				}
				bAdded = true;
			}
		}

		if (pkSk->bPointOn2 != POINT_NONE && !pkSk->IsChargeSkill())
		{
			pkSk->kDurationPoly2.SetVar("k", k/*bSkillLevel*/);
			int iDur = (int) pkSk->kDurationPoly2.Eval();

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AffectSystem::AddAffect(victimEntity, pkSk->dwVnum, pkSk->bPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded);
				else
				{
					if (ecs::PlayerRuntime::GetSectree(victimEntity))
					{
						FuncSplashAffect f(GetEntityHandle(), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), pkSk->iSplashRange, pkSk->dwVnum, pkSk->bPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded, pkSk->lMaxHit);
						ecs::PlayerRuntime::GetSectree(victimEntity)->ForEachAround(f);
					}
				}

				bAdded = true;
			}
			else
			{
				ecs::PointSystem::Change(victimEntity, pkSk->bPointOn2, iAmount2);
			}
		}

		// ADD_GRANDMASTER_SKILL
		if (pkSk->bPointOn3 != POINT_NONE && !pkSk->IsChargeSkill() && GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER)
		{
			pkSk->kDurationPoly3.SetVar("k", k/*bSkillLevel*/);
			int iDur = (int) pkSk->kDurationPoly3.Eval();


			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AffectSystem::AddAffect(victimEntity, pkSk->dwVnum, pkSk->bPointOn3, iAmount3, /*pkSk->dwAffectFlag3*/ 0, iDur, 0, !bAdded);
				else
				{
					if (ecs::PlayerRuntime::GetSectree(victimEntity))
					{
						FuncSplashAffect f(GetEntityHandle(), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), pkSk->iSplashRange, pkSk->dwVnum, pkSk->bPointOn3, iAmount3, /*pkSk->dwAffectFlag3*/ 0, iDur, 0, !bAdded, pkSk->lMaxHit);
						ecs::PlayerRuntime::GetSectree(victimEntity)->ForEachAround(f);
					}
				}

				bAdded = true;
			}
			else
			{
				ecs::PointSystem::Change(victimEntity, pkSk->bPointOn3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_DAMAGE;
	}
	else
	{
		if (dwVnum == SKILL_MUYEONG)
		{
			pkSk->kDurationPoly.SetVar("k", k/*bSkillLevel*/);
			pkSk->kDurationSPCostPoly.SetVar("k", k/*bSkillLevel*/);

			int iDur = (int32_t) pkSk->kDurationPoly.Eval();
			iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

			if (pkVictim == this)
				AddAffect(dwVnum,
						POINT_NONE, 0,
						AFF_MUYEONG,
						iDur,
						(int32_t) pkSk->kDurationSPCostPoly.Eval(),
						true);

			return BATTLE_NONE;
		}

		bool bAdded = false;
		pkSk->kDurationPoly.SetVar("k", k/*bSkillLevel*/);
		int iDur = (int) pkSk->kDurationPoly.Eval();

		if (iDur > 0)
		{
			iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);
			// AffectFlag°ˇ ľř°ĹłŞ, toggle ÇĎ´Â °ÍŔĚ ľĆ´Ď¶ó¸é..
			pkSk->kDurationSPCostPoly.SetVar("k", k/*bSkillLevel*/);

			if (pkSk->bPointOn2 != POINT_NONE)
			{
				AffectSystem::RemoveAffect(victimEntity, pkSk->dwVnum);

				int iDur2 = (int) pkSk->kDurationPoly2.Eval();

				if (iDur2 > 0)
				{
					if (test_server)
						LOG_INFO("SKILL_AFFECT: {} {} Dur:{} To:{} Amount:{}", GetName(), pkSk->szName, iDur2, pkSk->bPointOn2, iAmount2);

					iDur2 += GetPoint(POINT_PARTY_BUFFER_BONUS);
					AffectSystem::AddAffect(victimEntity, pkSk->dwVnum, pkSk->bPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur2, 0, false);
				}
				else
				{
					ecs::PointSystem::Change(victimEntity, pkSk->bPointOn2, iAmount2);
				}

				uint32_t affact_flag = pkSk->dwAffectFlag;

				// ADD_GRANDMASTER_SKILL
				if ((pkSk->dwVnum == SKILL_CHUNKEON && GetUsedSkillMasterType(pkSk->dwVnum) < SKILL_GRAND_MASTER))
					affact_flag = AFF_CHEONGEUN_WITH_FALL;
				// END_OF_ADD_GRANDMASTER_SKILL

				AffectSystem::AddAffect(victimEntity, pkSk->dwVnum,
						pkSk->bPointOn,
						iAmount,
						affact_flag,
						iDur,
						(int32_t) pkSk->kDurationSPCostPoly.Eval(),
						false);
			}
			else
			{
				if (test_server)
					LOG_INFO("SKILL_AFFECT: {} {} Dur:{} To:{} Amount:{}", GetName(), pkSk->szName, iDur, pkSk->bPointOn, iAmount);

				AffectSystem::AddAffect(victimEntity, pkSk->dwVnum,
						pkSk->bPointOn,
						iAmount,
						pkSk->dwAffectFlag,
						iDur,
						(int32_t) pkSk->kDurationSPCostPoly.Eval(),
						// ADD_GRANDMASTER_SKILL
						!bAdded);
				// END_OF_ADD_GRANDMASTER_SKILL
			}

			bAdded = true;
		}
		else
		{
			if (!pkSk->IsChargeSkill())
				ecs::PointSystem::Change(victimEntity, pkSk->bPointOn, iAmount);

			if (pkSk->bPointOn2 != POINT_NONE)
			{
				AffectSystem::RemoveAffect(victimEntity, pkSk->dwVnum);

				int iDur2 = (int) pkSk->kDurationPoly2.Eval();

				if (iDur2 > 0)
				{
					iDur2 += GetPoint(POINT_PARTY_BUFFER_BONUS);

					if (pkSk->IsChargeSkill())
						AffectSystem::AddAffect(victimEntity, pkSk->dwVnum, pkSk->bPointOn2, iAmount2, AFF_TANHWAN_DASH, iDur2, 0, false);
					else
						AffectSystem::AddAffect(victimEntity, pkSk->dwVnum, pkSk->bPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur2, 0, false);
				}
				else
				{
					ecs::PointSystem::Change(victimEntity, pkSk->bPointOn2, iAmount2);
				}

			}
		}

		// ADD_GRANDMASTER_SKILL
		if (pkSk->bPointOn3 != POINT_NONE && !pkSk->IsChargeSkill() && GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER)
		{

			pkSk->kDurationPoly3.SetVar("k", k/*bSkillLevel*/);
			int iDur = (int) pkSk->kDurationPoly3.Eval();

			LOG_INFO("try third {} {} {} {} 1894", pkSk->dwVnum, pkSk->bPointOn3, iDur, iAmount3);

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AffectSystem::AddAffect(victimEntity, pkSk->dwVnum, pkSk->bPointOn3, iAmount3, /*pkSk->dwAffectFlag3*/ 0, iDur, 0, !bAdded);
				else
				{
					if (ecs::PlayerRuntime::GetSectree(victimEntity))
					{
						FuncSplashAffect f(GetEntityHandle(), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), pkSk->iSplashRange, pkSk->dwVnum, pkSk->bPointOn3, iAmount3, /*pkSk->dwAffectFlag3*/ 0, iDur, 0, !bAdded, pkSk->lMaxHit);
						ecs::PlayerRuntime::GetSectree(victimEntity)->ForEachAround(f);
					}
				}

				bAdded = true;
			}
			else
			{
				ecs::PointSystem::Change(victimEntity, pkSk->bPointOn3, iAmount3);
			}
		}

#ifdef ENABLE_NEW_GYEONGGONG_SKILL
		if (pkSk->bPointOn2 == POINT_NONE && iAmount2 > 0 && dwVnum == SKILL_GYEONGGONG)
		{
			FuncSplashDamage f(ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), pkSk, this, -iAmount2, 0, pkSk->lMaxHit, pkWeapon, m_bDisableCooltime, IsPC()?&m_SkillUseInfo[dwVnum]: nullptr, GetSkillPower(dwVnum, bSkillLevel));
			if (ecs::PlayerRuntime::GetSectree(victimEntity))
				ecs::PlayerRuntime::GetSectree(victimEntity)->ForEachAround(f);

			else
			{
				f(pkVictim);
			}
		}
#endif

		// END_OF_ADD_GRANDMASTER_SKILL


		return BATTLE_NONE;
	}
}

bool CHARACTER::UseSkill(uint32_t dwVnum, entt::entity victim, bool bUseGrandMaster)
{
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	entt::entity victimEntity = victim;
#ifdef ENABLE_BUG_FIXES
	if ((dwVnum == SKILL_GEOMKYUNG || dwVnum == SKILL_GWIGEOM) &&
		!ItemSystem::IsValidItem(ItemSystem::GetWearItem(GetEntityHandle(), WEAR_WEAPON)))
		return false;
#endif

#ifdef ENABLE_PVP_ADVANCED
	switch (dwVnum)
	{
		case 94:
		case 95:
		case 96:
		case 109:
		case 110:
		case 111:
		{
			if (pkVictim)
			{
				if (this != pkVictim && ecs::PlayerRuntime::GetDesc(GetEntityHandle()) && ecs::PlayerRuntime::GetDesc(victimEntity))
				{
					if (ecs::QuestSystem::GetFlag(victimEntity, BLOCK_BUFF))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 518, "%s", ecs::PlayerRuntime::GetName(victimEntity).data());
#endif
						return false;
					}
				}
			}
		}
		break;
	}
#endif

	if (false == CanUseSkill(dwVnum))
		return false;

	// NO_GRANDMASTER
	if (test_server)
	{
		if (quest::CQuestManager::instance().GetEventFlag("no_grand_master"))
		{
			bUseGrandMaster = false;
		}
	}
	// END_OF_NO_GRANDMASTER

	if (g_bSkillDisable)
		return false;

	if (IsObserverMode())
		return false;

	if (!CanMove())
		return false;

	if (IsPolymorphed())
		return false;

	const bool bCanUseHorseSkill = CanUseHorseSkill();


	if (dwVnum == SKILL_HORSE_SUMMON)
	{
		if (GetSkillLevel(dwVnum) == 0)
			return false;

		return true;
	}

	// ¸»Ŕ» Ĺ¸°íŔÖÁö¸¸ ˝şĹłŔş »çżëÇŇ Ľö ľř´Â »óĹÂ¶ó¸é return false
	if (false == bCanUseHorseSkill && true == IsRiding())
		return false;

	CSkillProto * pkSk = CSkillManager::instance().Get(dwVnum);
	LOG_INFO("{}: USE_SKILL: {} pkVictim {}", GetName(), dwVnum, static_cast<const void*>(get_pointer(pkVictim)));

	if (!pkSk)
		return false;

	if (bCanUseHorseSkill && pkSk->dwType != SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	if (!bCanUseHorseSkill && pkSk->dwType == SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	if (GetSkillLevel(dwVnum) == 0)
		return false;


	// NO_GRANDMASTER
	if (GetSkillMasterType(dwVnum) < SKILL_GRAND_MASTER)
		bUseGrandMaster = false;
	// END_OF_NO_GRANDMASTER

	// MINING
	const entt::entity equippedWeapon = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_WEAPON);
	if (ItemSystem::IsValidItem(equippedWeapon) &&
		(ItemSystem::GetItemType(equippedWeapon) == ITEM_ROD ||
		 ItemSystem::GetItemType(equippedWeapon) == ITEM_PICK))
		return false;
	// END_OF_MINING

	m_SkillUseInfo[dwVnum].TargetVIDMap.clear();

	if (pkSk->IsChargeSkill())
	{
		if ((IsAffectFlag(AFF_TANHWAN_DASH)) || (pkVictim && (pkVictim != this)))
		{
			if (!pkVictim)
				return false;

			if (!IsAffectFlag(AFF_TANHWAN_DASH))
			{
				if (!UseSkill(dwVnum, this ? this->GetEntityHandle() : entt::null))
					return false;
			}

			m_SkillUseInfo[dwVnum].SetMainTargetVID(victimEntity);
			// DASH »óĹÂŔÇ ĹşČŻ°ÝŔş °ř°Ý±âĽú
			ComputeSkill(dwVnum, pkVictim ? pkVictim->GetEntityHandle() : entt::null);
			RemoveAffect(dwVnum);
			return true;
		}
	}

	if (dwVnum == SKILL_COMBO)
	{
		const uint8_t comboIndex = CombatSystem::ToggleComboIndex(
			GetEntityHandle(), GetSkillLevel(SKILL_COMBO));
		ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, "combo %d", comboIndex);
		return true;
	}

	// Toggle ÇŇ ¶§´Â SP¸¦ ľ˛Áö ľĘŔ˝ (SelfOnly·Î ±¸şĐ)
	if ((0 != pkSk->dwAffectFlag || pkSk->dwVnum == SKILL_MUYEONG) && (pkSk->dwFlag & SKILL_FLAG_TOGGLE) && RemoveAffect(pkSk->dwVnum))
	{
		return true;
	}

	if (IsAffectFlag(AFF_REVIVE_INVISIBLE))
		RemoveAffect(AFFECT_REVIVE_INVISIBLE);

	const float k = 1.0 * GetSkillPower(pkSk->dwVnum) * pkSk->bMaxLevel / 100;

	pkSk->SetPointVar("k", k);
	pkSk->kSplashAroundDamageAdjustPoly.SetVar("k", k);

	// ÄđĹ¸ŔÓ ĂĽĹ©
	pkSk->kCooldownPoly.SetVar("k", k);
	int iCooltime = (int) pkSk->kCooldownPoly.Eval();
	int lMaxHit = pkSk->lMaxHit ? pkSk->lMaxHit : -1;

	pkSk->SetSPCostVar("k", k);

	uint32_t dwCur = get_dword_time();

	if (dwVnum == SKILL_TERROR && m_SkillUseInfo[dwVnum].bUsed && m_SkillUseInfo[dwVnum].dwNextSkillUsableTime > dwCur )
	{
		LOG_INFO(" SKILL_TERROR's Cooltime is not delta over {}", m_SkillUseInfo[dwVnum].dwNextSkillUsableTime  - dwCur);
		return false;
	}

	int iNeededSP = 0;

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_HP_AS_COST))
	{
		pkSk->SetSPCostVar("maxhp", GetMaxHP());
		pkSk->SetSPCostVar("v", GetHP());
		iNeededSP = (int) pkSk->kSPCostPoly.Eval();

		// ADD_GRANDMASTER_SKILL
		if (GetSkillMasterType(dwVnum) >= SKILL_GRAND_MASTER && bUseGrandMaster)
		{
			iNeededSP = (int) pkSk->kGrandMasterAddSPCostPoly.Eval();
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		if (GetHP() < iNeededSP)
			return false;

		PointChange(POINT_HP, -iNeededSP);
	}
	else
	{
		// SKILL_FOMULA_REFACTORING
		pkSk->SetSPCostVar("maxhp", GetMaxHP());
		pkSk->SetSPCostVar("maxv", GetMaxSP());
		pkSk->SetSPCostVar("v", GetSP());

		iNeededSP = (int) pkSk->kSPCostPoly.Eval();

		if (GetSkillMasterType(dwVnum) >= SKILL_GRAND_MASTER && bUseGrandMaster)
		{
			iNeededSP = (int) pkSk->kGrandMasterAddSPCostPoly.Eval();
		}
		// END_OF_SKILL_FOMULA_REFACTORING

		if (GetSP() < iNeededSP)
			return false;

#ifdef TEXTS_IMPROVEMENT
		if (test_server) {
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 104, "%s#%d", pkSk->szName, iNeededSP);
		}
#endif
		PointChange(POINT_SP, -iNeededSP);
	}

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
	{
		pkVictim = this;
		victimEntity = GetEntityHandle();
	}
#ifdef ENABLE_WOLFMAN_CHARACTER
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_PARTY))
	{
		pkVictim = this;
		victimEntity = GetEntityHandle();
	}
#endif

	if ((pkSk->dwVnum == SKILL_MUYEONG) || (pkSk->IsChargeSkill() && !IsAffectFlag(AFF_TANHWAN_DASH) && !pkVictim))
	{
		// ĂłŔ˝ »çżëÇĎ´Â ą«żµÁřŔş ŔÚ˝Ĺżˇ°Ô Affect¸¦ şŮŔÎ´Ů.
		pkVictim = this;
		victimEntity = GetEntityHandle();
	}

	int iSplashCount = 1;
	if (false == m_bDisableCooltime)
	{
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
		if (dwVnum == SKILL_GYEONGGONG)
		{
			if (false ==
					m_SkillUseInfo[dwVnum].UseSkill(
						bUseGrandMaster, (nullptr != pkVictim && SKILL_HORSE_WILDATTACK != dwVnum) ? victimEntity : entt::null, ComputeCooltime(iCooltime * 1000), iSplashCount, 25000))
			{
				if (test_server)
					ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_NOTICE, "cooltime not finished %s %d", pkSk->szName, iCooltime);
				return false;
			}
		}
		else
		{
			if (false ==
					m_SkillUseInfo[dwVnum].UseSkill(
						bUseGrandMaster,
						(nullptr != pkVictim && SKILL_HORSE_WILDATTACK != dwVnum) ? victimEntity : entt::null,
				   		ComputeCooltime(iCooltime * 1000),
				   		iSplashCount,
				   		lMaxHit))
			{
				if (test_server)
					ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_NOTICE, "cooltime not finished %s %d", pkSk->szName, iCooltime);
				return false;
			}

		}
#else
		if (false ==
				m_SkillUseInfo[dwVnum].UseSkill(
					bUseGrandMaster,
					(NULL != pkVictim && SKILL_HORSE_WILDATTACK != dwVnum) ? victimEntity : entt::null,
				   	ComputeCooltime(iCooltime * 1000),
				   	iSplashCount,
				   	lMaxHit))
		{
			if (test_server)
				ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_NOTICE, "cooltime not finished %s %d", pkSk->szName, iCooltime);

			return false;
		}
#endif
	}

	if (dwVnum == SKILL_CHAIN)
	{
		ResetChainLightningIndex();
		AddChainLightningExcept(victimEntity);
	}

#ifdef GROUP_BUFF
	if (dwVnum == 94 || dwVnum == 95 || dwVnum == 96 || dwVnum == 110 || dwVnum == 111) {
		if (GetParty() && pkVictim)
		{
			LPPARTY party = ecs::SocialSystem::GetParty(victimEntity);
			if (party && GetParty()) {
				ComputeSkillParty(dwVnum, this ? this->GetEntityHandle() : entt::null);
			}
		}
	}
#endif

#ifdef __SKILL_COLOR_SYSTEM__
	if (pkVictim != nullptr && (dwVnum == 94 || dwVnum == 95 || dwVnum == 96 || dwVnum == 110 || dwVnum == 111))
	{
		uint8_t skill = 0;
		uint8_t id = 0;
		switch (dwVnum)
		{
		case 94:
			skill = ESkillColorLength::BUFF_BEGIN + 0;
			id = 3;
			break;
		case 95:
			skill = ESkillColorLength::BUFF_BEGIN + 1;
			id = 4;
			break;
		case 96:
			skill = ESkillColorLength::BUFF_BEGIN + 2;
			id = 5;
			break;
		case 110:
			skill = ESkillColorLength::BUFF_BEGIN + 3;
			id = 4;
			break;
		case 111:
			skill = ESkillColorLength::BUFF_BEGIN + 4;
			id = 5;
			break;
		default:
			break;
		}

		uint32_t data[ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
		memcpy(data, pkVictim->GetSkillColor(), sizeof(data));

		uint32_t dataAttacker[ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
		memcpy(dataAttacker, this->GetSkillColor(), sizeof(dataAttacker));

		data[skill][0] = dataAttacker[id][0];
		data[skill][1] = dataAttacker[id][1];
		data[skill][2] = dataAttacker[id][2];
		data[skill][3] = dataAttacker[id][3];
		data[skill][4] = dataAttacker[id][4];

		pkVictim->SetSkillColor(data[0]);

		TSkillColor db_pack;
		memcpy(db_pack.dwSkillColor, data, sizeof(data));
		db_pack.player_id = ecs::PlayerRuntime::GetPlayerID(victimEntity);
		db_clientdesc->DBPacketHeader(HEADER_GD_SKILL_COLOR_SAVE, 0, sizeof(TSkillColor));
		db_clientdesc->Packet(&db_pack, sizeof(TSkillColor));
	}
#endif
	if (pkVictim != nullptr && GetParty() && (dwVnum == 94 || dwVnum == 95 || dwVnum == 96 || dwVnum == 110 || dwVnum == 111))//razor93---az egesz csoport buffolasa egyszerre------
	{
		if (dwVnum == 66) // varázslat kioltás
		{
			return false;
		}

		if (ecs::SocialSystem::GetParty(victimEntity)){
			if (ecs::SocialSystem::GetParty(victimEntity) == GetParty()){
				ComputeSkillParty(dwVnum, this ? this->GetEntityHandle() : entt::null);
			}
		}
	}//------------------------------------------------------------------2024-12-30------------------------------------------------------------------------------
	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
		ComputeSkill(dwVnum, this ? this->GetEntityHandle() : entt::null);
#ifdef ENABLE_WOLFMAN_CHARACTER
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_PARTY))
		ComputeSkillParty(dwVnum, this ? this->GetEntityHandle() : entt::null);
#endif
	else if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_ATTACK))
		ComputeSkill(dwVnum, pkVictim ? pkVictim->GetEntityHandle() : entt::null);
	else if (dwVnum == SKILL_BYEURAK)
		ComputeSkill(dwVnum, pkVictim ? pkVictim->GetEntityHandle() : entt::null);
	else if (dwVnum == SKILL_MUYEONG || pkSk->IsChargeSkill())
		ComputeSkill(dwVnum, pkVictim ? pkVictim->GetEntityHandle() : entt::null);

	m_dwLastSkillTime = get_dword_time();

	return true;
}

int CHARACTER::GetUsedSkillMasterType(uint32_t dwVnum)
{
	const TSkillUseInfo& rInfo = m_SkillUseInfo[dwVnum];

	if (GetSkillMasterType(dwVnum) < SKILL_GRAND_MASTER)
		return GetSkillMasterType(dwVnum);

	if (rInfo.isGrandMaster)
		return GetSkillMasterType(dwVnum);

	return MIN(GetSkillMasterType(dwVnum), SKILL_MASTER);
}

int CHARACTER::GetSkillMasterType(uint32_t dwVnum) const
{
	if (!IsPC())
		return 0;

	if (dwVnum >= SKILL_MAX_NUM)
	{
		LOG_ERROR("{} skill vnum overflow {}", GetName(), dwVnum);
		return 0;
	}

	return m_pSkillLevels ? m_pSkillLevels[dwVnum].bMasterType:SKILL_NORMAL;
}

int CHARACTER::GetSkillPower(uint32_t dwVnum, uint8_t bLevel) const
{
	// ŔÎľîąÝÁö ľĆŔĚĹŰ
	if (dwVnum >= SKILL_LANGUAGE1 && dwVnum <= SKILL_LANGUAGE3 && IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_LANGUAGE))
	{
		return 100;
	}

	if (dwVnum >= GUILD_SKILL_START && dwVnum <= GUILD_SKILL_END)
	{
		if (GetGuild())
			return 100 * GetGuild()->GetSkillLevel(dwVnum) / 7 / 7;
		else
			return 0;
	}

	if (bLevel)
	{
		//SKILL_POWER_BY_LEVEL
		return GetSkillPowerByLevel(bLevel, true);
		//END_SKILL_POWER_BY_LEVEL;
	}

	if (dwVnum >= SKILL_MAX_NUM)
	{
		LOG_ERROR("{} skill vnum overflow {}", GetName(), dwVnum);
		return 0;
	}

	//SKILL_POWER_BY_LEVEL
	return GetSkillPowerByLevel(GetSkillLevel(dwVnum));
	//SKILL_POWER_BY_LEVEL
}

EVENTFUNC(skill_muyoung_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("skill_muyoung_event> <Factor> Null pointer");
		return 0;
	}

	auto*	ch = info->ch.Get();

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	const entt::entity character = ch->GetEntityHandle();

	if (!AffectSystem::IsAffectFlag(character, AFF_MUYEONG))
	{
		ch->StopMuyeongEvent();
		return 0;
	}

	// 1. Find Victim
	FFindNearVictim f(character, character);
	if (ecs::PlayerRuntime::GetSectree(character))
	{
		ecs::PlayerRuntime::GetSectree(character)->ForEachAround(f);
		// 2. Shoot!
		if (f.GetVictim() != entt::null)
		{
			ch->CreateFly(FLY_SKILL_MUYEONG, f.GetVictim());
			ch->ComputeSkill(SKILL_MUYEONG, f.GetVictim());
		}
	}

	return PASSES_PER_SEC(3);
}

void CHARACTER::StartMuyeongEvent()
{
	if (m_pkMuyeongEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = this;
	m_pkMuyeongEvent = event_create(skill_muyoung_event, info, PASSES_PER_SEC(1));
}

void CHARACTER::StopMuyeongEvent()
{
	event_cancel(&m_pkMuyeongEvent);
}

#ifdef ENABLE_NEW_GYEONGGONG_SKILL
EVENTFUNC(skill_gyeongGong_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("skill_gyeongGong_event> <Factor> Null pointer");
		return 0;
	}

	auto*	ch = info->ch.Get();

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	const entt::entity character = ch->GetEntityHandle();

	if (!AffectSystem::IsAffectFlag(character, AFF_GYEONGGONG))
	{
		ch->StopGyeongGongEvent();
		return 0;
	}

	ch->ComputeGyeongGongSkill(SKILL_GYEONGGONG, (ch ? ch->GetEntityHandle() : entt::null));

	return PASSES_PER_SEC(2);
}

void CHARACTER::StartGyeongGongEvent()
{
	if (m_pkGyeongGongEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = this;
	m_pkGyeongGongEvent = event_create(skill_gyeongGong_event, info, PASSES_PER_SEC(1));
}

void CHARACTER::StopGyeongGongEvent()
{
	event_cancel(&m_pkGyeongGongEvent);
}
#endif

void CHARACTER::SkillLearnWaitMoreTimeMessage(uint32_t ms)
{
#ifdef TEXTS_IMPROVEMENT
	if (ms < 3 * 60) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 345, "");
	} else if (ms < 5 * 60) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 264, "");
	} else if (ms < 10 * 60) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 262, "");
	} else if (ms < 30 * 60) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 290, "");
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 263, "");
	} else if (ms < 1 * 3600) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 447, "");
	} else if (ms < 2 * 3600) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 407, "");
	} else if (ms < 3 * 3600) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 464, "");
	} else if (ms < 6 * 3600) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 479, "");
	} else if (ms < 12 * 3600) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 446, "");
	} else if (ms < 18 * 3600) {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 254, "");
	} else {
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 435, "");
	}
#endif
}

bool CHARACTER::HasMobSkill() const
{
	return CountMobSkill() > 0;
}

size_t CHARACTER::CountMobSkill() const
{
	if (!m_pkMobData)
		return 0;

	size_t c = 0;

	for (size_t i = 0; i < MOB_SKILL_MAX_NUM; ++i)
		if (m_pkMobData->m_table.Skills[i].dwVnum)
			++c;

	return c;
}

const TMobSkillInfo* CHARACTER::GetMobSkill(unsigned int idx) const
{
	if (idx >= MOB_SKILL_MAX_NUM)
		return nullptr;

	if (!m_pkMobData)
		return nullptr;

	if (0 == m_pkMobData->m_table.Skills[idx].dwVnum)
		return nullptr;

	return &m_pkMobData->m_mobSkillInfo[idx];
}


EVENTINFO(mob_skill_event_info)
{
	DynamicCharacterPtr ch;
	PIXEL_POSITION pos;
	uint32_t vnum;
	int index;
	uint8_t level;

	mob_skill_event_info()
	: ch()
	, pos()
	, vnum(0)
	, index(0)
	, level(0)
	{
	}
};

EVENTFUNC(mob_skill_hit_event)
{
	mob_skill_event_info * info = dynamic_cast<mob_skill_event_info *>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("mob_skill_event_info> <Factor> Null pointer");
		return 0;
	}

	// <Factor>
	auto* ch = info->ch.Get();
	if (ch == nullptr) {
		return 0;
	}

	const entt::entity e = ch->GetEntityHandle();
	if (e != entt::null)
		g_dispatcher.trigger(ecs::EvSkillUsed { e, info->vnum });
	ch->ComputeSkillAtPosition(info->vnum, info->pos, info->level);
	ch->m_mapMobSkillEvent.erase(info->index);

	return 0;
}

#ifdef __VERSION_162__
struct FHealerParty
{
	FHealerParty(LegacyCharHandle pkHealer)
		: m_pkHealer(pkHealer),
		m_healer(pkHealer ? pkHealer->GetEntityHandle() : entt::null)
	{
	}

	void operator () (LegacyCharHandle ch)
	{
		const entt::entity target = ch->GetEntityHandle();
		int iRevive = (int)(ecs::PointSystem::GetMaxHP(m_healer) / 100 * 15);
		int iHP = (ecs::PointSystem::GetMaxHP(target) >= ch->GetHP() + iRevive) ? (int)(ch->GetHP() + iRevive) : (int)(ecs::PointSystem::GetMaxHP(target));
		ch->SetHP(iHP);
		NetworkSyncSystem::BroadcastEffect(g_registry, target, SE_EFFECT_HEALER);
		LOG_INFO("FHealerParty: {} (pointer: {}) heal the HP of {} (pointer: {}) with {} (new HP: {}).", ecs::PlayerRuntime::GetName(m_healer).data(), static_cast<const void*>(get_pointer(m_pkHealer)), ecs::PlayerRuntime::GetName(target).data(), static_cast<const void*>(get_pointer(ch)), iRevive, ch->GetHP());
	}

	LegacyCharHandle	m_pkHealer;
	entt::entity m_healer;
};
#endif

bool CHARACTER::UseMobSkill(unsigned int idx)
{
	if (IsPC())
		return false;

	const TMobSkillInfo* pInfo = GetMobSkill(idx);

	if (!pInfo)
		return false;

	uint32_t dwVnum = pInfo->dwSkillVnum;
	CSkillProto * pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
		return false;

	const float k = 1.0 * GetSkillPower(pkSk->dwVnum, pInfo->bSkillLevel) * pkSk->bMaxLevel / 100;

	pkSk->kCooldownPoly.SetVar("k", k);
	int iCooltime = (int) (pkSk->kCooldownPoly.Eval() * 1000);

	m_adwMobSkillCooltime[idx] = get_dword_time() + iCooltime;

	LOG_INFO("USE_MOB_SKILL: {} idx {} vnum {} cooltime {}", GetName(), idx, dwVnum, iCooltime);

#ifdef __VERSION_162__
	if ((IsMonster()) && (pkSk->dwVnum == HEALING_SKILL_VNUM))
	{
		LPPARTY pkParty = GetParty();
		if ((pkParty) && (IS_SET(pkSk->dwFlag, SKILL_FLAG_PARTY)))
		{
			FHealerParty f(this);
			pkParty->ForEachMemberPtr(f);
		}
		else
		{
			int iRevive = (int)(GetMaxHP() / 100 * 15);
			int iHP = (GetMaxHP() >= GetHP() + iRevive) ? (int)(GetHP() + iRevive) : (int)(GetMaxHP());
			SetHP(iHP);
			NetworkSyncSystem::BroadcastEffect(g_registry, GetEntityHandle(), SE_EFFECT_HEALER);
			LOG_INFO("FHealer: {} (pointer: {}) heal their HP with {} (new HP: {}).", GetName(), static_cast<const void*>(get_pointer(this)), iRevive, GetHP());
		}

		return true;
	}
#endif

	if (m_pkMobData->m_mobSkillInfo[idx].vecSplashAttack.empty())
	{
		LOG_ERROR("No skill hit data for mob {} index {}", GetName(), idx);
		return false;
	}

	for (size_t i = 0; i < m_pkMobData->m_mobSkillInfo[idx].vecSplashAttack.size(); i++)
	{
		PIXEL_POSITION pos = GetXYZ();
		const TMobSplashAttackInfo& rInfo = m_pkMobData->m_mobSkillInfo[idx].vecSplashAttack[i];

		if (rInfo.dwHitDistance)
		{
			float fx, fy;
			GetDeltaByDegree(GetRotation(), rInfo.dwHitDistance, &fx, &fy);
			pos.x += (int32_t) fx;
			pos.y += (int32_t) fy;
		}

		if (rInfo.dwTiming)
		{
			if (test_server)
				LOG_INFO("               timing {}ms", rInfo.dwTiming);

			mob_skill_event_info* info = AllocEventInfo<mob_skill_event_info>();

			info->ch = this;
			info->pos = pos;
			info->level = pInfo->bSkillLevel;
			info->vnum = dwVnum;
			info->index = i;

			// <Factor> Cancel existing event first
			auto it = m_mapMobSkillEvent.find(i);
			if (it != m_mapMobSkillEvent.end()) {
				LPEVENT existing = it->second;
				event_cancel(&existing);
				m_mapMobSkillEvent.erase(it);
			}

			m_mapMobSkillEvent.insert(std::make_pair(i, event_create(mob_skill_hit_event, info, PASSES_PER_SEC(rInfo.dwTiming) / 1000)));
		}
		else
		{
			ComputeSkillAtPosition(dwVnum, pos, pInfo->bSkillLevel);
		}
	}

	return true;
}

bool CHARACTER::IsUsableSkillMotion(uint32_t dwMotionIndex) const
{
	uint32_t selfJobGroup = (GetJob()+1) * 10 + GetSkillGroup();
#ifdef ENABLE_WOLFMAN_CHARACTER
	const uint32_t SKILL_NUM = 176;
#else
	const uint32_t SKILL_NUM = 158;
#endif
	static uint32_t s_anSkill2JobGroup[SKILL_NUM] = {
		0, // common_skill 0
		11, // job_skill 1
		11, // job_skill 2
		11, // job_skill 3
		11, // job_skill 4
		11, // job_skill 5
		11, // job_skill 6
		0, // common_skill 7
		0, // common_skill 8
		0, // common_skill 9
		0, // common_skill 10
		0, // common_skill 11
		0, // common_skill 12
		0, // common_skill 13
		0, // common_skill 14
		0, // common_skill 15
		12, // job_skill 16
		12, // job_skill 17
		12, // job_skill 18
		12, // job_skill 19
		12, // job_skill 20
		12, // job_skill 21
		0, // common_skill 22
		0, // common_skill 23
		0, // common_skill 24
		0, // common_skill 25
		0, // common_skill 26
		0, // common_skill 27
		0, // common_skill 28
		0, // common_skill 29
		0, // common_skill 30
		21, // job_skill 31
		21, // job_skill 32
		21, // job_skill 33
		21, // job_skill 34
		21, // job_skill 35
		21, // job_skill 36
		0, // common_skill 37
		0, // common_skill 38
		0, // common_skill 39
		0, // common_skill 40
		0, // common_skill 41
		0, // common_skill 42
		0, // common_skill 43
		0, // common_skill 44
		0, // common_skill 45
		22, // job_skill 46
		22, // job_skill 47
		22, // job_skill 48
		22, // job_skill 49
		22, // job_skill 50
		22, // job_skill 51
		0, // common_skill 52
		0, // common_skill 53
		0, // common_skill 54
		0, // common_skill 55
		0, // common_skill 56
		0, // common_skill 57
		0, // common_skill 58
		0, // common_skill 59
		0, // common_skill 60
		31, // job_skill 61
		31, // job_skill 62
		31, // job_skill 63
		31, // job_skill 64
		31, // job_skill 65
		31, // job_skill 66
		0, // common_skill 67
		0, // common_skill 68
		0, // common_skill 69
		0, // common_skill 70
		0, // common_skill 71
		0, // common_skill 72
		0, // common_skill 73
		0, // common_skill 74
		0, // common_skill 75
		32, // job_skill 76
		32, // job_skill 77
		32, // job_skill 78
		32, // job_skill 79
		32, // job_skill 80
		32, // job_skill 81
		0, // common_skill 82
		0, // common_skill 83
		0, // common_skill 84
		0, // common_skill 85
		0, // common_skill 86
		0, // common_skill 87
		0, // common_skill 88
		0, // common_skill 89
		0, // common_skill 90
		41, // job_skill 91
		41, // job_skill 92
		41, // job_skill 93
		41, // job_skill 94
		41, // job_skill 95
		41, // job_skill 96
		0, // common_skill 97
		0, // common_skill 98
		0, // common_skill 99
		0, // common_skill 100
		0, // common_skill 101
		0, // common_skill 102
		0, // common_skill 103
		0, // common_skill 104
		0, // common_skill 105
		42, // job_skill 106
		42, // job_skill 107
		42, // job_skill 108
		42, // job_skill 109
		42, // job_skill 110
		42, // job_skill 111
		0, // common_skill 112
		0, // common_skill 113
		0, // common_skill 114
		0, // common_skill 115
		0, // common_skill 116
		0, // common_skill 117
		0, // common_skill 118
		0, // common_skill 119
		0, // common_skill 120
		0, // common_skill 121
		0, // common_skill 122
		0, // common_skill 123
		0, // common_skill 124
		0, // common_skill 125
		0, // common_skill 126
		0, // common_skill 127
		0, // common_skill 128
		0, // common_skill 129
		0, // common_skill 130
		0, // common_skill 131
		0, // common_skill 132
		0, // common_skill 133
		0, // common_skill 134
		0, // common_skill 135
		0, // common_skill 136
		0, // job_skill 137
		0, // job_skill 138
		0, // job_skill 139
		0, // job_skill 140
		0, // common_skill 141
		0, // common_skill 142
		0, // common_skill 143
		0, // common_skill 144
		0, // common_skill 145
		0, // common_skill 146
		0, // common_skill 147
		0, // common_skill 148
		0, // common_skill 149
		0, // common_skill 150
		0, // common_skill 151
		0, // job_skill 152
		0, // job_skill 153
		0, // job_skill 154
		0, // job_skill 155
		0, // job_skill 156
		0, // job_skill 157
#ifdef ENABLE_WOLFMAN_CHARACTER
		0, // empty(reserved) 158
		0, // empty(reserved) 159
		0, // empty(reserved) 160
		0, // empty(reserved) 161
		0, // empty(reserved) 162
		0, // empty(reserved) 163
		0, // empty(reserved) 164
		0, // empty(reserved) 165
		0, // empty(reserved) 166
		0, // empty(reserved) 167
		0, // empty(reserved) 168
		0, // empty(reserved) 169
		51, // job_skill(WOLFMAN SKILL) 170
		51, // job_skill(WOLFMAN SKILL) 171
		51, // job_skill(WOLFMAN SKILL) 172
		51, // job_skill(WOLFMAN SKILL) 173
		51, // job_skill(WOLFMAN SKILL) 174
		51, // job_skill(WOLFMAN SKILL) 175
#endif
	}; // s_anSkill2JobGroup

	const uint32_t MOTION_MAX_NUM 	= 124;
#ifdef ENABLE_WOLFMAN_CHARACTER
	const uint32_t SKILL_LIST_MAX_COUNT	= 6;
#else
	const uint32_t SKILL_LIST_MAX_COUNT	= 5;
#endif
	static uint32_t s_anMotion2SkillVnumList[MOTION_MAX_NUM][SKILL_LIST_MAX_COUNT] =
	{
		// ˝şĹłĽö   ą«»ç˝şĹłID  ŔÚ°´˝şĹłID  Ľö¶ó˝şĹłID  ą«´ç˝şĹłID	ĽöŔÎÁ·(WOLFMAN) ˝şĹłID
		{   0,		0,			0,			0,			0		}, //  0

		// 1ąř Á÷±ş ±âş» ˝şĹł
#ifdef ENABLE_WOLFMAN_CHARACTER
		{   5,		1,			31,			61,			91,	170		}, //  1
		{   5,		2,			32,			62,			92,	171		}, //  2
		{   5,		3,			33,			63,			93,	172		}, //  3
		{   5,		4,			34,			64,			94,	173		}, //  4
		{   5,		5,			35,			65,			95,	174		}, //  5
		{   5,		6,			36,			66,			96,	175		}, //  6
#else
		{   4,		1,			31,			61,			91		}, //  1
		{   4,		2,			32,			62,			92		}, //  2
		{   4,		3,			33,			63,			93		}, //  3
		{   4,		4,			34,			64,			94		}, //  4
		{   4,		5,			35,			65,			95		}, //  5
		{   4,		6,			36,			66,			96		}, //  6
#endif
		{   0,		0,			0,			0,			0		}, //  7
		{   0,		0,			0,			0,			0		}, //  8
		// 1ąř Á÷±ş ±âş» ˝şĹł łˇ

		// ż©ŔŻşĐ
		{   0,		0,			0,			0,			0		}, //  9
		{   0,		0,			0,			0,			0		}, //  10
		{   0,		0,			0,			0,			0		}, //  11
		{   0,		0,			0,			0,			0		}, //  12
		{   0,		0,			0,			0,			0		}, //  13
		{   0,		0,			0,			0,			0		}, //  14
		{   0,		0,			0,			0,			0		}, //  15
		// ż©ŔŻşĐ łˇ

		// 2ąř Á÷±ş ±âş» ˝şĹł
		{   4,		16,			46,			76,			106		}, //  16
		{   4,		17,			47,			77,			107		}, //  17
		{   4,		18,			48,			78,			108		}, //  18
		{   4,		19,			49,			79,			109		}, //  19
		{   4,		20,			50,			80,			110		}, //  20
		{   4,		21,			51,			81,			111		}, //  21
		{   0,		0,			0,			0,			0		}, //  22
		{   0,		0,			0,			0,			0		}, //  23
		// 2ąř Á÷±ş ±âş» ˝şĹł łˇ

		// ż©ŔŻşĐ
		{   0,		0,			0,			0,			0		}, //  24
		{   0,		0,			0,			0,			0		}, //  25
		// ż©ŔŻşĐ łˇ

		// 1ąř Á÷±ş ¸¶˝şĹÍ ˝şĹł
#ifdef ENABLE_WOLFMAN_CHARACTER
		{   5,		1,			31,			61,			91,	170		}, //  26
		{   5,		2,			32,			62,			92,	171		}, //  27
		{   5,		3,			33,			63,			93,	172		}, //  28
		{   5,		4,			34,			64,			94,	173		}, //  29
		{   5,		5,			35,			65,			95,	174		}, //  30
		{   5,		6,			36,			66,			96,	175		}, //  31
#else
		{   4,		1,			31,			61,			91		}, //  26
		{   4,		2,			32,			62,			92		}, //  27
		{   4,		3,			33,			63,			93		}, //  28
		{   4,		4,			34,			64,			94		}, //  29
		{   4,		5,			35,			65,			95		}, //  30
		{   4,		6,			36,			66,			96		}, //  31
#endif
		{   0,		0,			0,			0,			0		}, //  32
		{   0,		0,			0,			0,			0		}, //  33
		// 1ąř Á÷±ş ¸¶˝şĹÍ ˝şĹł łˇ

		// ż©ŔŻşĐ
		{   0,		0,			0,			0,			0		}, //  34
		{   0,		0,			0,			0,			0		}, //  35
		{   0,		0,			0,			0,			0		}, //  36
		{   0,		0,			0,			0,			0		}, //  37
		{   0,		0,			0,			0,			0		}, //  38
		{   0,		0,			0,			0,			0		}, //  39
		{   0,		0,			0,			0,			0		}, //  40
		// ż©ŔŻşĐ łˇ

		// 2ąř Á÷±ş ¸¶˝şĹÍ ˝şĹł
		{   4,		16,			46,			76,			106		}, //  41
		{   4,		17,			47,			77,			107		}, //  42
		{   4,		18,			48,			78,			108		}, //  43
		{   4,		19,			49,			79,			109		}, //  44
		{   4,		20,			50,			80,			110		}, //  45
		{   4,		21,			51,			81,			111		}, //  46
		{   0,		0,			0,			0,			0		}, //  47
		{   0,		0,			0,			0,			0		}, //  48
		// 2ąř Á÷±ş ¸¶˝şĹÍ ˝şĹł łˇ

		// ż©ŔŻşĐ
		{   0,		0,			0,			0,			0		}, //  49
		{   0,		0,			0,			0,			0		}, //  50
		// ż©ŔŻşĐ łˇ

		// 1ąř Á÷±ş ±×·Łµĺ ¸¶˝şĹÍ ˝şĹł
#ifdef ENABLE_WOLFMAN_CHARACTER
		{   5,		1,			31,			61,			91,	170		}, //  51
		{   5,		2,			32,			62,			92,	171		}, //  52
		{   5,		3,			33,			63,			93,	172		}, //  53
		{   5,		4,			34,			64,			94,	173		}, //  54
		{   5,		5,			35,			65,			95,	174		}, //  55
		{   5,		6,			36,			66,			96,	175		}, //  56
#else
		{   4,		1,			31,			61,			91		}, //  51
		{   4,		2,			32,			62,			92		}, //  52
		{   4,		3,			33,			63,			93		}, //  53
		{   4,		4,			34,			64,			94		}, //  54
		{   4,		5,			35,			65,			95		}, //  55
		{   4,		6,			36,			66,			96		}, //  56
#endif
		{   0,		0,			0,			0,			0		}, //  57
		{   0,		0,			0,			0,			0		}, //  58
		// 1ąř Á÷±ş ±×·Łµĺ ¸¶˝şĹÍ ˝şĹł łˇ

		// ż©ŔŻşĐ
		{   0,		0,			0,			0,			0		}, //  59
		{   0,		0,			0,			0,			0		}, //  60
		{   0,		0,			0,			0,			0		}, //  61
		{   0,		0,			0,			0,			0		}, //  62
		{   0,		0,			0,			0,			0		}, //  63
		{   0,		0,			0,			0,			0		}, //  64
		{   0,		0,			0,			0,			0		}, //  65
		// ż©ŔŻşĐ łˇ

		// 2ąř Á÷±ş ±×·Łµĺ ¸¶˝şĹÍ ˝şĹł
		{   4,		16,			46,			76,			106		}, //  66
		{   4,		17,			47,			77,			107		}, //  67
		{   4,		18,			48,			78,			108		}, //  68
		{   4,		19,			49,			79,			109		}, //  69
		{   4,		20,			50,			80,			110		}, //  70
		{   4,		21,			51,			81,			111		}, //  71
		{   0,		0,			0,			0,			0		}, //  72
		{   0,		0,			0,			0,			0		}, //  73
		// 2ąř Á÷±ş ±×·Łµĺ ¸¶˝şĹÍ ˝şĹł łˇ

		//ż©ŔŻşĐ
		{   0,		0,			0,			0,			0		}, //  74
		{   0,		0,			0,			0,			0		}, //  75
		// ż©ŔŻşĐ łˇ

		// 1ąř Á÷±ş ĆŰĆĺĆ® ¸¶˝şĹÍ ˝şĹł
#ifdef ENABLE_WOLFMAN_CHARACTER
		{   5,		1,			31,			61,			91,	170		}, //  76
		{   5,		2,			32,			62,			92,	171		}, //  77
		{   5,		3,			33,			63,			93,	172		}, //  78
		{   5,		4,			34,			64,			94,	173		}, //  79
		{   5,		5,			35,			65,			95,	174		}, //  80
		{   5,		6,			36,			66,			96,	175		}, //  81
#else
		{   4,		1,			31,			61,			91		}, //  76
		{   4,		2,			32,			62,			92		}, //  77
		{   4,		3,			33,			63,			93		}, //  78
		{   4,		4,			34,			64,			94		}, //  79
		{   4,		5,			35,			65,			95		}, //  80
		{   4,		6,			36,			66,			96		}, //  81
#endif
		{   0,		0,			0,			0,			0		}, //  82
		{   0,		0,			0,			0,			0		}, //  83
		// 1ąř Á÷±ş ĆŰĆĺĆ® ¸¶˝şĹÍ ˝şĹł łˇ

		// ż©ŔŻşĐ
		{   0,		0,			0,			0,			0		}, //  84
		{   0,		0,			0,			0,			0		}, //  85
		{   0,		0,			0,			0,			0		}, //  86
		{   0,		0,			0,			0,			0		}, //  87
		{   0,		0,			0,			0,			0		}, //  88
		{   0,		0,			0,			0,			0		}, //  89
		{   0,		0,			0,			0,			0		}, //  90
		// ż©ŔŻşĐ łˇ

		// 2ąř Á÷±ş ĆŰĆĺĆ® ¸¶˝şĹÍ ˝şĹł
		{   4,		16,			46,			76,			106		}, //  91
		{   4,		17,			47,			77,			107		}, //  92
		{   4,		18,			48,			78,			108		}, //  93
		{   4,		19,			49,			79,			109		}, //  94
		{   4,		20,			50,			80,			110		}, //  95
		{   4,		21,			51,			81,			111		}, //  96
		{   0,		0,			0,			0,			0		}, //  97
		{   0,		0,			0,			0,			0		}, //  98
		// 2ąř Á÷±ş ĆŰĆĺĆ® ¸¶˝şĹÍ ˝şĹł łˇ

		// ż©ŔŻşĐ
		{   0,		0,			0,			0,			0		}, //  99
		{   0,		0,			0,			0,			0		}, //  100
		// ż©ŔŻşĐ łˇ

		// ±ćµĺ ˝şĹł
		{   1,  152,    0,    0,    0}, //  101
		{   1,  153,    0,    0,    0}, //  102
		{   1,  154,    0,    0,    0}, //  103
		{   1,  155,    0,    0,    0}, //  104
		{   1,  156,    0,    0,    0}, //  105
		{   1,  157,    0,    0,    0}, //  106
		// ±ćµĺ ˝şĹł łˇ

		// ż©ŔŻşĐ
		{   0,    0,    0,    0,    0}, //  107
		{   0,    0,    0,    0,    0}, //  108
		{   0,    0,    0,    0,    0}, //  109
		{   0,    0,    0,    0,    0}, //  110
		{   0,    0,    0,    0,    0}, //  111
		{   0,    0,    0,    0,    0}, //  112
		{   0,    0,    0,    0,    0}, //  113
		{   0,    0,    0,    0,    0}, //  114
		{   0,    0,    0,    0,    0}, //  115
		{   0,    0,    0,    0,    0}, //  116
		{   0,    0,    0,    0,    0}, //  117
		{   0,    0,    0,    0,    0}, //  118
		{   0,    0,    0,    0,    0}, //  119
		{   0,    0,    0,    0,    0}, //  120
		// ż©ŔŻşĐ łˇ

		// ˝Â¸¶ ˝şĹł
		{   2,  137,  140,    0,    0}, //  121
		{   1,  138,    0,    0,    0}, //  122
		{   1,  139,    0,    0,    0}, //  123
		// ˝Â¸¶ ˝şĹł łˇ
	};

	if (dwMotionIndex >= MOTION_MAX_NUM)
	{
		LOG_ERROR("OUT_OF_MOTION_VNUM: name={}, motion={}/{}", GetName(), dwMotionIndex, MOTION_MAX_NUM);
		return false;
	}

	uint32_t* skillVNums = s_anMotion2SkillVnumList[dwMotionIndex];

	uint32_t skillCount = *skillVNums++;
	if (skillCount >= SKILL_LIST_MAX_COUNT)
	{
		LOG_ERROR("OUT_OF_SKILL_LIST: name={}, count={}/{}", GetName(), skillCount, static_cast<int>(SKILL_LIST_MAX_COUNT));
		return false;
	}

	for (uint32_t skillIndex = 0; skillIndex != skillCount; ++skillIndex)
	{
		if (skillIndex >= SKILL_MAX_NUM)
		{
			LOG_ERROR("OUT_OF_SKILL_VNUM: name={}, skill={}/{}", GetName(), skillIndex, static_cast<int>(SKILL_MAX_NUM));
			return false;
		}

		uint32_t eachSkillVNum = skillVNums[skillIndex];
		if ( eachSkillVNum != 0 )
		{
			uint32_t eachJobGroup = s_anSkill2JobGroup[eachSkillVNum];

			if (0 == eachJobGroup || eachJobGroup == selfJobGroup)
			{
				// GUILDSKILL_BUG_FIX
				uint32_t eachSkillLevel = 0;

				if (eachSkillVNum >= GUILD_SKILL_START && eachSkillVNum <= GUILD_SKILL_END)
				{
					if (GetGuild())
						eachSkillLevel = GetGuild()->GetSkillLevel(eachSkillVNum);
					else
						eachSkillLevel = 0;
				}
				else
				{
					eachSkillLevel = GetSkillLevel(eachSkillVNum);
				}

				if (eachSkillLevel > 0)
				{
					return true;
				}
				// END_OF_GUILDSKILL_BUG_FIX
			}
		}
	}

	return false;
}

void CHARACTER::ClearSkill()
{
	SkillSystem::ClearSkill(GetEntityHandle());
}

void CHARACTER::ClearSubSkill()
{
	SkillSystem::ClearSubSkill(GetEntityHandle());
}

bool CHARACTER::ResetOneSkill(uint32_t dwVnum)
{
	return SkillSystem::ResetOneSkill(GetEntityHandle(), dwVnum);
}

eMountType GetMountLevelByVnum(uint32_t dwMountVnum, bool IsNew) // updated to 2014/12/10
{
	if (!dwMountVnum)
		return MOUNT_TYPE_NONE;

	switch (dwMountVnum)
	{
		// ### YES SKILL
		// @fixme116 begin
		case 20107: // normal military horse (no guild)
		case 20108: // normal military horse (guild member)
		case 20109: // normal military horse (guild master)
			if (IsNew)
				return MOUNT_TYPE_NONE;
		// @fixme116 end
		// Classic
		case 20110: // Classic Boar
		case 20111: // Classic Wolf
		case 20112: // Classic Tiger
		case 20113: // Classic Lion
		case 20114: // White Lion
		// Special Lv2
		case 20115: // Wild Battle Boar
		case 20116: // Fight Wolf
		case 20117: // Storm Tiger
		case 20118: // Battle Lion (bugged)
		case 20205: // Wild Battle Boar (alternative)
		case 20206: // Fight Wolf (alternative)
		case 20207: // Storm Tiger (alternative)
		case 20208: // Battle Lion (bugged) (alternative)
		// Royal Tigers
		case 20120: // blue
		case 20121: // dark red
		case 20122: // gold
		case 20123: // green
		case 20124: // pied
		case 20125: // white
		// Royal mounts (Special Lv3)
		case 20209: // Royal Boar
		case 20210: // Royal Wolf
		case 20211: // Royal Tiger
		case 20212: // Royal Lion
		//
		case 20215: // Rudolph m Lv3 (yes skill, yes atk)
		case 20218: // Rudolph f Lv3 (yes skill, yes atk)
		case 20225: // Dyno Lv3 (yes skill, yes atk)
		case 20230: // Turkey Lv3 (yes skill, yes atk)
			return MOUNT_TYPE_MILITARY;
			break;
		// ### NO SKILL YES ATK
		// @fixme116 begin
		case 20101:
		case 20102:
		case 20103:
		case 20104: // normal combat horse (no guild)
		case 20105: // normal combat horse (guild member)
		case 20106: // normal combat horse (guild master)
			if (IsNew)
				return MOUNT_TYPE_NONE;
		// @fixme116 end
		case 20119: // Black Horse (no skill, yes atk)
		case 20214: // Rudolph m Lv2 (no skill, yes atk)
		case 20217: // Rudolph f Lv2 (no skill, yes atk)
		case 20219: // Equus Porphyreus (no skill, yes atk)
		case 20220: // Comet (no skill, yes atk)
		case 20221: // Polar Predator (no skill, yes atk)
		case 20222: // Armoured Panda (no skill, yes atk)
		case 20224: // Dyno Lv2 (no skill, yes atk)
		case 20226: // Nightmare (no skill, yes atk)
		case 20227: // Unicorn (no skill, yes atk)
		case 20229: // Turkey Lv2 (no skill, yes atk)
		case 20231: // Leopard (no skill, yes atk)
		case 20232: // Black Panther (no skill, yes atk)
			return MOUNT_TYPE_COMBAT;
			break;
		// ### NO SKILL NO ATK
		// @fixme116 begin
		//case 20101: // normal beginner horse (no guild)		 // Ixtreeme
		//case 20102: // normal beginner horse (guild member)	 // Ixtreeme
		//case 20103: // normal beginner horse (guild master)	 // Ixtreeme
		//	if (IsNew)											 // Ixtreeme
		//		return MOUNT_TYPE_NONE;							 // Ixtreeme
		// @fixme116 end
		case 20213: // Rudolph m Lv1 (no skill, no atk)
		case 20216: // Rudolph f Lv1 (no skill, no atk)
		// Special Lv1
		case 20201: // Boar Lv1 (no skill, no atk)
		case 20202: // Wolf Lv1 (no skill, no atk)
		case 20203: // Tiger Lv1 (no skill, no atk)
		case 20204: // Lion Lv1 (no skill, no atk)
		//
		case 20223: // Dyno Lv1 (no skill, no atk)
		case 20228: // Turkey Lv1 (no skill, no atk)
			return MOUNT_TYPE_NORMAL;
			break;
		default:
			return MOUNT_TYPE_NONE;
			break;
	}
}

const int SKILL_COUNT = 6;
static const uint32_t SkillList[JOB_MAX_NUM][SKILL_GROUP_MAX_NUM][SKILL_COUNT] =
{
	{ {	1,	2,	3,	4,	5,	6	}, {	16,	17,	18,	19,	20,	21	} },
	{ {	31,	32,	33,	34,	35,	36	}, {	46,	47,	48,	49,	50,	51	} },
	{ {	61,	62,	63,	64,	65,	66	}, {	76,	77,	78,	79,	80,	81	} },
	{ {	91,	92,	93,	94,	95,	96	}, {	106,107,108,109,110,111	} },
#ifdef ENABLE_WOLFMAN_CHARACTER
	{ {	170,171,172,173,174,175	}, {	0,	0,	0,	0,	0,	0	} },
#endif
};

const uint32_t GetRandomSkillVnum(uint8_t bJob)
{
	// the chosen skill
	uint32_t dwSkillVnum = 0;
	do
	{
		// tmp stuff
		uint32_t tmpJob = (bJob != JOB_MAX_NUM)?MINMAX(0, bJob, JOB_MAX_NUM-1):number(0, JOB_MAX_NUM-1);
		uint32_t tmpSkillGroup = number(0, SKILL_GROUP_MAX_NUM-1);
		uint32_t tmpSkillCount = number(0, SKILL_COUNT-1);
		// set skill
		dwSkillVnum = SkillList[tmpJob][tmpSkillGroup][tmpSkillCount];

#if defined(ENABLE_WOLFMAN_CHARACTER) && !defined(USE_WOLFMAN_BOOKS)
		if (tmpJob==JOB_WOLFMAN)
			continue;
#endif

		if (dwSkillVnum != 0 && nullptr != CSkillManager::instance().Get(dwSkillVnum))
			break;
	} while (true);
	return dwSkillVnum;
}


