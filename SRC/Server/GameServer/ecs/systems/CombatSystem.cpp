#include "../../stdafx.h"

#include "CombatSystem.hpp"

#include <algorithm>
#include <boost/algorithm/string/find.hpp>
#include <random>
#include <thread>

#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/status_components.hpp"
#include "../components/vital_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include "../Registry.hpp"
#include "../VIDRegistry.hpp"
#include "../../utils.h"
#include "../../config.h"
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

static inline LPCHARACTER LegacyCharOf(entt::entity e)
{
    auto* vid = g_registry.try_get<ecs::VIDComponent>(e);
    if (!vid) {
        return nullptr;
    }

    return CHARACTER_MANAGER::instance().Find(vid->value);
}

static inline entt::entity EntityOf(LPCHARACTER ch)
{
    if (!ch) {
        return entt::null;
    }

    return CVIDRegistry::Instance().Find(ch->GetVID());
}

namespace CombatSystem {

bool CanBeginFight(entt::entity e)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        return ch->CanBeginFight();
    }

    return false;
}

void BeginFight(entt::entity attacker, entt::entity victim)
{
    if (LPCHARACTER ch = LegacyCharOf(attacker)) {
        ch->BeginFight(LegacyCharOf(victim));
    }
}

bool CanFight(entt::entity e)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        return ch->CanFight();
    }

    return false;
}

bool Attack(entt::entity attacker, entt::entity victim, uint8_t attackType)
{
    if (LPCHARACTER ch = LegacyCharOf(attacker)) {
        return ch->Attack(LegacyCharOf(victim), attackType);
    }

    return false;
}

bool Shoot(entt::entity attacker, uint8_t attackType)
{
    if (LPCHARACTER ch = LegacyCharOf(attacker)) {
        return ch->Shoot(attackType);
    }

    return false;
}

void SetVictim(entt::entity attacker, entt::entity victim)
{
    if (LPCHARACTER ch = LegacyCharOf(attacker)) {
        ch->SetVictim(LegacyCharOf(victim));
    }
}

entt::entity GetVictim(entt::entity attacker)
{
    if (LPCHARACTER ch = LegacyCharOf(attacker)) {
        return EntityOf(ch->GetVictim());
    }

    return entt::null;
}

entt::entity GetNearestVictim(entt::entity attacker, entt::entity from)
{
    if (LPCHARACTER ch = LegacyCharOf(attacker)) {
        return EntityOf(ch->GetNearestVictim(LegacyCharOf(from)));
    }

    return entt::null;
}

bool IsStun(entt::entity e)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        return ch->IsStun();
    }

    return false;
}

void Stun(entt::entity e)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        ch->Stun();
    }
}

bool IsDead(entt::entity e)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        return ch->IsDead();
    }

    return true;
}

void SetLastAttacked(entt::entity e, uint32_t tick)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        ch->SetLastAttacked(tick);
    }
}

} // namespace CombatSystem

