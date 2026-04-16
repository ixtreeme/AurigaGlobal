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


void DeathPenalty(entt::entity e, uint8_t bTown)
{
    if (LPCHARACTER ch = LegacyCharOf(e)) {
        ch->DeathPenalty(bTown);
    }
}


void RewardGold(entt::entity victim, entt::entity attacker)
{
    if (LPCHARACTER ch = LegacyCharOf(victim)) {
        ch->RewardGold(LegacyCharOf(attacker));
    }
}


void Reward(entt::entity victim, bool bItemDrop)
{
    if (LPCHARACTER ch = LegacyCharOf(victim)) {
        ch->Reward(bItemDrop);
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
		ChatPacketNew(CHAT_TYPE_INFO, 412, "");
#endif
		return;
	}

	if (number(0, 2) == 1) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 412, "");
#endif
		return;
	}

	if (IS_SET(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY))
	{
		REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

		// NO_DEATH_PENALTY_BUG_FIX
		if (!bTown) //   ڸ Ȱø  ȣ Ѵ. ( ͽô ġ гƼ )
		{
			if (FindAffect(AFFECT_NO_DEATH_PENALTY))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 384, "");
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


// char_battle.cpp slice BC3a helper surface duplicated into CombatSystem.cpp

#ifdef ENABLE_DROP_INSTANT_INVENTORY
static void __UpdateBattlePassCollectProgress(LPCHARACTER ch, uint32_t dwItemVnum, uint32_t dwCount)
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

static bool __TryAutoGiveRewardItem(LPCHARACTER ch, LPITEM item, uint32_t& dwGivenCount)
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
					ch->ChatPacketNew(
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
					ch->ChatPacketNew(
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
		ch->ChatPacketNew(
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

static void __GiveRewardItemToCharacterOrDrop(LPCHARACTER ch, LPCHARACTER pkVictim, LPITEM item, const PIXEL_POSITION& pos, bool bTrackBattlePass)
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
static std::string MakeItemLink(LPITEM pkItem, LPCHARACTER pkKiller, LPCHARACTER pkMob)
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
	LPCHARACTER pkAttacker = DistributeExp();

	if (!pkAttacker)
		return;


	if (!IsPC() && !m_pkMobData)
	{
		sys_err("Reward: NULL mob data (vid=%u race=%u name=%s map=%ld x=%ld y=%ld attacker=%s)",
			(uint32_t)GetVID(),
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

		pkAttacker->SetQuestNPCID(GetVID());
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

	if (!SECTREE_MANAGER::instance().GetMovablePosition(GetMapIndex(), pos.x, pos.y, pos))
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
						auto MakeHwidHostKey = [&](LPCHARACTER ch) -> std::string
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
						std::unordered_map<std::string, LPCHARACTER> mapWinnerByKey;
						mapWinnerByKey.reserve(16);

						pDungeon->ForEachMember([&](LPCHARACTER mch)
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

								auto itNew = m_map_kDamage.find(mch->GetVID());
								if (itNew != m_map_kDamage.end())
									dmgNew = itNew->second.iTotalDamage;

								auto itOld = m_map_kDamage.find(it->second->GetVID());
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
								LPCHARACTER rch = kv.second;
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

				std::priority_queue<std::pair<uint64_t, LPCHARACTER> > pq;

				uint64_t total_dam = 0;

				for (TDamageMap::iterator it = m_map_kDamage.begin(); it != m_map_kDamage.end(); ++it)
				{
					uint64_t iDamage = it->second.iTotalDamage;
					if (iDamage > 0)
					{
						LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(it->first);

						if (ch)
						{
							pq.push(std::make_pair(iDamage, ch));
							total_dam += iDamage;
						}
					}
				}

				std::vector<LPCHARACTER> v;

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
					std::vector<LPCHARACTER>::iterator it = v.begin();

					while (iItemIdx >= 0)
					{
						item = s_vec_item[iItemIdx--];

						if (!item)
						{
							sys_err("item null in vector idx %d", iItemIdx + 1);
							continue;
						}

						LPCHARACTER ch = *it;

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

				std::priority_queue<std::pair<uint64_t, LPCHARACTER> > pq;

				uint64_t total_dam = 0;

				for (TDamageMap::iterator it = m_map_kDamage.begin(); it != m_map_kDamage.end(); ++it)
				{
					uint64_t iDamage = it->second.iTotalDamage;
					if (iDamage > 0)
					{
						LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(it->first);

						if (ch)
						{
							pq.push(std::make_pair(iDamage, ch));
							total_dam += iDamage;
						}
					}
				}

				std::vector<LPCHARACTER> v;

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
					std::vector<LPCHARACTER>::iterator it = v.begin();

					while (iItemIdx >= 0)
					{
						item = s_vec_item[iItemIdx--];

						if (!item)
						{
							sys_err("item null in vector idx %d", iItemIdx + 1);
							continue;
						}

						item->AddToGround(GetMapIndex(), pos);

						LPCHARACTER ch = *it;

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
			(uint32_t)GetVID(),
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
				if (!SECTREE_MANAGER::instance().GetMovablePosition(GetMapIndex(), GetX(), GetY(), pos))
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
				pkAttacker->ChatPacket(CHAT_TYPE_PARTY, "gold_mul %d rate %d", iGoldMultipler, CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker));

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
						if (isAutoLoot)
						{
							pkAttacker->GiveGold(iGold / iSplitCount);
						}
						else if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold / iSplitCount)))
						{
#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93

							pkAttacker->GiveGold(iGold);
							iTotalGold += iGold;
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
static void ProcessStoneSpawnStep(LPCHARACTER ch);
#endif
static int64_t CalcReferenceBowHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim);
static int64_t CalcReferenceBasicHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim);
static int64_t CalcReferenceNormalHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim);

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
		auto it = m_map_kDamage.find(pAttacker->GetVID());
		if (it == m_map_kDamage.end())
		{
			m_map_kDamage.insert(std::make_pair(
				pAttacker->GetVID(),
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
						ChatPacketNew(CHAT_TYPE_INFO, 257, "%d", GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100);
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
					pAttacker->ChatPacketNew(CHAT_TYPE_INFO, 95, "%s#%d", GetName(), GetPoint(POINT_BLOCK));
					ChatPacketNew(CHAT_TYPE_INFO, 95, "%s#%d", pAttacker->GetName(), pAttacker->GetPoint(POINT_BLOCK));
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
					pAttacker->ChatPacketNew(CHAT_TYPE_INFO, 96, "%s#%d", GetName(), GetPoint(POINT_DODGE));
					ChatPacketNew(CHAT_TYPE_INFO, 96, "%s#%d", pAttacker->GetName(), pAttacker->GetPoint(POINT_DODGE));
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
			int iMapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex(lMapIndex);

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
			SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_NO_REWARD);
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
					int32_t vid = GetVID();
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
				CHARACTER_MANAGER::instance().for_each_pc([](LPCHARACTER ch) {
					ch->SendLeaderboardDataSkillMob(ch);
					});


			}



			pAttacker->ChatPacket(CHAT_TYPE_INFO, "Skill damage recorded: %d vs %s", dam, GetName());
		}
#endif
		if (test_server)
		{
			int iTmpPercent = 0; // @fixme136
			if (GetMaxHP() >= 0)
				iTmpPercent = (GetHP() * 100) / GetMaxHP();

			if (pAttacker)
			{
				pAttacker->ChatPacket(CHAT_TYPE_INFO, "-> %s, DAM %d HP %d(%d%%) %s%s",
					GetName(),
					dam,
					GetHP(),
					iTmpPercent,
					IsCritical ? "crit " : "",
					IsPenetrate ? "pene " : "",
					IsDeathBlow ? "deathblow " : "");
			}

			ChatPacket(CHAT_TYPE_PARTY, "<- %s, DAM %d HP %d(%d%%) %s%s",
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
			pAttacker->SetQuestNPCID(GetVID());
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
		TDamageMap::iterator it = m_map_kDamage.find(pAttacker->GetVID());

		if (it == m_map_kDamage.end())
		{
			m_map_kDamage.insert(TDamageMap::value_type(pAttacker->GetVID(), TBattleInfo(dam, 0)));
			it = m_map_kDamage.find(pAttacker->GetVID());
		}
		else
		{
			it->second.iTotalDamage += dam;
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

