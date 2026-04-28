#include "stdafx.h"
#include "utils.h"
#include "config.h"
#include "desc.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "battle.h"
#include "item.h"
#include "item_manager.h"
#include "mob_manager.h"
#include "vector.h"
#include "packet.h"
#include "pvp.h"
#include "profiler.h"
#include "guild.h"
#include "affect.h"
#include "unique_item.h"
#include "lua_incl.h"
#include "arena.h"
#include "sectree.h"
//#include "LostCastleDungeon.h"
#include "ani.h"
#include "locale_service.h"
#include <common/CommonDefines.h>
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"

#include "db.h"
//#include <Database/DBManager.h>
int battle_hit(LPCHARACTER ch, LPCHARACTER victim, int & iRetDam);

bool battle_distance_valid_by_xy(int32_t x, int32_t y, int32_t tx, int32_t ty)
{
	int32_t distance = DISTANCE_APPROX(x - tx, y - ty);

	if (distance > 170)
		return false;

	return true;
}

bool battle_distance_valid(LPCHARACTER ch, LPCHARACTER victim)
{
	return battle_distance_valid_by_xy(ch->GetX(), ch->GetY(), victim->GetX(), victim->GetY());
}

bool timed_event_cancel(LPCHARACTER ch)
{
	if (ch->GetTimedEvent())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 482, "");
#endif
		event_cancel(&ch->GetTimedEventRef());
		return true;
	}

	/* RECALL_DELAY
	   ???? ?????? ???? ????? ??????? ??? ???? ?? ??? ??? ????
	   if (ch->m_pk_RecallEvent)
	   {
	   event_cancel(&ch->m_pkRecallEvent);
	   return true;
	   }
	   END_OF_RECALL_DELAY */

	return false;
}

bool battle_is_attackable(LPCHARACTER ch, LPCHARACTER victim)
{
	// ���1aAI ��3�A��� �ߴ��N�U.
	if (victim->IsDead())
	{
		return false;
	}


#ifdef ENABLE_BUG_FIXES
	if (victim->GetMyShop())
	{
		return false;
	}
#endif

	// 3EA������ �ߴ�
	{
		SECTREE* sectree = nullptr;

		sectree = ch->GetSectree();
		if (sectree && sectree->IsAttr(ch->GetX(), ch->GetY(), ATTR_BANPK))
		{
			return false;
		}

		sectree = victim->GetSectree();
		if (sectree && sectree->IsAttr(victim->GetX(), victim->GetY(), ATTR_BANPK))
		{
			return false;
		}
	}

	// 3��! ��3�A��� �ߴ��N�U.
	if (ch->IsStun() || ch->IsDead())
	{
		return false;
	}

	if (ch->IsPC() && victim->IsPC())
	{
		CGuild* g1 = ch->GetGuild();
		CGuild* g2 = victim->GetGuild();

		if (g1 && g2)
		{
			if (g1->UnderWar(g2->GetID()))
				return true;
		}
	}

	if (CArenaManager::instance().CanAttack(ch, victim) == true)
		return true;

#ifdef __DEFENSE_WAVE__
	if (victim->GetRaceNum() == 20434 && ch->IsMonster())
	{
		return true;
	}
#endif

	bool bIsFarmMap = false;//razor93 2024.12.30
	switch (ch->GetMapIndex())
	{
	case 1:
	{
		if (victim->IsPC() && ch->IsPC())
			bIsFarmMap = true;
	}
	break;
	}
	const bool canAttack = CPVPManager::instance().CanAttack(ch, victim, bIsFarmMap);
	return canAttack;
}

