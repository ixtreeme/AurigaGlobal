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
#include "ecs/EntityInvariants.hpp"
#include "ecs/components/character_runtime_components.hpp"
#include "ecs/components/combat_components.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include <algorithm>
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"

#include "db.h"
//#include <Database/DBManager.h>
int battle_hit(entt::entity attacker, entt::entity victim, int & iRetDam);
namespace {
bool IsBattleCharacter(entt::entity e)
{
    return ecs::Invariants::HasAnyTypeTag(g_registry, e);
}

bool IsBattlePair(entt::entity attacker, entt::entity victim)
{
    return IsBattleCharacter(attacker) && IsBattleCharacter(victim);
}

bool HasCharacterType(entt::entity e, uint8_t type)
{
    if (!IsBattleCharacter(e))
        return false;
    const auto* state = g_registry.try_get<ecs::CharacterType>(e);
    return state && state->value == type;
}
}


bool battle_distance_valid_by_xy(int32_t x, int32_t y, int32_t tx, int32_t ty)
{
	int32_t distance = DISTANCE_APPROX(x - tx, y - ty);

	if (distance > 170)
		return false;

	return true;
}

bool battle_distance_valid(entt::entity character, entt::entity victim)
{
    if (!IsBattlePair(character, victim))
        return false;
	return battle_distance_valid_by_xy(ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim));
}

bool timed_event_cancel(entt::entity character)
{
    using namespace ecs::PlayerRuntime;
    if (!IsBattleCharacter(character) || !GetCharEvent(character, CharEvent::Timed))
        return false;
    // Detach before publishing: a callback must not cancel a replacement timer.
    CancelCharEvent(character, CharEvent::Timed);
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 482, "");
#endif
    return true;
}

bool battle_is_attackable(entt::entity character, entt::entity victim)
{
    if (!IsBattlePair(character, victim) || character == victim)
        return false;
	// ���1aAI ��3�A��� �ߴ��N�U.
	if (CombatSystem::IsDead(victim))
	{
		return false;
	}


#ifdef ENABLE_BUG_FIXES
	if (ecs::SocialSystem::GetMyShop(victim))
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

	if (CArenaManager::instance().CanAttack(character, victim) == true)
		return true;

#ifdef __DEFENSE_WAVE__
	if (ecs::PlayerRuntime::GetRaceNum(victim) == 20434 && ecs::PlayerRuntime::IsMonster(character))
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
    if (!IsBattlePair(character, victim) || character == victim)
        return BATTLE_NONE;
#if defined(ENABLE_CHECK_BATTLE)
	if (ecs::PlayerRuntime::IsPC(character)) {
		const bool bAttacking = (get_dword_time() - CombatSystem::GetLastAttackTime(character)) < (MountSystem::IsRiding(character) ? 800 : 750);
		if (!bAttacking) {
			return BATTLE_NONE;
		}

	}
#endif

	if (test_server && ecs::PlayerRuntime::IsPC(character))
		LOG_TRACE("battle_melee_attack : [{}] attack to [{}]", ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetName(victim).data());

	if (!battle_is_attackable(character, victim))
	{
		return BATTLE_NONE;
	}

	// �A�� A1A�
	int distance = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(character) - ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(character) - ecs::PlayerRuntime::GetY(victim));

	if (!HasCharacterType(victim, CHAR_TYPE_BUILDING))
	{
		int max = 300;

		if (false == ecs::PlayerRuntime::IsPC(character))
		{
			// ��1oA�A� �a?i ��1oA� �o�� �A���� ��?�
			max = (int)(CombatSystem::GetMobAttackRange(character) * 1.15f);
		}
		else
		{
			// PCAI �a?i ���! melee ��AI �a?i ��A� �o�� �A���! Aִ� �o�� �A��
			if (false == ecs::PlayerRuntime::IsPC(victim) && BATTLE_TYPE_MELEE == CombatSystem::GetMobBattleType(victim))
				max = MAX(300, (int)(CombatSystem::GetMobAttackRange(victim) * 1.15f));
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

    if (!IsBattlePair(character, victim))
        return BATTLE_NONE;
	ecs::PlayerRuntime::SetPosition(character, POS_FIGHTING);
	CombatSystem::SetVictim(character, victim);

	ecs::MovementSystem::SetRotation(character, GetDegreeFromPositionXY(
        ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character),
        ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim)));

	int dam = 0;
	int ret = battle_hit(character, victim, dam);
	return (ret);
}


