#include "stdafx.h"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/SocialSystem.hpp"
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
int battle_hit(entt::entity attacker, entt::entity victim, int & iRetDam);

bool battle_distance_valid_by_xy(int32_t x, int32_t y, int32_t tx, int32_t ty)
{
	int32_t distance = DISTANCE_APPROX(x - tx, y - ty);

	if (distance > 170)
		return false;

	return true;
}

bool battle_distance_valid(entt::entity character, entt::entity victim)
{
	return battle_distance_valid_by_xy(ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim));
}

bool timed_event_cancel(entt::entity character)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ch->GetTimedEvent())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 482, "");
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

bool battle_is_attackable(entt::entity character, entt::entity victim)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	// ���1aAI ��3�A��� �ߴ��N�U.
	if (CombatSystem::IsDead(victim))
	{
		return false;
	}


#ifdef ENABLE_BUG_FIXES
	if (pkVictim->GetMyShop())
	{
		return false;
	}
#endif

	// 3EA������ �ߴ�
	{
		SECTREE* sectree = nullptr;

		sectree = ecs::PlayerRuntime::GetSectree(character);
		if (sectree && sectree->IsAttr(ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ATTR_BANPK))
		{
			return false;
		}

		sectree = ecs::PlayerRuntime::GetSectree(victim);
		if (sectree && sectree->IsAttr(ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim), ATTR_BANPK))
		{
			return false;
		}
	}

	// 3��! ��3�A��� �ߴ��N�U.
	if (CombatSystem::IsStun(character) || CombatSystem::IsDead(character))
	{
		return false;
	}

	if (ecs::PlayerRuntime::IsPC(character) && ecs::PlayerRuntime::IsPC(victim))
	{
		CGuild* g1 = ecs::SocialSystem::GetGuild(character);
		CGuild* g2 = ecs::SocialSystem::GetGuild(victim);

		if (g1 && g2)
		{
			if (g1->UnderWar(g2->GetID()))
				return true;
		}
	}

	if (CArenaManager::instance().CanAttack(ch, pkVictim) == true)
		return true;

#ifdef __DEFENSE_WAVE__
	if (ecs::PlayerRuntime::GetRaceNum(victim) == 20434 && ch->IsMonster())
	{
		return true;
	}
#endif

	bool bIsFarmMap = false;//razor93 2024.12.30
	switch (ecs::PlayerRuntime::GetMapIndex(character))
	{
	case 1:
	{
		if (ecs::PlayerRuntime::IsPC(victim) && ecs::PlayerRuntime::IsPC(character))
			bIsFarmMap = true;
	}
	break;
	}
	const bool canAttack = CPVPManager::instance().CanAttack(character, victim, bIsFarmMap);
	return canAttack;
}

int battle_melee_attack(entt::entity character, entt::entity victim)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
#if defined(ENABLE_CHECK_BATTLE)
	if (ecs::PlayerRuntime::IsPC(character) && pkVictim) {
		const bool bAttacking = (get_dword_time() - ch->GetLastAttackTime()) < (ch->IsRiding() ? 800 : 750);
		if (!bAttacking) {
			return BATTLE_NONE;
		}

		//ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Melee Attack: %d", get_dword_time() - ch->GetLastAttackTime());
		//		if (!battle_distance_valid(ch, victim)) {
		//			return BATTLE_NONE;
		//		}
	}
