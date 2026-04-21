#include "../../stdafx.h"

#include "CombatSystem.hpp"

#include <algorithm>
#include <boost/algorithm/string/find.hpp>
#include <random>
#include <thread>
#include <utility>

#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/status_components.hpp"
#include "../components/vital_components.hpp"
#include "../CharacterAccessors.hpp"
#include "../AIHelpers.hpp"
#include "../SpatialHelpers.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include "../Registry.hpp"
#include "../VIDRegistry.hpp"
#include "../../utils.h"
#include "../../config.h"
#include "../../constants.h"
#include "../../desc.h"
#include "../../desc_manager.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../mob_manager.h"
#include "../../battle.h"
#include "../../pvp.h"
#include "../../skill.h"
#include "../../start_position.h"
#include "../../profiler.h"
#include "../../cmd.h"
#include "../../dungeon.h"
#include "../../log.h"
#include "../../unique_item.h"
#include "../../priv_manager.h"
#include "../../db.h"
#include "../../vector.h"
#include "../../marriage.h"
#include "../../arena.h"
#include "../../regen.h"
#include "../../exchange.h"
#include "../../shop_manager.h"
#include "../../dev_log.h"
#include "../../ani.h"
#include "../../BattleArena.h"
#include "../../packet.h"
#include "../../party.h"
#include "../../affect.h"
#include "../../guild.h"
#include "../../guild_manager.h"
#include "../../questmanager.h"
#include "../../questlua.h"
#ifdef __NEWPET_SYSTEM__
#include "../../New_PetSystem.h"
#endif
#ifdef ENABLE_BATTLE_PASS
#include "../../battle_pass.h"
#endif
#ifdef ENABLE_DUNGEON_SHARED_DROP_HWID
#include <unordered_map>
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
#endif

#ifdef ENABLE_EVENT_MANAGER
extern void Map1MassSpawnEvent_OnMobDead(uint32_t vid);
#endif

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);

static inline LegacyCharHandle LegacyCharOf(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return nullptr;
    }

    auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e);
    return legacy ? legacy->ptr : nullptr;
}

static inline entt::entity EntityOf(LegacyCharHandle ch)
{
    if (!ch) {
        return entt::null;
    }

    return AIHelpers::EcsOf(ch);
}

static inline ecs::CharacterRuntimeFlagsComponent* RuntimeFlags(LegacyCharHandle ch)
{
    return ecs::TryGetRuntimeFlags(EntityOf(ch));
}

static inline const ecs::CharacterRuntimeFlagsComponent* RuntimeFlags(const CHARACTER* ch)
{
    return ecs::TryGetRuntimeFlags(EntityOf(const_cast<CHARACTER*>(ch)));
}

static inline bool HasMoveState(LegacyCharHandle ch)
{
    const entt::entity e = EntityOf(ch);
    return e != entt::null && g_registry.valid(e) &&
        g_registry.all_of<ecs::MovementDestination>(e);
}

namespace CombatSystem {

bool CanBeginFight(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        return ch->CanBeginFight();
    }

    return false;
}

void BeginFight(entt::entity attacker, entt::entity victim)
{
    if (auto* ch = LegacyCharOf(attacker)) {
        ch->BeginFight(LegacyCharOf(victim));
    }
}

bool CanFight(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        return ch->CanFight();
    }

    return false;
}

bool Attack(entt::entity attacker, entt::entity victim, uint8_t attackType)
{
    if (auto* ch = LegacyCharOf(attacker)) {
        return ch->Attack(LegacyCharOf(victim), attackType);
    }

    return false;
}

bool Shoot(entt::entity attacker, uint8_t attackType)
{
    if (auto* ch = LegacyCharOf(attacker)) {
        return ch->Shoot(attackType);
    }

    return false;
}

void SetVictim(entt::entity attacker, entt::entity victim)
{
    if (auto* ch = LegacyCharOf(attacker)) {
        ch->SetVictim(LegacyCharOf(victim));
    }
}

entt::entity GetVictim(entt::entity attacker)
{
    if (auto* ch = LegacyCharOf(attacker)) {
        return EntityOf(ch->GetVictim());
    }

    return entt::null;
}

entt::entity GetNearestVictim(entt::entity attacker, entt::entity from)
{
    if (auto* ch = LegacyCharOf(attacker)) {
        return EntityOf(ch->GetNearestVictim(LegacyCharOf(from)));
    }

    return entt::null;
}

bool IsStun(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        return ch->IsStun();
    }

    return false;
}

void Stun(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->Stun();
    }
}

bool IsDead(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        return ch->IsDead();
    }

    return true;
}

void SetLastAttacked(entt::entity e, uint32_t tick)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->SetLastAttacked(tick);
    }
}


void DeathPenalty(entt::entity e, uint8_t bTown)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->DeathPenalty(bTown);
    }
}


void RewardGold(entt::entity victim, entt::entity attacker)
{
    if (auto* ch = LegacyCharOf(victim)) {
        ch->RewardGold(LegacyCharOf(attacker));
    }
}


void Reward(entt::entity victim, bool bItemDrop)
{
    if (auto* ch = LegacyCharOf(victim)) {
        ch->Reward(bItemDrop);
    }
}


void ItemDropPenalty(entt::entity victim, entt::entity killer)
{
    if (auto* ch = LegacyCharOf(victim)) {
        ch->ItemDropPenalty(LegacyCharOf(killer));
    }
}


void DistributeSP(entt::entity victim, entt::entity killer, int iMethod)
{
    if (auto* ch = LegacyCharOf(victim)) {
        ch->DistributeSP(LegacyCharOf(killer), iMethod);
    }
}


uint32_t GetAlignment(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        return ch->GetAlignment();
    }

    return 0;
}

uint32_t GetRealAlignment(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        return ch->GetRealAlignment();
    }

    return 0;
}

uint8_t GetAlignmentGrade(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        return ch->GetAlignmentGrade();
    }

    return 0;
}

void ApplyAlignmentBonus(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->ApplyAlignmentBonus();
    }
}

void UpdateAlignment(entt::entity e, uint32_t amount)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->UpdateAlignment(amount);
    }
}

void SetKillerMode(entt::entity e, bool isOn)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->SetKillerMode(isOn);
    }
}

bool IsKillerMode(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        return ch->IsKillerMode();
    }

    return false;
}

void UpdateKillerMode(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->UpdateKillerMode();
    }
}

void SetPKMode(entt::entity e, uint8_t bPKMode)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->SetPKMode(bPKMode);
    }
}

uint8_t GetPKMode(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        return ch->GetPKMode();
    }

    return PK_MODE_PROTECT;
}


void ForgetMyAttacker(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->ForgetMyAttacker();
    }
}

void AggregateMonster(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->AggregateMonster();
    }
}

void AggregateMonsterPlus(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->AggregateMonsterPlus();
    }
}

void AttractRanger(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->AttractRanger();
    }
}

void PullMonster(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->PullMonster();
    }
}


void SendLeaderboardData(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->SendLeaderboardData();
    }
}

void SendLeaderboardDataSkillMob(entt::entity e, entt::entity viewer)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->SendLeaderboardDataSkillMob(LegacyCharOf(viewer));
    }
}

void SendLeaderboardDataGuild(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->SendLeaderboardDataGuild();
    }
}

void CheckLeaderboardSkillMobChanges(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->CheckLeaderboardSkillMobChanges();
    }
}

} // namespace CombatSystem

// char_battle.cpp slice BE1 moved into CombatSystem.cpp

uint32_t CHARACTER::GetAlignment() const
{
	return m_iAlignment;
}

uint32_t CHARACTER::GetRealAlignment() const
{
	return m_iRealAlignment;
}

//void CHARACTER::ShowAlignment(bool bShow)
//{
//	if (bShow)
//	{
//		if (m_iAlignment != m_iRealAlignment)
//		{
//			m_iAlignment = m_iRealAlignment;
//			UpdatePacket();
//		}
//	}
//	else
//	{
//		if (m_iAlignment != 0)
//		{
//			m_iAlignment = 0;
//			UpdatePacket();
//		}
//	}
//}

uint8_t CHARACTER::GetAlignmentGrade() const
{
	uint32_t a = GetRealAlignment() / 10;

	if (a <= 4999) return 0;
	if (a <= 14999) return 1;
	if (a <= 19999) return 2;
	if (a <= 29999) return 3;
	if (a <= 49999) return 4;
	if (a <= 74999) return 5;
	if (a <= 99999) return 6;
	if (a <= 124999) return 7;
	if (a <= 174999) return 8;
	if (a <= 249999) return 9;
	if (a <= 499999) return 10;
	if (a <= 749999) return 11;
	if (a <= 999999) return 12;
	if (a <= 1499999) return 13;
	if (a <= 2499999) return 14;
	if (a <= 2999999) return 15;
	if (a <= 3499999) return 16;
	if (a <= 3999999) return 17;
	if (a <= 4499999) return 18;
	if (a <= 4999999) return 19;
	return 20;
}


void CHARACTER::ApplyAlignmentBonus()
{
	if (!IsPC()) return;
	const uint8_t g = GetAlignmentGrade();

	static const int hp[21] = { 500,1000,1500,2000,2500,4000,6000,8000,10000,12000,14000,16000,18000,20000,25000,30000,35000,40000,45000,50000,60000 };
	static const int mon[21] = { 1,3,5,7,9,12,15,18,21,25,25,30,35,40,50,55,60,65,70,75,85 };
	static const int hum[21] = { 1,3,5,7,9,12,15,18,21,25,25,30,35,40,50,55,60,65,70,75,85 };
	static const int met[21] = { 0,0,0,0,0,0,0,0,5,10,15,20,25,30,35,40,45,50,55,60,70 };
	static const int boss[21] = { 0,0,0,0,0,0,0,0,0,5,10,15,20,25,30,35,40,45,50,55,65 };
	static const int pvm[21] = { 0,0,0,0,0,5,5,5,5,5,10,10,15,20,25,30,35,40,45,50,60 };
	static const int normal[21] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,5,10,15,20,25,30,35,45 };
	static const int skill[21] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,5,10,15,20,25,30,35,45 };
	// grade nem vltozott -> a cache j, nem kell jraszmolni
	if (g == m_lastAlignmentGrade)
		return;

	// cache frissts (ezek tllnek ComputePoints kztt)
	m_alignBonusHP = hp[g];
	m_alignBonusMonster = mon[g];
	m_alignBonusHuman = hum[g];
	m_alignBonusMetin = met[g];
	m_alignBonusBoss = boss[g];
	m_alignBonusPvm = pvm[g];
	m_alignBonusNormal = normal[g];
	m_alignBonusSkill = skill[g];

	m_lastAlignmentGrade = g;
}

void CHARACTER::UpdateAlignment(uint32_t iAmount)
{
	//if (!IsPC()) return;
	const uint8_t oldGrade = GetAlignmentGrade();
	bool bShow = false;

	if (m_iAlignment == m_iRealAlignment)
		bShow = true;

	if (m_iAlignment != m_iRealAlignment)
		m_iAlignment = m_iRealAlignment;

	uint32_t i = m_iAlignment / 10;

	m_iRealAlignment = UMINMAX(0, m_iRealAlignment + iAmount, 50000000);
	const uint8_t newGrade = GetAlignmentGrade();
	if (oldGrade != newGrade)
	{
		ComputePoints(); // ekkor vltozik a cache + jraplnek pontok
	}
	if (bShow)
	{
		m_iAlignment = m_iRealAlignment;

		if (i != m_iAlignment / 10)
			UpdatePacket();
	}

}
//void CHARACTER::UpdateAlignment(uint32_t iAmount)
//{
//	const uint8_t oldGrade = GetAlignmentGrade();
//
//	m_iRealAlignment = UMINMAX(0, m_iRealAlignment + iAmount, 2500000);
//
//	if (m_iAlignment != m_iRealAlignment)
//		m_iAlignment = m_iRealAlignment;
//
//
//	const uint8_t newGrade = GetAlignmentGrade();
//
//	if (oldGrade != newGrade)
//		ComputePoints(); // ekkor vltozik a cache + jraplnek pontok
//	else
//		UpdatePacket();   
//}


void CHARACTER::SetKillerMode(bool isOn)
{
	if ((isOn ? ADD_CHARACTER_STATE_KILLER : 0) == IS_SET(m_bAddChrState, ADD_CHARACTER_STATE_KILLER))
		return;

	if (isOn)
		SET_BIT(m_bAddChrState, ADD_CHARACTER_STATE_KILLER);
	else
		REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_KILLER);

	m_iKillerModePulse = thecore_pulse();
	UpdatePacket();
	sys_log(0, "SetKillerMode Update %s[%d]", GetName(), GetPlayerID());
}

bool CHARACTER::IsKillerMode() const
{
	return IS_SET(m_bAddChrState, ADD_CHARACTER_STATE_KILLER);
}

void CHARACTER::UpdateKillerMode()
{
	if (!IsKillerMode())
		return;

	if (thecore_pulse() - m_iKillerModePulse >= PASSES_PER_SEC(30))
		SetKillerMode(false);

}

void CHARACTER::SetPKMode(uint8_t bPKMode)
{
	if (bPKMode >= PK_MODE_MAX_NUM)
		return;

	if (m_bPKMode == bPKMode)
		return;

	if (bPKMode == PK_MODE_GUILD && !GetGuild())
		bPKMode = PK_MODE_FREE;

	m_bPKMode = bPKMode;
	UpdatePacket();
	sys_log(0, "PK_MODE: %s %d", GetName(), m_bPKMode);
}

uint8_t CHARACTER::GetPKMode() const
{
	return m_bPKMode;
}

struct FuncForgetMyAttacker
{
	LegacyCharHandle m_ch;
	FuncForgetMyAttacker(LegacyCharHandle ch)
	{
		m_ch = ch;
	}
	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			if (ch->IsPC())
				return;
			if (ch->m_eVictim == AIHelpers::EcsOf(m_ch))
				ch->SetVictim(nullptr);
		}
	}
};

struct FuncAggregateMonster
{
	LegacyCharHandle m_ch;
	FuncAggregateMonster(LegacyCharHandle ch)
	{
		m_ch = ch;
	}
	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			if (ch->GetVictim())
				return;

			//if (number(1, 100) <= 50) // ӽ÷ 50% Ȯ  ´
			if (DISTANCE_APPROX(ch->GetX() - m_ch->GetX(), ch->GetY() - m_ch->GetY()) < 7000)
				if (ch->CanBeginFight())
					ch->BeginFight(m_ch);
		}
	}
};
#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
struct FuncAggregateMonsterPlus
{
	LegacyCharHandle m_ch;
	FuncAggregateMonsterPlus(LegacyCharHandle ch)
	{
		m_ch = ch;
	}
	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			if (ch->GetVictim())
				return;

			const int AGGRO_RANGE = 14000;

			if (DISTANCE_APPROX(ch->GetX() - m_ch->GetX(), ch->GetY() - m_ch->GetY()) < AGGRO_RANGE)
				if (ch->CanBeginFight())
					ch->BeginFight(m_ch);

		}
	}
};
#endif
struct FuncAttractRanger
{
	LegacyCharHandle m_ch;
	FuncAttractRanger(LegacyCharHandle ch)
	{
		m_ch = ch;
	}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			if (ch->GetVictim() && ch->GetVictim() != m_ch)
				return;
			if (ch->GetMobAttackRange() > 150)
			{
				int iNewRange = 150;//(int)(ch->GetMobAttackRange() * 0.2);
				if (iNewRange < 150)
					iNewRange = 150;

				ch->AddAffect(AFFECT_BOW_DISTANCE, POINT_BOW_DISTANCE, iNewRange - ch->GetMobAttackRange(), AFF_NONE, 3 * 60, 0, false);
			}
		}
	}
};

struct FuncPullMonster
{
	LegacyCharHandle m_ch;
	int m_iLength;
	FuncPullMonster(LegacyCharHandle ch, int iLength = 300)
	{
		m_ch = ch;
		m_iLength = iLength;
	}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			//if (ch->GetVictim() && ch->GetVictim() != m_ch)
			//return;
			float fDist = DISTANCE_APPROX(m_ch->GetX() - ch->GetX(), m_ch->GetY() - ch->GetY());
			if (fDist > 3000 || fDist < 100)
				return;

			float fNewDist = fDist - m_iLength;
			if (fNewDist < 100)
				fNewDist = 100;

			float degree = GetDegreeFromPositionXY(ch->GetX(), ch->GetY(), m_ch->GetX(), m_ch->GetY());
			float fx;
			float fy;

			GetDeltaByDegree(degree, fDist - fNewDist, &fx, &fy);
			int32_t tx = (int32_t)(ch->GetX() + fx);
			int32_t ty = (int32_t)(ch->GetY() + fy);

			ch->Sync(tx, ty);
			ch->Goto(tx, ty);
			ch->CalculateMoveDuration();

			ch->SyncPacket();
		}
	}
};


// char_battle.cpp slice BE2a moved into CombatSystem.cpp

void CHARACTER::ForgetMyAttacker()
{
	FuncForgetMyAttacker f(this);
	ecs::ForEachAround(g_registry, AIHelpers::EcsOf(this), f);
	ReviveInvisible(5);
}

void CHARACTER::AggregateMonster()
{
	FuncAggregateMonster f(this);
	ecs::ForEachAround(g_registry, AIHelpers::EcsOf(this), f);
}

#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
void CHARACTER::AggregateMonsterPlus()
{
	FuncAggregateMonsterPlus f(this);
	ecs::ForEachAround(g_registry, AIHelpers::EcsOf(this), f);
}
#endif
void CHARACTER::AttractRanger()
{
	FuncAttractRanger f(this);
	ecs::ForEachAround(g_registry, AIHelpers::EcsOf(this), f);
}

void CHARACTER::PullMonster()
{
	FuncPullMonster f(this);
	ecs::ForEachAround(g_registry, AIHelpers::EcsOf(this), f);
}


// char_battle.cpp slice BE3 moved into CombatSystem.cpp

#ifdef LEADERBOARD_RAZOR93


void CHARACTER::SendLeaderboardData()
{
	if (!GetDesc())
		return;

	// SQL lek?dez? top 10 j??osra
	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
		"SELECT name, level, r5, r8 FROM player.player ORDER BY r5 DESC LIMIT 10"));


	//if (!pMsg || !pMsg->Get()->uiNumRows)
	//{
	//	ecs::ChatSystem::Send(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, "Nincs leaderboard adat.");
	//	return;
	//}

	MYSQL_ROW row;
	MYSQL_RES* res = pMsg->Get()->pSQLResult;

	std::string result;

	while ((row = mysql_fetch_row(res)))
	{
		const char* name = row[0] ? row[0] : "Unknown";
		int level = row[1] ? atoi(row[1]) : 0;
		int metins = row[2] ? atoi(row[2]) : 0;
		int dmg = row[3] ? atoi(row[3]) : 0;

		char line[128];
		snprintf(line, sizeof(line), "%s;%d;%d;%d\n", name, level, metins, dmg);
		result += line;
	}

	// K?d? kliensnek
	TPacketGCLeaderboard p;
	p.header = HEADER_GC_LEADERBOARD_DATA;
	strlcpy(p.data, result.c_str(), sizeof(p.data));

	GetDesc()->Packet(&p, sizeof(p));


}


void CHARACTER::SendLeaderboardDataSkillMob(LPCHARACTER viewer)
{
	if (!viewer || !viewer->GetDesc())
		return;

	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
		"SELECT name, level, map1_skillmob, skill_victim "
		"FROM player.player ORDER BY map1_skillmob DESC LIMIT 10"));

	MYSQL_ROW row;
	MYSQL_RES* res = pMsg->Get()->pSQLResult;

	std::string result;

	while ((row = mysql_fetch_row(res)))
	{
		const char* name = row[0] ? row[0] : "Unknown";
		int level = row[1] ? atoi(row[1]) : 0;
		int dmg = row[2] ? atoi(row[2]) : 0;
		const char* victim = row[3] ? row[3] : "None";

		char line[256];

		snprintf(line, sizeof(line), "%s;%d;%s;%d\n", name, level, victim, dmg);

		result += line;
	}

	TPacketGCLeaderboardNews p;
	p.header = HEADER_GC_LEADERBOARD_NEWS;
	strlcpy(p.data, result.c_str(), sizeof(p.data));

	viewer->GetDesc()->Packet(&p, sizeof(p));
}

#ifdef LEADERBOARD_RAZOR93
void CHARACTER::SendLeaderboardDataGuild()
{
	if (!GetDesc())
		return;

	char szQuery[512];
	snprintf(szQuery, sizeof(szQuery),
		"SELECT g.name, IFNULL(p.name,'Unknown') AS master_name, g.win, g.draw, g.loss "
		"FROM player.guild%s AS g "
		"LEFT JOIN player.player%s AS p ON p.id = g.master "
		"ORDER BY (g.win - g.loss) DESC, g.win DESC, g.draw DESC, g.loss ASC "
		"LIMIT 10",
		get_table_postfix(), get_table_postfix());

	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(szQuery));
	if (!pMsg || !pMsg->Get() || !pMsg->Get()->pSQLResult)
		return;

	MYSQL_RES* res = pMsg->Get()->pSQLResult;
	MYSQL_ROW row;

	std::string result;
	result.reserve(1024);

	while ((row = mysql_fetch_row(res)))
	{
		const char* guildName = (row[0] && row[0][0]) ? row[0] : "Unknown";
		const char* masterName = (row[1] && row[1][0]) ? row[1] : "Unknown";

		int win = row[2] ? atoi(row[2]) : 0;
		int draw = row[3] ? atoi(row[3]) : 0;
		int loss = row[4] ? atoi(row[4]) : 0;
 
		char line[256];
		snprintf(line, sizeof(line), "%s;%s;%d;%d;%d\n", guildName, masterName, win, draw, loss);
		result += line;
	}

	TPacketGCLeaderboard p;
	p.header = HEADER_GC_LEADERBOARD_GUILD;
	strlcpy(p.data, result.c_str(), sizeof(p.data));

	GetDesc()->Packet(&p, sizeof(p));
}
#endif


#ifdef LEADERBOARD_RAZOR93

std::vector<LeaderboardEntry> CHARACTER::FetchTop10SkillMob()
{
	std::vector<LeaderboardEntry> list;
	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
		"SELECT name, level, skill_victim, map1_skillmob "
		"FROM player.player ORDER BY map1_skillmob DESC LIMIT 10"));

	MYSQL_ROW row;
	MYSQL_RES* res = pMsg->Get()->pSQLResult;

	while ((row = mysql_fetch_row(res)))
	{
		LeaderboardEntry e;
		e.name = row[0] ? row[0] : "Unknown";
		e.level = row[1] ? atoi(row[1]) : 0;
		e.victim = row[2] ? row[2] : "None";
		e.dmg = row[3] ? atoi(row[3]) : 0;
		list.push_back(e);
	}
	return list;
}


void CHARACTER::CheckLeaderboardSkillMobChanges()
{
	static std::vector<LeaderboardEntry> s_lastTop10;
	auto current = FetchTop10SkillMob();

	if (current.size() != s_lastTop10.size())
	{
		s_lastTop10 = current;
		return;
	}

	for (size_t i = 0; i < current.size(); ++i)
	{
		if (i >= s_lastTop10.size()) break;
		if (current[i].name != s_lastTop10[i].name ||
			current[i].dmg != s_lastTop10[i].dmg ||
			current[i].victim != s_lastTop10[i].victim)
		{
			char buf[512];
			snprintf(buf, sizeof(buf),
				"|cFFFF00FF[SKILL LEADERBOARD]|r: "
				"|cFFFFA500%s|r "
				"vs |cFF87CEFA%s|r "
				"|cFFFFFF00skill damage|r "
				"|cFF00FF00%d|r. "
				"|cFFFFFF00Place|r: |cFFFFA500%zu.|r",
				current[i].name.c_str(),
				current[i].victim.c_str(),
				current[i].dmg,
				i + 1);

			BroadcastNotice(buf);
			break;
		}
	}

	s_lastTop10 = current;
}


#endif

#endif

// char_battle.cpp slice BE2b moved into CombatSystem.cpp

void CHARACTER::UpdateAggrPointEx(LPCHARACTER pAttacker, EDamageType type, int dam, CHARACTER::TBattleInfo& info)
{
	// Ư ŸԿ   ö󰣴
	switch (type)
	{
	case DAMAGE_TYPE_NORMAL_RANGE:
		dam = (int)(dam * 1.2f);
		break;

	case DAMAGE_TYPE_RANGE:
		dam = (int)(dam * 1.5f);
		break;

	case DAMAGE_TYPE_MAGIC:
		dam = (int)(dam * 1.2f);
		break;

	default:
		break;
	}

	// ڰ    ʽ ش.
	if (pAttacker == GetVictim())
		dam = (int)(dam * 1.2f);

	info.iAggro += dam;

	if (info.iAggro < 0)
		info.iAggro = 0;

	//sys_log(0, "UpdateAggrPointEx for %s by %s dam %d total %d", GetName(), pAttacker->GetName(), dam, total);
	if (GetParty() && dam > 0 && type != DAMAGE_TYPE_SPECIAL)
	{
		LPPARTY pParty = GetParty();

		//     ϴ
		int iPartyAggroDist = dam;

		if (pParty->GetLeaderPID() == GetPacketVID())
			iPartyAggroDist /= 2;
		else
			iPartyAggroDist /= 3;

		pParty->SendMessage(this, PM_AGGRO_INCREASE, iPartyAggroDist, pAttacker->GetPacketVID());
	}

	ChangeVictimByAggro(info.iAggro, pAttacker);
}

void CHARACTER::UpdateAggrPoint(LPCHARACTER pAttacker, EDamageType type, int dam)
{
	if (IsDead() || IsStun())
		return;

	const entt::entity eAttacker = EntityOf(pAttacker);
	if (eAttacker == entt::null)
		return;

	TDamageMap::iterator it = m_map_kDamage.find(eAttacker);

	if (it == m_map_kDamage.end())
	{
		m_map_kDamage.insert(TDamageMap::value_type(eAttacker, TBattleInfo(0, dam)));
		it = m_map_kDamage.find(eAttacker);
	}

	UpdateAggrPointEx(pAttacker, type, dam, it->second);
}

void CHARACTER::ChangeVictimByAggro(int iNewAggro, LPCHARACTER pNewVictim)
{
	if (get_dword_time() - m_dwLastVictimSetTime < 3000) // 3ʴ ٷѴ
		return;

	if (pNewVictim == GetVictim())
	{
		if (m_iMaxAggro < iNewAggro)
		{
			m_iMaxAggro = iNewAggro;
			return;
		}

		// Aggro  
		TDamageMap::iterator it;
		TDamageMap::iterator itFind = m_map_kDamage.end();

		for (it = m_map_kDamage.begin(); it != m_map_kDamage.end(); ++it)
		{
			if (it->second.iAggro > iNewAggro)
			{
				auto* ch = LegacyCharOf(it->first);

				if (ch && !ch->IsDead() && DISTANCE_APPROX(ch->GetX() - GetX(), ch->GetY() - GetY()) < 5000)
				{
					itFind = it;
					iNewAggro = it->second.iAggro;
				}
			}
		}

		if (itFind != m_map_kDamage.end())
		{
			m_iMaxAggro = iNewAggro;
#ifdef __DEFENSE_WAVE__
			if (!IsDefanceWaweMastAttackMob(GetRaceNum()))
			{
				SetVictim(LegacyCharOf(itFind->first));
			}
#else
			SetVictim(LegacyCharOf(itFind->first));
#endif
			m_dwStateDuration = 1;
		}
	}
	else
	{
		if (m_iMaxAggro < iNewAggro)
		{
			m_iMaxAggro = iNewAggro;
#ifdef __DEFENSE_WAVE__
			if (!IsDefanceWaweMastAttackMob(GetRaceNum()))
			{
				SetVictim(pNewVictim);
			}
#else
			SetVictim(pNewVictim);
#endif
			m_dwStateDuration = 1;
		}
	}
}


// char_battle.cpp slice BD2b moved into CombatSystem.cpp

static uint32_t __GetPartyExpNP(const uint32_t level);
static uint32_t AdjustExpByLevel_Combat(const LegacyCharHandle ch, const uint32_t exp);

void CHARACTER::DistributeHP(LPCHARACTER pkKiller)
{
	if (pkKiller->GetDungeon()) //  ΰʴ´
		return;
}
#define ENABLE_NEWEXP_CALCULATION
#ifdef ENABLE_NEWEXP_CALCULATION
#define NEW_GET_LVDELTA(me, victim) aiPercentByDeltaLev[MINMAX(0, (victim + 15) - me, MAX_EXP_DELTA_OF_LEV - 1)]
typedef long double rate_t;
static void GiveExp(LegacyCharHandle from, LegacyCharHandle to, int iExp)
{
	if (test_server && iExp < 0)
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(to), CHAT_TYPE_INFO, "exp(%d) overflow", iExp);
		return;
	}
	// decrease/increase exp based on player<>mob level
	rate_t lvFactor = static_cast<rate_t>(NEW_GET_LVDELTA(to->GetLevel(), from->GetLevel())) / 100.0L;
	iExp *= lvFactor;
	// start calculating rate exp bonus
	int iBaseExp = iExp;
	rate_t rateFactor = 100;

	rateFactor += CPrivManager::instance().GetPriv(to, PRIV_EXP_PCT);
	if (to->IsEquipUniqueItem(UNIQUE_ITEM_LARBOR_MEDAL))
		rateFactor += 20;
	if (to->GetMapIndex() >= 660000 && to->GetMapIndex() < 670000)
		rateFactor += 20;