// ???? GET_BATTLE_VICTIM?? NULL?? ????? ???T?? j?? ??U??.
void battle_end_ex(entt::entity character)
{
	if (!IsBattleCharacter(character))
        return;
    const auto* state = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(character);
    if (state && state->position == POS_FIGHTING)
		ecs::PlayerRuntime::SetPosition(character, POS_STANDING);
}

void battle_end(entt::entity character)
{
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
    if (!IsBattlePair(attacker, victim))
        return 0;
	return CalcBattleDamage(iDam, ecs::PointSystem::GetLevel(attacker), ecs::PointSystem::GetLevel(victim));
}

int CalcMagicDamage(entt::entity attacker, entt::entity victim)
{
    if (!IsBattlePair(attacker, victim))
        return 0;
	int iDam = 0;

    // Legacy IsNPC meant every non-PC, including TagMonster and TagStone.
	if (!ecs::PlayerRuntime::IsPC(attacker))
	{
		iDam = CalcMeleeDamage(attacker, victim, false, false);
	}

	iDam += ecs::PointSystem::Get(attacker, POINT_PARTY_ATTACKER_BONUS);

	return CalcMagicDamageWithValue(iDam, attacker, victim);
}

float CalcAttackRating(entt::entity attacker, entt::entity victim, bool bIgnoreTargetRating)
{
    if (!IsBattlePair(attacker, victim))
        return 0;
	int iARSrc;
	int iERSrc;

	{
		int attacker_dx = ecs::PointSystem::GetPolymorphPoint(attacker, POINT_DX);
		int attacker_lv = ecs::PointSystem::GetLevel(attacker);

		int victim_dx = ecs::PointSystem::GetPolymorphPoint(victim, POINT_DX);
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
    if (!IsBattlePair(attacker, victim))
        return 0;
	// PvP???? ????????????
	if (!ecs::PlayerRuntime::IsPC(victim))
		iAtk += ecs::SocialSystem::GetMarriageBonus(attacker, UNIQUE_ITEM_MARRIAGE_ATTACK_BONUS);

	// PvP???? ????????????
	if (!ecs::PlayerRuntime::IsPC(attacker))
	{
		int iReduceDamagePct = ecs::SocialSystem::GetMarriageBonus(victim, UNIQUE_ITEM_MARRIAGE_TRANSFER_DAMAGE);
		iAtk = iAtk * (100 + iReduceDamagePct) / 100;
	}

	if (!ecs::PlayerRuntime::IsPC(attacker) && ecs::PlayerRuntime::IsPC(victim))
	{
		iAtk = (iAtk * CHARACTER_MANAGER::instance().GetMobDamageRate(attacker)) / 100;
	}

	if (ecs::PlayerRuntime::IsNPC(victim))
	{
#ifdef ENABLE_DS_RUNE
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_RUNE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_RUNE_MONSTERS)) / 100;
#endif
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_ANIMAL))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ANIMAL)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_UNDEAD))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_UNDEAD)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_DEVIL))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_DEVIL)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_HUMAN))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_HUMAN)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_ORC))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ORC)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_MILGYO))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_MILGYO)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_INSECT))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_INSECT)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_FIRE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_FIRE)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_ICE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ICE)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_DESERT))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_DESERT)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_TREE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_TREE)) / 100;
#ifdef ELEMENT_NEW_BONUSES
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_ATT_ELEC))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ELEC)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_ATT_FIRE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_FIRE)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_ATT_ICE))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_ICE)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_ATT_WIND))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_WIND)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_ATT_EARTH))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_EARTH)) / 100;
		if (ecs::PlayerRuntime::IsRaceFlag(victim, RACE_FLAG_ATT_DARK))
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_DARK)) / 100;
#endif
		if (HasCharacterType(victim, CHAR_TYPE_STONE)) {
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_METIN)) / 100;
		}
		else {
			if (ecs::PlayerRuntime::GetMobRank(victim) >= MOB_RANK_BOSS)
				iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_BOSS)) / 100;
		}