void CombatSystem_Update(entt::registry& reg, uint32_t tick)
{
    // migrated from CHARACTER::Attack
    auto view = reg.view<ecs::CombatActiveTag, ecs::CombatTarget, ecs::CombatStats, ecs::AttackCooldown, ecs::Health>();

    view.each([&](const entt::entity entity,
                  ecs::CombatTarget& combatTarget,
                  ecs::CombatStats& combatStats,
                  ecs::AttackCooldown& attackCooldown,
                  ecs::Health& attackerHealth) {
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

	return m_pointsInstant.position == POS_STANDING && !IsDead() && !IsStun();
}

void CHARACTER::BeginFight(LPCHARACTER pkVictim)
{
	SetVictim(pkVictim);
	SetPosition(POS_FIGHTING);
	SetNextStatePulse(1);
}

bool CHARACTER::CanFight() const
{
	return m_pointsInstant.position >= POS_FIGHTING ? true : false;
}

void CHARACTER::CreateFly(uint8_t bType, LPCHARACTER pkVictim)
{
	TPacketGCCreateFly packFly;

	packFly.bHeader = HEADER_GC_CREATE_FLY;
	packFly.bType = bType;
	packFly.dwStartVID = GetVID();
	packFly.dwEndVID = pkVictim->GetVID();

	PacketAround(&packFly, sizeof(TPacketGCCreateFly));
}

bool CHARACTER::Attack(LPCHARACTER pkVictim, uint8_t bType)
{
#ifdef ENABLE_BUG_FIXES
	if (pkVictim->GetMyShop())
		return false;
#endif

	if (test_server)
		sys_log(0, "[TEST_SERVER] Attack : %s type %d, MobBattleType %d", GetName(), bType, !GetMobBattleType() ? 0 : GetMobAttackRange());
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
			FlyTarget(pkVictim->GetVID(), pkVictim->GetX(), pkVictim->GetY(), HEADER_CG_FLY_TARGETING);
			iRet = Shoot(0) ? BATTLE_DAMAGE : BATTLE_NONE;
			break;
		case BATTLE_TYPE_MAGIC:
			FlyTarget(pkVictim->GetVID(), pkVictim->GetX(), pkVictim->GetY(), HEADER_CG_FLY_TARGETING);
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
//	//	ChatPacket(CHAT_TYPE_INFO, "Nincs leaderboard adat.");
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
//	//	ChatPacket(CHAT_TYPE_INFO, "Nincs leaderboard adat.");
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
	LPCHARACTER	m_me;
	uint8_t		m_bType;
	bool		m_bSucceed;

	CFuncShoot(LPCHARACTER ch, uint8_t bType) : m_me(ch), m_bType(bType), m_bSucceed(false)
	{
	}

	void operator () (uint32_t dwTargetVID)
	{
		if (m_bType > 1)
		{
			if (g_bSkillDisable)
				return;

			m_me->m_SkillUseInfo[m_bType].SetMainTargetVID(dwTargetVID);
			/*if (m_bType == SKILL_BIPABU || m_bType == SKILL_KWANKYEOK)
			  m_me->m_SkillUseInfo[m_bType].ResetHitCount();*/
		}

		LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(dwTargetVID);

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
		  pdw[0] = m_me->GetVID();
		  pdw[1] = pkVictim->GetVID();

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
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(dwTargetVID);
	TPacketGCFlyTargeting pack;

	//pack.bHeader	= HEADER_GC_FLY_TARGETING;
	pack.bHeader = (bHeader == HEADER_CG_FLY_TARGETING) ? HEADER_GC_FLY_TARGETING : HEADER_GC_ADD_FLY_TARGETING;
	pack.dwShooterVID = GetVID();

	if (pkVictim)
	{
		pack.dwTargetVID = pkVictim->GetVID();
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
	LPCHARACTER pkVictim = nullptr;

	TDamageMap::iterator it = m_map_kDamage.begin();

	// ϴ    ɷ .
	while (it != m_map_kDamage.end())
	{
		const VID& c_VID = it->first;
		++it;

		LPCHARACTER pAttacker = CHARACTER_MANAGER::instance().Find(c_VID);

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
		if (0 != (uint32_t)m_kVIDVictim)
			MonsterLog("  ");

		m_kVIDVictim.Reset();
		battle_end(this);
	}
	else
	{
		if (m_kVIDVictim != pkVictim->GetVID())
			MonsterLog("  : %s", pkVictim->GetName());

		m_kVIDVictim = pkVictim->GetVID();
		m_dwLastVictimSetTime = get_dword_time();
	}
}

LPCHARACTER CHARACTER::GetVictim() const
{
	return CHARACTER_MANAGER::instance().Find(m_kVIDVictim);
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
	if (IS_SET(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN))
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
	ch->m_pkStunEvent = nullptr;
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
	pack.vid = m_vid;
	PacketAround(&pack, sizeof(pack));

	SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN);

	if (m_pkStunEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = this;

	m_pkStunEvent = event_create(StunEvent, info, PASSES_PER_SEC(3));
}

bool CHARACTER::IsDead() const
{
	if (m_pointsInstant.position == POS_DEAD)
		return true;

	return false;
}

struct FuncSetLastAttacked
{
	FuncSetLastAttacked(uint32_t dwTime) : m_dwTime(dwTime)
	{
	}

	void operator () (LPCHARACTER ch)
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

	const VID vid = pkAttacker->GetVID();

	TDamageMap::iterator it = m_map_kDamage.find(vid);
	if (it == m_map_kDamage.end())
		m_map_kDamage.insert(TDamageMap::value_type(vid, TBattleInfo(iDamage, 0)));
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
		damageInfo.dwVID = (uint32_t)GetVID();
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

// char_battle.cpp slice BB2a helper surface moved into CombatSystem.cpp

static int64_t CalcReferenceNormalHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim);
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
static void ProcessStoneSpawnStep(LPCHARACTER ch)
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

