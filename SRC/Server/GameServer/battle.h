#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/AIHelpers.hpp"
#ifndef __INC_METIN_II_GAME_BATTLE_H__
#define __INC_METIN_II_GAME_BATTLE_H__

#include "char_interface.hpp"
#include <entt/entt.hpp>

enum EBattleTypes       // 상대방 기준
{
	BATTLE_NONE,
	BATTLE_DAMAGE,
	BATTLE_DEFENSE,
	BATTLE_DEAD
};

extern int	CalcAttBonus(entt::entity attacker, entt::entity victim, int iAtk);
extern int	CalcBattleDamage(int iDam, int iAttackerLev, int iVictimLev);
extern int	CalcMeleeDamage(entt::entity attacker, entt::entity victim, bool bIgnoreDefense = false, bool bIgnoreTargetRating = false);
extern int	CalcMagicDamage(entt::entity attacker, entt::entity victim);
extern int	CalcArrowDamage(entt::entity attacker, entt::entity victim, entt::entity bow, entt::entity arrow, bool bIgnoreDefense = false);
extern float	CalcAttackRating(entt::entity attacker, entt::entity victim, bool bIgnoreTargetRating = false);

extern bool	battle_is_attackable(entt::entity character, entt::entity victim);
extern int	battle_melee_attack(entt::entity character, entt::entity victim);
extern void	battle_end(entt::entity character);

extern bool	battle_distance_valid_by_xy(int32_t x, int32_t y, int32_t tx, int32_t ty);
extern bool	battle_distance_valid(entt::entity character, entt::entity victim);
extern int	battle_count_attackers(LPCHARACTER ch);

extern void	NormalAttackAffect(entt::entity attacker, entt::entity victim);

// 특성 공격
inline void AttackAffect(entt::entity attacker,
		entt::entity victim,
		uint8_t att_point,
		uint32_t immune_flag,
		uint32_t affect_idx,
		uint8_t affect_point,
	int32_t affect_amount,
		uint32_t affect_flag,
		int time,
		const char* name)
{
	if (ecs::PointSystem::Get(attacker, att_point) && !AffectSystem::IsAffectFlag(victim, affect_flag))
	{
		if (number(1, 100) <= ecs::PointSystem::Get(attacker, att_point) && !AffectSystem::IsImmune(victim, immune_flag))
		{
			AffectSystem::AddAffect(victim, affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

			if (test_server)
			{
				ecs::ChatSystem::Send(victim, CHAT_TYPE_PARTY, "%s %s(%ld%%) SUCCESS", ecs::PlayerRuntime::GetName(attacker).data(), name, ecs::PointSystem::Get(attacker, att_point));
			}
		}
		else if (test_server)
		{
			ecs::ChatSystem::Send(victim, CHAT_TYPE_PARTY, "%s %s(%ld%%) FAIL", ecs::PlayerRuntime::GetName(attacker).data(), name, ecs::PointSystem::Get(attacker, att_point));
		}
	}
}

inline void SkillAttackAffect(entt::entity victim,
		int success_pct,
		uint32_t immune_flag,
		uint32_t affect_idx,
		uint8_t affect_point,
	int32_t affect_amount,
		uint32_t affect_flag,
		int time,
		const char* name)
{
	if (success_pct && !AffectSystem::IsAffectFlag(victim, affect_flag))
	{
		if (number(1, 1000) <= success_pct && !AffectSystem::IsImmune(victim, immune_flag))
		{
			AffectSystem::AddAffect(victim, affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

			// SKILL_ATTACK_NO_LOG_TARGET_NAME_FIX
			if (test_server)
				ecs::ChatSystem::Send(victim, CHAT_TYPE_PARTY,
						"%s(%d%%) -> %s SUCCESS", name, success_pct, name);
			// END_OF_SKILL_ATTACK_LOG_NO_TARGET_NAME_FIX
		}
		else if (test_server)
		{
			// SKILL_ATTACK_NO_LOG_TARGET_NAME_FIX
			ecs::ChatSystem::Send(victim, CHAT_TYPE_PARTY, "%s(%d%%) -> %s FAIL", name, success_pct, name);
			// END_OF_SKILL_ATTACK_LOG_NO_TARGET_NAME_FIX
		}
	}
}

#ifdef ENABLE_ANTICHEAT
#define GET_SPEED_HACK_COUNT(ch)		((ch)->GetSpeedHackCount())
#define INCREASE_SPEED_HACK_COUNT(ch)	(++GET_SPEED_HACK_COUNT(ch))
int32_t GET_ATTACK_SPEED(entt::entity character);
void SET_ATTACK_TIME(entt::entity character, entt::entity victim, int32_t current_time);
void SET_ATTACKED_TIME(entt::entity character, entt::entity victim, int32_t current_time);
bool IS_SPEED_HACK(entt::entity character, entt::entity victim, int32_t current_time);
#endif
#endif