#ifdef NEW_POINT_EXP_DOUBLE_BONUS_RAZOR93



	int expDoubleBonus = to->GetPoint(POINT_EXP_DOUBLE_BONUS);

	if (expDoubleBonus > 0)
	{
		int extraBonus = 30;

		if (expDoubleBonus > 100)
		{
			
			extraBonus = 30 + ((expDoubleBonus - 100) / 10) * 10;
		}

		 
		rateFactor += extraBonus;
	}

#else
	if (to->GetPoint(POINT_EXP_DOUBLE_BONUS))
		if (number(1, 100) <= to->GetPoint(POINT_EXP_DOUBLE_BONUS))
			rateFactor += 30;
#endif
	if (to->IsEquipUniqueItem(UNIQUE_ITEM_DOUBLE_EXP))
		rateFactor += 50;

	switch (to->GetMountVnum())
	{
	case 20110:
	case 20111:
	case 20112:
	case 20113:
		if (to->IsEquipUniqueItem(71115) || to->IsEquipUniqueItem(71117) || to->IsEquipUniqueItem(71119) ||
			to->IsEquipUniqueItem(71121))
		{
			rateFactor += 10;
		}
		break;

	case 20114:
	case 20120:
	case 20121:
	case 20122:
	case 20123:
	case 20124:
	case 20125:
		rateFactor += 30;
		break;
	}

	if (to->GetPremiumRemainSeconds(PREMIUM_EXP) > 0)
		rateFactor += 50;
	if (to->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_EXP))
		rateFactor += 50;
	if (to->GetPoint(POINT_PC_BANG_EXP_BONUS) > 0)
	{
		if (to->IsPCBang())
			rateFactor += to->GetPoint(POINT_PC_BANG_EXP_BONUS);
	}
	rateFactor += to->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS);
	rateFactor += to->GetPoint(POINT_RAMADAN_CANDY_BONUS_EXP);
	rateFactor += to->GetPoint(POINT_MALL_EXPBONUS);
	// useless (never used except for china intoxication) = always 100
	rateFactor = rateFactor * static_cast<rate_t>(CHARACTER_MANAGER::instance().GetMobExpRate(to)) / 100.0L;
	// apply calculated rate bonus
	iExp *= (rateFactor / 100.0L);
	if (test_server)
		ecs::ChatSystem::Send(AIHelpers::EcsOf(to), CHAT_TYPE_INFO, "base_exp(%d) * rate(%Lf) = exp(%d)", iBaseExp, rateFactor / 100.0L, iExp);
	// you can get at maximum only 10% of the total required exp at once (so, you need to kill at least 10 mobs to level up) (useless)
	iExp = std::min(to->GetNextExp() / 10, (uint32_t)iExp);
	// it recalculate the given exp if the player level is greater than the exp_table size (useless)
	iExp = AdjustExpByLevel_Combat(to, iExp);

#ifdef __NEWPET_SYSTEM__
	CNewPetSystem* petSystemNew = to->GetNewPetSystem();
	if (petSystemNew)
	{
#ifdef ENABLE_NEW_PET_EDITS
		if (petSystemNew->GetLevel() < 100)
#else
		if (petSystemNew->GetLevel() < 120)
#endif
		{
			if ((petSystemNew->IsActivePet()) && (petSystemNew->GetLevelStep() < 4))
			{
				int tmpexp = iExp * 9 / 20;
				iExp = iExp - tmpexp;
				petSystemNew->SetExp(tmpexp, 0);
			}
		}
	}
#endif

	if (test_server)
		ecs::ChatSystem::Send(AIHelpers::EcsOf(to), CHAT_TYPE_INFO, "exp+minGNE+adjust(%d)", iExp);
	// set
	to->PointChange(POINT_EXP, iExp, true);
	from->CreateFly(FLY_EXP, to);
	// marriage
	{
		auto* you = to->GetMarryPartner();
		if (you)
		{
			// sometimes, this overflows
			uint32_t dwUpdatePoint = (2000.0L / to->GetLevel() / to->GetLevel() / 3) * iExp;

			if (to->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0 ||
				you->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0)
				dwUpdatePoint *= 3;

			marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(to->GetPlayerID());

			// DIVORCE_NULL_BUG_FIX
			if (pMarriage && pMarriage->IsNear())
				pMarriage->Update(dwUpdatePoint);
			// END_OF_DIVORCE_NULL_BUG_FIX
		}
	}
}
#else
static void GiveExp(LegacyCharHandle from, LegacyCharHandle to, int iExp)
{
	//  ġ 
	iExp = CALCULATE_VALUE_LVDELTA(to->GetLevel(), from->GetLevel(), iExp);

	int iBaseExp = iExp;

	// , ȸ ġ ̺Ʈ 
#ifdef ENABLE_EVENT_MANAGER
	const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(EXP_EVENT, to->GetEmpire());
	if (event != 0)
		iExp = iExp * (100 + (event->value[0] + CPrivManager::instance().GetPriv(to, PRIV_EXP_PCT))) / 100;
	else
		iExp = iExp * (100 + CPrivManager::instance().GetPriv(to, PRIV_EXP_PCT)) / 100;
#else
	iExp = iExp * (100 + CPrivManager::instance().GetPriv(to, PRIV_EXP_PCT)) / 100;
#endif

	// ӳ ⺻ Ǵ ġ ʽ
	{
		// 뵿 ޴
		if (to->IsEquipUniqueItem(UNIQUE_ITEM_LARBOR_MEDAL))
			iExp += iExp * 20 / 100;

		// Ÿ ġ ʽ
		if (to->GetMapIndex() >= 660000 && to->GetMapIndex() < 670000)
			iExp += iExp * 20 / 100; // 1.2 (20%)

		//  ġ ι Ӽ
		if (to->GetPoint(POINT_EXP_DOUBLE_BONUS))
			if (number(1, 100) <= to->GetPoint(POINT_EXP_DOUBLE_BONUS))
				iExp += iExp * 30 / 100; // 1.3 (30%)

		//   (2ð¥)
		if (to->IsEquipUniqueItem(UNIQUE_ITEM_DOUBLE_EXP))
			iExp += iExp * 50 / 100;

		switch (to->GetMountVnum())
		{
		case 20110:
		case 20111:
		case 20112:
		case 20113:
			if (to->IsEquipUniqueItem(71115) || to->IsEquipUniqueItem(71117) || to->IsEquipUniqueItem(71119) ||
				to->IsEquipUniqueItem(71121))
			{
				iExp += iExp * 10 / 100;
			}
			break;

		case 20114:
		case 20120:
		case 20121:
		case 20122:
		case 20123:
		case 20124:
		case 20125:
			//  ġ ʽ
			iExp += iExp * 30 / 100;
			break;
		}
	}

	//   Ǹ ġ ʽ
	{
		//  : ġ 
		if (to->GetPremiumRemainSeconds(PREMIUM_EXP) > 0)
		{
			iExp += (iExp * 50 / 100);
		}

		if (to->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_EXP) == true)
		{
			iExp += (iExp * 50 / 100);
		}

		// PC  ġ ʽ
		if (to->GetPoint(POINT_PC_BANG_EXP_BONUS) > 0)
		{
			if (to->IsPCBang() == true)
				iExp += (iExp * to->GetPoint(POINT_PC_BANG_EXP_BONUS) / 100);
		}

		// ȥ ʽ
		iExp += iExp * to->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS) / 100;
	}

	iExp += (iExp * to->GetPoint(POINT_RAMADAN_CANDY_BONUS_EXP) / 100);
	iExp += (iExp * to->GetPoint(POINT_MALL_EXPBONUS) / 100);

	if (test_server)
	{
		sys_log(0, "Bonus Exp : Ramadan Candy: %d MallExp: %d",
			to->GetPoint(POINT_RAMADAN_CANDY_BONUS_EXP),
			to->GetPoint(POINT_MALL_EXPBONUS)
		);
	}

	// ȹ  2005.04.21  85%
	iExp = iExp * CHARACTER_MANAGER::instance().GetMobExpRate(to) / 100;

	// ġ ѹ ȹ淮 
	iExp = MIN(to->GetNextExp() / 10, iExp);

	if (test_server)
	{
		if (quest::CQuestManager::instance().GetEventFlag("exp_bonus_log") && iBaseExp > 0)
			ecs::ChatSystem::Send(AIHelpers::EcsOf(to), CHAT_TYPE_INFO, "exp bonus %d%%", (iExp - iBaseExp) * 100 / iBaseExp);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(to), CHAT_TYPE_INFO, "exp(%d) base_exp(%d)", iExp, iBaseExp);
	}

	iExp = AdjustExpByLevel_Combat(to, iExp);

#ifdef __NEWPET_SYSTEM__
	CNewPetSystem* petSystemNew = to->GetNewPetSystem();
	if (petSystemNew) {
		if (petSystemNew->GetLevel() < 120)
		{
			if (petSystemNew->IsActivePet() && petSystemNew->GetLevelStep() < 4)
			{
				int tmpexp = iExp * 9 / 20;
				iExp = iExp - tmpexp;
				petSystemNew->SetExp(tmpexp, 0);
			}
		}
	}
#endif

	to->PointChange(POINT_EXP, iExp, true);
	from->CreateFly(FLY_EXP, to);

	{
		auto* you = to->GetMarryPartner();
		// κΰ  Ƽ̸ ݽ 
		if (you)
		{
			// 1 100%
			uint32_t dwUpdatePoint = 2000 * iExp / to->GetLevel() / to->GetLevel() / 3;

			if (to->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0 ||
				you->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0)
				dwUpdatePoint = (uint32_t)(dwUpdatePoint * 3);

			marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(to->GetPlayerID());

			// DIVORCE_NULL_BUG_FIX
			if (pMarriage && pMarriage->IsNear())
				pMarriage->Update(dwUpdatePoint);
			// END_OF_DIVORCE_NULL_BUG_FIX
		}
	}
}
#endif

namespace NPartyExpDistribute
{
	struct FPartyTotaler
	{
		int		total;
		int		member_count;
		int		x, y;

		FPartyTotaler(LegacyCharHandle center)
			: total(0), member_count(0), x(center->GetX()), y(center->GetY())
		{
		};

		void operator () (LegacyCharHandle ch)
		{
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
			{
				total += __GetPartyExpNP(ch->GetLevel());

				++member_count;
			}
		}
	};

	struct FPartyDistributor
	{
		int		total;
		LegacyCharHandle	c;
		int		x, y;
		uint32_t		_iExp;
		int		m_iMode;
		int		m_iMemberCount;

		FPartyDistributor(LegacyCharHandle center, int member_count, int total, uint32_t iExp, int iMode)
			: total(total), c(center), x(center->GetX()), y(center->GetY()), _iExp(iExp), m_iMode(iMode), m_iMemberCount(member_count)
		{
			if (m_iMemberCount == 0)
				m_iMemberCount = 1;
		};

		void operator () (LegacyCharHandle ch)
		{
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
			{
				uint32_t iExp2 = 0;

				switch (m_iMode)
				{
				case PARTY_EXP_DISTRIBUTION_NON_PARITY:
					iExp2 = (uint32_t)(_iExp * (float)__GetPartyExpNP(ch->GetLevel()) / total);
					break;

				case PARTY_EXP_DISTRIBUTION_PARITY:
					iExp2 = _iExp / m_iMemberCount;
					break;

				default:
					sys_err("Unknown party exp distribution mode %d", m_iMode);
					return;
				}

				GiveExp(c, ch, iExp2);
			}
		}
	};
}

typedef struct SDamageInfo
{
	int iDam;
	LegacyCharHandle pAttacker;
	LPPARTY pParty;

	void Clear()
	{
		pAttacker = nullptr;
		pParty = nullptr;
	}

	inline void Distribute(LegacyCharHandle ch, int iExp)
	{
		if (pAttacker)
			GiveExp(ch, pAttacker, iExp);
		else if (pParty)
		{
			NPartyExpDistribute::FPartyTotaler f(ch);
			pParty->ForEachOnlineMember(f);

			if (pParty->IsPositionNearLeader(ch))
				iExp = iExp * (100 + pParty->GetExpBonusPercent()) / 100;

			// ġ ֱ (Ƽ ȹ ġ 5%   )
			if (pParty->GetExpCentralizeCharacter())
			{
				auto* tch = pParty->GetExpCentralizeCharacter();

				if (DISTANCE_APPROX(ch->GetX() - tch->GetX(), ch->GetY() - tch->GetY()) <= PARTY_DEFAULT_RANGE)
				{
					int iExpCenteralize = (int)(iExp * 0.05f);
					iExp -= iExpCenteralize;

					GiveExp(ch, pParty->GetExpCentralizeCharacter(), iExpCenteralize);
				}
			}

			NPartyExpDistribute::FPartyDistributor fDist(ch, f.member_count, f.total, iExp, pParty->GetExpDistributionMode());
			pParty->ForEachOnlineMember(fDist);
		}
	}
} TDamageInfo;

LPCHARACTER CHARACTER::DistributeExp()
{
	int iExpToDistribute = GetExp();

	if (iExpToDistribute <= 0)
		return nullptr;

	uint64_t	iTotalDam = 0;
	auto* pkChrMostAttacked = static_cast<LegacyCharHandle>(nullptr);
	uint64_t iMostDam = 0;

	typedef std::vector<TDamageInfo> TDamageInfoTable;
	TDamageInfoTable damage_info_table;
	std::map<LPPARTY, TDamageInfo> map_party_damage;

	damage_info_table.reserve(m_map_kDamage.size());

	TDamageMap::iterator it = m_map_kDamage.begin();

	// ϴ    ɷ . (50m)
	while (it != m_map_kDamage.end())
	{
		const entt::entity eAttacker = it->first;
		uint64_t iDam = it->second.iTotalDamage;

		++it;

		auto* pAttacker = LegacyCharOf(eAttacker);

		// NPC ⵵ ϳ? -.-;
		if (!pAttacker || pAttacker->IsNPC() || DISTANCE_APPROX(GetX() - pAttacker->GetX(), GetY() - pAttacker->GetY()) > 5000)
			continue;

		iTotalDam += iDam;
		if (!pkChrMostAttacked || iDam > iMostDam)
		{
			pkChrMostAttacked = pAttacker;
			iMostDam = iDam;
		}

		if (pAttacker->GetParty())
		{
			std::map<LPPARTY, TDamageInfo>::iterator it = map_party_damage.find(pAttacker->GetParty());
			if (it == map_party_damage.end())
			{
				TDamageInfo di;
				di.iDam = iDam;
				di.pAttacker = nullptr;
				di.pParty = pAttacker->GetParty();
				map_party_damage.insert(std::make_pair(di.pParty, di));
			}
			else
			{
				it->second.iDam += iDam;
			}
		}
		else
		{
			TDamageInfo di;

			di.iDam = iDam;
			di.pAttacker = pAttacker;
			di.pParty = nullptr;

			//sys_log(0, "__ pq_damage %s %d", pAttacker->GetName(), iDam);
			//pq_damage.push(di);
			damage_info_table.push_back(di);
		}
	}

	for (std::map<LPPARTY, TDamageInfo>::iterator it = map_party_damage.begin(); it != map_party_damage.end(); ++it)
	{
		damage_info_table.push_back(it->second);
		//sys_log(0, "__ pq_damage_party [%u] %d", it->second.pParty->GetLeaderPID(), it->second.iDam);
	}

	SetExp(0);
	//m_map_kDamage.clear();

	if (iTotalDam == 0)	//  ذ 0̸ 
		return nullptr;

	if (m_pkChrStone)	//    ġ   ѱ.
	{
		//sys_log(0, "__ Give half to Stone : %d", iExpToDistribute>>1);
		int iExp = iExpToDistribute >> 1;
		m_pkChrStone->SetExp(m_pkChrStone->GetExp() + iExp);
		iExpToDistribute -= iExp;
	}

	sys_log(1, "%s total exp: %d, damage_info_table.size() == %d, TotalDam %d",
		GetName(), iExpToDistribute, damage_info_table.size(), iTotalDam);
	//sys_log(1, "%s total exp: %d, pq_damage.size() == %d, TotalDam %d",
	//GetName(), iExpToDistribute, pq_damage.size(), iTotalDam);

	if (damage_info_table.empty())
		return nullptr;

	//      HP ȸ Ѵ.
	DistributeHP(pkChrMostAttacked);	//  ý

	{
		//     ̳ Ƽ  ġ 20% + ڱⰡ ŭ ġ Դ´.
		TDamageInfoTable::iterator di = damage_info_table.begin();
		{
			TDamageInfoTable::iterator it;

			for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it)
			{
				if (it->iDam > di->iDam)
					di = it;
			}
		}

		int	iExp = iExpToDistribute / 5;
		iExpToDistribute -= iExp;

		float fPercent = (float)di->iDam / iTotalDam;

		if (fPercent > 1.0f)
		{
			sys_err("DistributeExp percent over 1.0 (fPercent %f name %s)", fPercent, di->pAttacker->GetName());
			fPercent = 1.0f;
		}

		iExp += (int)(iExpToDistribute * fPercent);

		//sys_log(0, "%s given exp percent %.1f + 20 dam %d", GetName(), fPercent * 100.0f, di.iDam);
#ifdef DISABLE_EXP_FROM_STONES_RAZOR93
		if (IsStone()) // razor93
		{
			//NEM HIVJA MEG A di->Distribute(this, iExp);
		}
		else
		{
			di->Distribute(this, iExp);//HA NEM STNONE AKKOR IGEN
		}
#else
		const int race = GetRaceNum();
		if (race == 8010 || race == 8020 || race == 8738 || race == 8739 || race == 8740 || race == 4811 || race == 4812 || race == 4813 || race == 4814 || race == 4815
			|| race == 8821 || race == 8822 || race == 8823 || race == 8824
			)
			return pkChrMostAttacked; // seggbe 
		di->Distribute(this, iExp);
#endif
		// 100%  Ծ Ѵ.
		if (fPercent == 1.0f)
			return pkChrMostAttacked;

		di->Clear();
	}

	{
		//  80% ġ йѴ.
		TDamageInfoTable::iterator it;

		for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it)
		{
			TDamageInfo& di = *it;

			float fPercent = (float)di.iDam / iTotalDam;

			if (fPercent > 1.0f)
			{
				sys_err("DistributeExp percent over 1.0 (fPercent %f name %s)", fPercent, di.pAttacker->GetName());
				fPercent = 1.0f;
			}

			//sys_log(0, "%s given exp percent %.1f dam %d", GetName(), fPercent * 100.0f, di.iDam);
			di.Distribute(this, (int)(iExpToDistribute * fPercent));
		}
	}

	return pkChrMostAttacked;
}

// ȭ   

// char_battle.cpp slice BC5 moved into CombatSystem.cpp

EVENTINFO(SCharDeadEventInfo)
{
	entt::entity entity;

	SCharDeadEventInfo()
		: entity(entt::null)
	{
	}
};

EVENTFUNC(dead_event)
{
	const SCharDeadEventInfo* info = dynamic_cast<SCharDeadEventInfo*>(event->info);
	if (info == nullptr)
	{
		sys_err("dead_event> <Factor> Null pointer");
		return 0;
	}

	auto* ch = LegacyCharOf(info->entity);
	if (ch == nullptr)
	{
		sys_err("DEAD_EVENT: cannot find char pointer with MOB entity(%u)", static_cast<uint32_t>(info->entity));
		return 0;
	}

	// Phase 10: WRITES_STATE - deferred until ECS component covers m_pkDeadEvent
	ch->m_pkDeadEvent = nullptr;
	{
		const entt::entity victimEntity = AIHelpers::EcsOf(ch);
		if (victimEntity != entt::null)
			g_dispatcher.trigger(ecs::EvCharDead { entt::null, victimEntity });
	}

	if (!ch->IsPC())
	{
		if (ch->IsMonster() == true)
		{
			if (ch->IsRevive() == false && ch->HasReviverInParty() == true)
			{
				ch->SetPosition(POS_STANDING);
				ch->SetHP(ch->GetMaxHP());

				ch->ViewReencode();

				ch->SetAggressive();
				ch->SetRevive(true);

				return 0;
			}
		}

		M2_DESTROY_CHARACTER(ch);
	}

	return 0;
}


void CHARACTER::Dead(LPCHARACTER pkKiller, bool bImmediateDead)
{
	// FakePlayers are normally excluded from death handling, but LostCastle clones must die.
	//if (IsFakePlayer() && !CLostCastleDungeon::instance().IsCloneVID(GetVID()))
	//	return;

	if (IsDead())
		return;

	if (GetInvincible())
		return;

	// LostCastle klonoknak nincs mob_proto (m_pkMobData == nullptr),
	// ezert a normal !IsPC() reward/resurrection ag GetMobTable()-t hivna es crashelne.
	// Itt egy safe halal pipeline + return.
	//if (IsFakePlayer() && CLostCastleDungeon::instance().IsCloneVID(GetVID()))
	//{
	//	if (!pkKiller && m_dwKillerPID)
	//		pkKiller = CHARACTER_MANAGER::instance().FindByPID(m_dwKillerPID);

	//	m_dwKillerPID = 0;

	//	if (auto* flags = RuntimeFlags(this))
	//		SET_BIT(flags->instantFlag, INSTANT_FLAG_NO_REWARD);

	//	SetPosition(POS_DEAD);
	//	ClearAffect(true);
	//	ClearSync();
	//	event_cancel(&m_pkStunEvent);

	//	if (pkKiller && pkKiller->IsPC())
	//		CLostCastleDungeon::instance().OnMobKilled(pkKiller, this);

	//	TPacketGCDead pack;
	//	pack.header = HEADER_GC_DEAD;
	//	pack.vid = GetPacketVID();
	//	PacketAround(&pack, sizeof(pack));

	//	if (auto* flags = RuntimeFlags(this))
	//		REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_STUN);

	//	if (GetDungeon())
	//		GetDungeon()->DeadCharacter(this);

	//	if (m_pkDeadEvent)
	//		event_cancel(&m_pkDeadEvent);

	//	SCharDeadEventInfo* pEventInfo = AllocEventInfo<SCharDeadEventInfo>();
	//	pEventInfo->vid = GetVID();
	//	m_pkDeadEvent = event_create(dead_event, pEventInfo, bImmediateDead ? 1 : PASSES_PER_SEC(1));
	//	return;
	//}

	if (IsPC())
	{
		if (IsHorseRiding()) {
			StopRiding();
		}
		else if (GetMountVnum()) {
			RemoveAffect(AFFECT_MOUNT_BONUS);
			m_dwMountVnum = 0;
			UnEquipSpecialRideUniqueItem();
			UpdatePacket();
		}
	}

	if (IsMonster() || IsStone())
	{
		LPDUNGEON dungeon = GetDungeon();
		if (dungeon)
		{
			dungeon->DecMonster();
		}
	}

#ifdef ENABLE_EVENT_MANAGER
	// Map1 mass-spawn wave tracking (Tanaka / Golden Frog)
	if (IsMonster() && GetMapIndex() == 1)
	{
		const uint32_t vnum = GetRaceNum();
		if (vnum == 5000u || vnum == 124u)
			Map1MassSpawnEvent_OnMobDead(GetPacketVID());
	}
#endif


	if (!pkKiller && m_dwKillerPID)
		pkKiller = CHARACTER_MANAGER::instance().FindByPID(m_dwKillerPID);

	m_dwKillerPID = 0; // ݵ ʱȭ ؾ DO NOT DELETE THIS LINE UNLESS YOU ARE 1000000% SURE

	bool isAgreedPVP = false;
	bool isUnderGuildWar = false;
	bool isDuel = false;

	if (pkKiller && pkKiller->IsPC())
	{
		if (pkKiller->m_pkChrTarget == this)
			pkKiller->SetTarget(nullptr);

		isAgreedPVP = CPVPManager::instance().Dead(this, pkKiller->GetPlayerID());
		isDuel = CArenaManager::instance().OnDead(pkKiller, this);
#ifdef ENABLE_PVP_ADVANCED
		if (isAgreedPVP || isDuel)
		{
			const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

			int betMoneyDead = GetQuestFlag(szTableStaticPvP[8]);
			int betMoneyKiller = pkKiller->GetQuestFlag(szTableStaticPvP[8]);

			if (betMoneyDead > 0 && betMoneyKiller > 0)
			{
				pkKiller->PointChange(POINT_GOLD, betMoneyDead * 2, true);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkKiller), CHAT_TYPE_INFO, 515, "%d", betMoneyDead);
#endif
			}

			for (unsigned int i = 0; i < _countof(szTableStaticPvP); i++) {
				char pkCh_Buf[CHAT_MAX_LEN + 1], pkKiller_Buf[CHAT_MAX_LEN + 1];

				snprintf(pkCh_Buf, sizeof(pkCh_Buf), "BINARY_Duel_Delete");
				snprintf(pkKiller_Buf, sizeof(pkKiller_Buf), "BINARY_Duel_Delete");

				ecs::ChatSystem::Send(AIHelpers::EcsOf(this), CHAT_TYPE_COMMAND, pkCh_Buf);
				SetQuestFlag(szTableStaticPvP[i], 0);

				ecs::ChatSystem::Send(AIHelpers::EcsOf(pkKiller), CHAT_TYPE_COMMAND, pkKiller_Buf);
				pkKiller->SetQuestFlag(szTableStaticPvP[i], 0);
			}
		}
#endif

		if (IsPC())
		{
			CGuild* g1 = GetGuild();
			CGuild* g2 = pkKiller->GetGuild();

			if (g1 && g2)
				if (g1->UnderWar(g2->GetID()))
					isUnderGuildWar = true;

			pkKiller->SetQuestNPCID(GetPacketVID());
			quest::CQuestManager::instance().Kill(pkKiller->GetPlayerID(), quest::QUEST_NO_NPC);
			CGuildManager::instance().Kill(pkKiller, this);
		}
	}

#ifdef ENABLE_QUEST_DIE_EVENT
	//if (IsPC())
	//{
	//	if (pkKiller)
	//		SetQuestNPCID(pkKiller->GetVID());
	//	// quest::CQuestManager::instance().Die(GetPlayerID(), quest::QUEST_NO_NPC);
	//	quest::CQuestManager::instance().Die(GetPlayerID(), (pkKiller)?pkKiller->GetRaceNum():quest::QUEST_NO_NPC);
	//}
	if (IsPC())
	{
		if (pkKiller) {
			SetQuestNPCID(pkKiller->GetPacketVID());
		}

		quest::CQuestManager::instance().Die(GetPlayerID(), (pkKiller) ? pkKiller->GetRaceNum() : quest::QUEST_NO_NPC);
	}
#endif

#ifdef ENABLE_RANKING
	if ((IsPC())) {
		if (((isAgreedPVP) || (isDuel)) && (pkKiller)) {
			SetRankPoints(1, pkKiller->GetRankPoints(1) + 1);
			pkKiller->SetRankPoints(0, pkKiller->GetRankPoints(0) + 1);
		}
		else if (isUnderGuildWar) {
			pkKiller->SetRankPoints(2, pkKiller->GetRankPoints(2) + 1);
		}
	}

	if (pkKiller) {
		if (pkKiller->IsPC()) {
			if (IsStone()) {
				if (pkKiller)
					pkKiller->SetRankPoints(5, pkKiller->GetRankPoints(5) + 1);
			}
			else if (IsMonster()) {
				if (GetMobRank() >= MOB_RANK_BOSS)
					pkKiller->SetRankPoints(7, pkKiller->GetRankPoints(7) + 1);
				else
					pkKiller->SetRankPoints(6, pkKiller->GetRankPoints(6) + 1);
			}
		}
	}
#endif

	/*
		if (pkKiller &&
				!isAgreedPVP &&
				!isUnderGuildWar &&
				IsPC() &&
				!isDuel)
		{
			if (GetGMLevel() == GM_PLAYER || test_server)
			{
				ItemDropPenalty(pkKiller);
			}
		}
	*/

#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
	if (IsPC()) {
#ifdef ENABLE_01092021
		if (pkKiller && !pkKiller->IsPC()) {
			pkKiller->SetTarget(nullptr);
		}
#endif
		ClearAffectSkills();
	}