int battle_melee_attack(LPCHARACTER ch, LPCHARACTER victim)
{
#if defined(ENABLE_CHECK_BATTLE)
	if (ch->IsPC() && victim) {
		const bool bAttacking = (get_dword_time() - ch->GetLastAttackTime()) < (ch->IsRiding() ? 800 : 750);
		if (!bAttacking) {
			return BATTLE_NONE;
		}

		//ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Melee Attack: %d", get_dword_time() - ch->GetLastAttackTime());
		//		if (!battle_distance_valid(ch, victim)) {
		//			return BATTLE_NONE;
		//		}
	}
#endif

	if (test_server && ch->IsPC())
		sys_log(0, "battle_melee_attack : [%s] attack to [%s]", ((ch)->GetName()), ((victim)->GetName()));

	if (!victim || ch == victim)
	{
		return BATTLE_NONE;
	}

	if (test_server && ch->IsPC())
		sys_log(0, "battle_melee_attack : [%s] attack to [%s]", ((ch)->GetName()), ((victim)->GetName()));

	if (!battle_is_attackable(ch, victim))
	{
		return BATTLE_NONE;
	}

	if (test_server && ch->IsPC())
		sys_log(0, "battle_melee_attack : [%s] attack to [%s]", ((ch)->GetName()), ((victim)->GetName()));

	// �A�� A1A�
	int distance = DISTANCE_APPROX(ch->GetX() - victim->GetX(), ch->GetY() - victim->GetY());

	if (!victim->IsBuilding())
	{
		int max = 300;

		if (false == ch->IsPC())
		{
			// ��1oA�A� �a?i ��1oA� �o�� �A���� ��?�
			max = (int)(ch->GetMobAttackRange() * 1.15f);
		}
		else
		{
			// PCAI �a?i ���! melee ��AI �a?i ��A� �o�� �A���! Aִ� �o�� �A��
			if (false == victim->IsPC() && BATTLE_TYPE_MELEE == victim->GetMobBattleType())
				max = MAX(300, (int)(victim->GetMobAttackRange() * 1.15f));
		}

#ifdef __DEFENSE_WAVE__
		if (ch->IsPC() && (victim->GetRaceNum() == 3960 || victim->GetRaceNum() == 3961 || victim->GetRaceNum() == 3962))
		{
			max += 400;
		}
#endif

		if (distance > max)
		{
			if (test_server)
				sys_log(0, "VICTIM_FAR: %s distance: %d max: %d", ((ch)->GetName()), distance, max);

			return BATTLE_NONE;
		}
	}

#ifdef TEXTS_IMPROVEMENT
	if (timed_event_cancel(ch)) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 456, "");
	}
	else if (timed_event_cancel(victim)) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 456, "");
	}
#endif

	ch->SetPosition(POS_FIGHTING);
	ch->SetVictim(victim);

	const PIXEL_POSITION& vpos = victim->GetXYZ();
	ch->SetRotationToXY(vpos.x, vpos.y);

	int dam;
	int ret = battle_hit(ch, victim, dam);
	return (ret);
}


// ???? GET_BATTLE_VICTIM?? NULL?? ????? ???T?? j?? ??U??.
void battle_end_ex(LPCHARACTER ch)
{
	if (ch->IsPosition(POS_FIGHTING))
		ch->SetPosition(POS_STANDING);
}

void battle_end(LPCHARACTER ch)
{
	battle_end_ex(ch);
}

// AG = Attack Grade
// AL = Attack Limit
int CalcBattleDamage(int iDam, int iAttackerLev, int iVictimLev)
{
	if (iDam < 3)
		iDam = number(1, 5);

	//return CALCULATE_DAMAGE_LVDELTA(iAttackerLev, iVictimLev, iDam);
	return iDam;
}

int CalcMagicDamageWithValue(int iDam, LPCHARACTER pkAttacker, LPCHARACTER pkVictim)
{
	return CalcBattleDamage(iDam, pkAttacker->GetLevel(), pkVictim->GetLevel());
}

int CalcMagicDamage(LPCHARACTER pkAttacker, LPCHARACTER pkVictim)
{
	int iDam = 0;

	if (pkAttacker->IsNPC())
	{
		iDam = CalcMeleeDamage(pkAttacker, pkVictim, false, false);
	}

	iDam += pkAttacker->GetPoint(POINT_PARTY_ATTACKER_BONUS);

	return CalcMagicDamageWithValue(iDam, pkAttacker, pkVictim);
}

float CalcAttackRating(LPCHARACTER pkAttacker, LPCHARACTER pkVictim, bool bIgnoreTargetRating)
{
	int iARSrc;
	int iERSrc;

	{
		int attacker_dx = pkAttacker->GetPolymorphPoint(POINT_DX);
		int attacker_lv = pkAttacker->GetLevel();

		int victim_dx = pkVictim->GetPolymorphPoint(POINT_DX);
		int victim_lv = pkAttacker->GetLevel();

		iARSrc = MIN(90, (attacker_dx * 4	+ attacker_lv * 2) / 6);
		iERSrc = MIN(90, (victim_dx	  * 4	+ victim_lv   * 2) / 6);
	}

	float fAR = ((float) iARSrc + 210.0f) / 300.0f; // fAR = 0.7 ~ 1.0

	if (bIgnoreTargetRating)
		return fAR;

	// ((Edx * 2 + 20) / (Edx + 110)) * 0.3
	float fER = ((float) (iERSrc * 2 + 5) / (iERSrc + 95)) * 3.0f / 10.0f;

	return fAR - fER;
}