#endif

	if (test_server && ecs::PlayerRuntime::IsPC(character))
		LOG_TRACE("battle_melee_attack : [{}] attack to [{}]", ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetName(victim).data());

	if (!pkVictim || ch == pkVictim)
	{
		return BATTLE_NONE;
	}

	if (test_server && ecs::PlayerRuntime::IsPC(character))
		LOG_TRACE("battle_melee_attack : [{}] attack to [{}]", ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetName(victim).data());

	if (!battle_is_attackable(character, victim))
	{
		return BATTLE_NONE;
	}

	if (test_server && ecs::PlayerRuntime::IsPC(character))
		LOG_TRACE("battle_melee_attack : [{}] attack to [{}]", ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetName(victim).data());

	// �A�� A1A�
	int distance = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(character) - ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(character) - ecs::PlayerRuntime::GetY(victim));

	if (!pkVictim->IsBuilding())
	{
		int max = 300;

		if (false == ecs::PlayerRuntime::IsPC(character))
		{
			// ��1oA�A� �a?i ��1oA� �o�� �A���� ��?�
			max = (int)(ch->GetMobAttackRange() * 1.15f);
		}
		else
		{
			// PCAI �a?i ���! melee ��AI �a?i ��A� �o�� �A���! Aִ� �o�� �A��
			if (false == ecs::PlayerRuntime::IsPC(victim) && BATTLE_TYPE_MELEE == pkVictim->GetMobBattleType())
				max = MAX(300, (int)(pkVictim->GetMobAttackRange() * 1.15f));
		}

#ifdef __DEFENSE_WAVE__
		if (ecs::PlayerRuntime::IsPC(character) && (ecs::PlayerRuntime::GetRaceNum(victim) == 3960 || ecs::PlayerRuntime::GetRaceNum(victim) == 3961 || ecs::PlayerRuntime::GetRaceNum(victim) == 3962))
		{
			max += 400;
		}
#endif

		if (distance > max)
		{
			if (test_server)
				LOG_TRACE("VICTIM_FAR: {} distance: {} max: {}", ecs::PlayerRuntime::GetName(character).data(), distance, max);

			return BATTLE_NONE;
		}
	}

#ifdef TEXTS_IMPROVEMENT
	if (timed_event_cancel(character)) {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 456, "");
	}
	else if (timed_event_cancel(victim)) {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 456, "");
	}
#endif

	ch->SetPosition(POS_FIGHTING);
	ch->SetVictim(pkVictim);

	const PIXEL_POSITION& vpos = pkVictim->GetXYZ();
	ch->SetRotationToXY(vpos.x, vpos.y);

	int dam;
	int ret = battle_hit(character, victim, dam);
	return (ret);
}


// ???? GET_BATTLE_VICTIM?? NULL?? ????? ???T?? j?? ??U??.
void battle_end_ex(entt::entity character)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ch->IsPosition(POS_FIGHTING))
		ch->SetPosition(POS_STANDING);
}

void battle_end(entt::entity character)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	battle_end_ex(character);
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

int CalcMagicDamageWithValue(int iDam, entt::entity attacker, entt::entity victim)
{
	return CalcBattleDamage(iDam, ecs::PointSystem::GetLevel(attacker), ecs::PointSystem::GetLevel(victim));
}

int CalcMagicDamage(entt::entity attacker, entt::entity victim)
{
	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	int iDam = 0;

	if (ecs::PlayerRuntime::IsNPC(attacker))
	{
		iDam = CalcMeleeDamage(attacker, victim, false, false);
	}

	iDam += ecs::PointSystem::Get(attacker, POINT_PARTY_ATTACKER_BONUS);

	return CalcMagicDamageWithValue(iDam, attacker, victim);
}

float CalcAttackRating(entt::entity attacker, entt::entity victim, bool bIgnoreTargetRating)
{
	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	int iARSrc;
	int iERSrc;

	{
		int attacker_dx = pkAttacker->GetPolymorphPoint(POINT_DX);
		int attacker_lv = ecs::PointSystem::GetLevel(attacker);

		int victim_dx = pkVictim->GetPolymorphPoint(POINT_DX);
		int victim_lv = ecs::PointSystem::GetLevel(attacker);

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

int CalcAttBonus(entt::entity attacker, entt::entity victim, int iAtk)
{
	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	// PvP???? ????????????
	if (!ecs::PlayerRuntime::IsPC(victim))
		iAtk += pkAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_ATTACK_BONUS);

	// PvP???? ????????????
	if (!ecs::PlayerRuntime::IsPC(attacker))
	{
		int iReduceDamagePct = pkVictim->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_TRANSFER_DAMAGE);
		iAtk = iAtk * (100 + iReduceDamagePct) / 100;
	}

	if (ecs::PlayerRuntime::IsNPC(attacker) && ecs::PlayerRuntime::IsPC(victim))
	{
		iAtk = (iAtk * CHARACTER_MANAGER::instance().GetMobDamageRate(attacker)) / 100;
	}

	if (ecs::PlayerRuntime::IsNPC(victim))
	{
#ifdef ENABLE_DS_RUNE
		if (pkVictim->IsRaceFlag(RACE_FLAG_RUNE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_RUNE_MONSTERS)) / 100;
#endif
		if (pkVictim->IsRaceFlag(RACE_FLAG_ANIMAL))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ANIMAL)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_UNDEAD))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_UNDEAD)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_DEVIL))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_DEVIL)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_HUMAN))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_HUMAN)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ORC))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ORC)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_MILGYO))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_MILGYO)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_INSECT))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_INSECT)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_FIRE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_FIRE)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ICE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ICE)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_DESERT))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_DESERT)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_TREE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_TREE)) / 100;