#endif
	SetPosition(POS_DEAD);
	ClearAffect(true);

	if (pkKiller && IsPC())
	{
		if (!pkKiller->IsPC())
		{
#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
			SetDeadByMonster(true);
#endif

			sys_log(1, "DEAD: %s %p WITH PENALTY", GetName(), this);
						if (auto* flags = RuntimeFlags(this))
				SET_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);
			LogManager::instance().CharLog(this, pkKiller->GetRaceNum(), "DEAD_BY_NPC", pkKiller->GetName());
		}
		else
		{
#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
			SetDeadByMonster(false);
#endif
			sys_log(1, "DEAD_BY_PC: %s %p KILLER %s %p", GetName(), this, pkKiller->GetName(), get_pointer(pkKiller));
						if (auto* flags = RuntimeFlags(this))
				REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);

			if (GetEmpire() != pkKiller->GetEmpire())
			{
				int64_t iEP = std::min(GetPoint(POINT_EMPIRE_POINT), pkKiller->GetPoint(POINT_EMPIRE_POINT));

				PointChange(POINT_EMPIRE_POINT, -(iEP / 10));
				pkKiller->PointChange(POINT_EMPIRE_POINT, iEP / 5);


				char buf[256];
				snprintf(buf, sizeof(buf),
					"%d %u %d %s %d %u %d %s",
					GetEmpire(), GetAlignment(), GetPKMode(), GetName(),
					pkKiller->GetEmpire(), pkKiller->GetAlignment(), pkKiller->GetPKMode(), pkKiller->GetName());

				LogManager::instance().CharLog(this, pkKiller->GetPlayerID(), "DEAD_BY_PC", buf);
			}
			else
			{
//				if (!isAgreedPVP && !isUnderGuildWar && !IsKillerMode() /*&& GetAlignment() >= 0*/ && !isDuel)
//				{
//					int iNoPenaltyProb = 0;
//
//					if (pkKiller->GetAlignment() >= 0)	// 1/3 percent down
//						iNoPenaltyProb = 33;
//					else				// 4/5 percent down
//						iNoPenaltyProb = 20;
//
//					if (number(1, 100) < iNoPenaltyProb) {
//#ifdef TEXTS_IMPROVEMENT
//						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkKiller), CHAT_TYPE_INFO, 413, "");
//#endif
//					}
//					else {
//						if (pkKiller->GetParty())
//						{
//							FPartyAlignmentCompute f(-20000, pkKiller->GetX(), pkKiller->GetY());
//							pkKiller->GetParty()->ForEachOnlineMember(f);
//
//							if (f.m_iCount == 0)
//								pkKiller->UpdateAlignment(-20000);
//							else
//							{
//								sys_log(0, "ALIGNMENT PARTY count %d amount %d", f.m_iCount, f.m_iAmount);
//
//								f.m_iStep = 1;
//								pkKiller->GetParty()->ForEachOnlineMember(f);
//							}
//						}
//						else
//							pkKiller->UpdateAlignment(-20000);
//					}
//				}

				char buf[256];
				snprintf(buf, sizeof(buf),
					"%d %u %d %s %d %u %d %s",
					GetEmpire(), GetAlignment(), GetPKMode(), GetName(),
					pkKiller->GetEmpire(), pkKiller->GetAlignment(), pkKiller->GetPKMode(), pkKiller->GetName());

				LogManager::instance().CharLog(this, pkKiller->GetPlayerID(), "DEAD_BY_PC", buf);
			}

#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = pkKiller->GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwToKillCount, dwMinLevel;
				uint32_t dwLevel = GetLevel();
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, PLAYER_KILL, &dwMinLevel, &dwToKillCount))
				{
#ifdef ENABLE_BATTLE_PASS_SECURITY_KILL
					if ((GetDesc()->GetHostName() != pkKiller->GetDesc()->GetHostName()) && CBattlePass::instance().IsEligibleForPlayerKill(pkKiller->GetPlayerID(), GetPlayerID()))
					{
						if (dwLevel >= dwMinLevel && pkKiller->GetMissionProgress(PLAYER_KILL, bBattlePassId) < dwToKillCount)
						{
							pkKiller->UpdateMissionProgress(PLAYER_KILL, bBattlePassId, 1, dwToKillCount);
							CBattlePass::instance().RegisterPlayerKill(pkKiller->GetPlayerID(), GetPlayerID());
						}
					}
#else
					if (dwLevel >= dwMinLevel && pkKiller->GetMissionProgress(PLAYER_KILL, bBattlePassId) < dwToKillCount)
						pkKiller->UpdateMissionProgress(PLAYER_KILL, bBattlePassId, 1, dwToKillCount);
#endif
				}
			}
			if (pkKiller && pkKiller->IsPC() && IsPC())
			{
				const char* szMapName;
				switch (GetMapIndex())
				{
				case 18: szMapName = "Owl Dungeon"; break;
				case 27: szMapName = "Slime Dungeon"; break;
				case 41: szMapName = "Map1"; break;
				case 63: szMapName = "Desert"; break;
				case 66: szMapName = "Devil Tower"; break;
				case 73: szMapName = "Ice Cave"; break;
				case 208: szMapName = "Beran Setou Dungeon"; break;
				case 216: szMapName = "Devil Catacomb"; break;
				case 217: szMapName = "Spider Dungeon"; break;
				case 218: szMapName = "Rune Dungeon"; break;
				case 351: szMapName = "Fire Dungeon"; break;
				case 352: szMapName = "Nemere Dungeon"; break;
				case 355: szMapName = "Orcs Dungeon"; break;
				case 356: szMapName = "DT2"; break;
				case 357: szMapName = "Pyramid"; break;
				case 362: szMapName = "Dark Forest"; break;
				case 363: szMapName = "Map2"; break;
				case 364: szMapName = "Ice Empire"; break;
				case 365: szMapName = "SD5"; break;
				case 366: szMapName = "Hydra Dungeon"; break;
				case 367: szMapName = "Monkey Dungeon"; break;
				default: szMapName = "Unknown Map"; break;
				}

				char szMsg[256];

				if (isAgreedPVP)
				{
					int iRankPoints = pkKiller->GetRankPoints(0); // PvP rangpont
					snprintf(szMsg, sizeof(szMsg),
						"|cff00ff00%s|r has killed |cffff0000%s|r Map: %s, PVP-Mode: DUEL (Winned duels: %d)",
						pkKiller->GetName(), GetName(), szMapName, iRankPoints);
				}
				else
				{
					snprintf(szMsg, sizeof(szMsg),
						"|cff00ff00%s|r has killed |cffff0000%s|r Map: %s, PVP-Mode: FREE!",
						pkKiller->GetName(), GetName(), szMapName);
				}

				BroadcastNotice(szMsg);
			}


#endif
		}
	}
	else
	{
		sys_log(1, "DEAD: %s %p", GetName(), this);
				if (auto* flags = RuntimeFlags(this))
			REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);
	}

	ClearSync();

	//sys_log(1, "stun cancel %s[%d]", GetName(), (uint32_t)GetVID());
	event_cancel(&m_pkStunEvent); //  ̺Ʈ δ.

	if (IsPC())
	{
		m_dwLastDeadTime = get_dword_time();
		//SetKillerMode(pkKiller && pkKiller->IsPC());
		SetKillerMode(false);
		GetDesc()->SetPhase(PHASE_DEAD);
	}
	else
	{
		// 忡 ݹ ʹ   Ѵ.
		if (!(RuntimeFlags(this) && IS_SET(RuntimeFlags(this)->instantFlag, INSTANT_FLAG_NO_REWARD)))
		{
			if (!(pkKiller && pkKiller->IsPC() && pkKiller->GetGuild() && pkKiller->GetGuild()->UnderAnyWar(GUILD_WAR_TYPE_FIELD)))
			{
				// Ȱϴ ʹ   ʴ´.
				if (GetMobTable().dwResurrectionVnum)
				{
					// DUNGEON_MONSTER_REBIRTH_BUG_FIX
					auto* chResurrect = CHARACTER_MANAGER::instance().SpawnMob(GetMobTable().dwResurrectionVnum, GetMapIndex(), GetX(), GetY(), GetZ(), true, (int)GetRotation());
					if (GetDungeon() && chResurrect)
					{
						chResurrect->SetDungeon(GetDungeon());
					}
					// END_OF_DUNGEON_MONSTER_REBIRTH_BUG_FIX

					Reward(false);
				}
				else if (IsRevive() == true)
				{
					Reward(false);
				}
				else
				{
					Reward(true); // Drops gold, item, etc..
				}
			}
			else
			{
				if (pkKiller->m_dwUnderGuildWarInfoMessageTime < get_dword_time())
				{
					pkKiller->m_dwUnderGuildWarInfoMessageTime = get_dword_time() + 60000;
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkKiller), CHAT_TYPE_INFO, 147, "");
#endif
				}
			}
		}
	}

	// BOSS_KILL_LOG
	if (GetMobRank() >= MOB_RANK_BOSS && pkKiller && pkKiller->IsPC())
	{
		char buf[51];
		snprintf(buf, sizeof(buf), "%d %ld", g_bChannel, pkKiller->GetMapIndex());
		if (IsStone())
			LogManager::instance().CharLog(pkKiller, GetRaceNum(), "STONE_KILL", buf);
		else
			LogManager::instance().CharLog(pkKiller, GetRaceNum(), "BOSS_KILL", buf);
	}
	// END_OF_BOSS_KILL_LOG

	TPacketGCDead pack;
	pack.header = HEADER_GC_DEAD;
	pack.vid = GetPacketVID();
	PacketAround(&pack, sizeof(pack));

		if (auto* flags = RuntimeFlags(this))
		REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_STUN);

	// ÷̾ ĳ̸
	if (GetDesc() != nullptr) {
		//
		// Ŭ̾Ʈ Ʈ Ŷ ٽ .
		//
		auto it = m_list_pkAffect.begin();

		while (it != m_list_pkAffect.end())
			SendAffectAddPacket(GetDesc(), *it++);
	}

	//
	// Dead ̺Ʈ ,
	//
	// Dead ̺Ʈ    Ŀ Destroy ǵ ָ,
	// PC  3 ִٰ    ش. 3  κ
	//   , ⼭    ޴´.
	if (isDuel == false)
	{
		if (m_pkDeadEvent)
		{
			sys_log(1, "DEAD_EVENT_CANCEL: %s %p %p", GetName(), this, get_pointer(m_pkDeadEvent));
			event_cancel(&m_pkDeadEvent);
		}

		if (IsStone())
		{
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
			ClearStone(pkKiller);
#else
			ClearStone();
#endif
		}

		if (GetDungeon())
		{
			GetDungeon()->DeadCharacter(this);
		}

		if (!IsPC())
		{
			SCharDeadEventInfo* pEventInfo = AllocEventInfo<SCharDeadEventInfo>();
			pEventInfo->entity = EntityOf(this);

			if (IsRevive() == false && HasReviverInParty() == true)
			{
				m_pkDeadEvent = event_create(dead_event, pEventInfo, bImmediateDead ? 1 : PASSES_PER_SEC(1));
			}
#ifdef __DEFENSE_WAVE__
			else if (GetRaceNum() >= 3950 && GetRaceNum() <= 3964)
			{
				m_pkDeadEvent = event_create(dead_event, pEventInfo, bImmediateDead ? 1 : PASSES_PER_SEC(1));
			}
#endif
			else
			{
				m_pkDeadEvent = event_create(dead_event, pEventInfo, bImmediateDead ? 1 : PASSES_PER_SEC(1));
			}

			sys_log(1, "DEAD_EVENT_CREATE: %s %p %p", GetName(), this, get_pointer(m_pkDeadEvent));
		}
	}

	if (m_pkExchange != nullptr)
	{
		m_pkExchange->Cancel();
	}

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (IsAttrTransferOpen() == true)
	{
		AttrTransfer_close(this);
	}
#endif

	if (IsCubeOpen() == true)
	{
		Cube_close(this);
	}

#ifdef ENABLE_ACCE_SYSTEM
	if (IsPC())
		CloseAcce();
#endif

	if (IsPC())
	{
		CShopManager::instance().StopShopping(this);
		CloseMyShop();
		CloseSafebox();
	}
}






void CombatSystem_Update(entt::registry& reg, uint32_t tick)
{
    // During the migration window, only process entities with an explicit active combat target.
    auto view = reg.view<ecs::CombatActiveTag, ecs::CombatTarget, ecs::LegacyCharPtr, ecs::CombatStats, ecs::AttackCooldown, ecs::Health>();

    view.each([&](const entt::entity entity,
                  ecs::CombatTarget& combatTarget,
                  const ecs::LegacyCharPtr& legacy,
                  ecs::CombatStats& combatStats,
                  ecs::AttackCooldown& attackCooldown,
                  ecs::Health& attackerHealth) {
        (void)legacy;
        (void)combatStats;
        (void)attackerHealth;

        if (combatTarget.target == entt::null || !reg.valid(combatTarget.target) ||
            !reg.all_of<ecs::Health>(combatTarget.target) ||
            reg.all_of<ecs::DeadTag>(combatTarget.target))
        {
            combatTarget.target = entt::null;
            reg.remove<ecs::CombatActiveTag>(entity);
            return;
        }

        const uint32_t attackPeriod = PASSES_PER_SEC(1);
        if (tick < attackCooldown.lastAttackTime || (tick - attackCooldown.lastAttackTime) < attackPeriod) {
            return;
        }

        auto& victimHealth = reg.get<ecs::Health>(combatTarget.target);
        const int32_t damage = 1;
        victimHealth.current = std::max<int32_t>(0, victimHealth.current - damage);
        attackCooldown.lastAttackTime = tick;

        reg.emplace_or_replace<ecs::DirtyTag>(combatTarget.target);
        g_dispatcher.trigger(ecs::EvEntityDamaged { entity, combatTarget.target, damage, DAMAGE_TYPE_NORMAL });

        if (victimHealth.current > 0) {
            return;
        }

        reg.emplace_or_replace<ecs::DeadTag>(combatTarget.target);
        if (auto* statusFlags = reg.try_get<ecs::StatusFlags>(combatTarget.target)) {
            statusFlags->isDead = true;
        }

        combatTarget.target = entt::null;
        reg.remove<ecs::CombatActiveTag>(entity);
        g_dispatcher.trigger(ecs::EvEntityDied { entity, combatTarget.target });
    });
}

// char_battle.cpp slice BA moved into CombatSystem.cpp

bool CHARACTER::CanBeginFight() const
{
	if (!CanMove())
		return false;

	return GetPosition() == POS_STANDING && !IsDead() && !IsStun();
}

void CHARACTER::BeginFight(LPCHARACTER pkVictim)
{
	SetVictim(pkVictim);
	SetPosition(POS_FIGHTING);
	SetNextStatePulse(1);
}

bool CHARACTER::CanFight() const
{
	return GetPosition() >= POS_FIGHTING ? true : false;
}

void CHARACTER::CreateFly(uint8_t bType, LPCHARACTER pkVictim)
{
	TPacketGCCreateFly packFly;

	packFly.bHeader = HEADER_GC_CREATE_FLY;
	packFly.bType = bType;
	packFly.dwStartVID = GetPacketVID();
	packFly.dwEndVID = pkVictim->GetPacketVID();

	PacketAround(&packFly, sizeof(TPacketGCCreateFly));
}

bool CHARACTER::Attack(LPCHARACTER pkVictim, uint8_t bType)
{
#ifdef ENABLE_BUG_FIXES
	if (pkVictim->GetMyShop())
		return false;
#endif

	if (test_server)
		sys_log(0, "[TEST_SERVER] Attack : %s type %d, MobBattleType %d", GetName(), bType, (!IsPC() && GetMobBattleType()) ? GetMobAttackRange() : 0);
	//PROF_UNIT puAttack("Attack");
	if (!CanMove())
		return false;
#ifdef ENABLE_ANTICHEAT
	SECTREE* sectree = GetSectree();
	SECTREE* vsectree = pkVictim->GetSectree();

	if (sectree && vsectree) {
		if (sectree->IsAttr(GetX(), GetY(), ATTR_BANPK) || vsectree->IsAttr(pkVictim->GetX(), pkVictim->GetY(), ATTR_BANPK)) {
			if (GetDesc()) {
				LogManager::instance().HackLog("ANTISAFEZONE", this);
				GetDesc()->DelayedDisconnect(3);
			}
		}
	}
#endif
	// if (pkVictim->GetParty())
	   // return false;

   // @fixme131
	if (!battle_is_attackable(this, pkVictim))
		return false;

	uint32_t dwCurrentTime = get_dword_time();

	if (IsPC()) {
#ifdef ENABLE_ANTICHEAT
		if (IS_SPEED_HACK(this, pkVictim, dwCurrentTime)) {
			return false;
		}
#endif


		if (bType == 0 && dwCurrentTime < GetSkipComboAttackByTime())
			return false;
	}

	pkVictim->SetSyncOwner(this);

	if (pkVictim->CanBeginFight())
		pkVictim->BeginFight(this);

	int iRet;

	if (bType == 0)
	{
		//
		// Ϲ 
		//
		switch (GetMobBattleType())
		{
		case BATTLE_TYPE_MELEE:
		case BATTLE_TYPE_POWER:
		case BATTLE_TYPE_TANKER:
		case BATTLE_TYPE_SUPER_POWER:
		case BATTLE_TYPE_SUPER_TANKER:
			iRet = battle_melee_attack(this, pkVictim);
			break;
		case BATTLE_TYPE_RANGE:
			FlyTarget(pkVictim->GetPacketVID(), pkVictim->GetX(), pkVictim->GetY(), HEADER_CG_FLY_TARGETING);
			iRet = Shoot(0) ? BATTLE_DAMAGE : BATTLE_NONE;
			break;
		case BATTLE_TYPE_MAGIC:
			FlyTarget(pkVictim->GetPacketVID(), pkVictim->GetX(), pkVictim->GetY(), HEADER_CG_FLY_TARGETING);
			iRet = Shoot(1) ? BATTLE_DAMAGE : BATTLE_NONE;
			break;
		default:
			sys_err("Unhandled battle type %d", GetMobBattleType());
			iRet = BATTLE_NONE;
			break;
		}
	}
	else
	{
		if (IsPC() == true)
		{
			if (dwCurrentTime - m_dwLastSkillTime > 1500)
			{
				sys_log(1, "HACK: Too long skill using term. Name(%s) PID(%u) delta(%u)",
					GetName(), GetPlayerID(), (dwCurrentTime - m_dwLastSkillTime));
				return false;
			}
		}

		sys_log(1, "Attack call ComputeSkill %d %s", bType, pkVictim ? pkVictim->GetName() : "");
		iRet = ComputeSkill(bType, pkVictim);
	}

	//if (test_server && IsPC())
	//	sys_log(0, "%s Attack %s type %u ret %d", GetName(), pkVictim->GetName(), bType, iRet);
	if (iRet == BATTLE_DAMAGE || iRet == BATTLE_DEAD)
	{
		OnMove(true);
		pkVictim->OnMove();

		// only pc sets victim null. For npc, state machine will reset this.
		if (BATTLE_DEAD == iRet && IsPC())
			SetVictim(nullptr);

		return true;
	}

	return false;
}

int CHARACTER::GetArrowAndBow(LPITEM* ppkBow, LPITEM* ppkArrow, int iArrowCount/* = 1 */)
{
	LPITEM pkBow;

	if (!(pkBow = GetWear(WEAR_WEAPON)) || pkBow->GetProto()->bSubType != WEAPON_BOW)
	{
		return 0;
	}

	LPITEM pkArrow;

	if (!(pkArrow = GetWear(WEAR_ARROW)) || pkArrow->GetType() != ITEM_WEAPON ||
		pkArrow->GetProto()->bSubType != WEAPON_ARROW)
	{
		return 0;
	}

	iArrowCount = std::min(iArrowCount, pkArrow->GetCount());

	*ppkBow = pkBow;
	*ppkArrow = pkArrow;

	return iArrowCount;
}
// char_battle.cpp slice BD1 moved into CombatSystem.cpp

void CHARACTER::DistributeSP(LPCHARACTER pkKiller, int iMethod)
{
	if (pkKiller->GetSP() >= pkKiller->GetMaxSP())
		return;

	bool bAttacking = (get_dword_time() - GetLastAttackTime()) < 3000;
	bool bMoving = (get_dword_time() - GetLastMoveTime()) < 3000;

	if (iMethod == 1)
	{
		int num = number(0, 3);

		if (!num)
		{
			int iLvDelta = GetLevel() - pkKiller->GetLevel();
			int iAmount = 0;

			if (iLvDelta >= 5)
				iAmount = 10;
			else if (iLvDelta >= 0)
				iAmount = 6;
			else if (iLvDelta >= -3)
				iAmount = 2;

			if (iAmount != 0)
			{
				iAmount += (iAmount * pkKiller->GetPoint(POINT_SP_REGEN)) / 100;

				if (iAmount >= 11)
					CreateFly(FLY_SP_BIG, pkKiller);
				else if (iAmount >= 7)
					CreateFly(FLY_SP_MEDIUM, pkKiller);
				else
					CreateFly(FLY_SP_SMALL, pkKiller);

				pkKiller->PointChange(POINT_SP, iAmount);
			}
		}
	}
	else
	{
		if (pkKiller->GetJob() == JOB_SHAMAN || (pkKiller->GetJob() == JOB_SURA && pkKiller->GetSkillGroup() == 2))
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + GetMaxSP() / 100;
			else if (bMoving)
				iAmount = 3 + GetMaxSP() * 2 / 100;
			else
				iAmount = 10 + GetMaxSP() * 3 / 100; // 

			iAmount += (iAmount * pkKiller->GetPoint(POINT_SP_REGEN)) / 100;
			pkKiller->PointChange(POINT_SP, iAmount);
		}
		else
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + pkKiller->GetMaxSP() / 200;
			else if (bMoving)
				iAmount = 2 + pkKiller->GetMaxSP() / 100;
			else
			{
				// 
				if (pkKiller->GetHP() < pkKiller->GetMaxHP())
					iAmount = 2 + (pkKiller->GetMaxSP() / 100); //   á
				else
					iAmount = 9 + (pkKiller->GetMaxSP() / 100); // ⺻
			}

			iAmount += (iAmount * pkKiller->GetPoint(POINT_SP_REGEN)) / 100;
			pkKiller->PointChange(POINT_SP, iAmount);
		}
	}
}



// char_battle.cpp slice BD2a helper surface duplicated into CombatSystem.cpp

static uint32_t __GetPartyExpNP(const uint32_t level)
{
	if (!level || level > PLAYER_EXP_TABLE_MAX)
		return 14000;
	return party_exp_distribute_table[level];
}


static uint32_t AdjustExpByLevel_Combat(const LegacyCharHandle ch, const uint32_t exp)
{
	if (PLAYER_MAX_LEVEL_CONST < ch->GetLevel())
	{
		double ret = 0.95;
		double factor = 0.1;

		for (int64_t i = 0; i < ch->GetLevel() - 100; ++i)
		{
			if ((i % 10) == 0)
				factor /= 2.0;

			ret *= 1.0 - factor;
		}

		ret = ret * static_cast<double>(exp);

		if (ret < 1.0)
			return 1;

		return static_cast<uint32_t>(ret);
	}

	return exp;
}


// char_battle.cpp slice BC1 moved into CombatSystem.cpp

static int __GetExpLossPerc(const uint32_t level)
{
	if (!level || level > PLAYER_EXP_TABLE_MAX)
		return 1;
	return aiExpLossPercents[level];
}


void CHARACTER::DeathPenalty(uint8_t bTown)
{
	sys_log(1, "DEATH_PERNALY_CHECK(%s) town(%d)", GetName(), bTown);

	Cube_close(this);
#ifdef __ATTR_TRANSFER_SYSTEM__
	AttrTransfer_close(this);
#endif
#ifdef ENABLE_ACCE_SYSTEM
	CloseAcce();
#endif

	if (CBattleArena::instance().IsBattleArenaMap(GetMapIndex()) == true)
	{
		return;
	}

	if (GetLevel() < 10) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 412, "");
#endif
		return;
	}

	if (number(0, 2) == 1) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 412, "");
#endif
		return;
	}

	if (RuntimeFlags(this) && IS_SET(RuntimeFlags(this)->instantFlag, INSTANT_FLAG_DEATH_PENALTY))
	{
				if (auto* flags = RuntimeFlags(this))
			REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);

		// NO_DEATH_PENALTY_BUG_FIX
		if (!bTown) //   ڸ Ȱø  ȣ Ѵ. ( ͽô ġ гƼ )
		{
			if (FindAffect(AFFECT_NO_DEATH_PENALTY))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 384, "");
#endif
				RemoveAffect(AFFECT_NO_DEATH_PENALTY);
				return;
			}
		}
		// END_OF_NO_DEATH_PENALTY_BUG_FIX

		int iLoss = ((GetNextExp() * __GetExpLossPerc(GetLevel())) / 100);

		iLoss = std::min(800000, iLoss);

		if (bTown)
			iLoss = 0;

		if (IsEquipUniqueItem(UNIQUE_ITEM_TEARDROP_OF_GODNESS))
			iLoss /= 2;

		sys_log(0, "DEATH_PENALTY(%s) EXP_LOSS: %d percent %d%%", GetName(), iLoss, __GetExpLossPerc(GetLevel()));

		PointChange(POINT_EXP, -iLoss, true);
	}
}


// char_battle.cpp slice BC4 moved into CombatSystem.cpp

struct TItemDropPenalty
{
	int iInventoryPct;		// Range: 1 ~ 1000
	int iInventoryQty;		// Range: --
	int iEquipmentPct;		// Range: 1 ~ 100
	int iEquipmentQty;		// Range: --
};

TItemDropPenalty aItemDropPenalty_kor[9] =
{
	{   0,   0,  0,  0 },	// 
	{   0,   0,  0,  0 },	// 
	{   0,   0,  0,  0 },	// 
	{   0,   0,  0,  0 },	// 
	{   0,   0,  0,  0 },	// 
	{  25,   1,  5,  1 },	// 
	{  50,   2, 10,  1 },	// 
	{  75,   4, 15,  1 },	// 
	{ 100,   8, 20,  1 },	// п
};