int CalcAttBonus(LPCHARACTER pkAttacker, LPCHARACTER pkVictim, int iAtk)
{
	// PvP???? ????????????
	if (!pkVictim->IsPC())
		iAtk += pkAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_ATTACK_BONUS);

	// PvP???? ????????????
	if (!pkAttacker->IsPC())
	{
		int iReduceDamagePct = pkVictim->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_TRANSFER_DAMAGE);
		iAtk = iAtk * (100 + iReduceDamagePct) / 100;
	}

	if (pkAttacker->IsNPC() && pkVictim->IsPC())
	{
		iAtk = (iAtk * CHARACTER_MANAGER::instance().GetMobDamageRate(pkAttacker)) / 100;
	}

	if (pkVictim->IsNPC())
	{
#ifdef ENABLE_DS_RUNE
		if (pkVictim->IsRaceFlag(RACE_FLAG_RUNE))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_RUNE_MONSTERS)) / 100;
#endif
		if (pkVictim->IsRaceFlag(RACE_FLAG_ANIMAL))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_ANIMAL)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_UNDEAD))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_UNDEAD)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_DEVIL))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_DEVIL)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_HUMAN))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_HUMAN)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ORC))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_ORC)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_MILGYO))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_MILGYO)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_INSECT))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_INSECT)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_FIRE))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_FIRE)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ICE))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_ICE)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_DESERT))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_DESERT)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_TREE))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_TREE)) / 100;
#ifdef ELEMENT_NEW_BONUSES
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_ELEC))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_ELEC)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_FIRE))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_FIRE)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_ICE))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_ICE)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_WIND))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_WIND)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_EARTH))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_EARTH)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_DARK))
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_DARK)) / 100;
#endif
		if (pkVictim->GetCharType() == CHAR_TYPE_STONE) {
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_METIN)) / 100;
		}
		else {
			if (pkVictim->GetMobRank() >= MOB_RANK_BOSS)
				iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_BOSS)) / 100;
		}

#ifdef ENABLE_NO_ATTBONUS_MONSTER_FOR_STONES
		if (pkVictim->GetCharType() != CHAR_TYPE_STONE) {
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_MONSTER)) / 100;
		}
#else
#ifdef ENABLE_MAP1_SKILL_MOB__disable
		if (!(pkVictim && pkVictim->IsMonster() && pkVictim->GetRaceNum() == 136 && pkAttacker->IsSkillHit()))
		{
			iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_MONSTER)) / 100;
		}
#else
		iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_MONSTER)) / 100;
#endif

#endif
	}
	else if (pkVictim->IsPC())
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		{
			const int A = pkAttacker->GetPoint(POINT_ATTBONUS_HUMAN);            // tamado bonusz (%)
			const int R = pkVictim->GetPoint(POINT_RESIST_MEZZIUOMINI);          // vedekezo resist (%)

			// 100 -> 20, 200 -> 40
			int effR = (R + 2) / 10;  

			if (effR < 0) effR = 0;
			//if (effR > 100) effR = 100; // cap 100%

			int net = A - effR;
			if (net < 0) net = 0; 

			if (net > 0)
				iAtk += (iAtk * net) / 100;
		}
#else

	iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_HUMAN)) / 100;
#endif

		switch (pkVictim->GetJob())
		{
			case JOB_WARRIOR:
				iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_WARRIOR)) / 100;
				break;

			case JOB_ASSASSIN:
				iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_ASSASSIN)) / 100;
				break;

			case JOB_SURA:
				iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_SURA)) / 100;
				break;

			case JOB_SHAMAN:
				iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_SHAMAN)) / 100;
				break;
#ifdef ENABLE_WOLFMAN_CHARACTER
			case JOB_WOLFMAN: // TODO: ?????? ATTBONUS �??
				iAtk += (iAtk * pkAttacker->GetPoint(POINT_ATTBONUS_WOLFMAN)) / 100;
				break;
#endif
		}
	}

	if (pkAttacker->IsPC() == true)
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		iAtk -= (iAtk * pkVictim->GetPoint(POINT_DEF_TALISMAN)) / 100;
#endif
		switch (pkAttacker->GetJob())
		{
			case JOB_WARRIOR:
				iAtk -= (iAtk * pkVictim->GetPoint(POINT_RESIST_WARRIOR)) / 100;
				break;

			case JOB_ASSASSIN:
				iAtk -= (iAtk * pkVictim->GetPoint(POINT_RESIST_ASSASSIN)) / 100;
				break;

			case JOB_SURA:
				iAtk -= (iAtk * pkVictim->GetPoint(POINT_RESIST_SURA)) / 100;
				break;

			case JOB_SHAMAN:
				iAtk -= (iAtk * pkVictim->GetPoint(POINT_RESIST_SHAMAN)) / 100;
				break;
#ifdef ENABLE_WOLFMAN_CHARACTER
			case JOB_WOLFMAN: // TODO: ?????? ???? �??
				iAtk -= (iAtk * pkVictim->GetPoint(POINT_RESIST_WOLFMAN)) / 100;
				break;
#endif
		}
	}

