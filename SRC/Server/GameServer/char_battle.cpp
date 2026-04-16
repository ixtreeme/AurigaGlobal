#include "stdafx.h"
#include "utils.h"
#include "config.h"
#include "desc.h"
#include "desc_manager.h"
#include "char_manager.h"

#ifdef ENABLE_EVENT_MANAGER
extern void Map1MassSpawnEvent_OnMobDead(uint32_t vid);
#endif
#include "item.h"
#include "item_manager.h"
#include "mob_manager.h"
#include "battle.h"
#include "pvp.h"
#include "skill.h"
#include "start_position.h"
#include "profiler.h"
#include "cmd.h"
#include "dungeon.h"
#include "log.h"
#include "unique_item.h"
#include "priv_manager.h"
#include "db.h"
#include "vector.h"
#include "marriage.h"
#include "arena.h"
#include "regen.h"
#include "exchange.h"
#include "shop_manager.h"
#include "dev_log.h"
#include "ani.h"
#include "BattleArena.h"
#include "packet.h"
#include "party.h"
#include "affect.h"
#include "guild.h"
#include "guild_manager.h"
#include "questmanager.h"
#include "questlua.h"
#ifdef __NEWPET_SYSTEM__
#include "New_PetSystem.h"
#endif
#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

#include <random>
#include <algorithm>
#include <boost/algorithm/string/find.hpp>
#include <thread>//Razor93
#ifdef ENABLE_DUNGEON_SHARED_DROP_HWID
#include <unordered_map>
//#include <vector>
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "OrcsDungeon.h"
#include "TritonTempleDungeon.h"
#include "ValentineDungeon.h"
#include "RuneDungeon.h"
#include "PyramidDungeonRazor93.h"
#include "NightmareDungeonRazor93.h"
//#include "LostCastleDungeon.h"
#include "Halloween2022Dungeon.h"
#include "VikingDungeon.h"
#include "EasterDungeon.h"
#endif
#endif
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
static uint32_t __GetPartyExpNP(const uint32_t level)
{
	if (!level || level > PLAYER_EXP_TABLE_MAX)
		return 14000;
	return party_exp_distribute_table[level];
}

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

uint32_t AdjustExpByLevel(const LPCHARACTER ch, const uint32_t exp)
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