void CHARACTER::ItemDropPenalty(LPCHARACTER pkKiller)
{

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (GetGMLevel() > GM_PLAYER) {
		return;
	}
#endif

	if (GetMyShop())
		return;

	if (GetLevel() < 50)
		return;

	if (CBattleArena::instance().IsBattleArenaMap(GetMapIndex()) == true)
	{
		return;
	}

	struct TItemDropPenalty* table = &aItemDropPenalty_kor[0];

	if (GetLevel() < 10)
		return;

	uint8_t iAlignIndex;

	if (GetRealAlignment()		<= 4999)		iAlignIndex = 0;
	else if (GetRealAlignment() <= 14999)		iAlignIndex = 1;
	else if (GetRealAlignment() <= 19999)		iAlignIndex = 2;
	else if (GetRealAlignment() <= 29999)		iAlignIndex = 3;
	else if (GetRealAlignment() <= 49999)		iAlignIndex = 4;
	else if (GetRealAlignment() <= 74999)		iAlignIndex = 5;
	else if (GetRealAlignment() <= 99999)		iAlignIndex = 6;
	else if (GetRealAlignment() <= 124999)		iAlignIndex = 7;
	else if (GetRealAlignment() <= 174999)		iAlignIndex = 8;
	else if (GetRealAlignment() <= 249999)		iAlignIndex = 9;
	else if (GetRealAlignment() <= 499999)		iAlignIndex = 10;
	else if (GetRealAlignment() <= 749999)		iAlignIndex = 11;
	else if (GetRealAlignment() <= 999999)		iAlignIndex = 12;
	else if (GetRealAlignment() <= 1499999)		iAlignIndex = 13;
	else if (GetRealAlignment() <= 2499999)		iAlignIndex = 14;
	else if (GetRealAlignment() == 2500000)		iAlignIndex = 15;
	else return;

	std::vector<std::pair<LPITEM, int> > vec_item;
	LPITEM pkItem;
	int	i;
	bool isDropAllEquipments = false;

	TItemDropPenalty& r = table[iAlignIndex];
	sys_log(0, "%s align %d inven_pct %d equip_pct %d", GetName(), iAlignIndex, r.iInventoryPct, r.iEquipmentPct);

	bool bDropInventory = r.iInventoryPct >= number(1, 1000);
	bool bDropEquipment = r.iEquipmentPct >= number(1, 100);
	bool bDropAntiDropUniqueItem = false;

	if ((bDropInventory || bDropEquipment) && IsEquipUniqueItem(UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY))
	{
		bDropInventory = false;
		bDropEquipment = false;
		bDropAntiDropUniqueItem = true;
	}

	if (bDropInventory) // Drop Inventory
	{
		std::vector<uint8_t> vec_bSlots;

		for (i = 0; i < INVENTORY_MAX_NUM; ++i)
			if (GetInventoryItem(i))
				vec_bSlots.push_back(i);

		if (!vec_bSlots.empty())
		{
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(vec_bSlots.begin(), vec_bSlots.end(), g);
			int iQty = std::min((int)vec_bSlots.size(), r.iInventoryQty);

			if (iQty)
				iQty = number(1, iQty);

			for (i = 0; i < iQty; ++i)
			{
				pkItem = GetInventoryItem(vec_bSlots[i]);

				if (IS_SET(pkItem->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
					continue;

				SyncQuickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
				vec_item.emplace_back(pkItem->RemoveFromCharacter(), INVENTORY);
			}
		}
		/*else if (iAlignIndex == 8)
			isDropAllEquipments = true;*/
	}

	if (bDropEquipment) // Drop Equipment
	{
		std::vector<uint8_t> vec_bSlots;

		for (i = 0; i < WEAR_MAX_NUM; ++i)
			if (GetWear(i))
				vec_bSlots.push_back(i);

		if (!vec_bSlots.empty())
		{
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(vec_bSlots.begin(), vec_bSlots.end(), g);
			int iQty;

			if (isDropAllEquipments)
				iQty = vec_bSlots.size();
			else
				iQty = std::min((int)vec_bSlots.size(), number(1, r.iEquipmentQty));

			if (iQty)
				iQty = number(1, iQty);

			for (i = 0; i < iQty; ++i)
			{
				pkItem = GetWear(vec_bSlots[i]);

				if (IS_SET(pkItem->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
					continue;

				SyncQuickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
				vec_item.emplace_back(pkItem->RemoveFromCharacter(), EQUIPMENT);
			}
		}
	}

	if (bDropAntiDropUniqueItem)
	{
		LPITEM pkItem;

		pkItem = GetWear(WEAR_UNIQUE1);

		if (pkItem && pkItem->GetVnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			SyncQuickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE1, 255);
			vec_item.emplace_back(pkItem->RemoveFromCharacter(), EQUIPMENT);
		}

		pkItem = GetWear(WEAR_UNIQUE2);

		if (pkItem && pkItem->GetVnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			SyncQuickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE2, 255);
			vec_item.emplace_back(pkItem->RemoveFromCharacter(), EQUIPMENT);
		}
	}

	{
		PIXEL_POSITION pos;
		pos.x = GetX();
		pos.y = GetY();

		unsigned int i;

		for (i = 0; i < vec_item.size(); ++i)
		{
			LPITEM item = vec_item[i].first;
			int window = vec_item[i].second;

			item->AddToGround(GetMapIndex(), pos);
			item->StartDestroyEvent();

			sys_log(0, "DROP_ITEM_PK: %s %d %d from %s", item->GetName(), pos.x, pos.y, GetName());
			LogManager::instance().ItemLog(this, item, "DEAD_DROP", (window == INVENTORY) ? "INVENTORY" : ((window == EQUIPMENT) ? "EQUIPMENT" : ""));

			pos.x = GetX() + number(-7, 7) * 20;
			pos.y = GetY() + number(-7, 7) * 20;
		}
	}
}


// char_battle.cpp slice BC3a helper surface duplicated into CombatSystem.cpp

#ifdef ENABLE_DROP_INSTANT_INVENTORY
static void __UpdateBattlePassCollectProgress(LegacyCharHandle ch, uint32_t dwItemVnum, uint32_t dwCount)
{
#ifdef ENABLE_BATTLE_PASS
	if (!ch || !dwCount)
		return;

	const uint8_t bBattlePassId = ch->GetBattlePassId();
	if (!bBattlePassId)
		return;

	auto updateMission = [&](uint32_t dwMissionType)
		{
			uint32_t dwMissionItemVnum = 0;
			uint32_t dwNeedCount = 0;

			if (!CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, dwMissionType, &dwMissionItemVnum, &dwNeedCount))
				return;

			if (dwMissionItemVnum != dwItemVnum)
				return;

			if (ch->GetMissionProgress(dwMissionType, bBattlePassId) >= dwNeedCount)
				return;

			ch->UpdateMissionProgress(dwMissionType, bBattlePassId, dwCount, dwNeedCount);
		};

	updateMission(COLLECT_ITEM);
	updateMission(COLLECT_ITEM1);
	updateMission(COLLECT_ITEM2);
#endif
}

static bool __TryAutoGiveRewardItem(LegacyCharHandle ch, LPITEM item, uint32_t& dwGivenCount)
{
	dwGivenCount = 0;

	if (!ch || !item)
		return false;

	const char* szItemName = item->GetName(ch->GetDesc() ? ch->GetDesc()->GetLanguage() : 0);

#ifdef ENABLE_EXTRA_INVENTORY
	if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
	{
#ifdef ENABLE_NEW_STACK_LIMIT
		int bCount = item->GetCount();
#else
		uint8_t bCount = item->GetCount();
#endif
		for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item2 = ch->GetExtraInventoryItem(i);
			if (!item2)
				continue;

			if (item2->GetVnum() != item->GetVnum())
				continue;

			int j = 0;
			for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
			{
				if (item2->GetSocket(j) != item->GetSocket(j))
					break;
			}

			if (j != ITEM_SOCKET_MAX_NUM)
				continue;

#ifdef ENABLE_NEW_STACK_LIMIT
			int bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount);
#else
			uint8_t bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount);
#endif
			if (bCount2 <= 0)
				continue;

			bCount -= bCount2;
			dwGivenCount += bCount2;
			item2->SetCount(item2->GetCount() + bCount2);

			if (bCount == 0)
			{
#ifdef TEXTS_IMPROVEMENT
				if (dwGivenCount > 0)
				{
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), 
#ifdef ENABLE_NEW_CHAT
						CHAT_TYPE_INFO_ITEM
#else
						CHAT_TYPE_INFO
#endif
						, 102, "%u#%s", dwGivenCount, szItemName);
				}
#endif

				item->SetCount(0);
				M2_DESTROY_ITEM(item);
				return true;
			}
		}

		item->SetCount(bCount);
	}
	else if (item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
#else
	if (item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
#endif
	{
#ifdef ENABLE_NEW_STACK_LIMIT
		int bCount = item->GetCount();
#else
		uint8_t bCount = item->GetCount();
#endif
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item2 = ch->GetInventoryItem(i);
			if (!item2)
				continue;

			if (item2->GetVnum() != item->GetVnum())
				continue;

			int j = 0;
			for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
			{
				if (item2->GetSocket(j) != item->GetSocket(j))
					break;
			}

			if (j != ITEM_SOCKET_MAX_NUM)
				continue;

#ifdef ENABLE_NEW_STACK_LIMIT
			int bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount);
#else
			uint8_t bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount);
#endif
			if (bCount2 <= 0)
				continue;

			bCount -= bCount2;
			dwGivenCount += bCount2;
			item2->SetCount(item2->GetCount() + bCount2);

			if (bCount == 0)
			{
#ifdef TEXTS_IMPROVEMENT
				if (dwGivenCount > 0)
				{
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), 
#ifdef ENABLE_NEW_CHAT
						CHAT_TYPE_INFO_ITEM
#else
						CHAT_TYPE_INFO
#endif
						, 102, "%u#%s", dwGivenCount, szItemName);
				}
#endif

				item->SetCount(0);
				M2_DESTROY_ITEM(item);
				return true;
			}
		}

		item->SetCount(bCount);
	}

	int iEmptyCell = -1;
	TItemPos pos;

	if (item->IsDragonSoul())
	{
		iEmptyCell = ch->GetEmptyDragonSoulInventory(item);
		pos = TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell);
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (item->IsExtraItem())
	{
		iEmptyCell = ch->GetEmptyExtraInventory(item);
		pos = TItemPos(EXTRA_INVENTORY, iEmptyCell);
	}
#endif
	else
	{
		iEmptyCell = ch->GetEmptyInventory(item->GetSize());
		pos = TItemPos(INVENTORY, iEmptyCell);
	}

	if (iEmptyCell == -1)
		return false;

	const uint32_t dwDirectCount = item->GetCount();
	item->AddToCharacter(ch, pos);
	dwGivenCount += dwDirectCount;

#ifdef TEXTS_IMPROVEMENT
	if (dwGivenCount > 0)
	{
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), 
#ifdef ENABLE_NEW_CHAT
			CHAT_TYPE_INFO_ITEM
#else
			CHAT_TYPE_INFO
#endif
			, 102, "%u#%s", dwGivenCount, szItemName);
	}
#endif

	char szHint[32 + 1];
	snprintf(szHint, sizeof(szHint), "%s %u %u", item->GetName(), item->GetCount(), item->GetOriginalVnum());
	LogManager::instance().ItemLog(ch, item, "GET", szHint);
	return true;
}

static void __GiveRewardItemToCharacterOrDrop(LegacyCharHandle ch, LegacyCharHandle pkVictim, LPITEM item, const PIXEL_POSITION& pos, bool bTrackBattlePass)
{
	if (!item)
		return;

	uint32_t dwGivenCount = 0;
	const uint32_t dwItemVnum = item->GetVnum();

	if (ch && __TryAutoGiveRewardItem(ch, item, dwGivenCount))
	{
		if (bTrackBattlePass && dwGivenCount > 0)
			__UpdateBattlePassCollectProgress(ch, dwItemVnum, dwGivenCount);
		return;
	}

	if (bTrackBattlePass && dwGivenCount > 0)
		__UpdateBattlePassCollectProgress(ch, dwItemVnum, dwGivenCount);

	item->AddToGround(pkVictim->GetMapIndex(), pos);

	if (ch && CBattleArena::instance().IsBattleArenaMap(ch->GetMapIndex()) == false)
		item->SetOwnership(ch, 60);

	item->StartDestroyEvent();

	sys_log(0, "DROP_ITEM: %s %d %d from %s", item->GetName(), pos.x, pos.y, pkVictim->GetName());
}
#endif


#ifdef ENABLE_RARE_DROP_NOTICE_RAZOR93
static std::string MakeItemLink(LPITEM pkItem, LegacyCharHandle pkKiller, LegacyCharHandle pkMob)
{
	char itemlink[512];
	int len = 0;

	// item link alap
	len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
		pkItem->GetVnum(),
		pkItem->GetSocket(0),
		pkItem->GetSocket(1),
		pkItem->GetSocket(2),
		0, 0);

	// bonuszok
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
		uint8_t type = pkItem->GetAttributeType(i);
		short   val = pkItem->GetAttributeValue(i);
		if (type && val)
			len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
	}


	int lang = LANGUAGE_EN;
	if (pkKiller && pkKiller->GetDesc())
		lang = pkKiller->GetDesc()->GetLanguage();


	const char* fmt = "|cffc71585[%s]|r looted a special item from |cff87ceeb[%s]|r: |cffffd700|H%s|h[%s]|h|r"; // EN default
	switch (lang) {
	case LANGUAGE_RO:
		fmt = "|cffc71585[%s]|r a primit un obiect rar de la |cff87ceeb[%s]|r: |cffffd700|H%s|h[%s]|h|r";
		break;
	case LANGUAGE_IT:
		fmt = "|cffc71585[%s]|r ha ottenuto un oggetto raro da |cff87ceeb[%s]|r: |cffffd700|H%s|h[%s]|h|r";
		break;
	case LANGUAGE_TR:
		fmt = "|cffc71585[%s]|r nadir bir esya elde etti (|cff87ceeb[%s]|r): |cffffd700|H%s|h[%s]|h|r";
		break;
	case LANGUAGE_DE:
		fmt = "|cffc71585[%s]|r hat einen seltenen Gegenstand von |cff87ceeb[%s]|r erhalten: |cffffd700|H%s|h[%s]|h|r";
		break;
	case LANGUAGE_PL:
		fmt = "|cffc71585[%s]|r otrzymal rzadki przedmiot od |cff87ceeb[%s]|r: |cffffd700|H%s|h[%s]|h|r";
		break;
	case LANGUAGE_PT:
		fmt = "|cffc71585[%s]|r obteve um item raro de |cff87ceeb[%s]|r: |cffffd700|H%s|h[%s]|h|r";
		break;
	case LANGUAGE_ES:
		fmt = "|cffc71585[%s]|r obtuvo un objeto raro de |cff87ceeb[%s]|r: |cffffd700|H%s|h[%s]|h|r";
		break;
	case LANGUAGE_CZ:
		fmt = "|cffc71585[%s]|r ziskal vzcny predmet z |cff87ceeb[%s]|r: |cffffd700|H%s|h[%s]|h|r";
		break;
	case LANGUAGE_HU:
		fmt = "|cffc71585[%s]|r ritka trgyat szerzett |cff87ceeb[%s]|r mobtl: |cffffd700|H%s|h[%s]|h|r";
		break;
	default:
		break;
	}


	char szChat[1024];
	snprintf(szChat, sizeof(szChat), fmt,
		pkKiller ? pkKiller->GetName() : "Player",
		pkMob ? pkMob->GetName() : "Mob",
		itemlink,
		pkItem ? pkItem->GetName() : "item");

	return std::string(szChat);
}




static std::set<uint32_t> verjema_szadba_ixtreeme =
{
		14590, 14591, 14592, 14593, 52040, 60001, 48421, 49009,
		49049, 60003, 71223, 71253, 71224, 71228, 71251, 71125,
		71126, 71127, 71139, 71166, 71171, 71176, 71177, 71221,
		71222, 71252, 71256, 71225, 71226, 71227, 71255, 71254,
		71233, 71250, 71128, 23014, 23015, 23016, 71137, 71140, 71185,
		// vek: 18000 - 18119
		//18000, 18001, 18002, 18003, 18004, 18005, 18006, 18007, 18008, 18009,
		//18010, 18011, 18012, 18013, 18014, 18015, 18016, 18017, 18018, 18019,
		//18020, 18021, 18022, 18023, 18024, 18025, 18026, 18027, 18028, 18029,
		//18030, 18031, 18032, 18033, 18034, 18035, 18036, 18037, 18038, 18039,
		//18040, 18041, 18042, 18043, 18044, 18045, 18046, 18047, 18048, 18049,
		//18050, 18051, 18052, 18053, 18054, 18055, 18056, 18057, 18058, 18059,
		//18060, 18061, 18062, 18063, 18064, 18065, 18066, 18067, 18068, 18069,
		//18070, 18071, 18072, 18073, 18074, 18075, 18076, 18077, 18078, 18079,
		//18080, 18081, 18082, 18083, 18084, 18085, 18086, 18087, 18088, 18089,
		//18090, 18091, 18092, 18093, 18094, 18095, 18096, 18097, 18098, 18099,
		//18100, 18101, 18102, 18103, 18104, 18105, 18106, 18107, 18108, 18109,
		//18110, 18111, 18112, 18113, 18114, 18115, 18116, 18117, 18118, 18119,
		53025, //luffy
		70402,//klnleges bonusz 5
		70403,//klnleges bonusz 10
		30617,//	Legends Bnuszol
		30618,//	Legends Megvltoztat
		86050,//	Talizmn megersto
		86051,//	Talizmn bvlo
		86052//	Talizmnersto,
		,18140, 18141, 18142, 18143, 18144, 18145, 18146, 18147, 18148, 18149,
		18150, 18151, 18152, 18153, 18154, 18155, 18156, 18157, 18158, 18159
	// uj mountok 
,611500, 611501, 611502, 611503, 611504, 611505, 611506, 611507, 611508,
611510, 611511, 611512, 611513, 611514, 611515, 611516, 611517, 611518,
611520, 611521, 611522, 611523, 611524, 611525, 611526, 611527, 611528,
611530, 611531, 611532, 611533, 611534, 611535, 611536, 611537, 611538,
611540, 611541, 611542, 611543, 611544,
	611545,
611546,
611547,
611548,
611549,
611550,
611551,
611552,
611553,
611554,
611555,
611556,
611557,
611558,
611559,
611560,
611561,
611562,
611563,
611564,
611565,
611566,
611567,
611568,
611569,
611570,
611571,
611572,
611573,
611574,
611575,
611576,
611577,
611578,
611579,
611580,
611581,
611582,
611583,
611584,
611585,
611586,
611587,
611588,
611589,
611590,
611591,
611592,
611593,
611594,
611595,
611596,
611597,
	611598,
611599,
611600,
611601,
611602,
611603,
611604,
611605,
611606,
611607,
611608,
611609,
611610,
611611,
611612,
611613,
611614,
611615,
611616,
611617,
611618,
611619,
611620,
611621,
611622,
611623,
611624,
611625,
611626,
611627,
611628,
611629,
611630,
611631,
611632,
611633,
611634,
611635,
611636,
611637,
611638,
611639,
611640,
611641,
611642,
611643,
611644,
611645,
611646,
611647,
611648,
611649,
611650,
611651,
611652,
611653,
611654,
611655,
611656,
611657,
611658,
611659,
611660,
611661,
611662,
611663,
611664,
611665,
611666,
60101//mikulas baba 30 napos petkszti
};
#endif

// char_battle.cpp slice BC3b moved into CombatSystem.cpp

void CHARACTER::Reward(bool bItemDrop)
{
	//PROF_UNIT puReward("Reward");
	auto* pkAttacker = DistributeExp();

	if (!pkAttacker)
		return;


	if (!IsPC() && !m_pkMobData)
	{
		sys_err("Reward: NULL mob data (vid=%u race=%u name=%s map=%ld x=%ld y=%ld attacker=%s)",
			GetPacketVID(),
			GetRaceNum(),
			GetName(),
			GetMapIndex(),
			GetX(),
			GetY(),
			pkAttacker ? pkAttacker->GetName() : "<null>");
		m_map_kDamage.clear();
		return;
	}
	//PROF_UNIT pu1("r1");
	if (pkAttacker->IsPC())
	{
		if ((GetLevel() - pkAttacker->GetLevel()) >= -10)
		{
			/*if (pkAttacker->GetRealAlignment() < 0) // trsra: minden gyilkols 2 pontot ad
			{
				if (pkAttacker->IsEquipUniqueItem(UNIQUE_ITEM_FASTER_ALIGNMENT_UP_BY_KILL))
					pkAttacker->UpdateAlignment(14);
				else
					pkAttacker->UpdateAlignment(7);
			}
			else*/
				pkAttacker->UpdateAlignment(2);
		}

		pkAttacker->SetQuestNPCID(GetPacketVID());
		quest::CQuestManager::instance().Kill(pkAttacker->GetPlayerID(), GetRaceNum());
		CHARACTER_MANAGER::instance().KillLog(GetRaceNum());
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
		COrcsDungeon::instance().OnMobKilled(pkAttacker, this);
		CTritonTempleDungeon::instance().OnMobKilled(pkAttacker, this);
		CValentineDungeon::instance().OnMobKilled(pkAttacker, this);
		CRuneDungeon::instance().OnMobKilled(pkAttacker, this);
		CPyramidDungeonRazor93::instance().OnMobKilled(pkAttacker, this);
		CNightmareDungeonRazor93::instance().OnMobKilled(pkAttacker, this);
		//CLostCastleDungeon::instance().OnMobKilled(pkAttacker, this);
		CHalloween2022Dungeon::instance().OnMobKilled(pkAttacker, this);
		CVikingDungeon::instance().OnMobKilled(pkAttacker, this);
		CEasterDungeon::instance().OnMobKilled(pkAttacker, this);
#endif

#ifdef ENABLE_BATTLE_PASS
		uint8_t bBattlePassId = pkAttacker->GetBattlePassId();
		if (bBattlePassId)
		{
			uint32_t dwMonsterVnum, dwToKillCount;
			if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, MONSTER_KILL, &dwMonsterVnum, &dwToKillCount))
			{
				if (dwMonsterVnum == GetRaceNum() && pkAttacker->GetMissionProgress(MONSTER_KILL, bBattlePassId) < dwToKillCount)
					pkAttacker->UpdateMissionProgress(MONSTER_KILL, bBattlePassId, 1, dwToKillCount);
			}
		}
#endif

		if (!number(0, 9))
		{
			if (pkAttacker->GetPoint(POINT_KILL_HP_RECOVERY))
			{
				int iHP = pkAttacker->GetMaxHP() * pkAttacker->GetPoint(POINT_KILL_HP_RECOVERY) / 100;
				pkAttacker->PointChange(POINT_HP, iHP);
				CreateFly(FLY_HP_SMALL, pkAttacker);
			}

			if (pkAttacker->GetPoint(POINT_KILL_SP_RECOVER))
			{
				int iSP = pkAttacker->GetMaxSP() * pkAttacker->GetPoint(POINT_KILL_SP_RECOVER) / 100;
				pkAttacker->PointChange(POINT_SP, iSP);
				CreateFly(FLY_SP_SMALL, pkAttacker);
			}
		}
	}
	//pu1.Pop();

#ifdef ENABLE_BLOCK_MULTIFARM
	if (pkAttacker->FindAffect(AFFECT_DROP_BLOCK, APPLY_NONE)) {
		return;
	}
#endif

	if (!bItemDrop)
		return;

	PIXEL_POSITION pos = GetXYZ();

	if (!ecs::GetMovablePosition(GetMapIndex(), pos.x, pos.y, pos))
		return;

	//
	//  
	//
	//PROF_UNIT pu2("r2");
	if (test_server)
		sys_log(0, "Drop money : Attacker %s", pkAttacker->GetName());
	RewardGold(pkAttacker);
	//pu2.Pop();

	//
	//  
	//
	//PROF_UNIT pu3("r3");
	LPITEM item;

	static std::vector<LPITEM> s_vec_item;
	s_vec_item.clear();

	if (ITEM_MANAGER::instance().CreateDropItem(this, pkAttacker, s_vec_item))
	{

#ifdef ENABLE_RARE_DROP_NOTICE_RAZOR93
		for (auto& item : s_vec_item)
		{
			if (verjema_szadba_ixtreeme.find(item->GetVnum()) != verjema_szadba_ixtreeme.end())
			{
				std::string message = MakeItemLink(item, pkAttacker, this);
				BroadcastNotice(message.c_str());
			}
		}
#endif
#ifdef ENABLE_DROP_INSTANT_INVENTORY
		const bool bInstantRewardToInventory = true;
#endif

		bool bSharedDungeonDrop = false;

#ifdef ENABLE_DUNGEON_SHARED_DROP_HWID
		// Dungeon party shared drop (ground + ownership) + HWID|HOST szures:
		// - csak mapindex 
		// - csak ha a killer partyban van
		// - ugyanazt a dropot kapja minden jogosult (kulon item peldany, ownershipelve)
		// - azonos HWID+HOST eseten csak 1 karakter kap (a legtobb dmg a mobra)

		if (GetDungeon() && pkAttacker && pkAttacker->IsPC() && !s_vec_item.empty())
		{
			const long lMapIndex = GetMapIndex(); // a megolt mob mapindexe

			if (
				(lMapIndex >= 3550000 && lMapIndex < 3560000)  // ork

				|| (lMapIndex >= 660000 && lMapIndex < 670000)   // dt
				|| (lMapIndex >= 3690000 && lMapIndex < 3700000)  // triton
				|| (lMapIndex >= 3570000 && lMapIndex < 3580000)  // pyramid
				|| (lMapIndex >= 3730000 && lMapIndex < 3740000)  // nightmare
				|| (lMapIndex >= 180000 && lMapIndex < 190000)   // bagoly
				|| (lMapIndex >= 2180000 && lMapIndex < 2190000)  // runa
				|| (lMapIndex >= 2120000 && lMapIndex < 2130000)  // meley
				|| (lMapIndex >= 3670000 && lMapIndex < 3680000)  // majom
				|| (lMapIndex >= 3520000 && lMapIndex < 3530000)  // nemere
				|| (lMapIndex >= 270000 && lMapIndex < 280000)   // slyme
				|| (lMapIndex >= 2080000 && lMapIndex < 2090000)  // beran
				|| (lMapIndex >= 2160000 && lMapIndex < 2170000)  // catacombe
				|| (lMapIndex >= 2090000 && lMapIndex < 2100000)  // ochao
				|| (lMapIndex >= 2100000 && lMapIndex < 2110000)  // valazslatos erdo
				|| (lMapIndex >= 3510000 && lMapIndex < 3520000)  // razador
				|| (lMapIndex >= 2170000 && lMapIndex < 2180000)  // pokbaro
				|| (lMapIndex >= 1610000 && lMapIndex < 1620000)  // vampir
				|| (lMapIndex >= 1790000 && lMapIndex < 1800000)  // viking
				)
			{
				if (pkAttacker->GetParty()) // CSAK partyra
				{
					CDungeon* pDungeon = GetDungeon();

					// csak akkor, ha a killer ugyanebben a dungeon instance-ben van
					if (pkAttacker->GetDungeon() == pDungeon)
					{
						// --- helper: HWID|HOST kulcs ugyanugy, ahogy nalad masutt is ---
						auto MakeHwidHostKey = [&](LegacyCharHandle ch) -> std::string
							{
								if (!ch || !ch->IsPC() || !ch->GetDesc())
									return std::string();

								DESC* d = ch->GetDesc();
								const char* hwid = d->GetHwid();
								const char* host = d->GetHostName();

								if (!hwid || !*hwid)
									return std::string();
								if (!host || !*host)
									return std::string();

								std::string key;
								key.reserve(128);
								key += hwid;
								key += "|";
								key += host;
								return key;
							};

						// 1) HWID|HOST alapjan 1 karakter / gep (dupe eseten a legtobb dmg kap)
						std::unordered_map<std::string, LegacyCharHandle> mapWinnerByKey;
						mapWinnerByKey.reserve(16);

						pDungeon->ForEachMember([&](LegacyCharHandle mch)
							{
								if (!mch || !mch->IsPC() || !mch->GetDesc())
									return;

								// ugyanabban a dungeon instance-ben kell legyen
								if (mch->GetDungeon() != pDungeon)
									return;

								//   ugyanazon a mapindexen legyen (INSTANCE) -> NINCS hibas normalizalas
								if (mch->GetMapIndex() != lMapIndex)
									return;

								// ugyanabban a partyban legyen
								if (mch->GetParty() != pkAttacker->GetParty())
									return;

								std::string key = MakeHwidHostKey(mch);

								// ha nincs hwid/host, fallback: account (ne kapjon duplan)
								if (key.empty())
									key = "ACC:" + std::to_string(mch->GetDesc()->GetAccountTable().id);

								auto it = mapWinnerByKey.find(key);
								if (it == mapWinnerByKey.end())
								{
									mapWinnerByKey.emplace(std::move(key), mch);
									return;
								}

								// dupe HWID|HOST: a legtobb dmg-et okozo kap
								uint64_t dmgNew = 0;
								uint64_t dmgOld = 0;

								auto itNew = m_map_kDamage.find(EntityOf(mch));
								if (itNew != m_map_kDamage.end())
									dmgNew = itNew->second.iTotalDamage;

								auto itOld = m_map_kDamage.find(EntityOf(it->second));
								if (itOld != m_map_kDamage.end())
									dmgOld = itOld->second.iTotalDamage;

								if (dmgNew > dmgOld)
									it->second = mch;
							});

						if (!mapWinnerByKey.empty())
						{
							// 2) template drop lementese (vnum/count/socket/attr)
							struct SPartySharedDropItem
							{
								uint32_t vnum;
								uint32_t count;
								long sockets[ITEM_SOCKET_MAX_NUM];
								TPlayerItemAttribute attrs[ITEM_ATTRIBUTE_MAX_NUM];
							};

							std::vector<SPartySharedDropItem> drops;
							drops.reserve(s_vec_item.size());

							for (LPITEM srcItem : s_vec_item)
							{
								if (!srcItem)
									continue;

								SPartySharedDropItem di{};
								di.vnum = srcItem->GetVnum();
								di.count = srcItem->GetCount();

								for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
									di.sockets[i] = srcItem->GetSocket(i);

								for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
									di.attrs[i] = srcItem->GetAttribute(i);

								drops.push_back(di);
							}

							// 3) kiosztas: minden HWID-unique winnernek ugyanaz a drop (ground + ownership)
							for (const auto& kv : mapWinnerByKey)
							{
								auto* rch = kv.second;
								if (!rch || !rch->IsPC() || !rch->GetDesc())
									continue;

								PIXEL_POSITION mpos = pos;

								// kis eltolas, hogy ne 1 pontra essen minden
								mpos.x = number(-7, 7) * 20 + GetX();
								mpos.y = number(-7, 7) * 20 + GetY();

								for (const auto& di : drops)
								{
									LPITEM newItem = ITEM_MANAGER::instance().CreateItem(di.vnum, di.count);
									if (!newItem)
										continue;

									for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
										newItem->SetSocket(i, di.sockets[i]);

									newItem->SetAttributes(di.attrs);

#ifdef ENABLE_DROP_INSTANT_INVENTORY
									if (bInstantRewardToInventory)
									{
										__GiveRewardItemToCharacterOrDrop(rch, this, newItem, mpos, true);
									}
									else
									{
										newItem->AddToGround(lMapIndex, mpos);

										if (CBattleArena::instance().IsBattleArenaMap(rch->GetMapIndex()) == false)
											newItem->SetOwnership(rch);

										newItem->StartDestroyEvent();
									}
#else
									newItem->AddToGround(lMapIndex, mpos);

									if (CBattleArena::instance().IsBattleArenaMap(rch->GetMapIndex()) == false)
										newItem->SetOwnership(rch);

									newItem->StartDestroyEvent();
#endif
								}
							}

							// 4) a template itemeket megsemmisitjuk, hogy ne duplazzon
							for (LPITEM srcItem : s_vec_item)
							{
								if (srcItem)
									M2_DESTROY_ITEM(srcItem);
							}

							s_vec_item.clear();
							bSharedDungeonDrop = true;
						}
					}
				}
			}
		}
#endif // ENABLE_DUNGEON_SHARED_DROP_HWID


		if (!bSharedDungeonDrop)
		{
#ifdef ENABLE_DROP_INSTANT_INVENTORY

			if (s_vec_item.size() == 0);
			else if (s_vec_item.size() == 1)
			{
				item = s_vec_item[0];

#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
				const bool bKeepGroundDrop = (pkAttacker && pkAttacker->GetParty());
#else
				const bool bKeepGroundDrop = false;
#endif

				if (bInstantRewardToInventory && !bKeepGroundDrop)
				{
					__GiveRewardItemToCharacterOrDrop(pkAttacker, this, item, pos, true);
				}
				else
				{
					item->AddToGround(GetMapIndex(), pos);

					if (CBattleArena::instance().IsBattleArenaMap(pkAttacker->GetMapIndex()) == false)
					{
#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
						if (pkAttacker->GetParty())
						{
							FPartyDropDiceRoll f(item, pkAttacker);
							f.Process(this);
						}
						else
							item->SetOwnership(pkAttacker);
#else
						item->SetOwnership(pkAttacker);
#endif
					}

					item->StartDestroyEvent();

					sys_log(0, "DROP_ITEM: %s %d %d from %s", item->GetName(), pos.x, pos.y, GetName());
				}

				pos.x = number(-7, 7) * 20;
				pos.y = number(-7, 7) * 20;
				pos.x += GetX();
				pos.y += GetY();
			}
			else
			{
				int iItemIdx = s_vec_item.size() - 1;

				std::priority_queue<std::pair<uint64_t, LegacyCharHandle> > pq;

				uint64_t total_dam = 0;

				for (TDamageMap::iterator it = m_map_kDamage.begin(); it != m_map_kDamage.end(); ++it)
				{
					uint64_t iDamage = it->second.iTotalDamage;
					if (iDamage > 0)
					{
						auto* ch = LegacyCharOf(it->first);

						if (ch)
						{
							pq.push(std::make_pair(iDamage, ch));
							total_dam += iDamage;
						}
					}
				}

				std::vector<LegacyCharHandle> v;

				while (!pq.empty() && pq.top().first * 10 >= total_dam)
				{
					v.push_back(pq.top().second);
					pq.pop();
				}

				if (v.empty())
				{
					while (iItemIdx >= 0)
					{
						item = s_vec_item[iItemIdx--];

						if (!item)
						{
							sys_err("item null in vector idx %d", iItemIdx + 1);
							continue;
						}

						item->AddToGround(GetMapIndex(), pos);

						if (pkAttacker && CBattleArena::instance().IsBattleArenaMap(pkAttacker->GetMapIndex()) == false)
							item->SetOwnership(pkAttacker);

						item->StartDestroyEvent();

						sys_log(0, "DROP_ITEM: %s %d %d by %s", item->GetName(), pos.x, pos.y, GetName());

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += GetX();
						pos.y += GetY();
					}
				}
				else
				{
					std::vector<LegacyCharHandle>::iterator it = v.begin();

					while (iItemIdx >= 0)
					{
						item = s_vec_item[iItemIdx--];

						if (!item)
						{
							sys_err("item null in vector idx %d", iItemIdx + 1);
							continue;
						}

						auto* ch = *it;

						if (ch->GetParty())
							ch = ch->GetParty()->GetNextOwnership(ch, GetX(), GetY());

						++it;

						if (it == v.end())
							it = v.begin();

#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
						const bool bKeepGroundDrop = (ch && ch->GetParty());
#else
						const bool bKeepGroundDrop = false;
#endif

						if (bInstantRewardToInventory && !bKeepGroundDrop)
						{
							__GiveRewardItemToCharacterOrDrop(ch, this, item, pos, true);
						}
						else
						{
							item->AddToGround(GetMapIndex(), pos);

							if (CBattleArena::instance().IsBattleArenaMap(ch->GetMapIndex()) == false)
							{
#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
								if (ch->GetParty())
								{
									FPartyDropDiceRoll f(item, ch);
									f.Process(this);
								}
								else
									item->SetOwnership(ch);
#else
								item->SetOwnership(ch);
#endif
							}

							item->StartDestroyEvent();

							sys_log(0, "DROP_ITEM: %s %d %d by %s", item->GetName(), pos.x, pos.y, GetName());
						}

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += GetX();
						pos.y += GetY();
					}
				}
			}

#else

			if (s_vec_item.size() == 0);
			else if (s_vec_item.size() == 1)
			{
				item = s_vec_item[0];
				item->AddToGround(GetMapIndex(), pos);

				if (CBattleArena::instance().IsBattleArenaMap(pkAttacker->GetMapIndex()) == false)
				{
#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
					if (pkAttacker->GetParty())
					{
						FPartyDropDiceRoll f(item, pkAttacker);
						f.Process(this);
					}
					else
						item->SetOwnership(pkAttacker);
#else
					item->SetOwnership(pkAttacker);
#endif
				}

				item->StartDestroyEvent();

				pos.x = number(-7, 7) * 20;
				pos.y = number(-7, 7) * 20;
				pos.x += GetX();
				pos.y += GetY();

				sys_log(0, "DROP_ITEM: %s %d %d from %s", item->GetName(), pos.x, pos.y, GetName());
			}
			else
			{
				int iItemIdx = s_vec_item.size() - 1;

				std::priority_queue<std::pair<uint64_t, LegacyCharHandle> > pq;

				uint64_t total_dam = 0;

				for (TDamageMap::iterator it = m_map_kDamage.begin(); it != m_map_kDamage.end(); ++it)
				{
					uint64_t iDamage = it->second.iTotalDamage;
					if (iDamage > 0)
					{
						auto* ch = LegacyCharOf(it->first);

						if (ch)
						{
							pq.push(std::make_pair(iDamage, ch));
							total_dam += iDamage;
						}
					}
				}

				std::vector<LegacyCharHandle> v;

				while (!pq.empty() && pq.top().first * 10 >= total_dam)
				{
					v.push_back(pq.top().second);
					pq.pop();
				}

				if (v.empty())
				{
					while (iItemIdx >= 0)
					{
						item = s_vec_item[iItemIdx--];

						if (!item)
						{
							sys_err("item null in vector idx %d", iItemIdx + 1);
							continue;
						}

						item->AddToGround(GetMapIndex(), pos);

						if (pkAttacker && CBattleArena::instance().IsBattleArenaMap(pkAttacker->GetMapIndex()) == false)
							item->SetOwnership(pkAttacker);

						item->StartDestroyEvent();

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += GetX();
						pos.y += GetY();

						sys_log(0, "DROP_ITEM: %s %d %d by %s", item->GetName(), pos.x, pos.y, GetName());
					}
				}
				else
				{
					std::vector<LegacyCharHandle>::iterator it = v.begin();

					while (iItemIdx >= 0)
					{
						item = s_vec_item[iItemIdx--];

						if (!item)
						{
							sys_err("item null in vector idx %d", iItemIdx + 1);
							continue;
						}

						item->AddToGround(GetMapIndex(), pos);

						auto* ch = *it;

						if (ch->GetParty())
							ch = ch->GetParty()->GetNextOwnership(ch, GetX(), GetY());

						++it;

						if (it == v.end())
							it = v.begin();

						if (CBattleArena::instance().IsBattleArenaMap(ch->GetMapIndex()) == false)
						{
#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
							if (ch->GetParty())
							{
								FPartyDropDiceRoll f(item, ch);
								f.Process(this);
							}
							else
								item->SetOwnership(ch);
#else
							item->SetOwnership(ch);
#endif
						}

						item->StartDestroyEvent();

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += GetX();
						pos.y += GetY();

						sys_log(0, "DROP_ITEM: %s %d %d by %s", item->GetName(), pos.x, pos.y, GetName());
					}
				}
			}

#endif
		}
	}

	m_map_kDamage.clear();
}