#ifdef ELEMENT_TARGET
	//[ mob -> PC ] ???? ??? ??? ????
	//2013/01/17
	//???? ??????? ???????? 30%?? ?????? ??g???? ?????? ?????.
	if (pkAttacker->IsNPC() && pkVictim->IsPC())
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		iAtk -= (iAtk * 30 * pkVictim->GetPoint(POINT_DEF_TALISMAN))		/ 10000;
#endif
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_ELEC))
			iAtk -= (iAtk * 30 * pkVictim->GetPoint(POINT_RESIST_ELEC))		/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_FIRE))
			iAtk -= (iAtk * 30 * pkVictim->GetPoint(POINT_RESIST_FIRE))		/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_ICE))
			iAtk -= (iAtk * 30 * pkVictim->GetPoint(POINT_RESIST_ICE))		/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_WIND))
			iAtk -= (iAtk * 30 * pkVictim->GetPoint(POINT_RESIST_WIND))		/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_EARTH))
			iAtk -= (iAtk * 30 * pkVictim->GetPoint(POINT_RESIST_EARTH))	/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_DARK))
			iAtk -= (iAtk * 30 * pkVictim->GetPoint(POINT_RESIST_DARK))		/ 10000;//difesa
#endif
#ifdef ENABLE_RESIST_MONSTER
		iAtk -= (iAtk * 30 * pkVictim->GetPoint(POINT_RESIST_MONSTER))		/ 10000;//resistenza mostri
#endif
	}

	return iAtk;
}

void Item_GetDamage(LPITEM pkItem, int* pdamMin, int* pdamMax)
{
	*pdamMin = 0;
	*pdamMax = 1;

	if (!pkItem)
		return;

	switch (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)))
	{
		case ITEM_ROD:
		case ITEM_PICK:
			return;
	}

	if (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) != ITEM_WEAPON)
		sys_err("Item_GetDamage - !ITEM_WEAPON vnum=%d, type=%d", pkItem->GetOriginalVnum(), ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)));

	*pdamMin = ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pkItem), 3);
	*pdamMax = ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pkItem), 4);
}