EVENTINFO(SCharDeadEventInfo)
{
	uint32_t vid;

	SCharDeadEventInfo()
		: vid(0)
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

	LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(info->vid);
	if (ch == nullptr)
	{
		sys_err("DEAD_EVENT: cannot find char pointer with MOB vid(%d)", info->vid);
		return 0;
	}

	ch->m_pkDeadEvent = nullptr;

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

#ifdef ENABLE_RARE_DROP_NOTICE_RAZOR93
std::string MakeItemLink(LPITEM pkItem, LPCHARACTER pkKiller, LPCHARACTER pkMob)
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

class FPartyAlignmentCompute
{
public:
	FPartyAlignmentCompute(uint32_t iAmount, int x, int y)
	{
		m_iAmount = iAmount;
		m_iCount = 0;
		m_iStep = 0;
		m_iKillerX = x;
		m_iKillerY = y;
	}

	void operator () (LPCHARACTER pkChr)
	{
		if (DISTANCE_APPROX(pkChr->GetX() - m_iKillerX, pkChr->GetY() - m_iKillerY) < PARTY_DEFAULT_RANGE)
		{
			if (m_iStep == 0)
			{
				++m_iCount;
			}
			else
			{
				pkChr->UpdateAlignment(m_iAmount / m_iCount);
			}
		}
	}

	uint32_t m_iAmount;
	uint32_t m_iCount;
	int m_iStep;

	int m_iKillerX;
	int m_iKillerY;
};



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

	//	SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_NO_REWARD);

	//	SetPosition(POS_DEAD);
	//	ClearAffect(true);
	//	ClearSync();
	//	event_cancel(&m_pkStunEvent);

	//	if (pkKiller && pkKiller->IsPC())
	//		CLostCastleDungeon::instance().OnMobKilled(pkKiller, this);

	//	TPacketGCDead pack;
	//	pack.header = HEADER_GC_DEAD;
	//	pack.vid = m_vid;
	//	PacketAround(&pack, sizeof(pack));

	//	REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN);

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
			Map1MassSpawnEvent_OnMobDead(static_cast<uint32_t>(GetVID()));
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
				pkKiller->ChatPacketNew(CHAT_TYPE_INFO, 515, "%d", betMoneyDead);
#endif
			}

			for (unsigned int i = 0; i < _countof(szTableStaticPvP); i++) {
				char pkCh_Buf[CHAT_MAX_LEN + 1], pkKiller_Buf[CHAT_MAX_LEN + 1];

				snprintf(pkCh_Buf, sizeof(pkCh_Buf), "BINARY_Duel_Delete");
				snprintf(pkKiller_Buf, sizeof(pkKiller_Buf), "BINARY_Duel_Delete");

				ChatPacket(CHAT_TYPE_COMMAND, pkCh_Buf);
				SetQuestFlag(szTableStaticPvP[i], 0);

				pkKiller->ChatPacket(CHAT_TYPE_COMMAND, pkKiller_Buf);
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

			pkKiller->SetQuestNPCID(GetVID());
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
			SetQuestNPCID(pkKiller->GetVID());
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
			SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
			LogManager::instance().CharLog(this, pkKiller->GetRaceNum(), "DEAD_BY_NPC", pkKiller->GetName());
		}
		else
		{
#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
			SetDeadByMonster(false);
#endif
			sys_log(1, "DEAD_BY_PC: %s %p KILLER %s %p", GetName(), this, pkKiller->GetName(), get_pointer(pkKiller));
			REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

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
//						pkKiller->ChatPacketNew(CHAT_TYPE_INFO, 413, "");
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
		REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
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
		if (!IS_SET(m_pointsInstant.instant_flag, INSTANT_FLAG_NO_REWARD))
		{
			if (!(pkKiller && pkKiller->IsPC() && pkKiller->GetGuild() && pkKiller->GetGuild()->UnderAnyWar(GUILD_WAR_TYPE_FIELD)))
			{
				// Ȱϴ ʹ   ʴ´.
				if (GetMobTable().dwResurrectionVnum)
				{
					// DUNGEON_MONSTER_REBIRTH_BUG_FIX
					LPCHARACTER chResurrect = CHARACTER_MANAGER::instance().SpawnMob(GetMobTable().dwResurrectionVnum, GetMapIndex(), GetX(), GetY(), GetZ(), true, (int)GetRotation());
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
					pkKiller->ChatPacketNew(CHAT_TYPE_INFO, 147, "");
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
	pack.vid = m_vid;
	PacketAround(&pack, sizeof(pack));

	REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN);

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
			pEventInfo->vid = GetVID();

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
	//	ChatPacket(CHAT_TYPE_INFO, "Nincs leaderboard adat.");
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
void CHARACTER::DistributeHP(LPCHARACTER pkKiller)
{
	if (pkKiller->GetDungeon()) //  ΰʴ´
		return;
}
#define ENABLE_NEWEXP_CALCULATION
#ifdef ENABLE_NEWEXP_CALCULATION
#define NEW_GET_LVDELTA(me, victim) aiPercentByDeltaLev[MINMAX(0, (victim + 15) - me, MAX_EXP_DELTA_OF_LEV - 1)]
typedef long double rate_t;
static void GiveExp(LPCHARACTER from, LPCHARACTER to, int iExp)
{
	if (test_server && iExp < 0)
	{
		to->ChatPacket(CHAT_TYPE_INFO, "exp(%d) overflow", iExp);
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
	rateFactor += to->GetPoint(POINT_EXP);
	// useless (never used except for china intoxication) = always 100
	rateFactor = rateFactor * static_cast<rate_t>(CHARACTER_MANAGER::instance().GetMobExpRate(to)) / 100.0L;
	// apply calculated rate bonus
	iExp *= (rateFactor / 100.0L);
	if (test_server)
		to->ChatPacket(CHAT_TYPE_INFO, "base_exp(%d) * rate(%Lf) = exp(%d)", iBaseExp, rateFactor / 100.0L, iExp);
	// you can get at maximum only 10% of the total required exp at once (so, you need to kill at least 10 mobs to level up) (useless)
	iExp = std::min(to->GetNextExp() / 10, (uint32_t)iExp);
	// it recalculate the given exp if the player level is greater than the exp_table size (useless)
	iExp = AdjustExpByLevel(to, iExp);

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
		to->ChatPacket(CHAT_TYPE_INFO, "exp+minGNE+adjust(%d)", iExp);
	// set
	to->PointChange(POINT_EXP, iExp, true);
	from->CreateFly(FLY_EXP, to);
	// marriage
	{
		LPCHARACTER you = to->GetMarryPartner();
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
static void GiveExp(LPCHARACTER from, LPCHARACTER to, int iExp)
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
	iExp += (iExp * to->GetPoint(POINT_EXP) / 100);

	if (test_server)
	{
		sys_log(0, "Bonus Exp : Ramadan Candy: %d MallExp: %d PointExp: %d",
			to->GetPoint(POINT_RAMADAN_CANDY_BONUS_EXP),
			to->GetPoint(POINT_MALL_EXPBONUS),
			to->GetPoint(POINT_EXP)
		);
	}

	// ȹ  2005.04.21  85%
	iExp = iExp * CHARACTER_MANAGER::instance().GetMobExpRate(to) / 100;

	// ġ ѹ ȹ淮 
	iExp = MIN(to->GetNextExp() / 10, iExp);

	if (test_server)
	{
		if (quest::CQuestManager::instance().GetEventFlag("exp_bonus_log") && iBaseExp > 0)
			to->ChatPacket(CHAT_TYPE_INFO, "exp bonus %d%%", (iExp - iBaseExp) * 100 / iBaseExp);
		to->ChatPacket(CHAT_TYPE_INFO, "exp(%d) base_exp(%d)", iExp, iBaseExp);
	}

	iExp = AdjustExpByLevel(to, iExp);

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
		LPCHARACTER you = to->GetMarryPartner();
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

		FPartyTotaler(LPCHARACTER center)
			: total(0), member_count(0), x(center->GetX()), y(center->GetY())
		{
		};

		void operator () (LPCHARACTER ch)
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
		LPCHARACTER	c;
		int		x, y;
		uint32_t		_iExp;
		int		m_iMode;
		int		m_iMemberCount;

		FPartyDistributor(LPCHARACTER center, int member_count, int total, uint32_t iExp, int iMode)
			: total(total), c(center), x(center->GetX()), y(center->GetY()), _iExp(iExp), m_iMode(iMode), m_iMemberCount(member_count)
		{
			if (m_iMemberCount == 0)
				m_iMemberCount = 1;
		};

		void operator () (LPCHARACTER ch)
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
	LPCHARACTER pAttacker;
	LPPARTY pParty;

	void Clear()
	{
		pAttacker = nullptr;
		pParty = nullptr;
	}

	inline void Distribute(LPCHARACTER ch, int iExp)
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
				LPCHARACTER tch = pParty->GetExpCentralizeCharacter();

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
#ifdef ENABLE_NEWEXP_CALCULATION_RAZOR93
	int map = GetMapIndex();
	if (map == 41 || map == 363)//map1, map2
	{
		//+500%
		iExpToDistribute = iExpToDistribute * 5;
	}
	else if (map == 364)//ice empore
	{
		// +300%
		iExpToDistribute = iExpToDistribute * 4;
	}
	else if (map == 368 || map == 63)//,nephtype,desert
	{
		// +200%
		iExpToDistribute = iExpToDistribute * 2;
	}

#endif

	if (iExpToDistribute <= 0)
		return nullptr;

	uint64_t	iTotalDam = 0;
	LPCHARACTER pkChrMostAttacked = nullptr;
	uint64_t iMostDam = 0;

	typedef std::vector<TDamageInfo> TDamageInfoTable;
	TDamageInfoTable damage_info_table;
	std::map<LPPARTY, TDamageInfo> map_party_damage;

	damage_info_table.reserve(m_map_kDamage.size());

	TDamageMap::iterator it = m_map_kDamage.begin();

	// ϴ    ɷ . (50m)
	while (it != m_map_kDamage.end())
	{
		const VID& c_VID = it->first;
		uint64_t iDam = it->second.iTotalDamage;

		++it;

		LPCHARACTER pAttacker = CHARACTER_MANAGER::instance().Find(c_VID);

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
	LPCHARACTER m_ch;
	FuncForgetMyAttacker(LPCHARACTER ch)
	{
		m_ch = ch;
	}
	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
			if (ch->IsPC())
				return;
			if (ch->m_kVIDVictim == m_ch->GetVID())
				ch->SetVictim(nullptr);
		}
	}
};

struct FuncAggregateMonster
{
	LPCHARACTER m_ch;
	FuncAggregateMonster(LPCHARACTER ch)
	{
		m_ch = ch;
	}
	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
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
	LPCHARACTER m_ch;
	FuncAggregateMonsterPlus(LPCHARACTER ch)
	{
		m_ch = ch;
	}
	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
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
	LPCHARACTER m_ch;
	FuncAttractRanger(LPCHARACTER ch)
	{
		m_ch = ch;
	}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
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
	LPCHARACTER m_ch;
	int m_iLength;
	FuncPullMonster(LPCHARACTER ch, int iLength = 300)
	{
		m_ch = ch;
		m_iLength = iLength;
	}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
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

void CHARACTER::ForgetMyAttacker()
{
	LPSECTREE pSec = GetSectree();
	if (pSec)
	{
		FuncForgetMyAttacker f(this);
		pSec->ForEachAround(f);
	}
	ReviveInvisible(5);
}

void CHARACTER::AggregateMonster()
{
	LPSECTREE pSec = GetSectree();
	if (pSec)
	{
		FuncAggregateMonster f(this);
		pSec->ForEachAround(f);
	}
}

#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
void CHARACTER::AggregateMonsterPlus()
{
	LPSECTREE pSec = GetSectree();
	if (pSec)
	{
		FuncAggregateMonsterPlus f(this);
		pSec->ForEachAround(f);
	}
}
#endif
void CHARACTER::AttractRanger()
{
	LPSECTREE pSec = GetSectree();
	if (pSec)
	{
		FuncAttractRanger f(this);
		pSec->ForEachAround(f);
	}
}

void CHARACTER::PullMonster()
{
	LPSECTREE pSec = GetSectree();
	if (pSec)
	{
		FuncPullMonster f(this);
		pSec->ForEachAround(f);
	}
}

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

		if (pParty->GetLeaderPID() == GetVID())
			iPartyAggroDist /= 2;
		else
			iPartyAggroDist /= 3;

		pParty->SendMessage(this, PM_AGGRO_INCREASE, iPartyAggroDist, pAttacker->GetVID());
	}

	ChangeVictimByAggro(info.iAggro, pAttacker);
}

void CHARACTER::UpdateAggrPoint(LPCHARACTER pAttacker, EDamageType type, int dam)
{
	if (IsDead() || IsStun())
		return;

	TDamageMap::iterator it = m_map_kDamage.find(pAttacker->GetVID());

	if (it == m_map_kDamage.end())
	{
		m_map_kDamage.insert(TDamageMap::value_type(pAttacker->GetVID(), TBattleInfo(0, dam)));
		it = m_map_kDamage.find(pAttacker->GetVID());
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
				LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(it->first);

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
				SetVictim(CHARACTER_MANAGER::instance().Find(itFind->first));
			}
#else
			SetVictim(CHARACTER_MANAGER::instance().Find(itFind->first));
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