// char_battle.cpp slice BC2 moved into CombatSystem.cpp

void CHARACTER::RewardGold(LPCHARACTER pkAttacker) {

	if (!pkAttacker || !pkAttacker->IsPC())
		return;

	if (!m_pkMobData)
	{
		sys_err("RewardGold: NULL mob data (vid=%u race=%u name=%s map=%ld x=%ld y=%ld attacker=%s)",
			GetPacketVID(),
			GetRaceNum(),
			GetName(),
			GetMapIndex(),
			GetX(),
			GetY(),
			pkAttacker ? pkAttacker->GetName() : "<null>");
		return;
	}
	if (pkAttacker && pkAttacker->IsPC()) {
		if (IsStone()) {
#ifdef ENABLE_ANTICHEAT
			if (pkAttacker->GetMapIndex() < 1000) {
				pkAttacker->ProcessCheatCheck(get_global_time());
			}
#endif
#ifdef DISABLE_GOLD_DROP_FROM_TAKAKA
			if (GetRaceNum() >= TANAKA) {
				return;
			}
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
			if (pkAttacker->FindAffect(AFFECT_DROP_BLOCK, APPLY_NONE)) {
				return;
			}
#endif

			bool drop = true;
			int mylvl = pkAttacker->GetLevel(), targetlvl = GetLevel();
			if (mylvl > targetlvl) {
				drop = mylvl - targetlvl <= 15 ? true : false;
			}

			if (drop) {
				int64_t gold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);

				if (gold <= 0) {
					return;
				}

				if (pkAttacker->GetPoint(POINT_MALL_GOLDBONUS)) {
					gold += (gold * pkAttacker->GetPoint(POINT_MALL_GOLDBONUS) / 100);
				}

				pkAttacker->PointChange(POINT_GOLD, gold, true);
			}
		}
		else {
#ifdef ENABLE_BLOCK_MULTIFARM
			if (pkAttacker->FindAffect(AFFECT_DROP_BLOCK, APPLY_NONE)) {
				return;
			}
#endif

			// ADD_PREMIUM
			bool isAutoLoot =
				(pkAttacker->GetPremiumRemainSeconds(PREMIUM_AUTOLOOT) > 0 ||
					pkAttacker->IsEquipUniqueGroup(UNIQUE_GROUP_AUTOLOOT))
				? true : false; // 3 
			// END_OF_ADD_PREMIUM

			PIXEL_POSITION pos;

			if (!isAutoLoot)
				if (!ecs::GetMovablePosition(GetMapIndex(), GetX(), GetY(), pos))
					return;

			int iTotalGold = 0;
			//
			// ---------   Ȯ  ----------
			//
			int iGoldPercent = MobRankStats[GetMobRank()].iGoldPercent;

			if (pkAttacker->IsPC())
				iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP)) / 100;

#ifdef ENABLE_EVENT_MANAGER
			if (pkAttacker->IsPC())
			{
				const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(YANG_DROP_EVENT, pkAttacker->GetEmpire());
				if (event != nullptr)
					iGoldPercent = iGoldPercent * (100 + (event->value[0] + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP))) / 100;
				else
					iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP)) / 100;
			}
#else
			if (pkAttacker->IsPC())
				iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP)) / 100;
#endif

			iGoldPercent = iGoldPercent * CHARACTER_MANAGER::instance().GetMobGoldDropRate(pkAttacker) / 100;

			// ADD_PREMIUM
			if (pkAttacker->GetPremiumRemainSeconds(PREMIUM_GOLD) > 0 ||
				pkAttacker->IsEquipUniqueGroup(UNIQUE_GROUP_LUCKY_GOLD))
				iGoldPercent += iGoldPercent;
			// END_OF_ADD_PREMIUM

			if (iGoldPercent > 100)
				iGoldPercent = 100;

			int iPercent;

			if (GetMobRank() >= MOB_RANK_BOSS)
				iPercent = ((iGoldPercent * PERCENT_LVDELTA_BOSS(pkAttacker->GetLevel(), GetLevel())) / 100);
			else
				iPercent = ((iGoldPercent * PERCENT_LVDELTA(pkAttacker->GetLevel(), GetLevel())) / 100);
			//int iPercent = CALCULATE_VALUE_LVDELTA(pkAttacker->GetLevel(), GetLevel(), iGoldPercent);

			if (number(1, 100) > iPercent)
				return;

			int iGoldMultipler = 1;

			if (1 == number(1, 50000)) // 1/50000 Ȯ  10
				iGoldMultipler *= 10;
			else if (1 == number(1, 10000)) // 1/10000 Ȯ  5
				iGoldMultipler *= 5;

			//  
			if (pkAttacker->GetPoint(POINT_GOLD_DOUBLE_BONUS))
				if (number(1, 100) <= pkAttacker->GetPoint(POINT_GOLD_DOUBLE_BONUS))
					iGoldMultipler *= 2;

			//
			// ---------     ----------
			//
			if (test_server)
				ecs::ChatSystem::Send(AIHelpers::EcsOf(pkAttacker), CHAT_TYPE_PARTY, "gold_mul %d rate %d", iGoldMultipler, CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker));

			//
			// ---------   ó -------------
			//
			LPITEM item;

			int iGold10DropPct = 100;
#ifdef ENABLE_EVENT_MANAGER
			const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(YANG_DROP_EVENT, pkAttacker->GetEmpire());
			if (event != nullptr)
				iGold10DropPct = (iGold10DropPct * 100) / (100 + event->value[0] + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD10_DROP));
			else
				iGold10DropPct = (iGold10DropPct * 100) / (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD10_DROP));
#else
			iGold10DropPct = (iGold10DropPct * 100) / (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD10_DROP));
#endif

			// MOB_RANK BOSS   ź
			if (GetMobRank() >= MOB_RANK_BOSS && !IsStone() && GetMobTable().dwGoldMax != 0)
			{
				if (1 == number(1, iGold10DropPct))
					iGoldMultipler *= 10; // 1% Ȯ  10

				int iSplitCount = number(25, 35);

				for (int i = 0; i < iSplitCount; ++i)
				{
					int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax) / iSplitCount;
					if (test_server)
						sys_log(0, "iGold %d", iGold);
					iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker) / 100;
					iGold *= iGoldMultipler;

					if (iGold == 0)
					{
						continue;
					}

					if (test_server)
					{
						sys_log(0, "Drop Moeny MobGoldAmountRate %d %d", CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker), iGoldMultipler);
						sys_log(0, "Drop Money gold %d GoldMin %d GoldMax %d", iGold, GetMobTable().dwGoldMax, GetMobTable().dwGoldMax);
					}

					// NOTE:  ź  3  ó  
					if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold)))
					{
#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93

						pkAttacker->GiveGold(iGold);
						iTotalGold += iGold;
#else
						pos.x = GetX() + ((number(-14, 14) + number(-14, 14)) * 23);
						pos.y = GetY() + ((number(-14, 14) + number(-14, 14)) * 23);

						item->AddToGround(GetMapIndex(), pos);
						item->StartDestroyEvent();

						iTotalGold += iGold; // Total gold
#endif
					}
				}
			}
			// 1% Ȯ  10  ߸. (10 )
			else if (1 == number(1, iGold10DropPct))
			{
				//
				//  ź 
				//
				for (int i = 0; i < 10; ++i)
				{
					int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);
					iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker) / 100;
					iGold *= iGoldMultipler;

					if (iGold == 0)
					{
						continue;
					}

					// NOTE:  ź  3  ó  
					if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold)))
					{
#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93

						pkAttacker->GiveGold(iGold);
						iTotalGold += iGold;
#else
						pos.x = GetX() + (number(-7, 7) * 20);
						pos.y = GetY() + (number(-7, 7) * 20);

						item->AddToGround(GetMapIndex(), pos);
						item->StartDestroyEvent();

						iTotalGold += iGold; // Total gold
#endif
					}

				}
			}
			else
			{
				//
				// Ϲ   
				//
				int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);
				iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker) / 100;
				iGold *= iGoldMultipler;

				int iSplitCount;

				if (iGold >= 3)
					iSplitCount = number(1, 3);
				else if (GetMobRank() >= MOB_RANK_BOSS)
				{
					iSplitCount = number(3, 10);

					if ((iGold / iSplitCount) == 0)
						iSplitCount = 1;
				}
				else
					iSplitCount = 1;

				if (iGold != 0)
				{
					iTotalGold += iGold; // Total gold

					for (int i = 0; i < iSplitCount; ++i)
					{
						const int64_t splitGold = iGold / iSplitCount;
						if (isAutoLoot)
						{
							pkAttacker->GiveGold(splitGold);
						}
						else if ((item = ITEM_MANAGER::instance().CreateItem(1, splitGold)))
						{
#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93

							pkAttacker->GiveGold(splitGold);
#else

							pos.x = GetX() + (number(-7, 7) * 20);
							pos.y = GetY() + (number(-7, 7) * 20);

							item->AddToGround(GetMapIndex(), pos);
							item->StartDestroyEvent();
#endif
						}
					}
				}
			}
		}

		//DBManager::instance().SendMoneyLog(MONEY_LOG_MONSTER, GetRaceNum(), iTotalGold);
	}
}

// char_battle.cpp slice BB2b moved into CombatSystem.cpp

#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
static void ProcessStoneSpawnStep(LegacyCharHandle ch);
#endif
static int64_t CalcReferenceBowHitDamage(LegacyCharHandle pAttacker, LegacyCharHandle pVictim);
static int64_t CalcReferenceBasicHitDamage(LegacyCharHandle pAttacker, LegacyCharHandle pVictim);
static int64_t CalcReferenceNormalHitDamage(LegacyCharHandle pAttacker, LegacyCharHandle pVictim);

bool CHARACTER::Damage(LPCHARACTER pAttacker, int64_t dam, EDamageType type) // returns true if dead
{
#ifdef DISABLE_PC_ATTACK_PC_ON_MAPIDEX1
	if (pAttacker && pAttacker->IsPC() && IsPC() && GetMapIndex() == 1)
		return false;
#endif
	if (GetInvincible())
		return false;

#ifdef __NEWPET_SYSTEM__
	if (IsImmortal())
		return false;
#endif

	if (pAttacker)
	{
		if (pAttacker->IsAffectFlag(AFF_GWIGUM) && !pAttacker->GetWear(WEAR_WEAPON))
		{
			pAttacker->RemoveAffect(SKILL_GWIGEOM);
			return false;
		}

		if (pAttacker->IsAffectFlag(AFF_GEOMGYEONG) && !pAttacker->GetWear(WEAR_WEAPON))
		{
			pAttacker->RemoveAffect(SKILL_GEOMKYUNG);
			return false;
		}

	}

	if ((IsPC() && IsAffectFlag(AFF_REVIVE_INVISIBLE)) || (pAttacker && (pAttacker->IsPC() && pAttacker->IsAffectFlag(AFF_REVIVE_INVISIBLE))))
		return false;

#ifdef ENABLE_NEWSTUFF
	if (pAttacker && IsStone() && pAttacker->IsPC())
	{
		if (GetEmpire() && GetEmpire() == pAttacker->GetEmpire())
		{
			SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
			return false;
		}
	}
#endif

	if (DAMAGE_TYPE_MAGIC == type)
	{
		dam = (int)((float)dam * (100 + (pAttacker->GetPoint(POINT_MAGIC_ATT_BONUS_PER) + pAttacker->GetPoint(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100.f + 0.5f);
	}

	// Ÿ ƴ   ó
	if (type != DAMAGE_TYPE_NORMAL && type != DAMAGE_TYPE_NORMAL_RANGE)
	{
		if (IsAffectFlag(AFF_TERROR))
		{
			int pct = GetSkillPower(SKILL_TERROR) / 400;

			if (number(1, 100) <= pct)
				return false;
		}
	}
#ifdef ENABLE_MAX_100K_DMG_ON_EVENT_MAP_RAZOR93
	if (pAttacker && pAttacker->IsPC() && GetMapIndex() == 1 && (IsMonster() || IsStone()))
	{
#ifdef DISABLE_DAMAGE_TYPE_NORMAL_RANGE_EVENT_MAP



		LPITEM pkWeap = pAttacker->GetWear(WEAR_WEAPON);
		if (pkWeap && pkWeap->GetProto()->bSubType == WEAPON_BOW)
		{
			 
			SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
			return false;  
		}
#endif // !DISABLE_DAMAGE_TYPE_NORMAL_RANGE_EVENT_MAP
		const int64_t fixed_dam = 100000;

		// [1] Regisztrljuk a sebzst a dropphoz
		const entt::entity eAttacker = EntityOf(pAttacker);
		if (eAttacker == entt::null)
			return false;

		auto it = m_map_kDamage.find(eAttacker);
		if (it == m_map_kDamage.end())
		{
			m_map_kDamage.insert(std::make_pair(
				eAttacker,
				TBattleInfo(fixed_dam, 0)
			));
		}
		else
		{
			it->second.iTotalDamage += fixed_dam;
		}



		SendDamagePacket(pAttacker, fixed_dam, DAMAGE_NORMAL);


		if (GetHP() <= fixed_dam)
		{
			SetHP(0);
			Dead(pAttacker);
			return true; // nem megy tovbb
		}
		else
		{
			PointChange(POINT_HP, -fixed_dam, false);
			return false; // nem megy tovbb
		}
	}
#endif



	int iCurHP = GetHP();
	int iCurSP = GetSP();

	bool IsCritical = false;
	bool IsPenetrate = false;
	bool IsDeathBlow = false;

	//PROF_UNIT puAttr("Attr");

	//
	//  ų,  ų(ڰ) ũƼð,   Ѵ.
	//   ʾƾ ϴµ Nerf(ٿ뷱)ġ    ũƼð
	//     ʰ, /2 ̻Ͽ Ѵ.
	//
	//  ̾߱Ⱑ Ƽ и ų ߰
	//
	// 20091109 : 簡  û   г,     70% 
	//

#if defined(ENABLE_DS_RUNE) || defined(ENABLE_MELEY_LAIR)
	int32_t itakehp = 0;
#endif

	if (type == DAMAGE_TYPE_MELEE || type == DAMAGE_TYPE_RANGE || type == DAMAGE_TYPE_MAGIC)
	{
		if (pAttacker)
		{
			// ũƼ
			int iCriticalPct = pAttacker->GetPoint(POINT_CRITICAL_PCT);

			if (!IsPC()) {
				iCriticalPct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);
				iCriticalPct += pAttacker->GetPoint(POINT_PVM_CRITICAL_PCT);
			}

			if (iCriticalPct)
			{
				if (iCriticalPct >= 10) // 10 ũ 5% + (4 1% ),  ġ 50̸ 20%
					iCriticalPct = 5 + (iCriticalPct - 10) / 4;
				else // 10  ܼ  , 10 = 5%
					iCriticalPct /= 2;

				//ũƼ   .
				iCriticalPct -= GetPoint(POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					EffectPacket(SE_CRITICAL);

					if (IsAffectFlag(AFF_MANASHIELD))
					{
						RemoveAffect(AFF_MANASHIELD);
					}
				}
			}

			// 
			int iPenetratePct = pAttacker->GetPoint(POINT_PENETRATE_PCT);

			if (!IsPC())
				iPenetratePct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);


			if (iPenetratePct)
			{
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(SKILL_RESIST_PENETRATE);

					if (nullptr != pkSk)
					{
						pkSk->SetPointVar("k", 1.0f * GetSkillPower(SKILL_RESIST_PENETRATE) / 100.0f);

						iPenetratePct -= static_cast<int>(pkSk->kPointPoly.Eval());
					}
				}

				if (iPenetratePct >= 10)
				{
					// 10 ũ 5% + (4 1% ),  ġ 50̸ 20%
					iPenetratePct = 5 + (iPenetratePct - 10) / 4;
				}
				else
				{
					// 10  ܼ  , 10 = 5%
					iPenetratePct /= 2;
				}

				//Ÿ   .
				iPenetratePct -= GetPoint(POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;
#ifdef TEXTS_IMPROVEMENT
					if (test_server) {
						ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 257, "%d", GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100);
					}
#endif
					dam += GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100;

					if (IsAffectFlag(AFF_MANASHIELD))
					{
						RemoveAffect(AFF_MANASHIELD);
					}
#ifdef ENABLE_EFFECT_PENETRATE
					EffectPacket(SE_PENETRATE);
#endif
				}
			}
		}
	}
	//
	// ޺ , Ȱ ,  Ÿ   Ӽ  Ѵ.
	//
	else if (type == DAMAGE_TYPE_NORMAL || type == DAMAGE_TYPE_NORMAL_RANGE)
	{
		if (type == DAMAGE_TYPE_NORMAL)
		{
			//  Ÿ    
			if (GetPoint(POINT_BLOCK) && number(1, 100) <= GetPoint(POINT_BLOCK))
			{
#ifdef TEXTS_IMPROVEMENT
				if (test_server) {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pAttacker), CHAT_TYPE_INFO, 95, "%s#%d", GetName(), GetPoint(POINT_BLOCK));
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 95, "%s#%d", pAttacker->GetName(), pAttacker->GetPoint(POINT_BLOCK));
				}
#endif
				SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
				return false;
			}
		}
		else if (type == DAMAGE_TYPE_NORMAL_RANGE)
		{
			// Ÿ Ÿ    
			if (GetPoint(POINT_DODGE) && number(1, 100) <= GetPoint(POINT_DODGE))
			{
#ifdef TEXTS_IMPROVEMENT
				if (test_server) {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pAttacker), CHAT_TYPE_INFO, 96, "%s#%d", GetName(), GetPoint(POINT_DODGE));
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 96, "%s#%d", pAttacker->GetName(), pAttacker->GetPoint(POINT_DODGE));
				}
#endif
				SendDamagePacket(pAttacker, 0, DAMAGE_DODGE);
				return false;
			}
		}

#ifndef ENABLE_NO_MALUS_JEONGWIHON
		if (IsAffectFlag(AFF_JEONGWIHON))
			dam = (int)(dam * (100 + GetSkillPower(SKILL_JEONGWI) * 25 / 100) / 100);
#endif

		if (IsAffectFlag(AFF_TERROR))
			dam = (int)(dam * (95 - GetSkillPower(SKILL_TERROR) / 5) / 100);

		//if (IsAffectFlag(AFF_HOSIN))
		//	dam = dam * (100 - GetPoint(POINT_RESIST_NORMAL_DAMAGE)) / 100;
		if (IsAffectFlag(AFF_HOSIN))
		{
			int32_t resist = GetPoint(POINT_RESIST_NORMAL_DAMAGE);

			// clamp 0..100
			if (resist < 0) resist = 0;
			if (resist > 100) resist = 100;

			// PvP: csak fele hasson
			if (pAttacker && pAttacker->IsPC() && IsPC())
				resist = (resist + 1) / 2; // kerekítve: 1->1, 2->1, 3->2...
			if (pAttacker && pAttacker->IsMonster() && IsPC())
				resist = (resist + 1) / 2; // kerekítve: 1->1, 2->1, 3->2...
			dam = dam * (100 - resist) / 100;
		}
		//
		//  Ӽ 
		//
		if (pAttacker)
		{
			if (type == DAMAGE_TYPE_NORMAL)
			{
				// ݻ
				if (GetPoint(POINT_REFLECT_MELEE))
				{
					int reflectDamage = dam * GetPoint(POINT_REFLECT_MELEE) / 100;

					// NOTE: ڰ IMMUNE_REFLECT Ӽ ִٸ ݻ縦  ϴ 
					// ƴ϶ 1/3  ؼ  ȹ û.
					if (pAttacker->IsImmune(IMMUNE_REFLECT))
						reflectDamage = int(reflectDamage / 3.0f + 0.5f);

					pAttacker->Damage(this, reflectDamage, DAMAGE_TYPE_SPECIAL);
				}
			}

			// ũƼ
			int iCriticalPct = pAttacker->GetPoint(POINT_CRITICAL_PCT);

			if (!IsPC()) {
				iCriticalPct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);
				iCriticalPct += pAttacker->GetPoint(POINT_PVM_CRITICAL_PCT);
			}

			if (iCriticalPct)
			{
				//ũƼ   .
				iCriticalPct -= GetPoint(POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					EffectPacket(SE_CRITICAL);
				}
			}

			// 
			int iPenetratePct = pAttacker->GetPoint(POINT_PENETRATE_PCT);

			if (!IsPC())
				iPenetratePct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

			{
				CSkillProto* pkSk = CSkillManager::instance().Get(SKILL_RESIST_PENETRATE);

				if (nullptr != pkSk)
				{
					pkSk->SetPointVar("k", 1.0f * GetSkillPower(SKILL_RESIST_PENETRATE) / 100.0f);

					iPenetratePct -= static_cast<int>(pkSk->kPointPoly.Eval());
				}
			}


			if (iPenetratePct)
			{

				//Ÿ   .
				iPenetratePct -= GetPoint(POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;
					dam += GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100;
#ifdef ENABLE_EFFECT_PENETRATE
					EffectPacket(SE_PENETRATE);
#endif
				}
			}

#ifdef ENABLE_BUG_FIXES
			if (int64_t iStealHP_ptr = pAttacker->GetPoint(POINT_STEAL_HP)) {
				if (number(1, 100) <= iStealHP_ptr) {
					int64_t iHP = std::min((int64_t)dam, std::max((int64_t)0, GetHP())) * pAttacker->GetPoint(POINT_STEAL_HP) / 100;


					if ((pAttacker->GetHP() > 0) && (pAttacker->GetHP() + iHP < pAttacker->GetMaxHP()) && (GetHP() > 0) && (iHP > 0)) {
						CreateFly(FLY_HP_MEDIUM, pAttacker);
						pAttacker->PointChange(POINT_HP, iHP);
#if defined(ENABLE_DS_RUNE) || defined(ENABLE_MELEY_LAIR)
						int32_t racevnum = GetRaceNum();
						if (
#if defined(ENABLE_DS_RUNE)
							racevnum == 3996 || racevnum == 3997 || racevnum == 3998 || racevnum == 4011 || racevnum == 4012 || racevnum == 4013
#endif
#if defined(ENABLE_MELEY_LAIR)
#ifdef ENABLE_DS_RUNE
							|| racevnum == 6118
#else
							racevnum == 6118
#endif
#endif
							)
						{
							itakehp = iHP;
						}
						else
						{
							PointChange(POINT_HP, -iHP);
						}
#else
						PointChange(POINT_HP, -iHP);
#endif
					}
				}
			}

			if (int64_t iStealSP_ptr = pAttacker->GetPoint(POINT_STEAL_SP)) {
				if (IsPC() && pAttacker->IsPC()) {
					if (number(1, 100) <= iStealSP_ptr) {
						int64_t iSP = std::min((int64_t)dam, std::max((int64_t)0, GetSP())) * pAttacker->GetPoint(POINT_STEAL_SP) / 100;


						if ((pAttacker->GetSP() > 0) && (pAttacker->GetSP() + iSP < pAttacker->GetMaxSP()) && (GetSP() > 0) && (iSP > 0))
						{
							CreateFly(FLY_SP_MEDIUM, pAttacker);
							pAttacker->PointChange(POINT_SP, iSP);
							PointChange(POINT_SP, -iSP);
						}
					}
				}
			}