#ifdef ENABLE_NO_ATTBONUS_MONSTER_FOR_STONES
		if (!HasCharacterType(victim, CHAR_TYPE_STONE)) {
			iAtk += (iAtk * ecs::PointSystem::Get(attacker, POINT_ATTBONUS_MONSTER)) / 100;
		}
#else
#ifdef ENABLE_MAP1_SKILL_MOB__disable
		if (!(ecs::PlayerRuntime::IsMonster(victim) && ecs::PlayerRuntime::GetRaceNum(victim) == 136 && CombatSystem::IsSkillHit(attacker)))
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

		switch (ecs::PlayerRuntime::GetJob(victim))
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
		}
	}

	if (ecs::PlayerRuntime::IsPC(attacker) == true)
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		iAtk -= (iAtk * ecs::PointSystem::Get(victim, POINT_DEF_TALISMAN)) / 100;
#endif
		switch (ecs::PlayerRuntime::GetJob(attacker))
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
		}
	}

#ifdef ELEMENT_TARGET
	//[ mob -> PC ] ???? ??? ??? ????
	//2013/01/17
	//???? ??????? ???????? 30%?? ?????? ??g???? ?????? ?????.
	if (!ecs::PlayerRuntime::IsPC(attacker) && ecs::PlayerRuntime::IsPC(victim))
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_DEF_TALISMAN))		/ 10000;
#endif
		if (ecs::PlayerRuntime::IsRaceFlag(attacker, RACE_FLAG_ATT_ELEC))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_ELEC))		/ 10000;
		if (ecs::PlayerRuntime::IsRaceFlag(attacker, RACE_FLAG_ATT_FIRE))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_FIRE))		/ 10000;
		if (ecs::PlayerRuntime::IsRaceFlag(attacker, RACE_FLAG_ATT_ICE))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_ICE))		/ 10000;
		if (ecs::PlayerRuntime::IsRaceFlag(attacker, RACE_FLAG_ATT_WIND))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_WIND))		/ 10000;
		if (ecs::PlayerRuntime::IsRaceFlag(attacker, RACE_FLAG_ATT_EARTH))
			iAtk -= (iAtk * 30 * ecs::PointSystem::Get(victim, POINT_RESIST_EARTH))	/ 10000;
		if (ecs::PlayerRuntime::IsRaceFlag(attacker, RACE_FLAG_ATT_DARK))
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

	if (!ItemSystem::IsValidItem(item))
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
    if (!IsBattlePair(attacker, victim))
        return 0;
	const entt::entity weapon = ItemSystem::GetWearItem(attacker, WEAR_WEAPON);
	bool bPolymorphed = AffectSystem::IsPolymorphed(attacker);

	if (ItemSystem::IsValidItem(weapon) && !(bPolymorphed && !AffectSystem::IsPolyMaintainStat(attacker)))
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

	if (bPolymorphed && !AffectSystem::IsPolyMaintainStat(attacker))
	{
		// MONKEY_ROD_ATTACK_BUG_FIX
		Item_GetDamage(weapon, &iDamMin, &iDamMax);
		// END_OF_MONKEY_ROD_ATTACK_BUG_FIX

		uint32_t dwMobVnum = AffectSystem::GetPolymorphVnum(attacker);
		const CMob * pMob = CMobManager::instance().Get(dwMobVnum);

		if (pMob)
		{
			int iPower = AffectSystem::GetPolymorphPower(attacker);
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
	else if (!ecs::PlayerRuntime::IsPC(attacker))
	{
		iDamMin = CombatSystem::GetMobDamageMin(attacker);
		iDamMax = CombatSystem::GetMobDamageMax(attacker);
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
			iDef += ecs::SocialSystem::GetMarriageBonus(victim, UNIQUE_ITEM_MARRIAGE_DEFENSE_BONUS);
	}

	if (!ecs::PlayerRuntime::IsPC(attacker))
		iAtk = (int) (iAtk * CombatSystem::GetMobDamageMultiplier(attacker));

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

		if (!ecs::PlayerRuntime::IsPC(attacker))
		{
			snprintf(szGradeAtkBonus, sizeof(szGradeAtkBonus), "=%d*%.1f", DEBUG_iPureAtk, CombatSystem::GetMobDamageMultiplier(attacker));
			DEBUG_iPureAtk = int(DEBUG_iPureAtk * CombatSystem::GetMobDamageMultiplier(attacker));
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
    if (!IsBattlePair(attacker, victim) || !ItemSystem::IsValidItem(bow) || !ItemSystem::IsValidItem(arrow))
        return 0;
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

	if (!ecs::PlayerRuntime::IsPC(attacker))
		iAtk = (int) (iAtk * CombatSystem::GetMobDamageMultiplier(attacker));

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
    if (!IsBattlePair(attacker, victim))
        return;
	// ?? ?????? U?????? U?? �??
	if (ecs::PointSystem::Get(attacker, POINT_POISON_PCT) && !AffectSystem::IsAffectFlag(victim, AFF_POISON))
	{
		if (number(1, 100) <= ecs::PointSystem::Get(attacker, POINT_POISON_PCT))
			AffectSystem::ApplyPoison(victim, attacker);
	}
    if (!IsBattlePair(attacker, victim))
        return;
	int iStunDuration = 2;
	if (ecs::PlayerRuntime::IsPC(attacker) && !ecs::PlayerRuntime::IsPC(victim))
		iStunDuration = 4;

	AttackAffect(attacker, victim, POINT_STUN_PCT, IMMUNE_STUN,  AFFECT_STUN, POINT_NONE,        0, AFF_STUN, iStunDuration, "STUN");
    if (!IsBattlePair(attacker, victim))
        return;
	AttackAffect(attacker, victim, POINT_SLOW_PCT, IMMUNE_SLOW,  AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, 20,		"SLOW");
}

int battle_hit(entt::entity attacker, entt::entity victim, int & iRetDam)
{
    iRetDam = 0;
    if (!IsBattlePair(attacker, victim) || attacker == victim)
        return BATTLE_NONE;
#if defined(ENABLE_CHECK_BATTLE)
	if (ecs::PlayerRuntime::IsPC(attacker)) {
		const bool bAttacking = (get_dword_time() - CombatSystem::GetLastAttackTime(attacker)) < (MountSystem::IsRiding(attacker) ? 800 : 750);
		if (!bAttacking) {
			return BATTLE_NONE;
		}

	}
#endif

	//PROF_UNIT puHit("Hit");
	if (test_server)
		LOG_TRACE("battle_hit : [{}] attack to [{}] : dam :{}", ecs::PlayerRuntime::GetName(attacker).data(), ecs::PlayerRuntime::GetName(victim).data(), iRetDam);

	int iDam = CalcMeleeDamage(attacker, victim);

	if (iDam <= 0)
		return (BATTLE_DAMAGE);

	NormalAttackAffect(attacker, victim);
    if (!IsBattlePair(attacker, victim))
        return BATTLE_NONE;

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

			default:
				break;
		}


	//???????? ?????? ????. (2011?? 2?? ???? ???�????? ????.)
	float attMul = CombatSystem::GetAttackMultiplier(attacker);
	float tempIDam = iDam;
	iDam = attMul * tempIDam + 0.5f;

#ifdef ENABLE_SOUL_SYSTEM
    // Soul consumption is still a legacy operation. Resolve only at this
    // boundary, never retain the pointer across affect callbacks.
    if (ecs::PlayerRuntime::IsPC(attacker))
    {
        auto* legacyAttacker = ecs::LegacyCharOf(attacker);
        if (!legacyAttacker)
            return BATTLE_NONE;
        iDam += legacyAttacker->GetSoulItemDamage(victim, iDam, RED_SOUL);
    }
#endif

	iRetDam = iDam;

	//PROF_UNIT puDam("Dam");
    if (!IsBattlePair(attacker, victim))
        return BATTLE_NONE;
    // The complete Damage pipeline has not yet been migrated.
    auto* legacyVictim = ecs::LegacyCharOf(victim);
    if (!legacyVictim)
        return BATTLE_NONE;
	if (legacyVictim->Damage(attacker, iDam, DAMAGE_TYPE_NORMAL))
		return (BATTLE_DEAD);


	return (BATTLE_DAMAGE);
}

#ifdef ENABLE_ANTICHEAT
int32_t GET_ATTACK_SPEED(entt::entity character)
{
    if (!IsBattleCharacter(character))
        return 1000;
    const int64_t denominator = 100 + ecs::PointSystem::Get(character, POINT_ATT_SPEED) +
        (MountSystem::IsRiding(character) ? 50 : 0);
    if (denominator <= 0)
        return 1000;
    int64_t speed = static_cast<int64_t>(ani_attack_speed(character)) * 100 / denominator;
    const auto weapon = ItemSystem::GetWearItem(character, WEAR_WEAPON);
    if (ItemSystem::IsValidItem(weapon) && ItemSystem::GetItemSubType(weapon) == WEAPON_DAGGER)
        speed /= 2;
    return static_cast<int32_t>(std::clamp<int64_t>(speed, 0, INT32_MAX));
}

void SET_ATTACK_TIME(entt::entity character, entt::entity victim, int32_t current_time)
{
    if (!IsBattlePair(character, victim) || !ecs::PlayerRuntime::IsPC(character))
        return;
    auto& audit = g_registry.get_or_emplace<ecs::AttackAudit>(character);
    audit.target = victim;
    audit.attackTime = static_cast<uint32_t>(current_time);
}

void SET_ATTACKED_TIME(entt::entity character, entt::entity victim, int32_t current_time)
{
    if (!IsBattlePair(character, victim) || !ecs::PlayerRuntime::IsPC(character))
        return;
    auto& audit = g_registry.get_or_emplace<ecs::AttackAudit>(victim);
    audit.attacker = character;
    audit.attackedTime = static_cast<uint32_t>(current_time);
}

bool IS_SPEED_HACK(entt::entity character, entt::entity victim, int32_t current_time)
{
    if (!IsBattlePair(character, victim) || character == victim || !ecs::PlayerRuntime::IsPC(character))
        return false;
    const auto now = static_cast<uint32_t>(current_time);
    const auto limit = static_cast<uint32_t>(GET_ATTACK_SPEED(character));
    const auto* attack = g_registry.try_get<ecs::AttackAudit>(character);
    const bool attackHack = attack && attack->target == victim && now - attack->attackTime < limit;
    const auto* attacked = g_registry.try_get<ecs::AttackAudit>(victim);
    const bool attackedHack = !attackHack && attacked && attacked->attacker == character &&
        now - attacked->attackedTime < limit;
    // Commit both logs before any notification can invalidate a component.
    SET_ATTACK_TIME(character, victim, current_time);
    SET_ATTACKED_TIME(character, victim, current_time);
    if (!attackHack && !attackedHack)
        return false;
    auto& count = g_registry.get<ecs::AttackAudit>(character).speedHackCount;
    if (count < INT_MAX)
        ++count;
    const int countSnapshot = count;
    if (attackHack && test_server)
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s attack hack! hack_count %d",
            ecs::PlayerRuntime::GetName(character).data(), countSnapshot);
    if (attackedHack && countSnapshot > 30)
    {
        ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You %s have been disconnected for hacking.",
            ecs::PlayerRuntime::GetName(character).data());
        if (IsBattleCharacter(character))
            if (auto* descriptor = ecs::PlayerRuntime::GetDesc(character))
                descriptor->DelayedDisconnect(3);
    }
    return true;
}
#endif