#ifdef ELEMENT_NEW_BONUSES
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_ELEC))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ELEC)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_FIRE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_FIRE)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_ICE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ICE)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_WIND))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_WIND)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_EARTH))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_EARTH)) / 100;
		if (pkVictim->IsRaceFlag(RACE_FLAG_ATT_DARK))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_DARK)) / 100;
#endif
		if (pkVictim->GetCharType() == CHAR_TYPE_STONE) {
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_METIN)) / 100;
		}
		else {
			if (pkVictim->GetMobRank() >= MOB_RANK_BOSS)
				iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_BOSS)) / 100;
		}

#ifdef ENABLE_NO_ATTBONUS_MONSTER_FOR_STONES
		if (pkVictim->GetCharType() != CHAR_TYPE_STONE) {
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_MONSTER)) / 100;
		}
#else
#ifdef ENABLE_MAP1_SKILL_MOB__disable
		if (!(pkVictim && pkVictim->IsMonster() && ecs::PlayerRuntime::GetRaceNum(victim) == 136 && pkAttacker->IsSkillHit()))
		{
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_MONSTER)) / 100;
		}
#else
		iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_MONSTER)) / 100;
#endif

#endif
	}
	else if (ecs::PlayerRuntime::IsPC(victim))
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		{
			const int A = ecs::PointSystem::Get(attacker, POINT_ATTBONUS_HUMAN);            // tamado bonusz (%)
			const int R = ecs::PointSystem::Get(victim, POINT_RESIST_MEZZIUOMINI);          // vedekezo resist (%)

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

	iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_HUMAN)) / 100;
#endif

		switch (pkVictim->GetJob())
		{
			case JOB_WARRIOR:
				iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_WARRIOR)) / 100;
				break;

			case JOB_ASSASSIN:
				iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ASSASSIN)) / 100;
				break;

			case JOB_SURA:
				iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_SURA)) / 100;
				break;

			case JOB_SHAMAN:
				iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_SHAMAN)) / 100;
				break;
#ifdef ENABLE_WOLFMAN_CHARACTER
			case JOB_WOLFMAN: // TODO: ?????? ATTBONUS �??
				iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_WOLFMAN)) / 100;
				break;
#endif
		}
	}

	if (ecs::PlayerRuntime::IsPC(attacker) == true)
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		iAtk -= (iAtk * ecs::PointSystem::Get(victim, POINT_DEF_TALISMAN)) / 100;
#endif
		switch (pkAttacker->GetJob())
		{
			case JOB_WARRIOR:
				iAtk -= (iAtk * ecs::PointSystem::Get(victim, POINT_RESIST_WARRIOR)) / 100;
				break;

			case JOB_ASSASSIN:
				iAtk -= (iAtk * ecs::PointSystem::Get(victim, POINT_RESIST_ASSASSIN)) / 100;
				break;

			case JOB_SURA:
				iAtk -= (iAtk * ecs::PointSystem::Get(victim, POINT_RESIST_SURA)) / 100;
				break;

			case JOB_SHAMAN:
				iAtk -= (iAtk * ecs::PointSystem::Get(victim, POINT_RESIST_SHAMAN)) / 100;
				break;
#ifdef ENABLE_WOLFMAN_CHARACTER
			case JOB_WOLFMAN: // TODO: ?????? ???? �??
				iAtk -= (iAtk * ecs::PointSystem::Get(victim, POINT_RESIST_WOLFMAN)) / 100;
				break;
#endif
		}
	}