int CalcMeleeDamage(LPCHARACTER pkAttacker, LPCHARACTER pkVictim, bool bIgnoreDefense, bool bIgnoreTargetRating)
{
	LPITEM pWeapon = pkAttacker->GetWear(WEAR_WEAPON);
	bool bPolymorphed = pkAttacker->IsPolymorphed();

	if (pWeapon && !(bPolymorphed && !pkAttacker->IsPolyMaintainStat()))
	{
		if (ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pWeapon)) != ITEM_WEAPON)
			return 0;

				switch (ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pWeapon)))
		{
			case WEAPON_SWORD:
			case WEAPON_DAGGER:
			case WEAPON_TWO_HANDED:
			case WEAPON_BELL:
			case WEAPON_FAN:
			case WEAPON_MOUNT_SPEAR:
#ifdef ENABLE_WOLFMAN_CHARACTER
			case WEAPON_CLAW:
#endif
				break;

			case WEAPON_BOW:
				sys_err("CalcMeleeDamage should not handle bows (name: %s)", ((pkAttacker)->GetName()));
				return 0;

			default:
				return 0;
		}
	}

	int iDam = 0;
	float fAR = CalcAttackRating(pkAttacker, pkVictim, bIgnoreTargetRating);
	int iDamMin = 0, iDamMax = 0;

	// TESTSERVER_SHOW_ATTACKINFO
	int DEBUG_iDamCur = 0;
	int DEBUG_iDamBonus = 0;
	// END_OF_TESTSERVER_SHOW_ATTACKINFO

	if (bPolymorphed && !pkAttacker->IsPolyMaintainStat())
	{
		// MONKEY_ROD_ATTACK_BUG_FIX
		Item_GetDamage(pWeapon, &iDamMin, &iDamMax);
		// END_OF_MONKEY_ROD_ATTACK_BUG_FIX

		uint32_t dwMobVnum = pkAttacker->GetPolymorphVnum();
		const CMob * pMob = CMobManager::instance().Get(dwMobVnum);

		if (pMob)
		{
			int iPower = pkAttacker->GetPolymorphPower();
			iDamMin += pMob->m_table.dwDamageRange[0] * iPower / 100;
			iDamMax += pMob->m_table.dwDamageRange[1] * iPower / 100;
		}
	}
	else if (pWeapon)
	{
		// MONKEY_ROD_ATTACK_BUG_FIX
		Item_GetDamage(pWeapon, &iDamMin, &iDamMax);
		// END_OF_MONKEY_ROD_ATTACK_BUG_FIX
	}
	else if (pkAttacker->IsNPC())
	{
		iDamMin = pkAttacker->GetMobDamageMin();
		iDamMax = pkAttacker->GetMobDamageMax();
	}

	iDam = number(iDamMin, iDamMax) * 2;

	// TESTSERVER_SHOW_ATTACKINFO
	DEBUG_iDamCur = iDam;
	// END_OF_TESTSERVER_SHOW_ATTACKINFO
	//
	int iAtk = 0;

	// level must be ignored when multiply by fAR, so subtract it before calculation.
	iAtk = pkAttacker->GetPoint(POINT_ATT_GRADE) + iDam - (pkAttacker->GetLevel() * 2);
	iAtk = (int) (iAtk * fAR);
	iAtk += pkAttacker->GetLevel() * 2; // and add again

	if (pWeapon)
	{
		iAtk += ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pWeapon), 5) * 2;

		// 2004.11.12.myevan.TESTSERVER_SHOW_ATTACKINFO
		DEBUG_iDamBonus = ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pWeapon), 5) * 2;
		///////////////////////////////////////////////
	}

	iAtk += pkAttacker->GetPoint(POINT_PARTY_ATTACKER_BONUS); // party attacker role bonus
	iAtk = (int) (iAtk * (100 + (pkAttacker->GetPoint(POINT_ATT_BONUS) + pkAttacker->GetPoint(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100);

	iAtk = CalcAttBonus(pkAttacker, pkVictim, iAtk);

	int iDef = 0;

	if (!bIgnoreDefense)
	{
		iDef = (pkVictim->GetPoint(POINT_DEF_GRADE) * (100 + pkVictim->GetPoint(POINT_DEF_BONUS)) / 100);

		if (!pkAttacker->IsPC())
			iDef += pkVictim->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_DEFENSE_BONUS);
	}

	if (pkAttacker->IsNPC())
		iAtk = (int) (iAtk * pkAttacker->GetMobDamageMultiply());

	iDam = MAX(0, iAtk - iDef);

	if (test_server)
	{
		int DEBUG_iLV = pkAttacker->GetLevel()*2;
		int DEBUG_iST = int((pkAttacker->GetPoint(POINT_ATT_GRADE) - DEBUG_iLV) * fAR);
		int DEBUG_iPT = pkAttacker->GetPoint(POINT_PARTY_ATTACKER_BONUS);
		int DEBUG_iWP = 0;
		int DEBUG_iPureAtk = 0;
		int DEBUG_iPureDam = 0;
		char szRB[32] = "";
		char szGradeAtkBonus[32] = "";

		DEBUG_iWP = int(DEBUG_iDamCur * fAR);
		DEBUG_iPureAtk = DEBUG_iLV + DEBUG_iST + DEBUG_iWP+DEBUG_iDamBonus;
		DEBUG_iPureDam = iAtk - iDef;

		if (pkAttacker->IsNPC())
		{
			snprintf(szGradeAtkBonus, sizeof(szGradeAtkBonus), "=%d*%.1f", DEBUG_iPureAtk, pkAttacker->GetMobDamageMultiply());
			DEBUG_iPureAtk = int(DEBUG_iPureAtk * pkAttacker->GetMobDamageMultiply());
		}

		if (DEBUG_iDamBonus != 0)
			snprintf(szRB, sizeof(szRB), "+RB(%d)", DEBUG_iDamBonus);

		char szPT[32] = "";

		if (DEBUG_iPT != 0)
			snprintf(szPT, sizeof(szPT), ", PT=%d", DEBUG_iPT);

		char szUnknownAtk[32] = "";

		if (iAtk != DEBUG_iPureAtk)
			snprintf(szUnknownAtk, sizeof(szUnknownAtk), "+?(%d)", iAtk-DEBUG_iPureAtk);

		char szUnknownDam[32] = "";

		if (iDam != DEBUG_iPureDam)
			snprintf(szUnknownDam, sizeof(szUnknownDam), "+?(%d)", iDam-DEBUG_iPureDam);

		char szMeleeAttack[128];

		snprintf(szMeleeAttack, sizeof(szMeleeAttack),
				"%s(%d)-%s(%d)=%d%s, ATK=LV(%d)+ST(%d)+WP(%d)%s%s%s, AR=%.3g%s",
				((pkAttacker)->GetName()),
				iAtk,
				((pkVictim)->GetName()),
				iDef,
				iDam,
				szUnknownDam,
				DEBUG_iLV,
				DEBUG_iST,
				DEBUG_iWP,
				szRB,
				szUnknownAtk,
				szGradeAtkBonus,
				fAR,
				szPT);

		ecs::ChatSystem::Send(AIHelpers::EcsOf(pkAttacker), CHAT_TYPE_TALKING, "%s", szMeleeAttack);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(pkVictim), CHAT_TYPE_TALKING, "%s", szMeleeAttack);
	}

	return CalcBattleDamage(iDam, pkAttacker->GetLevel(), pkVictim->GetLevel());
}