#else
			// HP ƿ
			if (pAttacker->GetPoint(POINT_STEAL_HP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iHP = MIN(dam, MAX(0, iCurHP)) * pAttacker->GetPoint(POINT_STEAL_HP) / 100;

					if (iHP > 0 && GetHP() >= iHP)
					{
						CreateFly(FLY_HP_SMALL, pAttacker);
						pAttacker->PointChange(POINT_HP, iHP);
#if defined(ENABLE_DS_RUNE) || defined(ENABLE_MELEY_LAIR)
						if (
#if defined(ENABLE_DS_RUNE)
							racevnum == 3996 || racevnum == 3997 || racevnum == 3998 || racevnum == 4011 || racevnum == 4012 || racevnum == 4013
#endif
#if defined(ENABLE_MELEY_LAIR)
#ifdef ENABLE_DS_RUNE
							|| racevnum == 6118
#else
							racevnum == 6118
#endif
#endif
							)
						{
							itakehp = iHP;
						}
						else
						{
							PointChange(POINT_HP, -iHP);
						}
#else
						PointChange(POINT_HP, -iHP);
#endif
					}
				}
			}

			// SP ƿ
			if (pAttacker->GetPoint(POINT_STEAL_SP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iCur;

					if (IsPC())
						iCur = iCurSP;
					else
						iCur = iCurHP;

					int iSP = MIN(dam, MAX(0, iCur)) * pAttacker->GetPoint(POINT_STEAL_SP) / 100;

					if (iSP > 0 && iCur >= iSP)
					{
						CreateFly(FLY_SP_SMALL, pAttacker);
						pAttacker->PointChange(POINT_SP, iSP);

						if (IsPC())
							PointChange(POINT_SP, -iSP);
					}
				}
			}
#endif

			//  ƿ
			if (pAttacker->GetPoint(POINT_STEAL_GOLD))
			{
				if (number(1, 100) <= pAttacker->GetPoint(POINT_STEAL_GOLD))
				{
					int iAmount = number(1, GetLevel());
					pAttacker->PointChange(POINT_GOLD, iAmount);
					DBManager::instance().SendMoneyLog(MONEY_LOG_MISC, 1, iAmount);
				}
			}

#ifdef ENABLE_BUG_FIXES
			int iAbsoHP_ptr = pAttacker->GetPoint(POINT_HIT_HP_RECOVERY);
			if (iAbsoHP_ptr > 0) {
				if (number(1, 100) <= iAbsoHP_ptr) {
					int iHPAbso = std::min(dam, GetHP()) * pAttacker->GetPoint(POINT_HIT_HP_RECOVERY) / 100;
					if ((pAttacker->GetHP() > 0) && (pAttacker->GetHP() + iHPAbso < pAttacker->GetMaxHP()) && (GetHP() > 0) && (iHPAbso > 0)) {
						CreateFly(FLY_HP_SMALL, pAttacker);
						pAttacker->PointChange(POINT_HP, iHPAbso);
					}
				}
			}

			int64_t iAbsoSP_ptr = pAttacker->GetPoint(POINT_HIT_SP_RECOVERY);
			if (iAbsoSP_ptr > 0) {
				if (number(1, 100) <= iAbsoSP_ptr) {
					int64_t iSPAbso = std::min(dam, GetSP()) * pAttacker->GetPoint(POINT_HIT_SP_RECOVERY) / 100;
					if ((pAttacker->GetSP() > 0) && (pAttacker->GetSP() + iSPAbso < pAttacker->GetMaxSP()) && (GetSP() > 0) && (iSPAbso > 0)) {
						CreateFly(FLY_SP_SMALL, pAttacker);
						pAttacker->PointChange(POINT_SP, iSPAbso);
					}
				}
			}
#else
			// ĥ  HPȸ
			if (pAttacker->GetPoint(POINT_HIT_HP_RECOVERY) && number(0, 4) > 0) // 80% Ȯ
			{
				int i = ((iCurHP >= 0) ? MIN(dam, iCurHP) : dam) * pAttacker->GetPoint(POINT_HIT_HP_RECOVERY) / 100; //@fixme107

				if (i)
				{
					CreateFly(FLY_HP_SMALL, pAttacker);
					pAttacker->PointChange(POINT_HP, i);
				}
			}

			// ĥ  SPȸ
			if (pAttacker->GetPoint(POINT_HIT_SP_RECOVERY) && number(0, 4) > 0) // 80% Ȯ
			{
				int i = ((iCurHP >= 0) ? MIN(dam, iCurHP) : dam) * pAttacker->GetPoint(POINT_HIT_SP_RECOVERY) / 100; //@fixme107

				if (i)
				{
					CreateFly(FLY_SP_SMALL, pAttacker);
					pAttacker->PointChange(POINT_SP, i);
				}
			}
#endif

			//   ش.
			if (pAttacker->GetPoint(POINT_MANA_BURN_PCT))
			{
				if (number(1, 100) <= pAttacker->GetPoint(POINT_MANA_BURN_PCT))
					PointChange(POINT_SP, -50);
			}
		}
	}

	//
	// Ÿ Ǵ ų  ʽ / 
	//
	switch (type)
	{
	case DAMAGE_TYPE_NORMAL:
	case DAMAGE_TYPE_NORMAL_RANGE:
	{
		if (pAttacker) {
			if (pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS))
				dam = dam * (100 + pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;
#ifdef ENABLE_MEDI_PVM
			if (IsNPC())
				dam = dam * (100 + pAttacker->GetPoint(POINT_ATTBONUS_MEDI_PVM)) / 100;
#endif
		}

		dam = dam * (100 - std::min((int64_t)99, GetPoint(POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;
	}
	break;
	case DAMAGE_TYPE_MELEE:
	case DAMAGE_TYPE_RANGE:
	case DAMAGE_TYPE_FIRE:
	case DAMAGE_TYPE_ICE:
	case DAMAGE_TYPE_ELEC:
	case DAMAGE_TYPE_MAGIC:
	{
		if (pAttacker) {
			const int64_t skillBonus = pAttacker->GetPoint(POINT_SKILL_DAMAGE_BONUS);
			if (skillBonus)
				dam = dam * (100 + skillBonus) / 100;
		}

		int64_t def = GetPoint(POINT_SKILL_DEFEND_BONUS);
		def = std::clamp<int64_t>(def, 0, 100);

		if (pAttacker && pAttacker->IsPC() && IsPC())
			def = (def * 75 + 50) / 100;

		dam = dam * (100 - def) / 100;

		
		if (pAttacker && pAttacker->IsPC() && IsNPC())
		{
			const int64_t normalRef = CalcReferenceBasicHitDamage(pAttacker, this);
			if (normalRef > 0)
			{
				int64_t minSkillDam = normalRef * 10;

				//const int64_t skillBonus = std::max<int64_t>(0, pAttacker->GetPoint(POINT_SKILL_DAMAGE_BONUS));
				//minSkillDam = minSkillDam * (100 + skillBonus) / 100;

				if (dam < minSkillDam)
					dam = minSkillDam;
			}
		}
	}
	break;
	}

	//
	// (żȣ)
	//
	if (IsAffectFlag(AFF_MANASHIELD))
	{
		// POINT_MANASHIELD  ۾ 
		int iDamageSPPart = dam / 3;
		int iDamageToSP = iDamageSPPart * GetPoint(POINT_MANASHIELD) / 100;
		int iSP = GetSP();

		// SP     
		if (iDamageToSP <= iSP)
		{
			PointChange(POINT_SP, -iDamageToSP);
			dam -= iDamageSPPart;
		}
		else
		{
			// ŷ ڶ ǰ  ￩ҋ
			PointChange(POINT_SP, -GetSP());
			dam -= iSP * 100 / std::max(GetPoint(POINT_MANASHIELD), (int64_t)1);
		}
	}

	//
	// ü   ( )
	//
	//if (GetPoint(POINT_MALL_DEFBONUS) > 0)
	//{
	//	int64_t dec_dam = std::min((int64_t)200, dam * GetPoint(POINT_MALL_DEFBONUS) / 100);//razor93
	//	dam -= dec_dam;
	//}

	if (pAttacker)
	{
		//
		// ü ݷ  ( )
		//
		if (pAttacker->GetPoint(POINT_MALL_ATTBONUS) > 0)
		{
			int64_t add_dam = std::min((int64_t)300, dam * pAttacker->GetLimitPoint(POINT_MALL_ATTBONUS) / 100);
			dam += add_dam;
		}

		if (pAttacker->IsPC())
		{
			int iEmpire = pAttacker->GetEmpire();
			int32_t lMapIndex = pAttacker->GetMapIndex();
			int iMapEmpire = ecs::GetEmpireFromMap(lMapIndex);

			// ٸ     10% 
			if (iEmpire && iMapEmpire && iEmpire != iMapEmpire)
			{
				dam = dam * 9 / 10;
			}

			if (!IsPC() && GetMonsterDrainSPPoint())
			{
				int iDrain = GetMonsterDrainSPPoint();

				if (iDrain <= pAttacker->GetSP())
					pAttacker->PointChange(POINT_SP, -iDrain);
				else
				{
					int iSP = pAttacker->GetSP();
					pAttacker->PointChange(POINT_SP, -iSP);
				}
			}

		}
		else if (pAttacker->IsGuardNPC())
		{
						if (auto* flags = RuntimeFlags(this))
				SET_BIT(flags->instantFlag, INSTANT_FLAG_NO_REWARD);
			Stun();
			return true;
		}
	}
	//puAttr.Pop();

	if (!GetSectree() || GetSectree()->IsAttr(GetX(), GetY(), ATTR_BANPK))
		return false;

	if (!IsPC())
	{
		if (m_pkParty && m_pkParty->GetLeader())
			m_pkParty->GetLeader()->SetLastAttacked(get_dword_time());
		else
			SetLastAttacked(get_dword_time());
	}

	if (IsStun())
	{
		Dead(pAttacker);
		return true;
	}

	if (IsDead())
		return true;

	//    ʵ .
	if (type == DAMAGE_TYPE_POISON)
	{
		if (GetHP() - dam <= 0)
		{
			dam = GetHP() - 1;
		}
	}
#ifdef ENABLE_WOLFMAN_CHARACTER
	else if (type == DAMAGE_TYPE_BLEEDING)
	{
		if (GetHP() - dam <= 0)
		{
			dam = GetHP();
		}
	}
#endif
	// ------------------------
	//  ̾ 
	// -----------------------
	if (pAttacker && pAttacker->IsPC())
	{
		int iDmgPct = CHARACTER_MANAGER::instance().GetUserDamageRate(pAttacker);
		dam = dam * iDmgPct / 100;
	}

	// STONE SKIN :   
	if (IsMonster() && IsStoneSkinner())
	{
		if (GetHPPct() < GetMobTable().bStoneSkinPoint)
			dam /= 2;
	}

	//PROF_UNIT puRest1("Rest1");
	if (pAttacker)
	{
		// DEATH BLOW : Ȯ  4  (!?  ̺Ʈ  ͸ )
		if (pAttacker->IsMonster() && pAttacker->IsDeathBlower())
		{
			if (pAttacker->IsDeathBlow())
			{
				if (number(1, 4) == GetJob())
				{
					IsDeathBlow = true;
					dam = dam * 4;
				}
			}
		}

		uint8_t damageFlag = 0;

		if (type == DAMAGE_TYPE_POISON)
			damageFlag = DAMAGE_POISON;
#if defined(ENABLE_WOLFMAN_CHARACTER) && !defined(USE_MOB_BLEEDING_AS_POISON)
		else if (type == DAMAGE_TYPE_BLEEDING)
			damageFlag = DAMAGE_BLEEDING;
#elif defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_MOB_BLEEDING_AS_POISON)
		else if (type == DAMAGE_TYPE_BLEEDING)
			damageFlag = DAMAGE_POISON;
#endif
		else
			damageFlag = DAMAGE_NORMAL;

		if (IsCritical == true)
			damageFlag |= DAMAGE_CRITICAL;

		if (IsPenetrate == true)
			damageFlag |= DAMAGE_PENETRATE;


		//  
		float damMul = this->GetDamMul();
		float tempDam = dam;
		dam = tempDam * damMul + 0.5f;

#ifdef ENABLE_BATTLE_PASS
		if (dam > 0)
		{
			uint8_t bBattlePassId = pAttacker->GetBattlePassId();
			if (bBattlePassId)
			{
				if (IsPC())
				{
					uint32_t dwMinLevel, dwDamage;
					uint32_t dwLevel = GetLevel();
					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, PLAYER_DAMAGE, &dwMinLevel, &dwDamage))
					{
						if (!pAttacker->IsCompletedMission(PLAYER_DAMAGE))
						{
							uint32_t dwDam = dam;
							if (dwLevel >= dwMinLevel && GetMissionProgress(PLAYER_DAMAGE, bBattlePassId) < dwDam)
							{
								pAttacker->UpdateMissionProgress(PLAYER_DAMAGE, bBattlePassId, dwDam, dwDamage);
							}
						}
					}
				}
				else
				{
					uint32_t dwMonsterVnum, dwDamage;
					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, MONSTER_DAMAGE, &dwMonsterVnum, &dwDamage))
					{
						uint32_t dwRaceNum = GetRaceNum();
						if (!pAttacker->IsCompletedMission(MONSTER_DAMAGE))
						{
							uint32_t dwDam = dam;
							if (dwMonsterVnum == dwRaceNum && GetMissionProgress(MONSTER_DAMAGE, bBattlePassId) < dwDam)
							{
								pAttacker->UpdateMissionProgress(MONSTER_DAMAGE, bBattlePassId, dwDam, dwDamage);
							}
						}
					}
				}
			}
		}
#endif

#if defined(ENABLE_DS_RUNE) || defined(ENABLE_MELEY_LAIR)
		if (!IsPC() && pAttacker && pAttacker->IsPC())
		{
			int32_t racevnum = GetRaceNum();
			LPDUNGEON dungeon = GetDungeon();
			if (dungeon)
			{
#if defined(ENABLE_DS_RUNE)
				if (racevnum == 3996 || racevnum == 3997 || racevnum == 3998 || racevnum == 4011 || racevnum == 4012 || racevnum == 4013)
				{
					int32_t type = dungeon->GetFlag("type");
					int32_t step = dungeon->GetFlag("step");
					if (type == 2)
					{
						if (step == 0)
						{
							int32_t per = (GetMaxHP() / 100) * 60;
							if (GetHP() - dam <= per)
							{
								dungeon->SetFlag("step", 1);
								if (racevnum == 3997) {
									dungeon->SpawnRegen("data/dungeon/rune/regen2_type3a.txt");
								}
								else if (racevnum == 3998) {
									dungeon->SpawnRegen("data/dungeon/rune/regen3_type3a.txt");
								}
								else if (racevnum == 3996) {
									dungeon->SpawnRegen("data/dungeon/rune/regen4_type3a.txt");
								}

								dungeon->Notice(905, "");
								dungeon->Notice(906, "");

								if (GetHP() > per)
								{
									PointChange(POINT_HP, -(GetHP() - per), false);
								}
								else
								{
									PointChange(POINT_HP, (per - GetHP()), false);
								}

								SetInvincible(true);
								return false;
							}
						}
						else if (step == 2)
						{
							int32_t per = (GetMaxHP() / 100) * 20;
							if (GetHP() - dam <= per)
							{
								dungeon->SetFlag("step", 3);
								if (racevnum == 3997) {
									dungeon->SpawnRegen("data/dungeon/rune/regen2_type3b.txt");
								}
								else if (racevnum == 3998) {
									dungeon->SpawnRegen("data/dungeon/rune/regen3_type3b.txt");
								}
								else if (racevnum == 3996) {
									dungeon->SpawnRegen("data/dungeon/rune/regen4_type3b.txt");
								}

								dungeon->Notice(907, "");
								dungeon->Notice(906, "");

								if (GetHP() > per)
								{
									PointChange(POINT_HP, -(GetHP() - per), false);
								}
								else
								{
									PointChange(POINT_HP, (per - GetHP()), false);
								}

								SetAttMul(2.0f);
								SetDamMul(2.0f);
								SetInvincible(true);
								return false;
							}
						}
					}
					else if (type == 3 && step == 0)
					{
						LPPARTY party = pAttacker->GetParty();
						if (party)
						{
							if (party->GetLeaderPID() == pAttacker->GetPlayerID())
							{
								int32_t per = (GetMaxHP() / 100) * 70;
								if (GetHP() - dam <= per)
								{
									dungeon->SetFlag("step", 1);
									dungeon->Notice(908, "");
								}
							}
							else
							{
								return false;
							}
						}
						else
						{
							dungeon->SetFlag("step", 1);
						}
					}
					else if (type == 8)
					{
						if (step == 0)
						{
							int32_t per = (GetMaxHP() / 100) * 50;
							if (GetHP() - dam <= per)
							{
								dungeon->SetFlag("step", 1);
								dungeon->SpawnRegen("data/dungeon/rune/regen8.txt");

								dungeon->Notice(907, "");
								dungeon->Notice(906, "");

								if (GetHP() > per)
								{
									PointChange(POINT_HP, -(GetHP() - per), false);
								}
								else
								{
									PointChange(POINT_HP, (per - GetHP()), false);
								}

								IncreaseMobRigHP(20);
								SetInvincible(true);
								return false;
							}
						}
						else if (step == 2)
						{
							int32_t per = (GetMaxHP() / 100) * 10;
							if (GetHP() - dam <= per)
							{
								dungeon->SetFlag("step", 3);
								dungeon->SpawnRegen("data/dungeon/rune/regen9.txt");

								dungeon->Notice(905, "");
								dungeon->Notice(906, "");

								if (GetHP() > per)
								{
									PointChange(POINT_HP, -(GetHP() - per), false);
								}
								else
								{
									PointChange(POINT_HP, (per - GetHP()), false);
								}

								SetAttMul(2.0f);
								SetDamMul(2.0f);
								SetInvincible(true);
								return false;
							}
						}
					}

					if (itakehp != 0)
					{
						PointChange(POINT_HP, -itakehp);
					}
				}
#endif
#if defined(ENABLE_MELEY_LAIR)
				if (racevnum == 6118)
				{
					int32_t vid = GetPacketVID();
					if (vid == dungeon->GetFlag("statue_vid1") || vid == dungeon->GetFlag("statue_vid2") || vid == dungeon->GetFlag("statue_vid3") || vid == dungeon->GetFlag("statue_vid4"))
					{
						int32_t floor = dungeon->GetFlag("floor");
						if (floor >= 1 && floor < 5)
						{
							int32_t per = (GetMaxHP() / 100) * 75;
							if (GetHP() - dam <= per)
							{
								dungeon->SetFlag("floor", floor + 1);

								if (GetHP() > per)
								{
									PointChange(POINT_HP, -(GetHP() - per), false);
								}
								else
								{
									PointChange(POINT_HP, (per - GetHP()), false);
								}

								SetInvincible(true);

								if (!FindAffect(AFFECT_STATUE))
								{
									AddAffect(AFFECT_STATUE, POINT_NONE, 0, AFF_STATUE1, 3600, 0, true);
								}

								if (floor == 4)
								{
									dungeon->KillAllMonsters();
									dungeon->ClearRegen();
								}

								return false;
							}
						}
						else if (floor >= 7 && floor < 11)
						{
							int32_t per = (GetMaxHP() / 100) * 50;
							if (GetHP() - dam <= per)
							{
								dungeon->SetFlag("floor", floor + 1);

								if (GetHP() > per)
								{
									PointChange(POINT_HP, -(GetHP() - per), false);
								}
								else
								{
									PointChange(POINT_HP, (per - GetHP()), false);
								}

								SetInvincible(true);

								if (!FindAffect(AFFECT_STATUE))
								{
									AddAffect(AFFECT_STATUE, POINT_NONE, 0, AFF_STATUE2, 3600, 0, true);
								}

								if (floor == 10)
								{
									dungeon->KillAllMonsters();
									dungeon->ClearRegen();
								}

								return false;
							}
						}
						else if (floor >= 13 && floor < 17)
						{
							int32_t per = (GetMaxHP() / 100) * 5;
							if (GetHP() - dam <= per)
							{
								dungeon->SetFlag("floor", floor + 1);

								if (GetHP() > per)
								{
									PointChange(POINT_HP, -(GetHP() - per), false);
								}
								else
								{
									PointChange(POINT_HP, (per - GetHP()), false);
								}

								SetInvincible(true);

								if (!FindAffect(AFFECT_STATUE))
								{
									AddAffect(AFFECT_STATUE, POINT_NONE, 0, AFF_STATUE3, 3600, 0, true);
								}

								if (floor == 17)
								{
									dungeon->KillAllMonsters();
								}

								return false;
							}
						}
					}

					if (itakehp != 0)
					{
						PointChange(POINT_HP, -itakehp);
					}
				}
#endif
			}
		}
#endif

		if (pAttacker)
			SendDamagePacket(pAttacker, dam, damageFlag);
#ifdef LEADERBOARD_RAZOR93

		if (pAttacker && pAttacker->IsPC() && pAttacker->IsSkillHit() && IsPC())
		{
			char szVictimEsc[CHARACTER_NAME_MAX_LEN * 2 + 1];
			DBManager::instance().EscapeString(szVictimEsc, sizeof(szVictimEsc), GetName(), strnlen(GetName(), CHARACTER_NAME_MAX_LEN));

			DBManager::instance().DirectQuery(
				"UPDATE player.player "
				"SET "
				"    skill_victim = IF(%d > map1_skillmob, '%s', skill_victim), "
				"    map1_skillmob = GREATEST(map1_skillmob, %d) "
				"WHERE id = %d",
				dam,
				szVictimEsc,
				dam,
				pAttacker->GetPlayerID()
			);
			CheckLeaderboardSkillMobChanges();
			if (GetMapIndex() == 41) {
				CHARACTER_MANAGER::instance().for_each_pc([](LegacyCharHandle ch) {
					ch->SendLeaderboardDataSkillMob(ch);
					});


			}



			ecs::ChatSystem::Send(AIHelpers::EcsOf(pAttacker), CHAT_TYPE_INFO, "Skill damage recorded: %d vs %s", dam, GetName());
		}
#endif
		if (test_server)
		{
			int iTmpPercent = 0; // @fixme136
			if (GetMaxHP() >= 0)
				iTmpPercent = (GetHP() * 100) / GetMaxHP();

			if (pAttacker)
			{
				ecs::ChatSystem::Send(AIHelpers::EcsOf(pAttacker), CHAT_TYPE_INFO, "-> %s, DAM %d HP %d(%d%%) %s%s",
					GetName(),
					dam,
					GetHP(),
					iTmpPercent,
					IsCritical ? "crit " : "",
					IsPenetrate ? "pene " : "",
					IsDeathBlow ? "deathblow " : "");
			}

			ecs::ChatSystem::Send(AIHelpers::EcsOf(this), CHAT_TYPE_PARTY, "<- %s, DAM %d HP %d(%d%%) %s%s",
				pAttacker ? pAttacker->GetName() : nullptr,
				dam,
				GetHP(),
				iTmpPercent,
				IsCritical ? "crit " : "",
				IsPenetrate ? "pene " : "",
				IsDeathBlow ? "deathblow " : "");
		}

#ifdef ENABLE_RANKING
		if (pAttacker->IsPC()) {
			if (IsPC()) {
				switch (type) {
				case DAMAGE_TYPE_NORMAL:
				case DAMAGE_TYPE_NORMAL_RANGE: {
					if (dam > pAttacker->GetRankPoints(3))
						pAttacker->SetRankPoints(3, dam);
				}
											 break;
				case DAMAGE_TYPE_MELEE:
				case DAMAGE_TYPE_RANGE:
				case DAMAGE_TYPE_FIRE:
				case DAMAGE_TYPE_ICE:
				case DAMAGE_TYPE_ELEC:
				case DAMAGE_TYPE_MAGIC: {
					if (dam > pAttacker->GetRankPoints(4))
						pAttacker->SetRankPoints(4, dam);
				}
									  break;
				default:
					break;
				}
			}
			else if (IsMonster()) {
				if (GetMobRank() >= MOB_RANK_BOSS) {
					switch (type) {
					case DAMAGE_TYPE_NORMAL:
					case DAMAGE_TYPE_NORMAL_RANGE: {
						if (dam > pAttacker->GetRankPoints(8))
							pAttacker->SetRankPoints(8, dam);
					}
												 break;
					case DAMAGE_TYPE_MELEE:
					case DAMAGE_TYPE_RANGE:
					case DAMAGE_TYPE_FIRE:
					case DAMAGE_TYPE_ICE:
					case DAMAGE_TYPE_ELEC:
					case DAMAGE_TYPE_MAGIC: {
						if (dam > pAttacker->GetRankPoints(9))
							pAttacker->SetRankPoints(9, dam);
					}
										  break;
					default:
						break;
					}
				}
				else if (!IsStone()) {
					switch (type) {
					case DAMAGE_TYPE_NORMAL:
					case DAMAGE_TYPE_NORMAL_RANGE: {
						if (dam > pAttacker->GetRankPoints(18))
							pAttacker->SetRankPoints(18, dam);
					}
												 break;
					case DAMAGE_TYPE_MELEE:
					case DAMAGE_TYPE_RANGE:
					case DAMAGE_TYPE_FIRE:
					case DAMAGE_TYPE_ICE:
					case DAMAGE_TYPE_ELEC:
					case DAMAGE_TYPE_MAGIC: {
						if (dam > pAttacker->GetRankPoints(19))
							pAttacker->SetRankPoints(19, dam);
					}
										  break;
					default:
						break;
					}
				}
			}
		}
#endif
	}

	//
	// !!!!!!!!!  HP ̴ κ !!!!!!!!!
	//
	if (!cannot_dead)
	{
#ifdef __DUNGEON_INFO_SYSTEM__
		if (!IsPC() && pAttacker && pAttacker->IsPC())
		{
			pAttacker->SetQuestDamage(GetRaceNum(), dam);
			pAttacker->SetQuestNPCID(GetPacketVID());
			quest::CQuestManager::instance().QuestDamage(pAttacker->GetPlayerID(), GetRaceNum());
		}
#endif

		if (GetHP() - dam <= 0) // @fixme137
			dam = GetHP();

		PointChange(POINT_HP, -dam, false);
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
		if (IsStone())
			ProcessStoneSpawnStep(this);
#endif
	}

	//puRest1.Pop();

	//PROF_UNIT puRest2("Rest2");
	if (pAttacker && dam > 0 && IsNPC())
	{
		//PROF_UNIT puRest20("Rest20");
		const entt::entity eAttacker = EntityOf(pAttacker);
		TDamageMap::iterator it = m_map_kDamage.end();
		if (eAttacker != entt::null)
		{
			it = m_map_kDamage.find(eAttacker);

			if (it == m_map_kDamage.end())
			{
				m_map_kDamage.insert(TDamageMap::value_type(eAttacker, TBattleInfo(dam, 0)));
				it = m_map_kDamage.find(eAttacker);
			}
			else
			{
				it->second.iTotalDamage += dam;
			}
		}
		//puRest20.Pop();

		//PROF_UNIT puRest21("Rest21");
#ifdef __DEFENSE_WAVE__
		if (GetRaceNum() != 20434)
		{
			StartRecoveryEvent();
		}
#else
		StartRecoveryEvent();
#endif
		//puRest21.Pop();

		//PROF_UNIT puRest22("Rest22");
		if (it != m_map_kDamage.end())
			UpdateAggrPointEx(pAttacker, type, dam, it->second);
		//puRest22.Pop();
	}
	//puRest2.Pop();

	//PROF_UNIT puRest3("Rest3");

#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
	if (GetHP() <= 0)
	{
		if (pAttacker && !pAttacker->IsNPC())
			m_dwKillerPID = pAttacker->GetPlayerID();
		else
			m_dwKillerPID = 0;

		if (!IsPC())
		{
			Dead(pAttacker, true);
			return true;
		}

		 
		Stun();
	}

#else

	if (GetHP() <= 0)
	{
		Stun();

		if (pAttacker && !pAttacker->IsNPC())
			m_dwKillerPID = pAttacker->GetPlayerID();
		else
			m_dwKillerPID = 0;
	}
#endif
#ifdef __DEFENSE_WAVE__
	if (GetRaceNum() == 20434)
	{
		LPDUNGEON dungeon = GetDungeon();
		if (dungeon)
		{
			dungeon->UpdateMastHP();
			if (dungeon->GetMast()->GetHP() <= 0)
			{
				dungeon->ClearRegen();
				dungeon->KillAll();
				dungeon->Notice(909, "");
				dungeon->Notice(910, "");
				dungeon->ExitAllLobby(2);
			}
		}
	}
#endif

	return false;
}

#ifdef LEADERBOARD_RAZOR93