#ifdef ELEMENT_TARGET
	//[ mob -> PC ] ???? ??? ??? ????
	//2013/01/17
	//???? ??????? ???????? 30%?? ?????? ??g???? ?????? ?????.
	if (ecs::PlayerRuntime::IsNPC(attacker) && ecs::PlayerRuntime::IsPC(victim))
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_DEF_TALISMAN))		/ 10000;
#endif
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_ELEC))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_ELEC))		/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_FIRE))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_FIRE))		/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_ICE))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_ICE))		/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_WIND))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_WIND))		/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_EARTH))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_EARTH))	/ 10000;
		if (pkAttacker->IsRaceFlag(RACE_FLAG_ATT_DARK))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_DARK))		/ 10000;//difesa
#endif
#ifdef ENABLE_RESIST_MONSTER
		iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_MONSTER))		/ 10000;//resistenza mostri
#endif
	}

	return iAtk;
}

void Item_GetDamage(entt::entity item, int* pdamMin, int* pdamMax)
{
	*pdamMin = 0;
	*pdamMax = 1;

	if (item == entt::null)
		return;

	switch (ItemSystem::GetItemType(item))
	{
		case ITEM_ROD:
		case ITEM_PICK:
			return;
	}

	if (ItemSystem::GetItemType(item) != ITEM_WEAPON)
		LOG_ERROR("Item_GetDamage - !ITEM_WEAPON vnum={}, type={}", ItemSystem::GetItemOriginalVnum(item), static_cast<int>(ItemSystem::GetItemType(item)));

	*pdamMin = ItemSystem::GetItemValue(item, 3);
	*pdamMax = ItemSystem::GetItemValue(item, 4);
}