int CalcArrowDamage(LPCHARACTER pkAttacker, LPCHARACTER pkVictim, LPITEM pkBow, LPITEM pkArrow, bool bIgnoreDefense)
{
	if (!pkBow || ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkBow)) != ITEM_WEAPON || ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkBow)) != WEAPON_BOW)
		return 0;

	if (!pkArrow)
		return 0;

	// Y??g ????
	int iDist = (int) (DISTANCE_SQRT(pkAttacker->GetX() - pkVictim->GetX(), pkAttacker->GetY() - pkVictim->GetY()));
	//int iGap = (iDist / 100) - 5 - ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pkBow), 5) - pkAttacker->GetPoint(POINT_BOW_DISTANCE);
	int iGap = (iDist / 100) - 5 - pkAttacker->GetPoint(POINT_BOW_DISTANCE);
	int iPercent = 100 - (iGap * 5);

	if (iPercent <= 0)
		return 0;
	else if (iPercent > 100)
		iPercent = 100;

	int iDam = 0;

	float fAR = CalcAttackRating(pkAttacker, pkVictim, false);
	iDam = number(ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pkBow), 3), ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pkBow), 4)) * 2 + ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pkArrow), 3);
	int iAtk;

	// level must be ignored when multiply by fAR, so subtract it before calculation.
	iAtk = pkAttacker->GetPoint(POINT_ATT_GRADE) + iDam - (pkAttacker->GetLevel() * 2);
	iAtk = (int) (iAtk * fAR);
	iAtk += pkAttacker->GetLevel() * 2; // and add again

	// Refine Grade
	iAtk += ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pkBow), 5) * 2;

	iAtk += pkAttacker->GetPoint(POINT_PARTY_ATTACKER_BONUS);
	iAtk = (int) (iAtk * (100 + (pkAttacker->GetPoint(POINT_ATT_BONUS) + pkAttacker->GetPoint(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100);

	iAtk = CalcAttBonus(pkAttacker, pkVictim, iAtk);

	int iDef = 0;

	if (!bIgnoreDefense)
		iDef = (pkVictim->GetPoint(POINT_DEF_GRADE) * (100 + pkAttacker->GetPoint(POINT_DEF_BONUS)) / 100);

	if (pkAttacker->IsNPC())
		iAtk = (int) (iAtk * pkAttacker->GetMobDamageMultiply());

	iDam = MAX(0, iAtk - iDef);

	int iPureDam = iDam;

	iPureDam = (iPureDam * iPercent) / 100;

	if (test_server)
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(pkAttacker), CHAT_TYPE_INFO, "ARROW %s -> %s, DAM %d DIST %d GAP %d %% %d",
				((pkAttacker)->GetName()),
				((pkVictim)->GetName()),
				iPureDam,
				iDist, iGap, iPercent);
	}

	return iPureDam;
	//return iDam;
}


void NormalAttackAffect(LPCHARACTER pkAttacker, LPCHARACTER pkVictim)
{
	// ?? ?????? U?????? U?? �??
	if (pkAttacker->GetPoint(POINT_POISON_PCT) && !pkVictim->IsAffectFlag(AFF_POISON))
	{
		if (number(1, 100) <= pkAttacker->GetPoint(POINT_POISON_PCT))
			pkVictim->AttackedByPoison(pkAttacker);
	}
#ifdef ENABLE_WOLFMAN_CHARACTER
	if (pkAttacker->GetPoint(POINT_BLEEDING_PCT) && !pkVictim->IsAffectFlag(AFF_BLEEDING))
	{
		if (number(1, 100) <= pkAttacker->GetPoint(POINT_BLEEDING_PCT))
			pkVictim->AttackedByBleeding(pkAttacker);
	}
#endif
	int iStunDuration = 2;
	if (pkAttacker->IsPC() && !pkVictim->IsPC())
		iStunDuration = 4;

	AttackAffect(pkAttacker, pkVictim, POINT_STUN_PCT, IMMUNE_STUN,  AFFECT_STUN, POINT_NONE,        0, AFF_STUN, iStunDuration, "STUN");
	AttackAffect(pkAttacker, pkVictim, POINT_SLOW_PCT, IMMUNE_SLOW,  AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, 20,		"SLOW");
}