//void CHARACTER::SendLeaderboardData()
//{
//	if (!GetDesc())
//		return;
//
//	// SQL lekrdezs top 10 jtkosra
//	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
//		"SELECT name, level, r5, r8 FROM player.player ORDER BY r5 DESC LIMIT 10"));
//	
//
//	//if (!pMsg || !pMsg->Get()->uiNumRows)
//	//{
//	//	ecs::ChatSystem::Send(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, "Nincs leaderboard adat.");
//	//	return;
//	//}
//
//	MYSQL_ROW row;
//	MYSQL_RES* res = pMsg->Get()->pSQLResult;
//
//	std::string result;
//
//	while ((row = mysql_fetch_row(res)))
//	{
//		const char* name = row[0] ? row[0] : "Unknown";
//		int level = row[1] ? atoi(row[1]) : 0;
//		int metins = row[2] ? atoi(row[2]) : 0;
//		int dmg = row[3] ? atoi(row[3]) : 0;
//
//		char line[128];
//		snprintf(line, sizeof(line), "%s;%d;%d;%d\n", name, level, metins, dmg);
//		result += line;
//	}
//
//	// Klds kliensnek
//	TPacketGCLeaderboard p;
//	p.header = HEADER_GC_LEADERBOARD_DATA;
//	strlcpy(p.data, result.c_str(), sizeof(p.data));
//
//	GetDesc()->Packet(&p, sizeof(p));
//}


//void CHARACTER::SendLeaderboardNews()
//{
//	if (!GetDesc())
//		return;
//
//	// SQL lekrdezs top 10 jtkosra
//	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
//	
//
//	"SELECT id, title, content,author FROM player.news ORDER BY id DESC LIMIT 5"));
//
//	//if (!pMsg || !pMsg->Get()->uiNumRows)
//	//{
//	//	ecs::ChatSystem::Send(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, "Nincs leaderboard adat.");
//	//	return;
//	//}
//
//	MYSQL_ROW row;
//	MYSQL_RES* res = pMsg->Get()->pSQLResult;
//
//	std::string result;
//
//	while ((row = mysql_fetch_row(res)))
//	{
//		int id = row[0] ? atoi(row[0]) : 0;
//		const char* title = row[1] ? row[1] : "Unknown";
//		const char* content = row[2] ? row[2] : "Unknown";
//		const char* author = row[3] ? row[3] : "Unknown";
//
//		char line[512];
//		snprintf(line, sizeof(line), "%d;%s;%s;%s\n", id, title, content, author);
//		result += line;
//	}
//
//
//	// Klds kliensnek
//	TPacketGCLeaderboardNews p;
//	p.header = HEADER_GC_LEADERBOARD_NEWS;
//	strlcpy(p.data, result.c_str(), sizeof(p.data));
//
//	GetDesc()->Packet(&p, sizeof(p));
//}

#endif

void CHARACTER::UseArrow(LPITEM pkArrow, uint32_t dwArrowCount)
{
	int iCount = pkArrow->GetCount();
	uint32_t dwVnum = pkArrow->GetVnum();
#if !defined(__INFINITE_ARROW__)
	iCount = iCount - MIN(iCount, dwArrowCount);
#endif
	pkArrow->SetCount(iCount);

	if (iCount == 0)
	{
		LPITEM pkNewArrow = FindSpecifyItem(dwVnum);

		sys_log(0, "UseArrow : FindSpecifyItem %u %p", dwVnum, get_pointer(pkNewArrow));

		if (pkNewArrow)
			EquipItem(pkNewArrow);
	}
}

class CFuncShoot
{
public:
	LegacyCharHandle	m_me;
	uint8_t		m_bType;
	bool		m_bSucceed;

	CFuncShoot(LegacyCharHandle ch, uint8_t bType) : m_me(ch), m_bType(bType), m_bSucceed(false)
	{
	}

	void operator () (uint32_t dwTargetVID)
	{
		if (m_bType > 1)
		{
			if (g_bSkillDisable)
				return;

			m_me->m_SkillUseInfo[m_bType].SetMainTargetVID(static_cast<entt::entity>(dwTargetVID));
			/*if (m_bType == SKILL_BIPABU || m_bType == SKILL_KWANKYEOK)
			  m_me->m_SkillUseInfo[m_bType].ResetHitCount();*/
		}

		auto* pkVictim = CHARACTER_MANAGER::instance().Find(dwTargetVID);

		if (!pkVictim)
			return;

		//  Ұ
		if (!battle_is_attackable(m_me, pkVictim))
			return;

		if (m_me->IsNPC())
		{
			if (DISTANCE_APPROX(m_me->GetX() - pkVictim->GetX(), m_me->GetY() - pkVictim->GetY()) > 5000)
				return;
		}

		LPITEM pkBow, pkArrow;

		switch (m_bType)
		{
		case 0: // ϹȰ
		{
			int iDam = 0;

			if (m_me->IsPC())
			{
				if (m_me->GetJob() != JOB_ASSASSIN)
					return;

				if (0 == m_me->GetArrowAndBow(&pkBow, &pkArrow))
					return;

				if (m_me->GetSkillGroup() != 0)
					if (!m_me->IsNPC() && m_me->GetSkillGroup() != 2)
					{
						if (m_me->GetSP() < 5)
							return;

						m_me->PointChange(POINT_SP, -5);
					}

				iDam = CalcArrowDamage(m_me, pkVictim, pkBow, pkArrow);
				m_me->UseArrow(pkArrow, 1);

#ifdef ENABLE_ANTICHEAT
				if (IS_SPEED_HACK(m_me, pkVictim, get_dword_time())) {
					iDam = 0;
				}
#endif
			}
			else
				iDam = CalcMeleeDamage(m_me, pkVictim);

			NormalAttackAffect(m_me, pkVictim);

			//   (nyl vdelem)
			int32_t lValue = pkVictim->GetPoint(POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= m_me->GetPoint(POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= m_me->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif

			if (lValue < 0)   lValue = 0;
			if (lValue > 100) lValue = 100;

			iDam = (int)((int64_t)iDam * (100 - lValue) / 100);
			//iDam = (int)((int64_t)iDam * (100 - lValue) * 20 / 10000);

#ifdef ENABLE_SOUL_SYSTEM // Arrow ninja
			iDam += m_me->GetSoulItemDamage(pkVictim, iDam, RED_SOUL);
#endif

			//sys_log(0, "%s arrow %s dam %d", m_me->GetName(), pkVictim->GetName(), iDam);

			m_me->OnMove(true);
			pkVictim->OnMove();

			if (pkVictim->CanBeginFight())
				pkVictim->BeginFight(m_me);

			pkVictim->Damage(m_me, iDam, DAMAGE_TYPE_NORMAL_RANGE);
			// Ÿġ  
		}
		break;



		case 1: // Ϲ 
		{
			int iDam;

			if (m_me->IsPC())
				return;

			iDam = CalcMagicDamage(m_me, pkVictim);

			NormalAttackAffect(m_me, pkVictim);

			//  
//#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
//						const int resist_magic = MINMAX(0, pkVictim->GetPoint(POINT_RESIST_MAGIC), 100);
//						const int resist_magic_reduction = MINMAX(0, (m_me->GetJob()==JOB_SURA) ? m_me->GetPoint(POINT_RESIST_MAGIC_REDUCTION)/2 : m_me->GetPoint(POINT_RESIST_MAGIC_REDUCTION), 50);
//						const int total_res_magic = MINMAX(0, resist_magic - resist_magic_reduction, 100);
//						iDam = iDam * (100 - total_res_magic) / 100;
//#else
			iDam = iDam * (100 - (int)(pkVictim->GetPoint(POINT_RESIST_MAGIC) / 2)) / 100;
			//#endif

									//sys_log(0, "%s arrow %s dam %d", m_me->GetName(), pkVictim->GetName(), iDam);

			m_me->OnMove(true);
			pkVictim->OnMove();

			if (pkVictim->CanBeginFight())
				pkVictim->BeginFight(m_me);

			pkVictim->Damage(m_me, iDam, DAMAGE_TYPE_MAGIC);
			// Ÿġ  
		}
		break;

		case SKILL_YEONSA:	// 
		{
			//int iUseArrow = 2 + (m_me->GetSkillPower(SKILL_YEONSA) *6/100);
			int iUseArrow = 1;

			// Ż ϴ°
			{
				if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
				{
					m_me->OnMove(true);
					pkVictim->OnMove();

					if (pkVictim->CanBeginFight())
						pkVictim->BeginFight(m_me);

					m_me->ComputeSkill(m_bType, pkVictim);
					m_me->UseArrow(pkArrow, iUseArrow);

					if (pkVictim->IsDead())
						break;

				}
				else
					break;
			}
		}
		break;


		case SKILL_KWANKYEOK:
		{
			int iUseArrow = 1;

			if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
			{
				m_me->OnMove(true);
				pkVictim->OnMove();

				if (pkVictim->CanBeginFight())
					pkVictim->BeginFight(m_me);

				sys_log(0, "%s kwankeyok %s", m_me->GetName(), pkVictim->GetName());
				m_me->ComputeSkill(m_bType, pkVictim);
				m_me->UseArrow(pkArrow, iUseArrow);
			}
		}
		break;

		case SKILL_GIGUNG:
		{
			int iUseArrow = 1;
			if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
			{
				m_me->OnMove(true);
				pkVictim->OnMove();

				if (pkVictim->CanBeginFight())
					pkVictim->BeginFight(m_me);

				sys_log(0, "%s gigung %s", m_me->GetName(), pkVictim->GetName());
				m_me->ComputeSkill(m_bType, pkVictim);
				m_me->UseArrow(pkArrow, iUseArrow);
			}
		}

		break;
		case SKILL_HWAJO:
		{
			int iUseArrow = 1;
			if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
			{
				m_me->OnMove(true);
				pkVictim->OnMove();

				if (pkVictim->CanBeginFight())
					pkVictim->BeginFight(m_me);

				sys_log(0, "%s hwajo %s", m_me->GetName(), pkVictim->GetName());
				m_me->ComputeSkill(m_bType, pkVictim);
				m_me->UseArrow(pkArrow, iUseArrow);
			}
		}

		break;

		case SKILL_HORSE_WILDATTACK_RANGE:
		{
			int iUseArrow = 1;
			if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
			{
				m_me->OnMove(true);
				pkVictim->OnMove();

				if (pkVictim->CanBeginFight())
					pkVictim->BeginFight(m_me);

				sys_log(0, "%s horse_wildattack %s", m_me->GetName(), pkVictim->GetName());
				m_me->ComputeSkill(m_bType, pkVictim);
				m_me->UseArrow(pkArrow, iUseArrow);
			}
		}

		break;

		case SKILL_MARYUNG:
			//case SKILL_GUMHWAN:
		case SKILL_TUSOK:
		case SKILL_BIPABU:
		case SKILL_NOEJEON:
		case SKILL_GEOMPUNG:


		case SKILL_MAHWAN:
		case SKILL_PABEOB:
#ifdef ENABLE_BUG_FIXES
		case SKILL_YONGBI:
#endif
			//case SKILL_CURSE:
		{
			m_me->OnMove(true);
			pkVictim->OnMove();

			if (pkVictim->CanBeginFight())
				pkVictim->BeginFight(m_me);

			sys_log(0, "%s - Skill %d -> %s", m_me->GetName(), m_bType, pkVictim->GetName());
			m_me->ComputeSkill(m_bType, pkVictim);
		}
		break;

		case SKILL_CHAIN:
		{
			m_me->OnMove(true);
			pkVictim->OnMove();

			if (pkVictim->CanBeginFight())
				pkVictim->BeginFight(m_me);

			sys_log(0, "%s - Skill %d -> %s", m_me->GetName(), m_bType, pkVictim->GetName());
			m_me->ComputeSkill(m_bType, pkVictim);

			// TODO     ϱ
		}
		break;
#ifndef ENABLE_BUG_FIXES
		case SKILL_YONGBI:
		{
			m_me->OnMove(true);
		}
		break;
#endif
		/*case SKILL_BUDONG:
		  {
		  m_me->OnMove(true);
		  pkVictim->OnMove();

		  uint32_t * pdw;
		  uint32_t dwEI = AllocEventInfo(sizeof(uint32_t) * 2, &pdw);
		  pdw[0] = m_me->GetPacketVID();
		  pdw[1] = pkVictim->GetPacketVID();

		  event_create(budong_event_func, dwEI, PASSES_PER_SEC(1));
		  }
		  break;*/

		default:
			sys_err("CFuncShoot: I don't know this type [%d] of range attack.", (int)m_bType);
			break;
#ifdef ENABLE_NINJA_SANGONG_X30_RAZOR93
		case SKILL_SANGONG:
		{
			if (pkVictim->IsStone() || pkVictim->GetMobRank() >= 4 || pkVictim->GetRaceNum())
			{
				int iDam = CalcMeleeDamage(m_me, pkVictim);

				if (m_me->GetJob() == JOB_ASSASSIN &&
					(pkVictim->IsStone() || pkVictim->GetMobRank() >= 4 || pkVictim->GetRaceNum() == 136))
				{
					int multiplier = 36; // alap multiplier


					if (pkVictim->GetRaceNum() == 331)
					{
						iDam = 0;
					}
					else
					{

						if (pkVictim->GetRaceNum() == 8055)
						{
							multiplier = 34;
						}
						else if (pkVictim->GetRaceNum() == 6193)
						{
							multiplier = 20;
						}
						else if (pkVictim->GetRaceNum() == 8010 ||
							pkVictim->GetRaceNum() == 8020 ||
							pkVictim->GetRaceNum() == 180 ||
							pkVictim->GetRaceNum() == 181 ||
							pkVictim->GetRaceNum() == 182)
						{
							multiplier = 20;
						}
						else if (pkVictim->GetRaceNum() == 180 ||
							pkVictim->GetRaceNum() == 181 ||
							pkVictim->GetRaceNum() == 182
							)
						{
							multiplier = 100;
						}
						else if (pkVictim->GetRaceNum() == 4582 ||
							pkVictim->GetRaceNum() == 4583 ||
							pkVictim->GetRaceNum() == 4584
							)
						{
							multiplier = 80	;
						}
						iDam *= multiplier;
						
					}
				}

				pkVictim->Damage(m_me, iDam, DAMAGE_TYPE_NORMAL);


				if (pkVictim->IsPC())
				{
					m_me->OnMove(true);
					pkVictim->OnMove();

					if (pkVictim->CanBeginFight())
						pkVictim->BeginFight(m_me);

					sys_log(0, "%s - Skill %d -> %s", m_me->GetName(), m_bType, pkVictim->GetName());
					m_me->ComputeSkill(m_bType, pkVictim);
				}


			}
			break;
		}

#else
		case SKILL_SANGONG:
#endif
		}

		m_bSucceed = true;
	}
};

bool CHARACTER::Shoot(uint8_t bType)
{
	sys_log(1, "Shoot %s type %u flyTargets.size %zu", GetName(), bType, m_vec_dwFlyTargets.size());

	if (!CanMove())
	{
		return false;
	}

	CFuncShoot f(this, bType);

	if (m_dwFlyTargetID != 0)
	{
		f(m_dwFlyTargetID);
		m_dwFlyTargetID = 0;
	}

	f = std::for_each(m_vec_dwFlyTargets.begin(), m_vec_dwFlyTargets.end(), f);
	m_vec_dwFlyTargets.clear();

	return f.m_bSucceed;
}

void CHARACTER::FlyTarget(uint32_t dwTargetVID, int32_t x, int32_t y, uint8_t bHeader)
{
	auto* pkVictim = CHARACTER_MANAGER::instance().Find(dwTargetVID);
	TPacketGCFlyTargeting pack;

	//pack.bHeader	= HEADER_GC_FLY_TARGETING;
	pack.bHeader = (bHeader == HEADER_CG_FLY_TARGETING) ? HEADER_GC_FLY_TARGETING : HEADER_GC_ADD_FLY_TARGETING;
	pack.dwShooterVID = GetPacketVID();

	if (pkVictim)
	{
		pack.dwTargetVID = pkVictim->GetPacketVID();
		pack.x = pkVictim->GetX();
		pack.y = pkVictim->GetY();

		if (bHeader == HEADER_CG_FLY_TARGETING)
			m_dwFlyTargetID = dwTargetVID;
		else
			m_vec_dwFlyTargets.push_back(dwTargetVID);
	}
	else
	{
		pack.dwTargetVID = 0;
		pack.x = x;
		pack.y = y;
	}

	sys_log(1, "FlyTarget %s vid %d x %d y %d", GetName(), pack.dwTargetVID, pack.x, pack.y);
	PacketAround(&pack, sizeof(pack), this);
}

LPCHARACTER CHARACTER::GetNearestVictim(LPCHARACTER pkChr)
{
	if (nullptr == pkChr)
		pkChr = this;

	float fMinDist = 99999.0f;
	auto* pkVictim = static_cast<LegacyCharHandle>(nullptr);

	TDamageMap::iterator it = m_map_kDamage.begin();

	// ϴ    ɷ .
	while (it != m_map_kDamage.end())
	{
		const entt::entity eAttacker = it->first;
		++it;

		auto* pAttacker = LegacyCharOf(eAttacker);

		if (!pAttacker)
			continue;

		if (pAttacker->IsAffectFlag(AFF_EUNHYUNG) ||
			pAttacker->IsAffectFlag(AFF_INVISIBILITY) ||
			pAttacker->IsAffectFlag(AFF_REVIVE_INVISIBLE))
			continue;

		float fDist = DISTANCE_APPROX(pAttacker->GetX() - pkChr->GetX(), pAttacker->GetY() - pkChr->GetY());

		if (fDist < fMinDist)
		{
			pkVictim = pAttacker;
			fMinDist = fDist;
		}
	}

	return pkVictim;
}

void CHARACTER::SetVictim(LPCHARACTER pkVictim)
{
	if (!pkVictim)
	{
		if (m_eVictim != entt::null)
			MonsterLog("  ");

		m_eVictim = entt::null;
		battle_end(this);
	}
	else
	{
		const entt::entity eVictim = AIHelpers::EcsOf(pkVictim);
		if (m_eVictim != eVictim)
			MonsterLog("  : %s", pkVictim->GetName());

		m_eVictim = eVictim;
		m_dwLastVictimSetTime = get_dword_time();
	}
}

LPCHARACTER CHARACTER::GetVictim() const
{
	if (m_eVictim == entt::null)
		return nullptr;

	if (auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(m_eVictim))
		return legacy->ptr;

	return nullptr;
}

LPCHARACTER CHARACTER::GetProtege() const // ȣؾ   
{
	if (m_pkChrStone)
		return m_pkChrStone;

	if (m_pkParty)
		return m_pkParty->GetLeader();

	return nullptr;
}

// char_battle.cpp slice BB1 moved into CombatSystem.cpp

bool CHARACTER::IsStun() const
{
	if (RuntimeFlags(this) && IS_SET(RuntimeFlags(this)->instantFlag, INSTANT_FLAG_STUN))
		return true;

	return false;
}

EVENTFUNC(StunEvent)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("StunEvent> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	// Phase 10: WRITES_STATE - deferred until ECS component covers m_pkStunEvent
	ch->m_pkStunEvent = nullptr;
	const entt::entity e = AIHelpers::EcsOf(ch);
	if (e != entt::null)
		g_dispatcher.trigger(ecs::EvStunBegin { e, 3000u });
	ch->Dead();
	return 0;
}

void CHARACTER::Stun()
{
	if (IsStun())
		return;

	if (IsDead())
		return;

	if (!IsPC() && m_pkParty)
	{
		m_pkParty->SendMessage(this, PM_ATTACKED_BY, 0, 0);
	}

	sys_log(1, "%s: Stun %p", GetName(), this);

	PointChange(POINT_HP_RECOVERY, -GetPoint(POINT_HP_RECOVERY));
	PointChange(POINT_SP_RECOVERY, -GetPoint(POINT_SP_RECOVERY));

	CloseMyShop();

	event_cancel(&m_pkRecoveryEvent); // ȸ ̺Ʈ δ.

	TPacketGCStun pack;
	pack.header = HEADER_GC_STUN;
	pack.vid = GetPacketVID();
	PacketAround(&pack, sizeof(pack));

		if (auto* flags = RuntimeFlags(this))
		SET_BIT(flags->instantFlag, INSTANT_FLAG_STUN);

	if (m_pkStunEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = this;

	m_pkStunEvent = event_create(StunEvent, info, PASSES_PER_SEC(3));
}

bool CHARACTER::IsDead() const
{
	if (GetPosition() == POS_DEAD)
		return true;

	return false;
}

struct FuncSetLastAttacked
{
	FuncSetLastAttacked(uint32_t dwTime) : m_dwTime(dwTime)
	{
	}

	void operator () (LegacyCharHandle ch)
	{
		ch->SetLastAttacked(m_dwTime);
	}

	uint32_t m_dwTime;
};

#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
void CHARACTER::RegisterDamageForExp(LPCHARACTER pkAttacker, int iDamage)
{
	if (!pkAttacker || !pkAttacker->IsPC())
		return;

	if (iDamage <= 0)
		iDamage = 1;

	const entt::entity eAttacker = EntityOf(pkAttacker);
	if (eAttacker == entt::null)
		return;

	TDamageMap::iterator it = m_map_kDamage.find(eAttacker);
	if (it == m_map_kDamage.end())
		m_map_kDamage.insert(TDamageMap::value_type(eAttacker, TBattleInfo(iDamage, 0)));
	else
		it->second.iTotalDamage += iDamage;

	// hogy Dead() vissza tudja keresni a killert, ha kell
	m_dwKillerPID = pkAttacker->GetPlayerID();
}


#endif
void CHARACTER::SetLastAttacked(uint32_t dwTime)
{
	if (!m_pkMobInst)
		return;
	assert(m_pkMobInst != NULL);

	m_pkMobInst->m_dwLastAttackedTime = dwTime;
	m_pkMobInst->m_posLastAttacked = GetXYZ();
}

void CHARACTER::SendDamagePacket(LPCHARACTER pAttacker, int Damage, uint8_t DamageFlag)
{
	if (IsPC() == true || (pAttacker->IsPC() == true && pAttacker->GetTarget() == this))
	{
		TPacketGCDamageInfo damageInfo;
		memset(&damageInfo, 0, sizeof(TPacketGCDamageInfo));

		damageInfo.header = HEADER_GC_DAMAGE_INFO;
		damageInfo.dwVID = GetPacketVID();
		damageInfo.flag = DamageFlag;
		damageInfo.damage = Damage;
#ifdef ENABLE_TARGET_DAMAGE_RAZOR93
		PacketAround(&damageInfo, sizeof(TPacketGCDamageInfo));
		return;
#endif

		if (GetDesc() != nullptr)
		{
			GetDesc()->Packet(&damageInfo, sizeof(TPacketGCDamageInfo));
		}

		if (pAttacker->GetDesc() != nullptr)
		{
			pAttacker->GetDesc()->Packet(&damageInfo, sizeof(TPacketGCDamageInfo));
		}

		if (GetArenaObserverMode() == false && GetArena() != nullptr) {
			GetArena()->SendPacketToObserver(&damageInfo, sizeof(TPacketGCDamageInfo));
		}
	}
}

//
// CHARACTER::Damage ޼ҵ this  ԰ Ѵ.
//
// Arguments
//    pAttacker		: 
//    dam		: 
//    EDamageType	:   ΰ?
//
// Return value
//    true		: dead
//    false		: not dead yet
//

// char_battle.cpp slice BB2b1 map helpers moved into CombatSystem.cpp

#ifdef __ENABLE_BERAN_ADDONS_
bool IsBeranMap(int lMapIndex)
{
	int lMinIndex = 208 * 10000, lMaxIndex = 208 * 10000 + 10000;
	if (((lMapIndex >= lMinIndex) && (lMapIndex < lMaxIndex)) || (lMapIndex == 208))
		return true;

	return false;
}
#endif

#ifdef __ENABLE_SPIDER_ADDONS_
bool IsSpiderMap(int lMapIndex)
{
	int lMinIndex = 217 * 10000, lMaxIndex = 217 * 10000 + 10000;
	if (((lMapIndex >= lMinIndex) && (lMapIndex < lMaxIndex)) || (lMapIndex == 217))
		return true;

	return false;
}
#endif

// char_battle.cpp slice BB2a helper surface moved into CombatSystem.cpp

static int64_t CalcReferenceNormalHitDamage(LegacyCharHandle pAttacker, LegacyCharHandle pVictim);
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
static void ProcessStoneSpawnStep(LegacyCharHandle ch)
{
	if (!ch || !ch->IsStone() || ch->GetMaxHP() <= 0)
		return;

	const int iPercent = (ch->GetHP() * 100) / ch->GetMaxHP();
	const uint32_t dwVnum = number(
		MIN(ch->GetMobTable().sAttackSpeed, ch->GetMobTable().sMovingSpeed),
		MAX(ch->GetMobTable().sAttackSpeed, ch->GetMobTable().sMovingSpeed));

	int wantStep = 0;
	if (iPercent <= 10) wantStep = 10;
	else if (iPercent <= 20) wantStep = 9;
	else if (iPercent <= 30) wantStep = 8;
	else if (iPercent <= 40) wantStep = 7;
	else if (iPercent <= 50) wantStep = 6;
	else if (iPercent <= 60) wantStep = 5;
	else if (iPercent <= 70) wantStep = 4;
	else if (iPercent <= 80) wantStep = 3;
	else if (iPercent <= 90) wantStep = 2;
	else if (iPercent <= 99) wantStep = 1;
	else return;

	for (int step = ch->GetMaxSP() + 1; step <= wantStep; ++step)
	{
		ch->SetMaxSP(step);
		ch->SendMovePacket(FUNC_ATTACK, 0, ch->GetX(), ch->GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(ch);

		if (step == 10 || step == 9)
			CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ch->GetMapIndex(), ch->GetX() - 1500, ch->GetY() - 1500, ch->GetX() + 1500, ch->GetY() + 1500);
		else if (step == 8 || step == 7 || step == 6 || step == 3 || step == 1)
			CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ch->GetMapIndex(), ch->GetX() - 1000, ch->GetY() - 1000, ch->GetX() + 1000, ch->GetY() + 1000);
		else if (step == 5 || step == 4 || step == 2)
			CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ch->GetMapIndex(), ch->GetX() - 500, ch->GetY() - 500, ch->GetX() + 500, ch->GetY() + 500);

		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}

	ch->UpdatePacket();
}
#endif
static int64_t CalcReferenceBowHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim)
{
	if (!pAttacker || !pVictim)
		return 0;

	LPITEM pkBow = nullptr;
	LPITEM pkArrow = nullptr;

	if (0 == pAttacker->GetArrowAndBow(&pkBow, &pkArrow))
		return 0;

	int64_t dam = CalcArrowDamage(pAttacker, pVictim, pkBow, pkArrow);
	if (dam <= 0)
		return 0;

	int32_t lValue = pVictim->GetPoint(POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
	lValue -= pAttacker->GetPoint(POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
	lValue -= pAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif

	if (lValue < 0)
		lValue = 0;
	if (lValue > 100)
		lValue = 100;

	dam = dam * (100 - lValue) / 100;

#ifdef ENABLE_SOUL_SYSTEM
	dam += pAttacker->GetSoulItemDamage(pVictim, dam, RED_SOUL);
#endif

	if (pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS))
		dam = dam * (100 + pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

#ifdef ENABLE_MEDI_PVM
	if (pVictim->IsNPC())
		dam = dam * (100 + pAttacker->GetPoint(POINT_ATTBONUS_MEDI_PVM)) / 100;
#endif

	dam = dam * (100 - std::min((int64_t)99, pVictim->GetPoint(POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;

	return std::max<int64_t>(0, dam);
}

static int64_t CalcReferenceBasicHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim)
{
	if (!pAttacker || !pVictim)
		return 0;

	int64_t dam = 0;

	LPITEM pkWeapon = pAttacker->GetWear(WEAR_WEAPON);
	if (pkWeapon && pkWeapon->GetType() == ITEM_WEAPON && pkWeapon->GetSubType() == WEAPON_BOW)
		dam = CalcReferenceBowHitDamage(pAttacker, pVictim);
	else
		dam = CalcReferenceNormalHitDamage(pAttacker, pVictim);

	if (dam <= 0)
		return 0;

	const int64_t skillBonus = std::max<int64_t>(0, pAttacker->GetPoint(POINT_SKILL_DAMAGE_BONUS));
	if (skillBonus)
		dam = dam * (100 + skillBonus) / 100;

	return dam;
}
static int64_t CalcReferenceNormalHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim)
{
	if (!pAttacker || !pVictim)
		return 0;

	int64_t dam = CalcMeleeDamage(pAttacker, pVictim);
	if (dam <= 0)
		return 0;

	LPITEM pkWeapon = pAttacker->GetWear(WEAR_WEAPON);
	if (pkWeapon)
	{
		int32_t lValue = 0;

		switch (pkWeapon->GetSubType())
		{
		case WEAPON_SWORD:
			lValue = pVictim->GetPoint(POINT_RESIST_SWORD);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= pAttacker->GetPoint(POINT_ATTBONUS_IRR_SPADA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= pAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_TWO_HANDED:
			lValue = pVictim->GetPoint(POINT_RESIST_TWOHAND);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= pAttacker->GetPoint(POINT_ATTBONUS_IRR_SPADONE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= pAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_DAGGER:
#ifdef ENABLE_WOLFMAN_CHARACTER
		case WEAPON_CLAW:
#endif
			lValue = pVictim->GetPoint(POINT_RESIST_DAGGER);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= pAttacker->GetPoint(POINT_ATTBONUS_IRR_PUGNALE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= pAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_BELL:
			lValue = pVictim->GetPoint(POINT_RESIST_BELL);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= pAttacker->GetPoint(POINT_ATTBONUS_IRR_CAMPANA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= pAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_FAN:
			lValue = pVictim->GetPoint(POINT_RESIST_FAN);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= pAttacker->GetPoint(POINT_ATTBONUS_IRR_VENTAGLIO);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= pAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_BOW:
			lValue = pVictim->GetPoint(POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= pAttacker->GetPoint(POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= pAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		default:
			lValue = 0;
			break;
		}

		if (lValue < 0)
			lValue = 0;
		if (lValue > 100)
			lValue = 100;

		dam = dam * (100 - lValue) / 100;
	}

	dam = static_cast<int64_t>(pAttacker->GetAttMul() * static_cast<double>(dam) + 0.5);

#ifdef ENABLE_SOUL_SYSTEM
	dam += pAttacker->GetSoulItemDamage(pVictim, dam, RED_SOUL);
#endif

	if (pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS))
		dam = dam * (100 + pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

#ifdef ENABLE_MEDI_PVM
	if (pVictim->IsNPC())
		dam = dam * (100 + pAttacker->GetPoint(POINT_ATTBONUS_MEDI_PVM)) / 100;
#endif

	dam = dam * (100 - std::min((int64_t)99, pVictim->GetPoint(POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;

	return std::max<int64_t>(0, dam);
}

bool CHARACTER::IsAggressive() const
{
	return IS_SET(GetAIFlag(), AIFLAG_AGGRESSIVE) || AIHelpers::IsAggressive(AIHelpers::EcsOf(const_cast<CHARACTER*>(this)));
}

void CHARACTER::SetAggressive()
{
		if (auto* flags = RuntimeFlags(this))
		SET_BIT(flags->aiFlag, AIFLAG_AGGRESSIVE);
	AIHelpers::SetAggressive(AIHelpers::EcsOf(this), true);
}

bool CHARACTER::IsCoward() const
{
	return IS_SET(GetAIFlag(), AIFLAG_COWARD) || AIHelpers::IsCoward(AIHelpers::EcsOf(const_cast<CHARACTER*>(this)));
}

void CHARACTER::SetCoward()
{
		if (auto* flags = RuntimeFlags(this))
		SET_BIT(flags->aiFlag, AIFLAG_COWARD);
	AIHelpers::SetCoward(AIHelpers::EcsOf(this), true);
}

bool CHARACTER::IsBerserker() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_BERSERK))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(AIHelpers::EcsOf(const_cast<CHARACTER*>(this))))
		return flags->isBerserk;

	return false;
}

bool CHARACTER::IsStoneSkinner() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_STONESKIN))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(AIHelpers::EcsOf(const_cast<CHARACTER*>(this))))
		return flags->isStoneSkinner;

	return false;
}

bool CHARACTER::IsGodSpeeder() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_GODSPEED))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(AIHelpers::EcsOf(const_cast<CHARACTER*>(this))))
		return flags->isGodSpeed;

	return false;
}