int CalcMeleeDamage(entt::entity attacker, entt::entity victim, bool bIgnoreDefense, bool bIgnoreTargetRating)
{
	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	const entt::entity weapon = ItemSystem::GetWearItem(attacker, WEAR_WEAPON);
	bool bPolymorphed = pkAttacker->IsPolymorphed();

	if (ItemSystem::IsValidItem(weapon) && !(bPolymorphed && !pkAttacker->IsPolyMaintainStat()))
	{
		if (ItemSystem::GetItemType(weapon) != ITEM_WEAPON)
			return 0;

		switch (ItemSystem::GetItemSubType(weapon))
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
				LOG_ERROR("CalcMeleeDamage should not handle bows (name: {})", ecs::PlayerRuntime::GetName(attacker).data());
				return 0;

			default:
				return 0;
		}
	}

	int iDam = 0;
	float fAR = CalcAttackRating(attacker, victim, bIgnoreTargetRating);
	int iDamMin = 0, iDamMax = 0;

	// TESTSERVER_SHOW_ATTACKINFO
	int DEBUG_iDamCur = 0;
	int DEBUG_iDamBonus = 0;
	// END_OF_TESTSERVER_SHOW_ATTACKINFO

	if (bPolymorphed && !pkAttacker->IsPolyMaintainStat())
	{
		// MONKEY_ROD_ATTACK_BUG_FIX
		Item_GetDamage(weapon, &iDamMin, &iDamMax);
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
	else if (ItemSystem::IsValidItem(weapon))
	{
		// MONKEY_ROD_ATTACK_BUG_FIX
		Item_GetDamage(weapon, &iDamMin, &iDamMax);
		// END_OF_MONKEY_ROD_ATTACK_BUG_FIX
	}
	else if (ecs::PlayerRuntime::IsNPC(attacker))
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
	iAtk = ecs::PointSystem::Get(attacker, POINT_ATT_GRADE) + iDam - (ecs::PointSystem::GetLevel(attacker) * 2);
	iAtk = (int) (iAtk * fAR);
	iAtk += ecs::PointSystem::GetLevel(attacker) * 2; // and add again

	if (ItemSystem::IsValidItem(weapon))
	{
		iAtk += ItemSystem::GetItemValue(weapon, 5) * 2;

		// 2004.11.12.myevan.TESTSERVER_SHOW_ATTACKINFO
		DEBUG_iDamBonus = ItemSystem::GetItemValue(weapon, 5) * 2;
		///////////////////////////////////////////////
	}

	iAtk += ecs::PointSystem::Get(attacker, POINT_PARTY_ATTACKER_BONUS); // party attacker role bonus
	iAtk = (int) (iAtk * (100 + (ecs::PointSystem::Get(attacker, POINT_ATT_BONUS) + ecs::PointSystem::Get(attacker, POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100);

	iAtk = CalcAttBonus(attacker, victim, iAtk);

	int iDef = 0;

	if (!bIgnoreDefense)
	{
		iDef = (ecs::PointSystem::Get(victim, POINT_DEF_GRADE) * (100 + ecs::PointSystem::Get(victim, POINT_DEF_BONUS)) / 100);

		if (!ecs::PlayerRuntime::IsPC(attacker))
			iDef += pkVictim->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_DEFENSE_BONUS);
	}

	if (ecs::PlayerRuntime::IsNPC(attacker))
		iAtk = (int) (iAtk * pkAttacker->GetMobDamageMultiply());

	iDam = MAX(0, iAtk - iDef);

	if (test_server)
	{
		int DEBUG_iLV = ecs::PointSystem::GetLevel(attacker)*2;
		int DEBUG_iST = int((ecs::PointSystem::Get(attacker, POINT_ATT_GRADE) - DEBUG_iLV) * fAR);
		int DEBUG_iPT = ecs::PointSystem::Get(attacker, POINT_PARTY_ATTACKER_BONUS);
		int DEBUG_iWP = 0;
		int DEBUG_iPureAtk = 0;
		int DEBUG_iPureDam = 0;
		char szRB[32] = "";
		char szGradeAtkBonus[32] = "";

		DEBUG_iWP = int(DEBUG_iDamCur * fAR);
		DEBUG_iPureAtk = DEBUG_iLV + DEBUG_iST + DEBUG_iWP+DEBUG_iDamBonus;
		DEBUG_iPureDam = iAtk - iDef;

		if (ecs::PlayerRuntime::IsNPC(attacker))
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
				ecs::PlayerRuntime::GetName(attacker).data(),
				iAtk,
				ecs::PlayerRuntime::GetName(victim).data(),
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

		ecs::ChatSystem::Send(attacker, CHAT_TYPE_TALKING, "%s", szMeleeAttack);
		ecs::ChatSystem::Send(victim, CHAT_TYPE_TALKING, "%s", szMeleeAttack);
	}

	return CalcBattleDamage(iDam, ecs::PointSystem::GetLevel(attacker), ecs::PointSystem::GetLevel(victim));
}

int CalcArrowDamage(entt::entity attacker, entt::entity victim, entt::entity bow, entt::entity arrow, bool bIgnoreDefense)
{
	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	if (bow == entt::null || ItemSystem::GetItemType(bow) != ITEM_WEAPON || ItemSystem::GetItemSubType(bow) != WEAPON_BOW)
		return 0;

	if (arrow == entt::null)
		return 0;

	// Y??g ????
	int iDist = (int) (DISTANCE_SQRT(ecs::PlayerRuntime::GetX(attacker) - ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(attacker) - ecs::PlayerRuntime::GetY(victim)));
	//int iGap = (iDist / 100) - 5 - ItemSystem::GetItemValue((pkBow ? pkBow->GetEntityHandle() : entt::null), 5) - ecs::PointSystem::Get(attacker, POINT_BOW_DISTANCE);
	int iGap = (iDist / 100) - 5 - ecs::PointSystem::Get(attacker, POINT_BOW_DISTANCE);
	int iPercent = 100 - (iGap * 5);

	if (iPercent <= 0)
		return 0;
	else if (iPercent > 100)
		iPercent = 100;

	int iDam = 0;

	float fAR = CalcAttackRating(attacker, victim, false);
	iDam = number(ItemSystem::GetItemValue(bow, 3), ItemSystem::GetItemValue(bow, 4)) * 2 + ItemSystem::GetItemValue(arrow, 3);
	int iAtk;

	// level must be ignored when multiply by fAR, so subtract it before calculation.
	iAtk = ecs::PointSystem::Get(attacker, POINT_ATT_GRADE) + iDam - (ecs::PointSystem::GetLevel(attacker) * 2);
	iAtk = (int) (iAtk * fAR);
	iAtk += ecs::PointSystem::GetLevel(attacker) * 2; // and add again

	// Refine Grade
	iAtk += ItemSystem::GetItemValue(bow, 5) * 2;

	iAtk += ecs::PointSystem::Get(attacker, POINT_PARTY_ATTACKER_BONUS);
	iAtk = (int) (iAtk * (100 + (ecs::PointSystem::Get(attacker, POINT_ATT_BONUS) + ecs::PointSystem::Get(attacker, POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100);

	iAtk = CalcAttBonus(attacker, victim, iAtk);

	int iDef = 0;

	if (!bIgnoreDefense)
		iDef = (ecs::PointSystem::Get(victim, POINT_DEF_GRADE) * (100 + ecs::PointSystem::Get(attacker, POINT_DEF_BONUS)) / 100);

	if (ecs::PlayerRuntime::IsNPC(attacker))
		iAtk = (int) (iAtk * pkAttacker->GetMobDamageMultiply());

	iDam = MAX(0, iAtk - iDef);

	int iPureDam = iDam;

	iPureDam = (iPureDam * iPercent) / 100;

	if (test_server)
	{
		ecs::ChatSystem::Send(attacker, CHAT_TYPE_INFO, "ARROW %s -> %s, DAM %d DIST %d GAP %d %% %d",
				ecs::PlayerRuntime::GetName(attacker).data(),
				ecs::PlayerRuntime::GetName(victim).data(),
				iPureDam,
				iDist, iGap, iPercent);
	}

	return iPureDam;
	//return iDam;
}


void NormalAttackAffect(entt::entity attacker, entt::entity victim)
{
	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	// ?? ?????? U?????? U?? �??
	if (ecs::PointSystem::Get(attacker, POINT_POISON_PCT) && !AffectSystem::IsAffectFlag(victim, AFF_POISON))
	{
		if (number(1, 100) <= ecs::PointSystem::Get(attacker, POINT_POISON_PCT))
			pkVictim->AttackedByPoison((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null));
	}
#ifdef ENABLE_WOLFMAN_CHARACTER
	if (ecs::PointSystem::Get(attacker, POINT_BLEEDING_PCT) && !AffectSystem::IsAffectFlag(victim, AFF_BLEEDING))
	{
		if (number(1, 100) <= ecs::PointSystem::Get(attacker, POINT_BLEEDING_PCT))
			pkVictim->AttackedByBleeding((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null));
	}
#endif
	int iStunDuration = 2;
	if (ecs::PlayerRuntime::IsPC(attacker) && !ecs::PlayerRuntime::IsPC(victim))
		iStunDuration = 4;

	AttackAffect(attacker, victim, POINT_STUN_PCT, IMMUNE_STUN,  AFFECT_STUN, POINT_NONE,        0, AFF_STUN, iStunDuration, "STUN");
	AttackAffect(attacker, victim, POINT_SLOW_PCT, IMMUNE_SLOW,  AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, 20,		"SLOW");
}

int battle_hit(entt::entity attacker, entt::entity victim, int & iRetDam)
{
	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
#if defined(ENABLE_CHECK_BATTLE)
	if (ecs::PlayerRuntime::IsPC(attacker) && pkVictim) {
		const bool bAttacking = (get_dword_time() - pkAttacker->GetLastAttackTime()) < (pkAttacker->IsRiding() ? 800 : 750);
		if (!bAttacking) {
			return BATTLE_NONE;
		}

//ecs::ChatSystem::Send(attacker, CHAT_TYPE_INFO, "Melee Attack: %d", get_dword_time() - pkAttacker->GetLastAttackTime());
//		if (!battle_distance_valid(pkAttacker, pkVictim)) {
//			return BATTLE_NONE;
//		}
	}
#endif

	//PROF_UNIT puHit("Hit");
	if (test_server)
		LOG_TRACE("battle_hit : [{}] attack to [{}] : dam :{}", ecs::PlayerRuntime::GetName(attacker).data(), ecs::PlayerRuntime::GetName(victim).data(), iRetDam);

	int iDam = CalcMeleeDamage(attacker, victim);

	if (iDam <= 0)
		return (BATTLE_DAMAGE);

	NormalAttackAffect(attacker, victim);

	// ?????? ???
	//iDam = iDam * (100 - ecs::PointSystem::Get(victim, POINT_RESIST)) / 100;
	const entt::entity weapon = ItemSystem::GetWearItem(
		attacker, WEAR_WEAPON);

	if (ItemSystem::IsValidItem(weapon))
		switch (ItemSystem::GetItemSubType(weapon))
		{
			case WEAPON_SWORD:
			{
				int32_t lValue = ecs::PointSystem::Get(victim, POINT_RESIST_SWORD);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_SPADA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}
			case WEAPON_TWO_HANDED:
			{
				int32_t lValue = ecs::PointSystem::Get(victim, POINT_RESIST_TWOHAND);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_SPADONE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}
			case WEAPON_DAGGER:
			{
				int32_t lValue = ecs::PointSystem::Get(victim, POINT_RESIST_DAGGER);

#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_PUGNALE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif


				//if (ecs::PlayerRuntime::IsPC(attacker) && ecs::PlayerRuntime::IsPC(victim))
				//	lValue += 15;

				// clamp 0..100
				if (lValue < 0)   lValue = 0;
				if (lValue > 100) lValue = 100;

				iDam = iDam * (100 - lValue) / 100;
				break;
			}


			case WEAPON_BELL:
			{
				int32_t lValue = ecs::PointSystem::Get(victim, POINT_RESIST_BELL);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_CAMPANA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif

				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}
			case WEAPON_FAN:
			{
				int32_t lValue = ecs::PointSystem::Get(victim, POINT_RESIST_FAN);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_VENTAGLIO);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}

			case WEAPON_BOW:
			{
				int32_t lValue = ecs::PointSystem::Get(victim, POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif
				lValue = lValue < 0 ? 0 :  lValue;
				iDam = iDam * (100 - lValue) / 100;
				break;
			}

#ifdef ENABLE_WOLFMAN_CHARACTER
			case WEAPON_CLAW:
			{
				int32_t lValue = ecs::PointSystem::Get(victim, POINT_RESIST_DAGGER);
#ifdef ENABLE_NEW_BONUS_TALISMAN
				lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_PUGNALE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
				lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
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
//	if (ecs::PlayerRuntime::IsPC(attacker) /*&& pkAttacker->IsSkillHit()*/
//		&& ecs::PlayerRuntime::GetRaceNum(victim) == 136)
//	{
//		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
//			"UPDATE player.player "
//			"SET map1_skillmob = GREATEST(map1_skillmob, %d) "
//			"WHERE id=%u",
//			iRetDam, ecs::PlayerRuntime::GetPlayerID(attacker)));
//
//		///*pkAttacker->*/viChatPacket(CHAT_TYPE_TALKING, "You hit a Skill Mob for %d damage!", iRetDam);
//		ecs::ChatSystem::Send(attacker, CHAT_TYPE_INFO, "You hit a Skill Mob for %d damage!", iRetDam);
//
//
//
//
//		LOG_TRACE("DEBUG MAP1_SKILL_MOB: attacker={} (id={}) victimVnum={} dmg={} skillhit={}",
//			ecs::PlayerRuntime::GetName(attacker).data(),
//			ecs::PlayerRuntime::GetPlayerID(attacker),
//			ecs::PlayerRuntime::GetRaceNum(victim),
//			iRetDam,
//			pkAttacker->IsSkillHit());
//
//	}
//#endif

	return (BATTLE_DAMAGE);
}

#ifdef ENABLE_ANTICHEAT
int32_t GET_ATTACK_SPEED(entt::entity character) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch) {
		return 1000;
	}

	int32_t default_bonus = 100;
	int32_t riding_bonus = ch->IsRiding() ? 50 : 0;
	int32_t ani_speed = ani_attack_speed(ch);
	int32_t real_speed = (ani_speed * 100) / (default_bonus + ecs::PointSystem::Get(character, POINT_ATT_SPEED) + riding_bonus);

	const entt::entity item = ItemSystem::GetWearItem(character, WEAR_WEAPON);
	return ItemSystem::IsValidItem(item) && ItemSystem::GetItemSubType(item) == WEAPON_DAGGER
		? real_speed / 2
		: real_speed;
}

void SET_ATTACK_TIME(entt::entity character, entt::entity victim, int32_t current_time) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	if (pkVictim && ch && ecs::PlayerRuntime::IsPC(character)) {
		ch->GetAttackLogRef().dwVID = ecs::PlayerRuntime::GetPacketVID(victim);
		ch->GetAttackLogRef().dwTime = current_time;
	}
}

void SET_ATTACKED_TIME(entt::entity character, entt::entity victim, int32_t current_time) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	if (pkVictim && ch && ecs::PlayerRuntime::IsPC(character)) {
		pkVictim->GetAttackedLogRef().dwPID = (ecs::PlayerRuntime::GetPlayerID(character));
		pkVictim->GetAttackedLogRef().dwAttackedTime = current_time;
	}
}

bool IS_SPEED_HACK(entt::entity character, entt::entity victim, int32_t current_time) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	LPCHARACTER pkVictim = ecs::LegacyCharOf(victim);
	if (pkVictim && ch && ecs::PlayerRuntime::IsPC(character)) {
		if (ch->GetAttackLogRef().dwVID == ecs::PlayerRuntime::GetPacketVID(victim))
		{
			if (current_time - ch->GetAttackLogRef().dwTime < GET_ATTACK_SPEED(character))
			{
				INCREASE_SPEED_HACK_COUNT(ch);

				if (test_server)
				{
					LOG_TRACE("{} attack hack! time (delta, limit)=({}, {}) hack_count {}", ecs::PlayerRuntime::GetName(character).data(), current_time - ch->GetAttackLogRef().dwTime, GET_ATTACK_SPEED(character), ch->GetSpeedHackCount());

					ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s attack hack! time (delta, limit)=(%u, %u) hack_count %d",
							ecs::PlayerRuntime::GetName(character).data(),
							current_time - ch->GetAttackLogRef().dwTime,
							GET_ATTACK_SPEED(character),
							ch->GetSpeedHackCount());
				}

				SET_ATTACK_TIME(character, victim, current_time);
				SET_ATTACKED_TIME(character, victim, current_time);
				return true;
			}
		}

		SET_ATTACK_TIME(character, victim, current_time);

		if (pkVictim->GetAttackedLogRef().dwPID == (ecs::PlayerRuntime::GetPlayerID(character))) {
			if (current_time - pkVictim->GetAttackedLogRef().dwAttackedTime < GET_ATTACK_SPEED(character)) {
				INCREASE_SPEED_HACK_COUNT(ch);
				if (ch->GetSpeedHackCount() > 30) {
					ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You %s have been disconnected for hacking.", ecs::PlayerRuntime::GetName(character).data());
					//std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE account.account SET status= 'BLOCK' WHERE id = %d", ecs::PlayerRuntime::GetDesc(character)->GetAccountTable().id));
					ecs::PlayerRuntime::GetDesc(character)->DelayedDisconnect(3);
				}

				SET_ATTACKED_TIME(character, victim, current_time);
				return true;
			}
		}

		SET_ATTACKED_TIME(character, victim, current_time);
		return false;
	}

	return false;
}
#endif