int battle_hit(LPCHARACTER pkAttacker, LPCHARACTER pkVictim, int & iRetDam)
{
#if defined(ENABLE_CHECK_BATTLE)
	if (pkAttacker->IsPC() && pkVictim) {
		const bool bAttacking = (get_dword_time() - pkAttacker->GetLastAttackTime()) < (pkAttacker->IsRiding() ? 800 : 750);
		if (!bAttacking) {
			return BATTLE_NONE;
		}

//ecs::ChatSystem::Send(AIHelpers::EcsOf(pkAttacker), CHAT_TYPE_INFO, "Melee Attack: %d", get_dword_time() - pkAttacker->GetLastAttackTime());
//		if (!battle_distance_valid(pkAttacker, pkVictim)) {
//			return BATTLE_NONE;
//		}
	}
#endif

	//PROF_UNIT puHit("Hit");
	if (test_server)
		sys_log(0, "battle_hit : [%s] attack to [%s] : dam :%d type :%d", ((pkAttacker)->GetName()), ((pkVictim)->GetName()), iRetDam);

	int iDam = CalcMeleeDamage(pkAttacker, pkVictim);

	if (iDam <= 0)
		return (BATTLE_DAMAGE);

	NormalAttackAffect(pkAttacker, pkVictim);

	// ?????? ???
	//iDam = iDam * (100 - pkVictim->GetPoint(POINT_RESIST)) / 100;
	LPITEM pkWeapon = pkAttacker->GetWear(WEAR_WEAPON);

	if (pkWeapon)
		switch (pkWeapon->GetSubType())
		{
			case WEAPON_SWORD:
			{
				int32_t lValue = pkVictim->GetPoint(POINT_RESIST_SWORD);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= pkAttacker->GetPoint(POINT_ATTBONUS_IRR_SPADA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= pkAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}
			case WEAPON_TWO_HANDED:
			{
				int32_t lValue = pkVictim->GetPoint(POINT_RESIST_TWOHAND);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= pkAttacker->GetPoint(POINT_ATTBONUS_IRR_SPADONE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= pkAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}
			case WEAPON_DAGGER:
			{
				int32_t lValue = pkVictim->GetPoint(POINT_RESIST_DAGGER);

#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= pkAttacker->GetPoint(POINT_ATTBONUS_IRR_PUGNALE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= pkAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif

				  
				//if (pkAttacker->IsPC() && pkVictim->IsPC())
				//	lValue += 15;

				// clamp 0..100
				if (lValue < 0)   lValue = 0;
				if (lValue > 100) lValue = 100;

				iDam = iDam * (100 - lValue) / 100;
				break;
			}


			case WEAPON_BELL:
			{
				int32_t lValue = pkVictim->GetPoint(POINT_RESIST_BELL);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= pkAttacker->GetPoint(POINT_ATTBONUS_IRR_CAMPANA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= pkAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif

				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}
			case WEAPON_FAN:
			{
				int32_t lValue = pkVictim->GetPoint(POINT_RESIST_FAN);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= pkAttacker->GetPoint(POINT_ATTBONUS_IRR_VENTAGLIO);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= pkAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}
			
			case WEAPON_BOW:
			{
				int32_t lValue = pkVictim->GetPoint(POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= pkAttacker->GetPoint(POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= pkAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}
			
#ifdef ENABLE_WOLFMAN_CHARACTER
			case WEAPON_CLAW:
			{
				int32_t lValue = pkVictim->GetPoint(POINT_RESIST_DAGGER);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= pkAttacker->GetPoint(POINT_ATTBONUS_IRR_PUGNALE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= pkAttacker->GetPoint(POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}
#endif
			default:
				break;
		}


	//???????? ?????? ????. (2011?? 2?? ???? ???�????? ????.)
	float attMul = pkAttacker->GetAttMul();
	float tempIDam = iDam;
	iDam = attMul * tempIDam + 0.5f;

#ifdef ENABLE_SOUL_SYSTEM
	iDam += pkAttacker->GetSoulItemDamage(pkVictim, iDam, RED_SOUL);
#endif

	iRetDam = iDam;

	//PROF_UNIT puDam("Dam");
	if (pkVictim->Damage(pkAttacker, iDam, DAMAGE_TYPE_NORMAL))
		return (BATTLE_DEAD);
//#ifdef ENABLE_MAP1_SKILL_MOB
//	if (pkAttacker->IsPC() /*&& pkAttacker->IsSkillHit()*/
//		&& pkVictim->GetRaceNum() == 136)
//	{
//		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
//			"UPDATE player.player "
//			"SET map1_skillmob = GREATEST(map1_skillmob, %d) "
//			"WHERE id=%u",
//			iRetDam, pkAttacker->GetPlayerID()));
//
//		///*pkAttacker->*/viChatPacket(CHAT_TYPE_TALKING, "You hit a Skill Mob for %d damage!", iRetDam);
//		ecs::ChatSystem::Send(AIHelpers::EcsOf(pkAttacker), CHAT_TYPE_INFO, "You hit a Skill Mob for %d damage!", iRetDam);
//		
//		
//
//
//		sys_log(0, "DEBUG MAP1_SKILL_MOB: attacker=%s (id=%u) victimVnum=%d dmg=%d skillhit=%d",
//			((pkAttacker)->GetName()),
//			pkAttacker->GetPlayerID(),
//			pkVictim->GetRaceNum(),
//			iRetDam,
//			pkAttacker->IsSkillHit());
//
//	}
//#endif

	return (BATTLE_DAMAGE);
}

#ifdef ENABLE_ANTICHEAT
int32_t GET_ATTACK_SPEED(LPCHARACTER ch) {
	if (!ch) {
		return 1000;
	}

	int32_t default_bonus = 100;
	int32_t riding_bonus = ch->IsRiding() ? 50 : 0;
	int32_t ani_speed = ani_attack_speed(ch);
	int32_t real_speed = (ani_speed * 100) / (default_bonus + ch->GetPoint(POINT_ATT_SPEED) + riding_bonus);

	LPITEM item = ch->GetWear(WEAR_WEAPON);
	return item && ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, item)) == WEAPON_DAGGER ? real_speed / 2 : real_speed;
}

void SET_ATTACK_TIME(LPCHARACTER ch, LPCHARACTER victim, int32_t current_time) {
	if (victim && ch && ch->IsPC()) {
		ch->GetAttackLogRef().dwVID = victim->GetPacketVID();
		ch->GetAttackLogRef().dwTime = current_time;
	}
}

void SET_ATTACKED_TIME(LPCHARACTER ch, LPCHARACTER victim, int32_t current_time) {
	if (victim && ch && ch->IsPC()) {
		victim->GetAttackedLogRef().dwPID = ((ch)->GetPlayerID());
		victim->GetAttackedLogRef().dwAttackedTime = current_time;
	}
}

bool IS_SPEED_HACK(LPCHARACTER ch, LPCHARACTER victim, int32_t current_time) {
	if (victim && ch && ch->IsPC()) {
		if (ch->GetAttackLogRef().dwVID == victim->GetPacketVID())
		{
			if (current_time - ch->GetAttackLogRef().dwTime < GET_ATTACK_SPEED(ch))
			{
				INCREASE_SPEED_HACK_COUNT(ch);
	
				if (test_server)
				{
					sys_log(0, "%s attack hack! time (delta, limit)=(%u, %u) hack_count %d",
							((ch)->GetName()),
							current_time - ch->GetAttackLogRef().dwTime,
							GET_ATTACK_SPEED(ch),
							ch->GetSpeedHackCount());
	
					ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s attack hack! time (delta, limit)=(%u, %u) hack_count %d",
							((ch)->GetName()),
							current_time - ch->GetAttackLogRef().dwTime,
							GET_ATTACK_SPEED(ch),
							ch->GetSpeedHackCount());
				}
	
				SET_ATTACK_TIME(ch, victim, current_time);
				SET_ATTACKED_TIME(ch, victim, current_time);
				return true;
			}
		}
	
		SET_ATTACK_TIME(ch, victim, current_time);
	
		if (victim->GetAttackedLogRef().dwPID == ((ch)->GetPlayerID())) {
			if (current_time - victim->GetAttackedLogRef().dwAttackedTime < GET_ATTACK_SPEED(ch)) {
				INCREASE_SPEED_HACK_COUNT(ch);
				if (ch->GetSpeedHackCount() > 30) {
					ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "You %s have been disconnected for hacking.", ((ch)->GetName()));
					//std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE account.account SET status= 'BLOCK' WHERE id = %d", ch->GetDesc()->GetAccountTable().id));
					ch->GetDesc()->DelayedDisconnect(3);
				}
	
				SET_ATTACKED_TIME(ch, victim, current_time);
				return true;
			}
		}
	
		SET_ATTACKED_TIME(ch, victim, current_time);
		return false;
	}
	
	return false;
}
#endif