bool CHARACTER::IsDeathBlower() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_DEATHBLOW))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(AIHelpers::EcsOf(const_cast<CHARACTER*>(this))))
		return flags->isDeathBlower;

	return false;
}

bool CHARACTER::IsReviver() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_REVIVE))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(AIHelpers::EcsOf(const_cast<CHARACTER*>(this))))
		return flags->isReviver;

	return false;
}

void CHARACTER::SetNoAttackShinsu()
{
		if (auto* flags = RuntimeFlags(this))
		SET_BIT(flags->aiFlag, AIFLAG_NOATTACKSHINSU);
	AIHelpers::SetNoAttackShinsu(AIHelpers::EcsOf(this), true);
}

bool CHARACTER::IsNoAttackShinsu() const
{
	return IS_SET(GetAIFlag(), AIFLAG_NOATTACKSHINSU) || AIHelpers::IsNoAttackShinsu(AIHelpers::EcsOf(const_cast<CHARACTER*>(this)));
}

void CHARACTER::SetNoAttackChunjo()
{
		if (auto* flags = RuntimeFlags(this))
		SET_BIT(flags->aiFlag, AIFLAG_NOATTACKCHUNJO);
	AIHelpers::SetNoAttackChunjo(AIHelpers::EcsOf(this), true);
}

bool CHARACTER::IsNoAttackChunjo() const
{
	return IS_SET(GetAIFlag(), AIFLAG_NOATTACKCHUNJO) || AIHelpers::IsNoAttackChunjo(AIHelpers::EcsOf(const_cast<CHARACTER*>(this)));
}

void CHARACTER::SetNoAttackJinno()
{
		if (auto* flags = RuntimeFlags(this))
		SET_BIT(flags->aiFlag, AIFLAG_NOATTACKJINNO);
	AIHelpers::SetNoAttackJinno(AIHelpers::EcsOf(this), true);
}

bool CHARACTER::IsNoAttackJinno() const
{
	return IS_SET(GetAIFlag(), AIFLAG_NOATTACKJINNO) || AIHelpers::IsNoAttackJinno(AIHelpers::EcsOf(const_cast<CHARACTER*>(this)));
}

void CHARACTER::SetAttackMob()
{
		if (auto* flags = RuntimeFlags(this))
		SET_BIT(flags->aiFlag, AIFLAG_ATTACKMOB);
	AIHelpers::SetAttackMob(AIHelpers::EcsOf(this), true);
}

bool CHARACTER::IsAttackMob() const
{
	return IS_SET(GetAIFlag(), AIFLAG_ATTACKMOB) || AIHelpers::IsAttackMob(AIHelpers::EcsOf(const_cast<CHARACTER*>(this)));
}

int CHARACTER::GetHPPct() const
{
	if (GetMaxHP() <= 0)
		return 0;

	return static_cast<int>((static_cast<int64_t>(GetHP()) * 100) / static_cast<int64_t>(GetMaxHP()));
}

bool CHARACTER::IsBerserk() const
{
	return m_pkMobInst != nullptr ? m_pkMobInst->m_IsBerserk : false;
}

void CHARACTER::SetBerserk(bool mode)
{
	if (m_pkMobInst != nullptr)
		m_pkMobInst->m_IsBerserk = mode;
}

bool CHARACTER::IsGodSpeed() const
{
	return m_pkMobInst != nullptr ? m_pkMobInst->m_IsGodSpeed : false;
}

void CHARACTER::SetGodSpeed(bool mode)
{
	if (m_pkMobInst == nullptr)
		return;

	m_pkMobInst->m_IsGodSpeed = mode;

	if (mode == true)
		SetPoint(POINT_ATT_SPEED, 250);
	else
		SetPoint(POINT_ATT_SPEED, m_pkMobData->m_table.sAttackSpeed);
}

bool CHARACTER::IsDeathBlow() const
{
	return number(1, 100) <= m_pkMobData->m_table.bDeathBlowPoint;
}

bool CHARACTER::IsRevive() const
{
	return m_pkMobInst != nullptr ? m_pkMobInst->m_IsRevive : false;
}

void CHARACTER::SetRevive(bool mode)
{
	if (m_pkMobInst != nullptr)
		m_pkMobInst->m_IsRevive = mode;
}

void CHARACTER::SetComboSequence(uint8_t seq)
{
	m_bComboSequence = seq;
}

uint8_t CHARACTER::GetComboSequence() const
{
	return m_bComboSequence;
}

void CHARACTER::SetLastComboTime(uint32_t time)
{
	m_dwLastComboTime = time;
}

uint32_t CHARACTER::GetLastComboTime() const
{
	return m_dwLastComboTime;
}

void CHARACTER::SetValidComboInterval(int interval)
{
	m_iValidComboInterval = interval;
}

int CHARACTER::GetValidComboInterval() const
{
	return m_iValidComboInterval;
}

uint8_t CHARACTER::GetComboIndex() const
{
	return m_bComboIndex;
}

void CHARACTER::IncreaseComboHackCount(int k)
{
	m_iComboHackCount += k;

	if (m_iComboHackCount >= 10)
	{
		if (GetDesc())
			if (GetDesc()->DelayedDisconnect(number(2, 7)))
			{
				sys_log(0, "COMBO_HACK_DISCONNECT: %s count: %d", GetName(), m_iComboHackCount);
				LogManager::instance().HackLog("Combo", this);
			}
	}
}

void CHARACTER::ResetComboHackCount()
{
	m_iComboHackCount = 0;
}

void CHARACTER::SkipComboAttackByTime(int interval)
{
	m_dwSkipComboAttackByTime = get_dword_time() + interval;
}

uint32_t CHARACTER::GetSkipComboAttackByTime() const
{
	return m_dwSkipComboAttackByTime;
}

void CHARACTER::ResetChatCounter()
{
	m_bChatCounter = 0;
	m_bMountCounter = 0;
}

uint8_t CHARACTER::IncreaseChatCounter()
{
	return ++m_bChatCounter;
}

uint8_t CHARACTER::GetChatCounter() const
{
	return m_bChatCounter;
}

void CHARACTER::SetStone(LPCHARACTER pkChrStone)
{
	m_pkChrStone = pkChrStone;

	if (m_pkChrStone)
	{
		if (!pkChrStone->m_set_pkChrSpawnedBy.contains(this))
			pkChrStone->m_set_pkChrSpawnedBy.insert(this);
	}
}

#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
struct FuncDeadSpawnedByStone
{
	LPCHARACTER m_pkKiller;

	FuncDeadSpawnedByStone(LPCHARACTER pkKiller)
		: m_pkKiller(pkKiller)
	{
	}

	void operator () (LegacyCharHandle ch)
	{
		if (auto* flags = RuntimeFlags(ch))
			SET_BIT(flags->instantFlag, INSTANT_FLAG_NO_REWARD);
		ch->Dead(nullptr);
		ch->SetStone(nullptr);
	}
};
#else
struct FuncDeadSpawnedByStone
{
	void operator () (LegacyCharHandle ch)
	{
		ch->Dead(nullptr);
		ch->SetStone(nullptr);
	}
};
#endif

#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
void CHARACTER::ClearStone(LPCHARACTER pkKiller)
{
	if (!m_set_pkChrSpawnedBy.empty())
	{
		FuncDeadSpawnedByStone f(pkKiller);
		std::for_each(m_set_pkChrSpawnedBy.begin(), m_set_pkChrSpawnedBy.end(), f);
		m_set_pkChrSpawnedBy.clear();
	}

	if (!m_pkChrStone)
		return;

	m_pkChrStone->m_set_pkChrSpawnedBy.erase(this);
	m_pkChrStone = nullptr;
}
#else
void CHARACTER::ClearStone()
{
	if (!m_set_pkChrSpawnedBy.empty())
	{
		FuncDeadSpawnedByStone f;
		std::for_each(m_set_pkChrSpawnedBy.begin(), m_set_pkChrSpawnedBy.end(), f);
		m_set_pkChrSpawnedBy.clear();
	}

	if (!m_pkChrStone)
		return;

	m_pkChrStone->m_set_pkChrSpawnedBy.erase(this);
	m_pkChrStone = nullptr;
}
#endif

void CHARACTER::ClearTarget()
{
	if (m_pkChrTarget)
	{
		m_pkChrTarget->m_set_pkChrTargetedBy.erase(this);
		m_pkChrTarget = nullptr;
	}

	TPacketGCTarget p;

	p.header = HEADER_GC_TARGET;
	p.dwVID = 0;
	p.bHPPercent = 0;
#ifdef __VIEW_TARGET_DECIMAL_HP__
	p.iMinHP = 0;
	p.iMaxHP = 0;
#endif

	CHARACTER_SET::iterator it = m_set_pkChrTargetedBy.begin();

	while (it != m_set_pkChrTargetedBy.end())
	{
		LPCHARACTER pkChr = *(it++);
		pkChr->m_pkChrTarget = nullptr;

		if (!pkChr->GetDesc())
		{
			sys_err("%s %p does not have desc", pkChr->GetName(), get_pointer(pkChr));
			abort();
		}

		pkChr->GetDesc()->Packet(&p, sizeof(TPacketGCTarget));
	}

	m_set_pkChrTargetedBy.clear();
}

void CHARACTER::SetTarget(LPCHARACTER pkChrTarget)
{
	if (m_pkChrTarget == pkChrTarget)
		return;

	if (m_pkChrTarget)
		m_pkChrTarget->m_set_pkChrTargetedBy.erase(this);

	m_pkChrTarget = pkChrTarget;

	TPacketGCTarget p;
	p.header = HEADER_GC_TARGET;

	if (m_pkChrTarget)
	{
		m_pkChrTarget->m_set_pkChrTargetedBy.insert(this);
	p.dwVID = m_pkChrTarget->GetPacketVID();

#ifdef __VIEW_TARGET_PLAYER_HP__
		if ((m_pkChrTarget->GetMaxHP() <= 0))
		{
			p.bHPPercent = 0;
#ifdef __VIEW_TARGET_DECIMAL_HP__
			p.iMinHP = 0;
			p.iMaxHP = 0;
#endif
		}
		else if (m_pkChrTarget->IsPC() && !m_pkChrTarget->IsPolymorphed())
		{
			p.bHPPercent = MINMAX(0, m_pkChrTarget->GetHPPct(), 100);
#ifdef __VIEW_TARGET_DECIMAL_HP__
			p.iMinHP = m_pkChrTarget->GetHP();
			p.iMaxHP = m_pkChrTarget->GetMaxHP();
#endif
		}
#else
		if ((m_pkChrTarget->IsPC() && !m_pkChrTarget->IsPolymorphed()) || (m_pkChrTarget->GetMaxHP() <= 0))
			p.bHPPercent = 0;
#endif
		else
		{
			if (m_pkChrTarget->GetRaceNum() == 20101 ||
				m_pkChrTarget->GetRaceNum() == 20102 ||
				m_pkChrTarget->GetRaceNum() == 20103 ||
				m_pkChrTarget->GetRaceNum() == 20104 ||
				m_pkChrTarget->GetRaceNum() == 20105 ||
				m_pkChrTarget->GetRaceNum() == 20106 ||
				m_pkChrTarget->GetRaceNum() == 20107 ||
				m_pkChrTarget->GetRaceNum() == 20108 ||
				m_pkChrTarget->GetRaceNum() == 20109)
			{
				LPCHARACTER owner = m_pkChrTarget->GetVictim();

				if (owner)
				{
					int iHorseHealth = owner->GetHorseHealth();
					int iHorseMaxHealth = owner->GetHorseMaxHealth();
#ifdef __VIEW_TARGET_DECIMAL_HP__
					if (iHorseMaxHealth)
					{
						p.bHPPercent = MINMAX(0, iHorseHealth * 100 / iHorseMaxHealth, 100);
						p.iMinHP = 100;
						p.iMaxHP = 100;
					}
					else
					{
						p.bHPPercent = 100;
						p.iMinHP = 100;
						p.iMaxHP = 100;
					}
				}
				else
				{
					p.bHPPercent = 100;
					p.iMinHP = 100;
					p.iMaxHP = 100;
				}
			}
			else
			{
				if (m_pkChrTarget->GetMaxHP() <= 0)
				{
					p.bHPPercent = 0;
					p.iMinHP = 0;
					p.iMaxHP = 0;
				}
				else
				{
					p.bHPPercent = std::min((m_pkChrTarget->GetHP() * 100) / m_pkChrTarget->GetMaxHP(), (int64_t)100);
					p.iMinHP = m_pkChrTarget->GetHP();
					p.iMaxHP = m_pkChrTarget->GetMaxHP();
				}
			}
		}
	}
	else
	{
		p.dwVID = 0;
		p.bHPPercent = 0;
		p.iMinHP = 0;
		p.iMaxHP = 0;
	}
#else
					if (iHorseMaxHealth)
						p.bHPPercent = MINMAX(0, iHorseHealth * 100 / iHorseMaxHealth, 100);

					else
						p.bHPPercent = 100;
}
				else
					p.bHPPercent = 100;
			}
			else
			{
				if (m_pkChrTarget->GetMaxHP() <= 0)
					p.bHPPercent = 0;
				else
					p.bHPPercent = MINMAX(0, (m_pkChrTarget->GetHP() * 100) / m_pkChrTarget->GetMaxHP(), 100);
			}
		}
	}
	else
	{
		p.dwVID = 0;
		p.bHPPercent = 0;
	}
#endif
#ifdef ELEMENT_TARGET
	p.bElement = 0;
	if (m_pkChrTarget) {
		if (m_pkChrTarget->IsPC()) {
			LPITEM item = m_pkChrTarget->GetWear(WEAR_PENDANT);
			if (item) {
				uint32_t vnum = item->GetVnum();
				if (vnum >= 10750 && vnum <= 10950) {
					p.bElement = 1;
				}
				else if (vnum >= 9600 && vnum <= 9800) {
					p.bElement = 2;
				}
				else if (vnum >= 9830 && vnum <= 10030) {
					p.bElement = 3;
				}
				else if (vnum >= 10520 && vnum <= 10720) {
					p.bElement = 4;
				}
				else if (vnum >= 10060 && vnum <= 10260) {
					p.bElement = 5;
				}
				else if (vnum >= 10290 && vnum <= 10490) {
					p.bElement = 6;
				}
			}
		}
		else if (m_pkChrTarget->IsMonster() || m_pkChrTarget->IsStone()) {
			if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_ELEC)) {
				p.bElement = 1;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_FIRE)) {
				p.bElement = 2;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_ICE)) {
				p.bElement = 3;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_WIND)) {
				p.bElement = 4;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_EARTH)) {
				p.bElement = 5;
			}
			else if (m_pkChrTarget->IsRaceFlag(RACE_FLAG_ATT_DARK)) {
				p.bElement = 6;
			}
		}
	}
#endif
	GetDesc()->Packet(&p, sizeof(TPacketGCTarget));
}

void CHARACTER::BroadcastTargetPacket()
{
	if (m_set_pkChrTargetedBy.empty())
		return;

	TPacketGCTarget p;

	p.header = HEADER_GC_TARGET;
	p.dwVID = GetPacketVID();

#ifdef __VIEW_TARGET_DECIMAL_HP__
	if (GetMaxHP() <= 0)
	{
		p.bHPPercent = 0;
		p.iMinHP = 0;
		p.iMaxHP = 0;
	}
	else
	{
		p.bHPPercent = std::min((GetHP() * 100) / GetMaxHP(), (int64_t)100);
		p.iMinHP = GetHP();
		p.iMaxHP = GetMaxHP();
	}
#else
	if (IsPC())
		p.bHPPercent = 0;
	else if (GetMaxHP() <= 0)
		p.bHPPercent = 0;
	else
		p.bHPPercent = MINMAX(0, GetHPPct(), 100);
#endif

	CHARACTER_SET::iterator it = m_set_pkChrTargetedBy.begin();

	while (it != m_set_pkChrTargetedBy.end())
	{
		LPCHARACTER pkChr = *it++;

		if (!pkChr->GetDesc())
		{
			sys_err("%s %p does not have desc", pkChr->GetName(), get_pointer(pkChr));
			abort();
		}

		pkChr->GetDesc()->Packet(&p, sizeof(TPacketGCTarget));
	}
}

void CHARACTER::CheckTarget()
{
	if (!m_pkChrTarget)
		return;

	if (DISTANCE_APPROX(GetX() - m_pkChrTarget->GetX(), GetY() - m_pkChrTarget->GetY()) >= 4800)
		SetTarget(nullptr);
}

bool CHARACTER::IsChangeAttackPosition(LPCHARACTER target) const
{
	if (!IsNPC())
		return true;

	uint32_t dwChangeTime = AI_CHANGE_ATTACK_POISITION_TIME_NEAR;

	if (DISTANCE_APPROX(GetX() - target->GetX(), GetY() - target->GetY()) >
		AI_CHANGE_ATTACK_POISITION_DISTANCE + GetMobAttackRange())
		dwChangeTime = AI_CHANGE_ATTACK_POISITION_TIME_FAR;

	return get_dword_time() - m_dwLastChangeAttackPositionTime > dwChangeTime;
}

int CHARACTER::GetLeadershipSkillLevel() const
{
	return GetSkillLevel(SKILL_LEADERSHIP);
}

void CHARACTER::ReviveInvisible(int iDur)
{
	AddAffect(AFFECT_REVIVE_INVISIBLE, POINT_NONE, 0, AFF_REVIVE_INVISIBLE, iDur, 0, true);
}

void CHARACTER::CowardEscape()
{
	int iDist[4] = {500, 1000, 3000, 5000};

	for (int iDistIdx = 2; iDistIdx >= 0; --iDistIdx)
		for (int iTryCount = 0; iTryCount < 8; ++iTryCount)
		{
			SetRotation(number(0, 359));

			float fx, fy;
			float fDist = number(iDist[iDistIdx], iDist[iDistIdx + 1]);

			GetDeltaByDegree(GetRotation(), fDist, &fx, &fy);

			bool bIsWayBlocked = false;
			for (int j = 1; j <= 100; ++j)
			{
				if (!ecs::IsMovablePosition(GetMapIndex(), GetX() + (int)fx * j / 100, GetY() + (int)fy * j / 100))
				{
					bIsWayBlocked = true;
					break;
				}
			}

			if (bIsWayBlocked)
				continue;

			m_dwStateDuration = PASSES_PER_SEC(1);

			int iDestX = GetX() + (int)fx;
			int iDestY = GetY() + (int)fy;

			if (Goto(iDestX, iDestY))
				SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

			sys_log(0, "WAEGU move to %d %d (far)", iDestX, iDestY);
			return;
		}
}

void CHARACTER::DetermineDropMetinStone()
{
#ifdef ENABLE_NEWSTUFF
	if (g_NoDropMetinStone)
	{
		m_dwDropMetinStone = 0;
		return;
	}
#endif

	static const uint32_t c_adwMetin[] =
	{
#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_WOLFMAN_STONES)
		28012,
#endif
		28030,
		28031,
		28032,
		28033,
		28034,
		28035,
		28036,
		28037,
		28038,
		28039,
		28040,
		28041,
		28042,
		28043,
#if defined(ENABLE_MAGIC_REDUCTION_SYSTEM) && defined(USE_MAGIC_REDUCTION_STONES)
		28044,
		28045,
#endif
	};
	uint32_t stone_num = GetRaceNum();
	int idx = std::lower_bound(aStoneDrop, aStoneDrop + STONE_INFO_MAX_NUM, stone_num) - aStoneDrop;
	if (idx >= STONE_INFO_MAX_NUM || aStoneDrop[idx].dwMobVnum != stone_num)
	{
		m_dwDropMetinStone = 0;
	}
	else
	{
		const SStoneDropInfo& info = aStoneDrop[idx];
		m_bDropMetinStonePct = info.iDropPct;
		{
			m_dwDropMetinStone = c_adwMetin[number(0, sizeof(c_adwMetin) / sizeof(uint32_t) - 1)];
			int iGradePct = number(1, 100);
			for (int iStoneLevel = 0; iStoneLevel < STONE_LEVEL_MAX_NUM; iStoneLevel++)
			{
				int iLevelGradePortion = info.iLevelPct[iStoneLevel];
				if (iGradePct <= iLevelGradePortion)
				{
					break;
				}
				else
				{
					iGradePct -= iLevelGradePortion;
					m_dwDropMetinStone += 100;
				}
			}
		}
	}
}

bool CHARACTER::CanSummon(int iLeaderShip)
{
	return ((iLeaderShip >= 20) || ((iLeaderShip >= 12) && ((m_dwLastDeadTime + 180) > get_dword_time())));
}

bool CHARACTER::Return()
{
	if (!IsNPC())
		return false;

	int x, y;
	SetVictim(nullptr);

	x = m_pkMobInst->m_posLastAttacked.x;
	y = m_pkMobInst->m_posLastAttacked.y;

	SetRotationToXY(x, y);

	if (!Goto(x, y))
		return false;

	SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

	if (test_server)
		sys_log(0, "%s %p A÷±âÇI°í µ13A°!AÚ! %d %d", GetName(), this, x, y);

	if (GetParty())
		GetParty()->SendMessage(this, PM_RETURN, x, y);

	return true;
}

bool CHARACTER::Follow(LPCHARACTER pkChr, float fMinDistance)
{
	if (IsPC())
	{
		sys_err("CHARACTER::Follow : PC cannot use this method", GetName());
		return false;
	}

	if (IS_SET(GetAIFlag(), AIFLAG_NOMOVE))
	{
		if (pkChr->IsPC())
		{
			if (!GetParty() || !GetParty()->GetLeader() || GetParty()->GetLeader() == this)
			{
				if (get_dword_time() - m_pkMobInst->m_dwLastAttackedTime >= 15000)
				{
					if (m_pkMobData->m_table.wAttackRange < DISTANCE_APPROX(pkChr->GetX() - GetX(), pkChr->GetY() - GetY()))
						if (Return())
							return true;
				}
			}
		}
		return false;
	}

	int32_t x = pkChr->GetX();
	int32_t y = pkChr->GetY();

	if (pkChr->IsPC())
	{
		if (!GetParty() || !GetParty()->GetLeader() || GetParty()->GetLeader() == this)
		{
			if (get_dword_time() - m_pkMobInst->m_dwLastAttackedTime >= 15000)
			{
				if (5000 < DISTANCE_APPROX(m_pkMobInst->m_posLastAttacked.x - GetX(), m_pkMobInst->m_posLastAttacked.y - GetY()))
					if (Return())
						return true;
			}
		}
	}

#ifndef ENABLE_BUG_FIXES
	if (IsGuardNPC())
	{
		if (5000 < DISTANCE_APPROX(m_pkMobInst->m_posLastAttacked.x - GetX(), m_pkMobInst->m_posLastAttacked.y - GetY()))
			if (Return())
				return true;
	}
#endif

#ifdef __NEWPET_SYSTEM__
	if (HasMoveState(pkChr) &&
		GetMobBattleType() != BATTLE_TYPE_RANGE &&
		GetMobBattleType() != BATTLE_TYPE_MAGIC &&
		false == IsPet() && false == IsNewPet()
#else
	if (HasMoveState(pkChr) &&
		GetMobBattleType() != BATTLE_TYPE_RANGE &&
		GetMobBattleType() != BATTLE_TYPE_MAGIC &&
		false == IsPet()
#endif
		)
	{
		float rot = pkChr->GetRotation();
		float rot_delta = GetDegreeDelta(rot, GetDegreeFromPositionXY(GetX(), GetY(), pkChr->GetX(), pkChr->GetY()));

		float yourSpeed = pkChr->GetMoveSpeed();
		float mySpeed = GetMoveSpeed();

		float fDist = DISTANCE_SQRT(x - GetX(), y - GetY());
		float fFollowSpeed = mySpeed - yourSpeed * cos(rot_delta * M_PI / 180);

		if (fFollowSpeed >= 0.1f)
		{
			float fMeetTime = fDist / fFollowSpeed;
			float fYourMoveEstimateX, fYourMoveEstimateY;

			if (fMeetTime * yourSpeed <= 100000.0f)
			{
				GetDeltaByDegree(pkChr->GetRotation(), fMeetTime * yourSpeed, &fYourMoveEstimateX, &fYourMoveEstimateY);

				x += (int32_t)fYourMoveEstimateX;
				y += (int32_t)fYourMoveEstimateY;

				float fDistNew = sqrt(((double)x - GetX()) * (x - GetX()) + ((double)y - GetY()) * (y - GetY()));
				if (fDist < fDistNew)
				{
					x = (int32_t)(GetX() + (x - GetX()) * fDist / fDistNew);
					y = (int32_t)(GetY() + (y - GetY()) * fDist / fDistNew);
				}
			}
		}
	}

	SetRotationToXY(x, y);

	float fDist = DISTANCE_SQRT(x - GetX(), y - GetY());

	if (fDist <= fMinDistance)
		return false;

	float fx, fy;

	if (IsChangeAttackPosition(pkChr) && GetMobRank() < MOB_RANK_BOSS)
	{
		SetChangeAttackPositionTime();

		int retry = 16;
		int dx, dy;
		int rot = (int)GetDegreeFromPositionXY(x, y, GetX(), GetY());

		while (--retry)
		{
			if (fDist < 500.0f)
				GetDeltaByDegree((rot + number(-90, 90) + number(-90, 90)) % 360, fMinDistance, &fx, &fy);
			else
				GetDeltaByDegree(number(0, 359), fMinDistance, &fx, &fy);

			dx = x + (int)fx;
			dy = y + (int)fy;

			LPSECTREE tree = ecs::SectorAt(GetMapIndex(), dx, dy);

			if (nullptr == tree)
				break;

			if (0 == (tree->GetAttribute(dx, dy) & (ATTR_BLOCK | ATTR_OBJECT)))
				break;
		}

		if (!Goto(dx, dy))
			return false;
	}
	else
	{
		float fDistToGo = fDist - fMinDistance;
		GetDeltaByDegree(GetRotation(), fDistToGo, &fx, &fy);

		if (!Goto(GetX() + (int)fx, GetY() + (int)fy))
			return false;
	}

	SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	return true;
}



