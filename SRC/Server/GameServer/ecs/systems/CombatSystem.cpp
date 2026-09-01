#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"
#include "AffectSystem.hpp"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"
#include "QuestSystem.hpp"
#include "NetworkSyncSystem.hpp"

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
#include "../EntityFactory.hpp"
#include "../EntityInvariants.hpp"
#include "../Registry.hpp"
#include "../NetworkService.hpp"
#include "../VIDRegistry.hpp"
#include "ItemSystem.hpp"
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
#include <Core/Logging.hpp>
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

static inline ecs::CharacterRuntimeFlagsComponent* RuntimeFlags(entt::entity character)
{
    return ecs::TryGetRuntimeFlags(character);
}

static inline bool HasMoveState(entt::entity character)
{
    return character != entt::null && g_registry.valid(character) &&
        g_registry.all_of<ecs::MovementDestination>(character);
}

namespace CombatSystem {

void SetComboSequence(entt::entity e, uint8_t sequence)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	g_registry.get_or_emplace<ecs::AttackCooldown>(e).comboSequence = sequence;
}

uint8_t GetComboSequence(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	const auto* cooldown = g_registry.try_get<ecs::AttackCooldown>(e);
	return cooldown ? cooldown->comboSequence : 0;
}

void SetLastComboTime(entt::entity e, uint32_t time)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	g_registry.get_or_emplace<ecs::AttackCooldown>(e).lastComboTime = time;
}

uint32_t GetLastComboTime(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	const auto* cooldown = g_registry.try_get<ecs::AttackCooldown>(e);
	return cooldown ? cooldown->lastComboTime : 0;
}

void SetValidComboInterval(entt::entity e, int interval)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	g_registry.get_or_emplace<ecs::AttackCooldown>(e).validComboInterval = interval;
}

int GetValidComboInterval(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	const auto* cooldown = g_registry.try_get<ecs::AttackCooldown>(e);
	return cooldown ? cooldown->validComboInterval : 0;
}

uint8_t GetComboIndex(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	const auto* cooldown = g_registry.try_get<ecs::AttackCooldown>(e);
	return cooldown ? cooldown->comboIndex : 0;
}

uint8_t ToggleComboIndex(entt::entity e, uint8_t skillLevel)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	auto& cooldown = g_registry.get_or_emplace<ecs::AttackCooldown>(e);
	cooldown.comboIndex = cooldown.comboIndex ? 0 : skillLevel;
	return cooldown.comboIndex;
}

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
    ecs::Invariants::ValidateCharacterTags(g_registry, attacker, "combat.attack.attacker");
    ecs::Invariants::ValidateCharacterTags(g_registry, victim, "combat.attack.victim");
    ecs::Invariants::ValidateCommonIdentity(g_registry, attacker, "combat.attack.attacker");
    ecs::Invariants::ValidateCommonIdentity(g_registry, victim, "combat.attack.victim");

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
        auto* victim = ch->GetVictim();
        return victim ? victim->GetEntityHandle() : entt::null;
    }

    return entt::null;
}

entt::entity GetNearestVictim(entt::entity attacker, entt::entity from)
{
    if (auto* ch = LegacyCharOf(attacker)) {
        auto* victim = ch->GetNearestVictim(LegacyCharOf(from));
        return victim ? victim->GetEntityHandle() : entt::null;
    }

    return entt::null;
}

bool IsStun(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;

    if (g_registry.all_of<ecs::StunTag>(e))
        return true;

    const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
    if (status && status->isStunned)
        return true;

    const auto* runtime = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
    return runtime && IS_SET(runtime->instantFlag, INSTANT_FLAG_STUN);
}

void Stun(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e)) {
        ch->Stun();
    }
}

bool IsDead(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return true;

    if (g_registry.all_of<ecs::DeadTag>(e))
        return true;

    const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
    if (status && status->isDead)
        return true;

    const auto* runtime = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
    return runtime && runtime->position == POS_DEAD;
}

bool Damage(entt::entity target, entt::entity attacker, int damage, uint8_t damageType)
{
    auto* legacyTarget = LegacyCharOf(target);
    if (!legacyTarget)
        return false;

    return legacyTarget->Damage(
        LegacyCharOf(attacker), damage, static_cast<EDamageType>(damageType));
}

void Dead(entt::entity victim, entt::entity killer, bool immediate)
{
    // Compatibility boundary until the complete death pipeline is component-native.
    if (auto* legacyVictim = LegacyCharOf(victim))
        legacyVictim->Dead(LegacyCharOf(killer), immediate);
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

float GetAttackMultiplier(entt::entity e)
{
    if (auto* character = LegacyCharOf(e))
        return character->GetAttMul();
    return 1.0f;
}

void SetAttackMultiplier(entt::entity e, float multiplier)
{
    if (auto* character = LegacyCharOf(e))
        character->SetAttMul(multiplier);
}

float GetDamageMultiplier(entt::entity e)
{
    if (auto* character = LegacyCharOf(e))
        return character->GetDamMul();
    return 1.0f;
}

void SetDamageMultiplier(entt::entity e, float multiplier)
{
    if (auto* character = LegacyCharOf(e))
        character->SetDamMul(multiplier);
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

	m_iAlignment = m_iRealAlignment;
	const uint32_t oldVisibleAlignment = m_iAlignment / 10;

	m_iRealAlignment = UMINMAX(0, m_iRealAlignment + iAmount, 50000000);
	m_iAlignment = m_iRealAlignment;

	const uint8_t newGrade = GetAlignmentGrade();
	if (oldGrade != newGrade)
	{
		ComputePoints(); // ekkor vltozik a cache + jraplnek pontok
	}
	if (auto* combat = g_registry.try_get<ecs::CombatStats>(GetEntityHandle())) {
		combat->alignment = m_iAlignment;
		combat->realAlignment = m_iRealAlignment;
		g_registry.emplace_or_replace<ecs::DirtyTag>(GetEntityHandle());
	}

	if (oldVisibleAlignment != m_iAlignment / 10)
		NetworkSyncSystem::BroadcastCharAdditionalInfo(g_registry, GetEntityHandle());

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
	// B.1.5 + C.4: read and write via ECS StatusFlags.isKillerMode.
	if ((isOn ? ADD_CHARACTER_STATE_KILLER : 0) == IS_SET(GetAddChrStateFlag(), ADD_CHARACTER_STATE_KILLER))
		return;

	if (auto* status = g_registry.try_get<ecs::StatusFlags>(GetEntityHandle())) {
		status->isKillerMode = isOn;
		g_registry.emplace_or_replace<ecs::DirtyTag>(GetEntityHandle());
	}

	m_iKillerModePulse = thecore_pulse();
	NetworkSyncSystem::UpdatePacket(GetEntityHandle());
	LOG_INFO("SetKillerMode Update {}[{}]", GetName(), GetPlayerID());
}

bool CHARACTER::IsKillerMode() const
{
	// B.1.5: read via getter -> ECS StatusFlags.isKillerMode bit.
	return IS_SET(GetAddChrStateFlag(), ADD_CHARACTER_STATE_KILLER);
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
	if (auto* combat = g_registry.try_get<ecs::CombatStats>(GetEntityHandle())) {
		combat->pkMode = bPKMode;
		g_registry.emplace_or_replace<ecs::DirtyTag>(GetEntityHandle());
	}
	NetworkSyncSystem::UpdatePacket(GetEntityHandle());
	LOG_INFO("PK_MODE: {} {}", GetName(), m_bPKMode);
}

uint8_t CHARACTER::GetPKMode() const
{
	return m_bPKMode;
}

struct FuncForgetMyAttacker
{
	entt::entity m_character;
	explicit FuncForgetMyAttacker(entt::entity character) : m_character(character) {}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			const entt::entity candidate = ch->GetEntityHandle();
			if (ecs::PlayerRuntime::IsPC(candidate))
				return;
			if (ch->m_eVictim == m_character)
				CombatSystem::SetVictim(candidate, entt::null);
		}
	}
};

struct FuncAggregateMonster
{
	entt::entity m_character;
	explicit FuncAggregateMonster(entt::entity character) : m_character(character) {}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			const entt::entity candidate = ch->GetEntityHandle();
			if (ecs::PlayerRuntime::IsPC(candidate))
				return;
			if (!ch->IsMonster())
				return;
			if (ch->GetVictim())
				return;

			//if (number(1, 100) <= 50) // ӽ÷ 50% Ȯ  ´
			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(candidate) - ecs::PlayerRuntime::GetX(m_character), ecs::PlayerRuntime::GetY(candidate) - ecs::PlayerRuntime::GetY(m_character)) < 7000)
				if (CombatSystem::CanBeginFight(candidate))
					CombatSystem::BeginFight(candidate, m_character);
		}
	}
};
#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
struct FuncAggregateMonsterPlus
{
	entt::entity m_character;
	explicit FuncAggregateMonsterPlus(entt::entity character) : m_character(character) {}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			const entt::entity candidate = ch->GetEntityHandle();
			if (ecs::PlayerRuntime::IsPC(candidate))
				return;
			if (!ch->IsMonster())
				return;
			if (ch->GetVictim())
				return;

			const int AGGRO_RANGE = 14000;

			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(candidate) - ecs::PlayerRuntime::GetX(m_character), ecs::PlayerRuntime::GetY(candidate) - ecs::PlayerRuntime::GetY(m_character)) < AGGRO_RANGE)
				if (CombatSystem::CanBeginFight(candidate))
					CombatSystem::BeginFight(candidate, m_character);

		}
	}
};
#endif
struct FuncAttractRanger
{
	entt::entity m_character;
	explicit FuncAttractRanger(entt::entity character) : m_character(character) {}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			const entt::entity candidate = ch->GetEntityHandle();
			if (ecs::PlayerRuntime::IsPC(candidate))
				return;
			if (!ch->IsMonster())
				return;
			if (ch->m_eVictim != entt::null && ch->m_eVictim != m_character)
				return;
			if (ch->GetMobAttackRange() > 150)
			{
				int iNewRange = 150;//(int)(ch->GetMobAttackRange() * 0.2);
				if (iNewRange < 150)
					iNewRange = 150;

				AffectSystem::AddAffect(candidate, AFFECT_BOW_DISTANCE, POINT_BOW_DISTANCE, iNewRange - ch->GetMobAttackRange(), AFF_NONE, 3 * 60, 0, false);
			}
		}
	}
};

struct FuncPullMonster
{
	entt::entity m_character;
	int m_iLength;
	FuncPullMonster(entt::entity character, int iLength = 300)
	{
		m_character = character;
		m_iLength = iLength;
	}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			auto* ch = static_cast<LegacyCharHandle>(ent);
			const entt::entity candidate = ch->GetEntityHandle();
			if (ecs::PlayerRuntime::IsPC(candidate))
				return;
			if (!ch->IsMonster())
				return;
			//if (ch->GetVictim() && ch->GetVictim() != m_ch)
			//return;
			float fDist = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(m_character) - ecs::PlayerRuntime::GetX(candidate), ecs::PlayerRuntime::GetY(m_character) - ecs::PlayerRuntime::GetY(candidate));
			if (fDist > 3000 || fDist < 100)
				return;

			float fNewDist = fDist - m_iLength;
			if (fNewDist < 100)
				fNewDist = 100;

			float degree = GetDegreeFromPositionXY(ecs::PlayerRuntime::GetX(candidate), ecs::PlayerRuntime::GetY(candidate), ecs::PlayerRuntime::GetX(m_character), ecs::PlayerRuntime::GetY(m_character));
			float fx;
			float fy;

			GetDeltaByDegree(degree, fDist - fNewDist, &fx, &fy);
			int32_t tx = (int32_t)(ecs::PlayerRuntime::GetX(candidate) + fx);
			int32_t ty = (int32_t)(ecs::PlayerRuntime::GetY(candidate) + fy);

			ch->Sync(tx, ty);
			ch->Goto(tx, ty);
			ch->CalculateMoveDuration();

			NetworkSyncSystem::BroadcastSyncPacket(g_registry, candidate);
		}
	}
};


// char_battle.cpp slice BE2a moved into CombatSystem.cpp

void CHARACTER::ForgetMyAttacker()
{
	FuncForgetMyAttacker f(GetEntityHandle());
	ecs::ForEachAround(g_registry, GetEntityHandle(), f);
	ReviveInvisible(5);
}

void CHARACTER::AggregateMonster()
{
	FuncAggregateMonster f(GetEntityHandle());
	ecs::ForEachAround(g_registry, GetEntityHandle(), f);
}

#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
void CHARACTER::AggregateMonsterPlus()
{
	FuncAggregateMonsterPlus f(GetEntityHandle());
	ecs::ForEachAround(g_registry, GetEntityHandle(), f);
}
#endif
void CHARACTER::AttractRanger()
{
	FuncAttractRanger f(GetEntityHandle());
	ecs::ForEachAround(g_registry, GetEntityHandle(), f);
}

void CHARACTER::PullMonster()
{
	FuncPullMonster f(GetEntityHandle());
	ecs::ForEachAround(g_registry, GetEntityHandle(), f);
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
	//	ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "Nincs leaderboard adat.");
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
	if (!viewer || !ecs::PlayerRuntime::GetDesc((viewer ? viewer->GetEntityHandle() : entt::null)))
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

	ecs::PlayerRuntime::GetDesc((viewer ? viewer->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
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

	//LOG_INFO(0, "UpdateAggrPointEx for %s by %s dam %d total %d", GetName(), ecs::PlayerRuntime::GetName((pAttacker ? pAttacker->GetEntityHandle() : entt::null)).data(), dam, total);
	if (GetParty() && dam > 0 && type != DAMAGE_TYPE_SPECIAL)
	{
		LPPARTY pParty = GetParty();

		//     ϴ
		int iPartyAggroDist = dam;

		if (pParty->GetLeaderPID() == GetPacketVID())
			iPartyAggroDist /= 2;
		else
			iPartyAggroDist /= 3;

		pParty->SendMessage(this, PM_AGGRO_INCREASE, iPartyAggroDist, ecs::PlayerRuntime::GetPacketVID((pAttacker ? pAttacker->GetEntityHandle() : entt::null)));
	}

	ChangeVictimByAggro(info.iAggro, pAttacker);
}

void CHARACTER::UpdateAggrPoint(LPCHARACTER pAttacker, EDamageType type, int dam)
{
	if (IsDead() || IsStun())
		return;

	const entt::entity eAttacker = pAttacker ? pAttacker->GetEntityHandle() : entt::null;
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

				if (ch && !ch->IsDead() && DISTANCE_APPROX(ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) - GetX(), ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) - GetY()) < 5000)
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
		ecs::ChatSystem::Send((to ? to->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "exp(%d) overflow", iExp);
		return;
	}
	// decrease/increase exp based on player<>mob level
	rate_t lvFactor = static_cast<rate_t>(NEW_GET_LVDELTA(ecs::PointSystem::GetLevel((to ? to->GetEntityHandle() : entt::null)), ecs::PointSystem::GetLevel((from ? from->GetEntityHandle() : entt::null)))) / 100.0L;
	iExp *= lvFactor;
	// start calculating rate exp bonus
	int iBaseExp = iExp;
	rate_t rateFactor = 100;

	rateFactor += CPrivManager::instance().GetPriv(to, PRIV_EXP_PCT);
	if (to->IsEquipUniqueItem(UNIQUE_ITEM_LARBOR_MEDAL))
		rateFactor += 20;
	if (ecs::PlayerRuntime::GetMapIndex((to ? to->GetEntityHandle() : entt::null)) >= 660000 && ecs::PlayerRuntime::GetMapIndex((to ? to->GetEntityHandle() : entt::null)) < 670000)
		rateFactor += 20;
#ifdef NEW_POINT_EXP_DOUBLE_BONUS_RAZOR93



	int expDoubleBonus = ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_EXP_DOUBLE_BONUS);

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
	if (ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_EXP_DOUBLE_BONUS))
		if (number(1, 100) <= ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_EXP_DOUBLE_BONUS))
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
	if (ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_PC_BANG_EXP_BONUS) > 0)
	{
		if (to->IsPCBang())
			rateFactor += ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_PC_BANG_EXP_BONUS);
	}
	rateFactor += to->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS);
	rateFactor += ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_RAMADAN_CANDY_BONUS_EXP);
	rateFactor += ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_MALL_EXPBONUS);
	// useless (never used except for china intoxication) = always 100
	rateFactor = rateFactor * static_cast<rate_t>(CHARACTER_MANAGER::instance().GetMobExpRate(((to) ? (to)->GetEntityHandle() : entt::null))) / 100.0L;
	// apply calculated rate bonus
	iExp *= (rateFactor / 100.0L);
	if (test_server)
		ecs::ChatSystem::Send((to ? to->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "base_exp(%d) * rate(%Lf) = exp(%d)", iBaseExp, rateFactor / 100.0L, iExp);
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
		ecs::ChatSystem::Send((to ? to->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "exp+minGNE+adjust(%d)", iExp);
	// set
	ecs::PointSystem::Change((to ? to->GetEntityHandle() : entt::null), POINT_EXP, iExp, true);
	from->CreateFly(FLY_EXP, to);
	// marriage
	{
		auto* you = to->GetMarryPartner();
		if (you)
		{
			// sometimes, this overflows
			uint32_t dwUpdatePoint = (2000.0L / ecs::PointSystem::GetLevel((to ? to->GetEntityHandle() : entt::null)) / ecs::PointSystem::GetLevel((to ? to->GetEntityHandle() : entt::null)) / 3) * iExp;

			if (to->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0 ||
				you->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0)
				dwUpdatePoint *= 3;

			marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID((to ? to->GetEntityHandle() : entt::null)));

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
	iExp = CALCULATE_VALUE_LVDELTA(ecs::PointSystem::GetLevel((to ? to->GetEntityHandle() : entt::null)), ecs::PointSystem::GetLevel((from ? from->GetEntityHandle() : entt::null)), iExp);

	int iBaseExp = iExp;

	// , ȸ ġ ̺Ʈ
#ifdef ENABLE_EVENT_MANAGER
	const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(EXP_EVENT, ecs::PlayerRuntime::GetEmpire((to ? to->GetEntityHandle() : entt::null)));
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
		if (ecs::PlayerRuntime::GetMapIndex((to ? to->GetEntityHandle() : entt::null)) >= 660000 && ecs::PlayerRuntime::GetMapIndex((to ? to->GetEntityHandle() : entt::null)) < 670000)
			iExp += iExp * 20 / 100; // 1.2 (20%)

		//  ġ ι Ӽ
		if (ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_EXP_DOUBLE_BONUS))
			if (number(1, 100) <= ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_EXP_DOUBLE_BONUS))
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
		if (ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_PC_BANG_EXP_BONUS) > 0)
		{
			if (to->IsPCBang() == true)
				iExp += (iExp * ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_PC_BANG_EXP_BONUS) / 100);
		}

		// ȥ ʽ
		iExp += iExp * to->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS) / 100;
	}

	iExp += (iExp * ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_RAMADAN_CANDY_BONUS_EXP) / 100);
	iExp += (iExp * ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_MALL_EXPBONUS) / 100);

	if (test_server)
	{
		LOG_INFO("Bonus Exp : Ramadan Candy: {} MallExp: {}", ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_RAMADAN_CANDY_BONUS_EXP), ecs::PointSystem::Get((to ? to->GetEntityHandle() : entt::null), POINT_MALL_EXPBONUS));
	}

	// ȹ  2005.04.21  85%
	iExp = iExp * CHARACTER_MANAGER::instance().GetMobExpRate(((to) ? (to)->GetEntityHandle() : entt::null)) / 100;

	// ġ ѹ ȹ淮
	iExp = MIN(to->GetNextExp() / 10, iExp);

	if (test_server)
	{
		if (quest::CQuestManager::instance().GetEventFlag("exp_bonus_log") && iBaseExp > 0)
			ecs::ChatSystem::Send((to ? to->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "exp bonus %d%%", (iExp - iBaseExp) * 100 / iBaseExp);
		ecs::ChatSystem::Send((to ? to->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "exp(%d) base_exp(%d)", iExp, iBaseExp);
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

	ecs::PointSystem::Change((to ? to->GetEntityHandle() : entt::null), POINT_EXP, iExp, true);
	from->CreateFly(FLY_EXP, to);

	{
		auto* you = to->GetMarryPartner();
		// κΰ  Ƽ̸ ݽ
		if (you)
		{
			// 1 100%
			uint32_t dwUpdatePoint = 2000 * iExp / ecs::PointSystem::GetLevel((to ? to->GetEntityHandle() : entt::null)) / ecs::PointSystem::GetLevel((to ? to->GetEntityHandle() : entt::null)) / 3;

			if (to->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0 ||
				you->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0)
				dwUpdatePoint = (uint32_t)(dwUpdatePoint * 3);

			marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID((to ? to->GetEntityHandle() : entt::null)));

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
			: total(0), member_count(0), x(ecs::PlayerRuntime::GetX((center ? center->GetEntityHandle() : entt::null))), y(ecs::PlayerRuntime::GetY((center ? center->GetEntityHandle() : entt::null)))
		{
		};

		void operator () (LegacyCharHandle ch)
		{
			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) - x, ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) - y) <= PARTY_DEFAULT_RANGE)
			{
				total += __GetPartyExpNP(ecs::PointSystem::GetLevel((ch ? ch->GetEntityHandle() : entt::null)));

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
			: total(total), c(center), x(ecs::PlayerRuntime::GetX((center ? center->GetEntityHandle() : entt::null))), y(ecs::PlayerRuntime::GetY((center ? center->GetEntityHandle() : entt::null))), _iExp(iExp), m_iMode(iMode), m_iMemberCount(member_count)
		{
			if (m_iMemberCount == 0)
				m_iMemberCount = 1;
		};

		void operator () (LegacyCharHandle ch)
		{
			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) - x, ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) - y) <= PARTY_DEFAULT_RANGE)
			{
				uint32_t iExp2 = 0;

				switch (m_iMode)
				{
				case PARTY_EXP_DISTRIBUTION_NON_PARITY:
					iExp2 = (uint32_t)(_iExp * (float)__GetPartyExpNP(ecs::PointSystem::GetLevel((ch ? ch->GetEntityHandle() : entt::null))) / total);
					break;

				case PARTY_EXP_DISTRIBUTION_PARITY:
					iExp2 = _iExp / m_iMemberCount;
					break;

				default:
					LOG_ERROR("Unknown party exp distribution mode {}", m_iMode);
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

				if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) - ecs::PlayerRuntime::GetX((tch ? tch->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) - ecs::PlayerRuntime::GetY((tch ? tch->GetEntityHandle() : entt::null))) <= PARTY_DEFAULT_RANGE)
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
		if (!pAttacker || ecs::PlayerRuntime::IsNPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) || DISTANCE_APPROX(GetX() - ecs::PlayerRuntime::GetX((pAttacker ? pAttacker->GetEntityHandle() : entt::null)), GetY() - ecs::PlayerRuntime::GetY((pAttacker ? pAttacker->GetEntityHandle() : entt::null))) > 5000)
			continue;

		iTotalDam += iDam;
		if (!pkChrMostAttacked || iDam > iMostDam)
		{
			pkChrMostAttacked = pAttacker;
			iMostDam = iDam;
		}

		if (ecs::SocialSystem::GetParty((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
		{
			std::map<LPPARTY, TDamageInfo>::iterator it = map_party_damage.find(ecs::SocialSystem::GetParty((pAttacker ? pAttacker->GetEntityHandle() : entt::null)));
			if (it == map_party_damage.end())
			{
				TDamageInfo di;
				di.iDam = iDam;
				di.pAttacker = nullptr;
				di.pParty = ecs::SocialSystem::GetParty((pAttacker ? pAttacker->GetEntityHandle() : entt::null));
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

			//LOG_INFO(0, "__ pq_damage %s %d", ecs::PlayerRuntime::GetName((pAttacker ? pAttacker->GetEntityHandle() : entt::null)).data(), iDam);
			//pq_damage.push(di);
			damage_info_table.push_back(di);
		}
	}

	for (std::map<LPPARTY, TDamageInfo>::iterator it = map_party_damage.begin(); it != map_party_damage.end(); ++it)
	{
		damage_info_table.push_back(it->second);
		//LOG_INFO(0, "__ pq_damage_party [%u] %d", it->second.pParty->GetLeaderPID(), it->second.iDam);
	}

	SetExp(0);
	//m_map_kDamage.clear();

	if (iTotalDam == 0)	//  ذ 0̸
		return nullptr;

	if (m_pkChrStone)	//    ġ   ѱ.
	{
		//LOG_INFO(0, "__ Give half to Stone : %d", iExpToDistribute>>1);
		int iExp = iExpToDistribute >> 1;
		m_pkChrStone->SetExp(m_pkChrStone->GetExp() + iExp);
		iExpToDistribute -= iExp;
	}

	LOG_TRACE("{} total exp: {}, damage_info_table.size() == {}, TotalDam {}", GetName(), iExpToDistribute, damage_info_table.size(), iTotalDam);
	//LOG_INFO(1, "%s total exp: %d, pq_damage.size() == %d, TotalDam %d",
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
			LOG_ERROR("DistributeExp percent over 1.0 (fPercent {} name {})", fPercent, ecs::PlayerRuntime::GetName((di->pAttacker ? di->pAttacker->GetEntityHandle() : entt::null)).data());
			fPercent = 1.0f;
		}

		iExp += (int)(iExpToDistribute * fPercent);

		//LOG_INFO(0, "%s given exp percent %.1f + 20 dam %d", GetName(), fPercent * 100.0f, di.iDam);
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
				LOG_ERROR("DistributeExp percent over 1.0 (fPercent {} name {})", fPercent, ecs::PlayerRuntime::GetName((di.pAttacker ? di.pAttacker->GetEntityHandle() : entt::null)).data());
				fPercent = 1.0f;
			}

			//LOG_INFO(0, "%s given exp percent %.1f dam %d", GetName(), fPercent * 100.0f, di.iDam);
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
		LOG_ERROR("dead_event> <Factor> Null pointer");
		return 0;
	}

	auto* ch = LegacyCharOf(info->entity);
	if (ch == nullptr)
	{
		LOG_ERROR("DEAD_EVENT: cannot find char pointer with MOB entity({})", static_cast<uint32_t>(info->entity));
		return 0;
	}

	// Phase 10: WRITES_STATE - deferred until ECS component covers m_pkDeadEvent
	ch->m_pkDeadEvent = nullptr;
	{
		const entt::entity victimEntity = (ch ? ch->GetEntityHandle() : entt::null);
		if (victimEntity != entt::null)
			g_dispatcher.trigger(ecs::EvCharDead { entt::null, victimEntity });
	}

	if (!ecs::PlayerRuntime::IsPC((ch ? ch->GetEntityHandle() : entt::null)))
	{
		if (ch->IsMonster() == true)
		{
			if (ch->IsRevive() == false && ch->HasReviverInParty() == true)
			{
				ch->SetPosition(POS_STANDING);
				ch->SetHP(ecs::PointSystem::GetMaxHP((ch ? ch->GetEntityHandle() : entt::null)));

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

	//	if (auto* flags = RuntimeFlags(GetEntityHandle()))
	//		SET_BIT(flags->instantFlag, INSTANT_FLAG_NO_REWARD);

	//	SetPosition(POS_DEAD);
	//	ClearAffect(true);
	//	ClearSync();
	//	event_cancel(&m_pkStunEvent);

	//	if (pkKiller && ecs::PlayerRuntime::IsPC((pkKiller ? pkKiller->GetEntityHandle() : entt::null)))
	//		CLostCastleDungeon::instance().OnMobKilled(pkKiller, this);

	//	TPacketGCDead pack;
	//	pack.header = HEADER_GC_DEAD;
	//	pack.vid = GetPacketVID();
	//	PacketAround(&pack, sizeof(pack));

	//	if (auto* flags = RuntimeFlags(GetEntityHandle()))
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
			NetworkSyncSystem::UpdatePacket(GetEntityHandle());
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

	if (pkKiller && ecs::PlayerRuntime::IsPC((pkKiller ? pkKiller->GetEntityHandle() : entt::null)))
	{
		if (pkKiller->m_pkChrTarget == this)
			pkKiller->SetTarget(nullptr);

		isAgreedPVP = CPVPManager::instance().Dead(GetEntityHandle(), ecs::PlayerRuntime::GetPlayerID((pkKiller ? pkKiller->GetEntityHandle() : entt::null)));
		isDuel = CArenaManager::instance().OnDead(pkKiller, this);
#ifdef ENABLE_PVP_ADVANCED
		if (isAgreedPVP || isDuel)
		{
			const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

			int betMoneyDead = GetQuestFlag(szTableStaticPvP[8]);
			int betMoneyKiller = ecs::QuestSystem::GetFlag((pkKiller ? pkKiller->GetEntityHandle() : entt::null), szTableStaticPvP[8]);

			if (betMoneyDead > 0 && betMoneyKiller > 0)
			{
				ecs::PointSystem::Change((pkKiller ? pkKiller->GetEntityHandle() : entt::null), POINT_GOLD, betMoneyDead * 2, true);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew((pkKiller ? pkKiller->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 515, "%d", betMoneyDead);
#endif
			}

			for (unsigned int i = 0; i < _countof(szTableStaticPvP); i++) {
				char pkCh_Buf[CHAT_MAX_LEN + 1], pkKiller_Buf[CHAT_MAX_LEN + 1];

				snprintf(pkCh_Buf, sizeof(pkCh_Buf), "BINARY_Duel_Delete");
				snprintf(pkKiller_Buf, sizeof(pkKiller_Buf), "BINARY_Duel_Delete");

				ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, pkCh_Buf);
				SetQuestFlag(szTableStaticPvP[i], 0);

				ecs::ChatSystem::Send((pkKiller ? pkKiller->GetEntityHandle() : entt::null), CHAT_TYPE_COMMAND, pkKiller_Buf);
				ecs::QuestSystem::SetFlag((pkKiller ? pkKiller->GetEntityHandle() : entt::null), szTableStaticPvP[i], 0);
			}
		}
#endif

		if (IsPC())
		{
			CGuild* g1 = GetGuild();
			CGuild* g2 = ecs::SocialSystem::GetGuild((pkKiller ? pkKiller->GetEntityHandle() : entt::null));

			if (g1 && g2)
				if (g1->UnderWar(g2->GetID()))
					isUnderGuildWar = true;

			pkKiller->SetQuestNPCID(GetPacketVID());
			quest::CQuestManager::instance().Kill(ecs::PlayerRuntime::GetPlayerID((pkKiller ? pkKiller->GetEntityHandle() : entt::null)), quest::QUEST_NO_NPC);
			CGuildManager::instance().Kill(pkKiller, this);
		}
	}

#ifdef ENABLE_QUEST_DIE_EVENT
	//if (IsPC())
	//{
	//	if (pkKiller)
	//		SetQuestNPCID(pkKiller->GetVID());
	//	// quest::CQuestManager::instance().Die(GetPlayerID(), quest::QUEST_NO_NPC);
	//	quest::CQuestManager::instance().Die(GetPlayerID(), (pkKiller)?ecs::PlayerRuntime::GetRaceNum((pkKiller ? pkKiller->GetEntityHandle() : entt::null)):quest::QUEST_NO_NPC);
	//}
	if (IsPC())
	{
		if (pkKiller) {
			SetQuestNPCID(ecs::PlayerRuntime::GetPacketVID((pkKiller ? pkKiller->GetEntityHandle() : entt::null)));
		}

		quest::CQuestManager::instance().Die(GetPlayerID(), (pkKiller) ? ecs::PlayerRuntime::GetRaceNum((pkKiller ? pkKiller->GetEntityHandle() : entt::null)) : quest::QUEST_NO_NPC);
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
		if (ecs::PlayerRuntime::IsPC((pkKiller ? pkKiller->GetEntityHandle() : entt::null))) {
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
		if (pkKiller && !ecs::PlayerRuntime::IsPC((pkKiller ? pkKiller->GetEntityHandle() : entt::null))) {
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
		if (!ecs::PlayerRuntime::IsPC((pkKiller ? pkKiller->GetEntityHandle() : entt::null)))
		{
#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
			SetDeadByMonster(true);
#endif

			LOG_TRACE("DEAD: {} {} WITH PENALTY", GetName(), static_cast<const void*>(this));
						if (auto* flags = RuntimeFlags(GetEntityHandle()))
				SET_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);
			LogManager::instance().CharLog(this, ecs::PlayerRuntime::GetRaceNum((pkKiller ? pkKiller->GetEntityHandle() : entt::null)), "DEAD_BY_NPC", ecs::PlayerRuntime::GetName((pkKiller ? pkKiller->GetEntityHandle() : entt::null)).data());
		}
		else
		{
#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
			SetDeadByMonster(false);
#endif
			LOG_TRACE("DEAD_BY_PC: {} {} KILLER {} {}", GetName(), static_cast<const void*>(this), ecs::PlayerRuntime::GetName((pkKiller ? pkKiller->GetEntityHandle() : entt::null)).data(), static_cast<const void*>(get_pointer(pkKiller)));
						if (auto* flags = RuntimeFlags(GetEntityHandle()))
				REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);

			if (GetEmpire() != ecs::PlayerRuntime::GetEmpire((pkKiller ? pkKiller->GetEntityHandle() : entt::null)))
			{
				int64_t iEP = std::min(GetPoint(POINT_EMPIRE_POINT), ecs::PointSystem::Get((pkKiller ? pkKiller->GetEntityHandle() : entt::null), POINT_EMPIRE_POINT));

				PointChange(POINT_EMPIRE_POINT, -(iEP / 10));
				ecs::PointSystem::Change((pkKiller ? pkKiller->GetEntityHandle() : entt::null), POINT_EMPIRE_POINT, iEP / 5);


				char buf[256];
				snprintf(buf, sizeof(buf),
					"%d %u %d %s %d %u %d %s",
					GetEmpire(), GetAlignment(), GetPKMode(), GetName(),
					ecs::PlayerRuntime::GetEmpire((pkKiller ? pkKiller->GetEntityHandle() : entt::null)), pkKiller->GetAlignment(), pkKiller->GetPKMode(), ecs::PlayerRuntime::GetName((pkKiller ? pkKiller->GetEntityHandle() : entt::null)).data());

				LogManager::instance().CharLog(this, ecs::PlayerRuntime::GetPlayerID((pkKiller ? pkKiller->GetEntityHandle() : entt::null)), "DEAD_BY_PC", buf);
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
//						ecs::ChatSystem::SendNew((pkKiller ? pkKiller->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 413, "");
//#endif
//					}
//					else {
//						if (ecs::SocialSystem::GetParty((pkKiller ? pkKiller->GetEntityHandle() : entt::null)))
//						{
//							FPartyAlignmentCompute f(-20000, ecs::PlayerRuntime::GetX((pkKiller ? pkKiller->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((pkKiller ? pkKiller->GetEntityHandle() : entt::null)));
//							ecs::SocialSystem::GetParty((pkKiller ? pkKiller->GetEntityHandle() : entt::null))->ForEachOnlineMember(f);
//
//							if (f.m_iCount == 0)
//								pkKiller->UpdateAlignment(-20000);
//							else
//							{
//								0, "ALIGNMENT PARTY count %d amount %d", f.m_iCount, f.m_iAmount);
//
//								f.m_iStep = 1;
//								ecs::SocialSystem::GetParty((pkKiller ? pkKiller->GetEntityHandle() : entt::null))->ForEachOnlineMember(f);
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
					ecs::PlayerRuntime::GetEmpire((pkKiller ? pkKiller->GetEntityHandle() : entt::null)), pkKiller->GetAlignment(), pkKiller->GetPKMode(), ecs::PlayerRuntime::GetName((pkKiller ? pkKiller->GetEntityHandle() : entt::null)).data());

				LogManager::instance().CharLog(this, ecs::PlayerRuntime::GetPlayerID((pkKiller ? pkKiller->GetEntityHandle() : entt::null)), "DEAD_BY_PC", buf);
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
					if ((GetDesc()->GetHostName() != ecs::PlayerRuntime::GetDesc((pkKiller ? pkKiller->GetEntityHandle() : entt::null))->GetHostName()) && CBattlePass::instance().IsEligibleForPlayerKill(ecs::PlayerRuntime::GetPlayerID((pkKiller ? pkKiller->GetEntityHandle() : entt::null)), GetPlayerID()))
					{
						if (dwLevel >= dwMinLevel && pkKiller->GetMissionProgress(PLAYER_KILL, bBattlePassId) < dwToKillCount)
						{
							pkKiller->UpdateMissionProgress(PLAYER_KILL, bBattlePassId, 1, dwToKillCount);
							CBattlePass::instance().RegisterPlayerKill(ecs::PlayerRuntime::GetPlayerID((pkKiller ? pkKiller->GetEntityHandle() : entt::null)), GetPlayerID());
						}
					}
#else
					if (dwLevel >= dwMinLevel && pkKiller->GetMissionProgress(PLAYER_KILL, bBattlePassId) < dwToKillCount)
						pkKiller->UpdateMissionProgress(PLAYER_KILL, bBattlePassId, 1, dwToKillCount);
#endif
				}
			}
			if (pkKiller && ecs::PlayerRuntime::IsPC((pkKiller ? pkKiller->GetEntityHandle() : entt::null)) && IsPC())
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
						ecs::PlayerRuntime::GetName((pkKiller ? pkKiller->GetEntityHandle() : entt::null)).data(), GetName(), szMapName, iRankPoints);
				}
				else
				{
					snprintf(szMsg, sizeof(szMsg),
						"|cff00ff00%s|r has killed |cffff0000%s|r Map: %s, PVP-Mode: FREE!",
						ecs::PlayerRuntime::GetName((pkKiller ? pkKiller->GetEntityHandle() : entt::null)).data(), GetName(), szMapName);
				}

				BroadcastNotice(szMsg);
			}


#endif
		}
	}
	else
	{
		LOG_TRACE("DEAD: {} {}", GetName(), static_cast<const void*>(this));
				if (auto* flags = RuntimeFlags(GetEntityHandle()))
			REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);
	}

	ClearSync();

	//LOG_INFO(1, "stun cancel %s[%d]", GetName(), (uint32_t)GetVID());
	event_cancel(&m_pkStunEvent); //  ̺Ʈ δ.

	if (IsPC())
	{
		m_dwLastDeadTime = get_dword_time();
		//SetKillerMode(pkKiller && ecs::PlayerRuntime::IsPC((pkKiller ? pkKiller->GetEntityHandle() : entt::null)));
		SetKillerMode(false);
		GetDesc()->SetPhase(PHASE_DEAD);
	}
	else
	{
		// 忡 ݹ ʹ   Ѵ.
		if (!(RuntimeFlags(GetEntityHandle()) && IS_SET(RuntimeFlags(GetEntityHandle())->instantFlag, INSTANT_FLAG_NO_REWARD)))
		{
			if (!(pkKiller && ecs::PlayerRuntime::IsPC((pkKiller ? pkKiller->GetEntityHandle() : entt::null)) && ecs::SocialSystem::GetGuild((pkKiller ? pkKiller->GetEntityHandle() : entt::null)) && ecs::SocialSystem::GetGuild((pkKiller ? pkKiller->GetEntityHandle() : entt::null))->UnderAnyWar(GUILD_WAR_TYPE_FIELD)))
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
					ecs::ChatSystem::SendNew((pkKiller ? pkKiller->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 147, "");
#endif
				}
			}
		}
	}

	// BOSS_KILL_LOG
	if (GetMobRank() >= MOB_RANK_BOSS && pkKiller && ecs::PlayerRuntime::IsPC((pkKiller ? pkKiller->GetEntityHandle() : entt::null)))
	{
		char buf[51];
		snprintf(buf, sizeof(buf), "%d %ld", g_bChannel, ecs::PlayerRuntime::GetMapIndex((pkKiller ? pkKiller->GetEntityHandle() : entt::null)));
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

		if (auto* flags = RuntimeFlags(GetEntityHandle()))
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
			LOG_TRACE("DEAD_EVENT_CANCEL: {} {} {}", GetName(), static_cast<const void*>(this), static_cast<const void*>(get_pointer(m_pkDeadEvent)));
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
			pEventInfo->entity = GetEntityHandle();

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

			LOG_TRACE("DEAD_EVENT_CREATE: {} {} {}", GetName(), static_cast<const void*>(this), static_cast<const void*>(get_pointer(m_pkDeadEvent)));
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
        // LPENTITY.4-fixup.2.f note: m_bAddChrState DEAD bit is set later by
        // CHARACTER::SetPosition(POS_DEAD) in the legacy Dead() flow.
        // EvEntityDied below has no current sink, so the legacy bit only
        // gets set if the legacy battle.cpp Damage path runs in parallel.
        // This leaves a transient drift window where ECS reports DEAD but
        // legacy does not. Resolution is deferred to LPENTITY.6 when this
        // ECS combat tick is unified with the legacy death path.

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
	packFly.dwEndVID = ecs::PlayerRuntime::GetPacketVID((pkVictim ? pkVictim->GetEntityHandle() : entt::null));

	PacketAround(&packFly, sizeof(TPacketGCCreateFly));
}

bool CHARACTER::Attack(LPCHARACTER pkVictim, uint8_t bType)
{
#ifdef ENABLE_BUG_FIXES
	if (pkVictim->GetMyShop())
		return false;
#endif

	if (test_server)
		LOG_TRACE("[TEST_SERVER] Attack : {} type {}, MobBattleType {}", GetName(), bType, (!IsPC() && GetMobBattleType()) ? GetMobAttackRange() : 0);
	//PROF_UNIT puAttack("Attack");
	if (!CanMove())
		return false;
#ifdef ENABLE_ANTICHEAT
	SECTREE* sectree = GetSectree();
	SECTREE* vsectree = ecs::PlayerRuntime::GetSectree((pkVictim ? pkVictim->GetEntityHandle() : entt::null));

	if (sectree && vsectree) {
		if (sectree->IsAttr(GetX(), GetY(), ATTR_BANPK) || vsectree->IsAttr(ecs::PlayerRuntime::GetX((pkVictim ? pkVictim->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((pkVictim ? pkVictim->GetEntityHandle() : entt::null)), ATTR_BANPK)) {
			if (GetDesc()) {
				LogManager::instance().HackLog("ANTISAFEZONE", this);
				GetDesc()->DelayedDisconnect(3);
			}
		}
	}
#endif
	// if (ecs::SocialSystem::GetParty((pkVictim ? pkVictim->GetEntityHandle() : entt::null)))
	   // return false;

   // @fixme131
	if (!battle_is_attackable(GetEntityHandle(), (pkVictim ? pkVictim->GetEntityHandle() : entt::null)))
		return false;

	uint32_t dwCurrentTime = get_dword_time();

	if (IsPC()) {
#ifdef ENABLE_ANTICHEAT
		if (IS_SPEED_HACK(GetEntityHandle(), (pkVictim ? pkVictim->GetEntityHandle() : entt::null), dwCurrentTime)) {
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
			iRet = battle_melee_attack(GetEntityHandle(), (pkVictim ? pkVictim->GetEntityHandle() : entt::null));
			break;
		case BATTLE_TYPE_RANGE:
			FlyTarget(ecs::PlayerRuntime::GetPacketVID((pkVictim ? pkVictim->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetX((pkVictim ? pkVictim->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((pkVictim ? pkVictim->GetEntityHandle() : entt::null)), HEADER_CG_FLY_TARGETING);
			iRet = Shoot(0) ? BATTLE_DAMAGE : BATTLE_NONE;
			break;
		case BATTLE_TYPE_MAGIC:
			FlyTarget(ecs::PlayerRuntime::GetPacketVID((pkVictim ? pkVictim->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetX((pkVictim ? pkVictim->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((pkVictim ? pkVictim->GetEntityHandle() : entt::null)), HEADER_CG_FLY_TARGETING);
			iRet = Shoot(1) ? BATTLE_DAMAGE : BATTLE_NONE;
			break;
		default:
			LOG_ERROR("Unhandled battle type {}", GetMobBattleType());
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
				LOG_INFO("HACK: Too long skill using term. Name({}) PID({}) delta({})", GetName(), GetPlayerID(), (dwCurrentTime - m_dwLastSkillTime));
				return false;
			}
		}

		LOG_TRACE("Attack call ComputeSkill {} {}", bType, pkVictim ? ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data() : "");
		iRet = ComputeSkill(bType, pkVictim);
	}

	//if (test_server && IsPC())
	//	0, "%s Attack %s type %u ret %d", GetName(), ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data(), bType, iRet);
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

int CHARACTER::GetArrowAndBow(entt::entity* ppkBow, entt::entity* ppkArrow, int iArrowCount/* = 1 */)
{
	const entt::entity bow = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_WEAPON);
	if (!ItemSystem::IsValidItem(bow))
	{
		return 0;
	}

	const TItemTable* bowProto = ItemSystem::GetItemProto(bow);
	if (!bowProto || bowProto->bSubType != WEAPON_BOW)
	{
		return 0;
	}

	const entt::entity arrow = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_ARROW);
	if (!ItemSystem::IsValidItem(arrow) || ItemSystem::GetItemType(arrow) != ITEM_WEAPON)
	{
		return 0;
	}

	const TItemTable* arrowProto = ItemSystem::GetItemProto(arrow);
	if (!arrowProto || arrowProto->bSubType != WEAPON_ARROW)
	{
		return 0;
	}

	iArrowCount = std::min(iArrowCount, static_cast<int>(ItemSystem::GetItemCount(arrow)));

	*ppkBow = bow;
	*ppkArrow = arrow;

	return iArrowCount;
}
// char_battle.cpp slice BD1 moved into CombatSystem.cpp

void CHARACTER::DistributeSP(LPCHARACTER pkKiller, int iMethod)
{
	if (pkKiller->GetSP() >= ecs::PointSystem::GetMaxSP((pkKiller ? pkKiller->GetEntityHandle() : entt::null)))
		return;

	bool bAttacking = (get_dword_time() - GetLastAttackTime()) < 3000;
	bool bMoving = (get_dword_time() - GetLastMoveTime()) < 3000;

	if (iMethod == 1)
	{
		int num = number(0, 3);

		if (!num)
		{
			int iLvDelta = GetLevel() - ecs::PointSystem::GetLevel((pkKiller ? pkKiller->GetEntityHandle() : entt::null));
			int iAmount = 0;

			if (iLvDelta >= 5)
				iAmount = 10;
			else if (iLvDelta >= 0)
				iAmount = 6;
			else if (iLvDelta >= -3)
				iAmount = 2;

			if (iAmount != 0)
			{
				iAmount += (iAmount * ecs::PointSystem::Get((pkKiller ? pkKiller->GetEntityHandle() : entt::null), POINT_SP_REGEN)) / 100;

				if (iAmount >= 11)
					CreateFly(FLY_SP_BIG, pkKiller);
				else if (iAmount >= 7)
					CreateFly(FLY_SP_MEDIUM, pkKiller);
				else
					CreateFly(FLY_SP_SMALL, pkKiller);

				ecs::PointSystem::Change((pkKiller ? pkKiller->GetEntityHandle() : entt::null), POINT_SP, iAmount);
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

			iAmount += (iAmount * ecs::PointSystem::Get((pkKiller ? pkKiller->GetEntityHandle() : entt::null), POINT_SP_REGEN)) / 100;
			ecs::PointSystem::Change((pkKiller ? pkKiller->GetEntityHandle() : entt::null), POINT_SP, iAmount);
		}
		else
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + ecs::PointSystem::GetMaxSP((pkKiller ? pkKiller->GetEntityHandle() : entt::null)) / 200;
			else if (bMoving)
				iAmount = 2 + ecs::PointSystem::GetMaxSP((pkKiller ? pkKiller->GetEntityHandle() : entt::null)) / 100;
			else
			{
				//
				if (pkKiller->GetHP() < ecs::PointSystem::GetMaxHP((pkKiller ? pkKiller->GetEntityHandle() : entt::null)))
					iAmount = 2 + (ecs::PointSystem::GetMaxSP((pkKiller ? pkKiller->GetEntityHandle() : entt::null)) / 100); //   á
				else
					iAmount = 9 + (ecs::PointSystem::GetMaxSP((pkKiller ? pkKiller->GetEntityHandle() : entt::null)) / 100); // ⺻
			}

			iAmount += (iAmount * ecs::PointSystem::Get((pkKiller ? pkKiller->GetEntityHandle() : entt::null), POINT_SP_REGEN)) / 100;
			ecs::PointSystem::Change((pkKiller ? pkKiller->GetEntityHandle() : entt::null), POINT_SP, iAmount);
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
	if (PLAYER_MAX_LEVEL_CONST < ecs::PointSystem::GetLevel((ch ? ch->GetEntityHandle() : entt::null)))
	{
		double ret = 0.95;
		double factor = 0.1;

		for (int64_t i = 0; i < ecs::PointSystem::GetLevel((ch ? ch->GetEntityHandle() : entt::null)) - 100; ++i)
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
	LOG_INFO("DEATH_PERNALY_CHECK({}) town({})", GetName(), bTown);

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
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 412, "");
#endif
		return;
	}

	if (number(0, 2) == 1) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 412, "");
#endif
		return;
	}

	if (RuntimeFlags(GetEntityHandle()) && IS_SET(RuntimeFlags(GetEntityHandle())->instantFlag, INSTANT_FLAG_DEATH_PENALTY))
	{
				if (auto* flags = RuntimeFlags(GetEntityHandle()))
			REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);

		// NO_DEATH_PENALTY_BUG_FIX
		if (!bTown) //   ڸ Ȱø  ȣ Ѵ. ( ͽô ġ гƼ )
		{
			if (FindAffect(AFFECT_NO_DEATH_PENALTY))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 384, "");
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

		LOG_INFO("DEATH_PENALTY({}) EXP_LOSS: {} percent {}%", GetName(), iLoss, __GetExpLossPerc(GetLevel()));

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

	std::vector<std::pair<entt::entity, int>> vec_item;
	const entt::entity ownerEntity = GetEntityHandle();
	int	i;
	bool isDropAllEquipments = false;

	TItemDropPenalty& r = table[iAlignIndex];
	LOG_INFO("{} align {} inven_pct {} equip_pct {}", GetName(), iAlignIndex, r.iInventoryPct, r.iEquipmentPct);

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
			if (ItemSystem::IsValidItem(ItemSystem::GetInventoryItem(ownerEntity, i)))
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
				const entt::entity itemEntity =
					ItemSystem::GetInventoryItem(ownerEntity, vec_bSlots[i]);
				if (IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
					continue;

				SyncQuickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
				if (ItemSystem::RemoveItemEcs(itemEntity))
					vec_item.emplace_back(itemEntity, INVENTORY);
			}
		}
		/*else if (iAlignIndex == 8)
			isDropAllEquipments = true;*/
	}

	if (bDropEquipment) // Drop Equipment
	{
		std::vector<uint8_t> vec_bSlots;

		for (i = 0; i < WEAR_MAX_NUM; ++i)
			if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, i)))
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
				const entt::entity itemEntity =
					ItemSystem::GetWearItem(ownerEntity, vec_bSlots[i]);
				if (IS_SET(ItemSystem::GetItemAntiFlag(itemEntity), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
					continue;

				SyncQuickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
				if (ItemSystem::RemoveItemEcs(itemEntity))
					vec_item.emplace_back(itemEntity, EQUIPMENT);
			}
		}
	}

	if (bDropAntiDropUniqueItem)
	{
		const entt::entity unique1 = ItemSystem::GetWearItem(ownerEntity, WEAR_UNIQUE1);
		if (ItemSystem::IsValidItem(unique1) &&
			ItemSystem::GetItemVnum(unique1) == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			SyncQuickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE1, 255);
			if (ItemSystem::RemoveItemEcs(unique1))
				vec_item.emplace_back(unique1, EQUIPMENT);
		}

		const entt::entity unique2 = ItemSystem::GetWearItem(ownerEntity, WEAR_UNIQUE2);
		if (ItemSystem::IsValidItem(unique2) &&
			ItemSystem::GetItemVnum(unique2) == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			SyncQuickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE2, 255);
			if (ItemSystem::RemoveItemEcs(unique2))
				vec_item.emplace_back(unique2, EQUIPMENT);
		}
	}

	{
		PIXEL_POSITION pos;
		pos.x = GetX();
		pos.y = GetY();

		unsigned int i;

		for (i = 0; i < vec_item.size(); ++i)
		{
			const entt::entity item = vec_item[i].first;
			if (!ItemSystem::IsValidItem(item))
				continue;
			int window = vec_item[i].second;

			if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
					item, GetMapIndex(), pos, 300))
				continue;

			LOG_INFO("DROP_ITEM_PK: {} {} {} from {}",
				ItemSystem::GetItemName(item), pos.x, pos.y, GetName());
			LogManager::instance().ItemLogEntity(
				GetEntityHandle(), item, "DEAD_DROP",
				(window == INVENTORY) ? "INVENTORY" :
				((window == EQUIPMENT) ? "EQUIPMENT" : ""));

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

static bool __TryAutoGiveRewardItem(LegacyCharHandle ch, entt::entity itemEntity, uint32_t& dwGivenCount)
{
	dwGivenCount = 0;

	const entt::entity owner = ch ? ch->GetEntityHandle() : entt::null;
	if (!ch || owner == entt::null || !g_registry.valid(owner) ||
		!ItemSystem::IsValidItem(itemEntity))
		return false;

	const uint32_t itemVnum = ItemSystem::GetItemVnum(itemEntity);
	const TItemTable* itemProto = ItemSystem::GetItemProto(itemEntity);
#ifdef ENABLE_MULTI_NAMES
	const uint8_t language = ecs::NetworkService::GetLanguage(owner);
	const std::string itemName = itemProto
		? itemProto->szLocaleName[language]
		: ItemSystem::GetItemName(itemEntity);
#else
	const std::string itemName = itemProto
		? itemProto->szLocaleName
		: ItemSystem::GetItemName(itemEntity);
#endif
	int remainingCount = static_cast<int>(ItemSystem::GetItemCount(itemEntity));

	const auto socketsMatch = [itemEntity](entt::entity candidate) {
		for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
		{
			if (ItemSystem::GetItemSocket(candidate, socket) !=
				ItemSystem::GetItemSocket(itemEntity, socket))
				return false;
		}
		return true;
	};

	const auto mergeIntoInventory = [&](int slotCount, const auto& getItem) {
		for (int cell = 0; cell < slotCount && remainingCount > 0; ++cell)
		{
			const entt::entity candidate = getItem(cell);
			if (!ItemSystem::IsValidItem(candidate) ||
				ItemSystem::GetItemVnum(candidate) != itemVnum ||
				!socketsMatch(candidate))
				continue;

			const int capacity = std::max(
				0, static_cast<int>(g_bItemCountLimit) -
				static_cast<int>(ItemSystem::GetItemCount(candidate)));
			const int moved = std::min(capacity, remainingCount);
			if (moved <= 0)
				continue;

			if (!ItemSystem::AddItemCountEcs(candidate, moved))
				continue;

			remainingCount -= moved;
			dwGivenCount += static_cast<uint32_t>(moved);
		}
	};

	if (ItemSystem::IsItemVnumStackable(itemVnum))
	{

#ifdef ENABLE_EXTRA_INVENTORY
		if (ItemSystem::IsExtraItem(itemEntity))
		{
			mergeIntoInventory(EXTRA_INVENTORY_MAX_NUM, [owner](int cell) {
				return ItemSystem::GetExtraInventoryItem(
					owner, static_cast<uint16_t>(cell));
			});
		}
		else
#endif
		{
			mergeIntoInventory(INVENTORY_MAX_NUM, [owner](int cell) {
				return ItemSystem::GetInventoryItem(
					owner, static_cast<uint16_t>(cell));
			});
		}

		if (remainingCount == 0)
		{
			ItemSystem::DestroyItemEntityEcs(itemEntity, "COMBAT_INSTANT_LOOT_STACKED");
#ifdef TEXTS_IMPROVEMENT
			if (dwGivenCount > 0)
			{
				ecs::ChatSystem::SendNew(owner,
#ifdef ENABLE_NEW_CHAT
					CHAT_TYPE_INFO_ITEM
#else
					CHAT_TYPE_INFO
#endif
					, 102, "%u#%s", dwGivenCount, itemName.c_str());
			}
#endif
			return true;
		}

		if (!ItemSystem::SetItemCountEcs(
				itemEntity, static_cast<uint32_t>(remainingCount)))
			return false;
	}

	const int emptyCell = ItemSystem::GetEmptyInventoryPositionEcs(owner, itemEntity);
	if (emptyCell < 0)
		return false;

	uint8_t window = INVENTORY;
	if (ItemSystem::IsDragonSoulItem(itemEntity))
		window = DRAGON_SOUL_INVENTORY;
#ifdef ENABLE_EXTRA_INVENTORY
	else if (ItemSystem::IsExtraItem(itemEntity))
		window = EXTRA_INVENTORY;
#endif

	const uint32_t directCount = ItemSystem::GetItemCount(itemEntity);
	if (!ItemSystem::PlaceItemEcs(
			owner, itemEntity, window, static_cast<uint16_t>(emptyCell)))
		return false;

	dwGivenCount += directCount;

#ifdef TEXTS_IMPROVEMENT
	if (dwGivenCount > 0)
	{
		ecs::ChatSystem::SendNew(owner,
#ifdef ENABLE_NEW_CHAT
			CHAT_TYPE_INFO_ITEM
#else
			CHAT_TYPE_INFO
#endif
			, 102, "%u#%s", dwGivenCount, itemName.c_str());
	}
#endif

	char hint[96];
	snprintf(hint, sizeof(hint), "%s %u %u", itemName.c_str(),
		ItemSystem::GetItemCount(itemEntity),
		ItemSystem::GetItemOriginalVnum(itemEntity));
	LogManager::instance().ItemLogEntity(owner, itemEntity, "GET", hint);
	return true;
}

static void __GiveRewardItemToCharacterOrDrop(LegacyCharHandle ch, LegacyCharHandle pkVictim, entt::entity itemEntity, const PIXEL_POSITION& pos, bool bTrackBattlePass)
{
	if (!ItemSystem::IsValidItem(itemEntity))
		return;

	uint32_t dwGivenCount = 0;
	const uint32_t dwItemVnum = ItemSystem::GetItemVnum(itemEntity);

	if (ch && __TryAutoGiveRewardItem(ch, itemEntity, dwGivenCount))
	{
		if (bTrackBattlePass && dwGivenCount > 0)
			__UpdateBattlePassCollectProgress(ch, dwItemVnum, dwGivenCount);
		return;
	}

	if (bTrackBattlePass && dwGivenCount > 0)
		__UpdateBattlePassCollectProgress(ch, dwItemVnum, dwGivenCount);

	if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
			itemEntity,
			ecs::PlayerRuntime::GetMapIndex((pkVictim ? pkVictim->GetEntityHandle() : entt::null)),
			pos, 300))
		return;

	if (ch && CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex((ch ? ch->GetEntityHandle() : entt::null))) == false)
		ItemSystem::SetGroundOwnershipLegacyBoundary(
			itemEntity, (ch ? ch->GetEntityHandle() : entt::null), 60);

	LOG_INFO("DROP_ITEM: {} {} {} from {}", ItemSystem::GetItemName(itemEntity),
		pos.x, pos.y, ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data());
}
#endif


#ifdef ENABLE_RARE_DROP_NOTICE_RAZOR93
static std::string MakeItemLink(entt::entity item, LegacyCharHandle pkKiller, LegacyCharHandle pkMob)
{
	char itemlink[512];
	int len = 0;

	// item link alap
	len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
		ItemSystem::GetItemVnum(item),
		ItemSystem::GetItemSocket(item, 0),
		ItemSystem::GetItemSocket(item, 1),
		ItemSystem::GetItemSocket(item, 2),
		0, 0);

	// bonuszok
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
		uint8_t type = ItemSystem::GetItemAttributeType(item, i);
		short   val = ItemSystem::GetItemAttributeValue(item, i);
		if (type && val)
			len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
	}


	int lang = LANGUAGE_EN;
	if (pkKiller && ecs::PlayerRuntime::GetDesc((pkKiller ? pkKiller->GetEntityHandle() : entt::null)))
		lang = ecs::PlayerRuntime::GetDesc((pkKiller ? pkKiller->GetEntityHandle() : entt::null))->GetLanguage();


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
		pkKiller ? ecs::PlayerRuntime::GetName((pkKiller ? pkKiller->GetEntityHandle() : entt::null)).data() : "Player",
		pkMob ? ecs::PlayerRuntime::GetName((pkMob ? pkMob->GetEntityHandle() : entt::null)).data() : "Mob",
		itemlink,
		item != entt::null ? ItemSystem::GetItemName(item) : "item");

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
		LOG_ERROR("Reward: NULL mob data (vid={} race={} name={} map={} x={} y={} attacker={})", GetPacketVID(), GetRaceNum(), GetName(), GetMapIndex(), GetX(), GetY(), pkAttacker ? ecs::PlayerRuntime::GetName((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)).data() : "<null>");
		m_map_kDamage.clear();
		return;
	}
	//PROF_UNIT pu1("r1");
	if (ecs::PlayerRuntime::IsPC((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)))
	{
		if ((GetLevel() - ecs::PointSystem::GetLevel((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null))) >= -10)
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
		quest::CQuestManager::instance().Kill(ecs::PlayerRuntime::GetPlayerID((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)), GetRaceNum());
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
			if (ecs::PointSystem::Get((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_KILL_HP_RECOVERY))
			{
				int iHP = ecs::PointSystem::GetMaxHP((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)) * ecs::PointSystem::Get((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_KILL_HP_RECOVERY) / 100;
				ecs::PointSystem::Change((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_HP, iHP);
				CreateFly(FLY_HP_SMALL, pkAttacker);
			}

			if (ecs::PointSystem::Get((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_KILL_SP_RECOVER))
			{
				int iSP = ecs::PointSystem::GetMaxSP((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)) * ecs::PointSystem::Get((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_KILL_SP_RECOVER) / 100;
				ecs::PointSystem::Change((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_SP, iSP);
				CreateFly(FLY_SP_SMALL, pkAttacker);
			}
		}
	}
	//pu1.Pop();

#ifdef ENABLE_BLOCK_MULTIFARM
	if (AffectSystem::FindAffect((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), AFFECT_DROP_BLOCK, APPLY_NONE)) {
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
		LOG_TRACE("Drop money : Attacker {}", ecs::PlayerRuntime::GetName((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)).data());
	RewardGold(pkAttacker);
	//pu2.Pop();

	//
	//
	//
	//PROF_UNIT pu3("r3");
	entt::entity itemEntity = entt::null;

	std::vector<entt::entity> s_vec_item;
	s_vec_item.clear();

	if (ITEM_MANAGER::instance().CreateDropItem(this, pkAttacker, s_vec_item))
	{

#ifdef ENABLE_RARE_DROP_NOTICE_RAZOR93
		for (const entt::entity dropItem : s_vec_item)
		{
			if (verjema_szadba_ixtreeme.find(ItemSystem::GetItemVnum(dropItem)) != verjema_szadba_ixtreeme.end())
			{
		std::string message = MakeItemLink(dropItem, pkAttacker, this);
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

		if (GetDungeon() && pkAttacker && ecs::PlayerRuntime::IsPC((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)) && !s_vec_item.empty())
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
				if (ecs::SocialSystem::GetParty((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null))) // CSAK partyra
				{
					CDungeon* pDungeon = GetDungeon();

					// csak akkor, ha a killer ugyanebben a dungeon instance-ben van
					if (pkAttacker->GetDungeon() == pDungeon)
					{
						// --- helper: HWID|HOST kulcs ugyanugy, ahogy nalad masutt is ---
						auto MakeHwidHostKey = [&](LegacyCharHandle ch) -> std::string
							{
								if (!ch || !ecs::PlayerRuntime::IsPC((ch ? ch->GetEntityHandle() : entt::null)) || !ecs::PlayerRuntime::GetDesc((ch ? ch->GetEntityHandle() : entt::null)))
									return std::string();

								DESC* d = ecs::PlayerRuntime::GetDesc((ch ? ch->GetEntityHandle() : entt::null));
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
								if (!mch || !ecs::PlayerRuntime::IsPC((mch ? mch->GetEntityHandle() : entt::null)) || !ecs::PlayerRuntime::GetDesc((mch ? mch->GetEntityHandle() : entt::null)))
									return;

								// ugyanabban a dungeon instance-ben kell legyen
								if (mch->GetDungeon() != pDungeon)
									return;

								//   ugyanazon a mapindexen legyen (INSTANCE) -> NINCS hibas normalizalas
								if (ecs::PlayerRuntime::GetMapIndex((mch ? mch->GetEntityHandle() : entt::null)) != lMapIndex)
									return;

								// ugyanabban a partyban legyen
								if (ecs::SocialSystem::GetParty((mch ? mch->GetEntityHandle() : entt::null)) != ecs::SocialSystem::GetParty((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)))
									return;

								std::string key = MakeHwidHostKey(mch);

								// ha nincs hwid/host, fallback: account (ne kapjon duplan)
								if (key.empty())
									key = "ACC:" + std::to_string(ecs::PlayerRuntime::GetDesc((mch ? mch->GetEntityHandle() : entt::null))->GetAccountTable().id);

								auto it = mapWinnerByKey.find(key);
								if (it == mapWinnerByKey.end())
								{
									mapWinnerByKey.emplace(std::move(key), mch);
									return;
								}

								// dupe HWID|HOST: a legtobb dmg-et okozo kap
								uint64_t dmgNew = 0;
								uint64_t dmgOld = 0;

								auto itNew = m_map_kDamage.find(mch->GetEntityHandle());
								if (itNew != m_map_kDamage.end())
									dmgNew = itNew->second.iTotalDamage;

								auto itOld = m_map_kDamage.find(it->second->GetEntityHandle());
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

							for (const entt::entity srcItem : s_vec_item)
							{
								if (!ItemSystem::IsValidItem(srcItem))
									continue;

								SPartySharedDropItem di{};
								di.vnum = ItemSystem::GetItemVnum(srcItem);
								di.count = ItemSystem::GetItemCount(srcItem);

								for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
									di.sockets[i] = ItemSystem::GetItemSocket(srcItem, i);

								for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
									di.attrs[i] = ItemSystem::GetItemAttribute(srcItem, i);

								drops.push_back(di);
							}

							// 3) kiosztas: minden HWID-unique winnernek ugyanaz a drop (ground + ownership)
							for (const auto& kv : mapWinnerByKey)
							{
								auto* rch = kv.second;
								if (!rch || !ecs::PlayerRuntime::IsPC((rch ? rch->GetEntityHandle() : entt::null)) || !ecs::PlayerRuntime::GetDesc((rch ? rch->GetEntityHandle() : entt::null)))
									continue;

								PIXEL_POSITION mpos = pos;

								// kis eltolas, hogy ne 1 pontra essen minden
								mpos.x = number(-7, 7) * 20 + GetX();
								mpos.y = number(-7, 7) * 20 + GetY();

								for (const auto& di : drops)
								{
									const entt::entity newItem =
										ItemSystem::CreateItemEcs(di.vnum, di.count);
									if (!ItemSystem::IsValidItem(newItem))
										continue;

									for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
										ItemSystem::SetItemSocket(newItem, i, di.sockets[i]);

									for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
										ItemSystem::SetItemAttribute(
											newItem, i, di.attrs[i].bType, di.attrs[i].sValue);

#ifdef ENABLE_DROP_INSTANT_INVENTORY
									if (bInstantRewardToInventory)
									{
										__GiveRewardItemToCharacterOrDrop(rch, this, newItem, mpos, true);
									}
									else
									{
										if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
												newItem, lMapIndex, mpos, 300))
										{
											ItemSystem::DestroyItemEntityEcs(
												newItem, "SHARED_DROP_PLACE_FAIL");
											continue;
										}

										if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex((rch ? rch->GetEntityHandle() : entt::null))) == false)
											ItemSystem::SetGroundOwnershipLegacyBoundary(
												newItem, (rch ? rch->GetEntityHandle() : entt::null));
									}
#else
									if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
											newItem, lMapIndex, mpos, 300))
									{
										ItemSystem::DestroyItemEntityEcs(
											newItem, "SHARED_DROP_PLACE_FAIL");
										continue;
									}

									if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex((rch ? rch->GetEntityHandle() : entt::null))) == false)
										ItemSystem::SetGroundOwnershipLegacyBoundary(
											newItem, (rch ? rch->GetEntityHandle() : entt::null));
#endif
								}
							}

							// 4) a template itemeket megsemmisitjuk, hogy ne duplazzon
							for (const entt::entity srcItem : s_vec_item)
							{
								if (ItemSystem::IsValidItem(srcItem))
									ItemSystem::DestroyItemEntityEcs(
										srcItem,
										"COMBAT_SHARED_DROP_TEMPLATE");
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
				itemEntity = s_vec_item[0];
				if (!ItemSystem::IsValidItem(itemEntity))
				{
					LOG_ERROR("invalid item entity in single drop");
					m_map_kDamage.clear();
					return;
				}

#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
				const bool bKeepGroundDrop = (pkAttacker && ecs::SocialSystem::GetParty((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)));
#else
				const bool bKeepGroundDrop = false;
#endif

				if (bInstantRewardToInventory && !bKeepGroundDrop)
				{
					__GiveRewardItemToCharacterOrDrop(pkAttacker, this, itemEntity, pos, true);
				}
				else
				{
					if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
							itemEntity, GetMapIndex(), pos, 300))
					{
						LOG_ERROR("failed to place single drop entity {}",
							static_cast<uint32_t>(itemEntity));
						m_map_kDamage.clear();
						return;
					}

					if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null))) == false)
					{
#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
						if (ecs::SocialSystem::GetParty((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)))
						{
							FPartyDropDiceRoll f(itemEntity, pkAttacker);
							f.Process(this);
						}
						else
							ItemSystem::SetGroundOwnershipLegacyBoundary(
								itemEntity, (pkAttacker ? pkAttacker->GetEntityHandle() : entt::null));
#else
						ItemSystem::SetGroundOwnershipLegacyBoundary(
							itemEntity, (pkAttacker ? pkAttacker->GetEntityHandle() : entt::null));
#endif
					}

					LOG_INFO("DROP_ITEM: {} {} {} from {}",
						ItemSystem::GetItemName(itemEntity), pos.x, pos.y, GetName());
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
						itemEntity = s_vec_item[iItemIdx--];

						if (!ItemSystem::IsValidItem(itemEntity))
						{
							LOG_ERROR("item null in vector idx {}", iItemIdx + 1);
							continue;
						}

						if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
								itemEntity, GetMapIndex(), pos, 300))
							continue;

						if (pkAttacker && CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null))) == false)
							ItemSystem::SetGroundOwnershipLegacyBoundary(
								itemEntity, (pkAttacker ? pkAttacker->GetEntityHandle() : entt::null));

						LOG_INFO("DROP_ITEM: {} {} {} by {}",
							ItemSystem::GetItemName(itemEntity), pos.x, pos.y, GetName());

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
						itemEntity = s_vec_item[iItemIdx--];

						if (!ItemSystem::IsValidItem(itemEntity))
						{
							LOG_ERROR("item null in vector idx {}", iItemIdx + 1);
							continue;
						}

						auto* ch = *it;

						if (ecs::SocialSystem::GetParty((ch ? ch->GetEntityHandle() : entt::null)))
							ch = ecs::SocialSystem::GetParty((ch ? ch->GetEntityHandle() : entt::null))->GetNextOwnership(ch, GetX(), GetY());

						++it;

						if (it == v.end())
							it = v.begin();

#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
						const bool bKeepGroundDrop = (ch && ecs::SocialSystem::GetParty((ch ? ch->GetEntityHandle() : entt::null)));
#else
						const bool bKeepGroundDrop = false;
#endif

						if (bInstantRewardToInventory && !bKeepGroundDrop)
						{
							__GiveRewardItemToCharacterOrDrop(ch, this, itemEntity, pos, true);
						}
						else
						{
							if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
									itemEntity, GetMapIndex(), pos, 300))
								continue;

							if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex((ch ? ch->GetEntityHandle() : entt::null))) == false)
							{
#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
								if (ecs::SocialSystem::GetParty((ch ? ch->GetEntityHandle() : entt::null)))
								{
									FPartyDropDiceRoll f(itemEntity, ch);
									f.Process(this);
								}
								else
									ItemSystem::SetGroundOwnershipLegacyBoundary(
										itemEntity, (ch ? ch->GetEntityHandle() : entt::null));
#else
								ItemSystem::SetGroundOwnershipLegacyBoundary(
									itemEntity, (ch ? ch->GetEntityHandle() : entt::null));
#endif
							}

							LOG_INFO("DROP_ITEM: {} {} {} by {}",
								ItemSystem::GetItemName(itemEntity), pos.x, pos.y, GetName());
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
				itemEntity = s_vec_item[0];
				if (!ItemSystem::IsValidItem(itemEntity))
				{
					LOG_ERROR("invalid item entity in single ground drop");
					m_map_kDamage.clear();
					return;
				}
				if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
						itemEntity, GetMapIndex(), pos, 300))
				{
					LOG_ERROR("failed to place single ground drop entity {}",
						static_cast<uint32_t>(itemEntity));
					m_map_kDamage.clear();
					return;
				}

				if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null))) == false)
				{
#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
					if (ecs::SocialSystem::GetParty((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)))
					{
						FPartyDropDiceRoll f(itemEntity, pkAttacker);
						f.Process(this);
					}
					else
						ItemSystem::SetGroundOwnershipLegacyBoundary(
							itemEntity, (pkAttacker ? pkAttacker->GetEntityHandle() : entt::null));
#else
					ItemSystem::SetGroundOwnershipLegacyBoundary(
						itemEntity, (pkAttacker ? pkAttacker->GetEntityHandle() : entt::null));
#endif
				}

				pos.x = number(-7, 7) * 20;
				pos.y = number(-7, 7) * 20;
				pos.x += GetX();
				pos.y += GetY();

				LOG_INFO("DROP_ITEM: {} {} {} from {}",
					ItemSystem::GetItemName(itemEntity), pos.x, pos.y, GetName());
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
						itemEntity = s_vec_item[iItemIdx--];

						if (!ItemSystem::IsValidItem(itemEntity))
						{
							LOG_ERROR("item null in vector idx {}", iItemIdx + 1);
							continue;
						}

						if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
								itemEntity, GetMapIndex(), pos, 300))
							continue;

						if (pkAttacker && CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null))) == false)
							ItemSystem::SetGroundOwnershipLegacyBoundary(
								itemEntity, (pkAttacker ? pkAttacker->GetEntityHandle() : entt::null));

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += GetX();
						pos.y += GetY();

						LOG_INFO("DROP_ITEM: {} {} {} by {}",
							ItemSystem::GetItemName(itemEntity), pos.x, pos.y, GetName());
					}
				}
				else
				{
					std::vector<LegacyCharHandle>::iterator it = v.begin();

					while (iItemIdx >= 0)
					{
						itemEntity = s_vec_item[iItemIdx--];

						if (!ItemSystem::IsValidItem(itemEntity))
						{
							LOG_ERROR("item null in vector idx {}", iItemIdx + 1);
							continue;
						}

						if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
								itemEntity, GetMapIndex(), pos, 300))
							continue;

						auto* ch = *it;

						if (ecs::SocialSystem::GetParty((ch ? ch->GetEntityHandle() : entt::null)))
							ch = ecs::SocialSystem::GetParty((ch ? ch->GetEntityHandle() : entt::null))->GetNextOwnership(ch, GetX(), GetY());

						++it;

						if (it == v.end())
							it = v.begin();

						if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex((ch ? ch->GetEntityHandle() : entt::null))) == false)
						{
#ifdef ENABLE_DICE_SYSTEM_OFFOLVA
							if (ecs::SocialSystem::GetParty((ch ? ch->GetEntityHandle() : entt::null)))
							{
								FPartyDropDiceRoll f(itemEntity, ch);
								f.Process(this);
							}
							else
								ItemSystem::SetGroundOwnershipLegacyBoundary(
									itemEntity, (ch ? ch->GetEntityHandle() : entt::null));
#else
							ItemSystem::SetGroundOwnershipLegacyBoundary(
								itemEntity, (ch ? ch->GetEntityHandle() : entt::null));
#endif
						}

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += GetX();
						pos.y += GetY();

						LOG_INFO("DROP_ITEM: {} {} {} by {}",
							ItemSystem::GetItemName(itemEntity), pos.x, pos.y, GetName());
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

	if (!pkAttacker || !ecs::PlayerRuntime::IsPC((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)))
		return;

	if (!m_pkMobData)
	{
		LOG_ERROR("RewardGold: NULL mob data (vid={} race={} name={} map={} x={} y={} attacker={})", GetPacketVID(), GetRaceNum(), GetName(), GetMapIndex(), GetX(), GetY(), pkAttacker ? ecs::PlayerRuntime::GetName((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)).data() : "<null>");
		return;
	}
	if (pkAttacker && ecs::PlayerRuntime::IsPC((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null))) {
		if (IsStone()) {
#ifdef ENABLE_ANTICHEAT
			if (ecs::PlayerRuntime::GetMapIndex((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)) < 1000) {
				pkAttacker->ProcessCheatCheck(get_global_time());
			}
#endif
#ifdef DISABLE_GOLD_DROP_FROM_TAKAKA
			if (GetRaceNum() >= TANAKA) {
				return;
			}
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
			if (AffectSystem::FindAffect((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), AFFECT_DROP_BLOCK, APPLY_NONE)) {
				return;
			}
#endif

			bool drop = true;
			int mylvl = ecs::PointSystem::GetLevel((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)), targetlvl = GetLevel();
			if (mylvl > targetlvl) {
				drop = mylvl - targetlvl <= 15 ? true : false;
			}

			if (drop) {
				int64_t gold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);

				if (gold <= 0) {
					return;
				}

				if (ecs::PointSystem::Get((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_MALL_GOLDBONUS)) {
					gold += (gold * ecs::PointSystem::Get((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_MALL_GOLDBONUS) / 100);
				}

				ecs::PointSystem::Change((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_GOLD, gold, true);
			}
		}
		else {
#ifdef ENABLE_BLOCK_MULTIFARM
			if (AffectSystem::FindAffect((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), AFFECT_DROP_BLOCK, APPLY_NONE)) {
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

			if (ecs::PlayerRuntime::IsPC((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)))
				iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP)) / 100;

#ifdef ENABLE_EVENT_MANAGER
			if (ecs::PlayerRuntime::IsPC((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)))
			{
				const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(YANG_DROP_EVENT, ecs::PlayerRuntime::GetEmpire((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)));
				if (event != nullptr)
					iGoldPercent = iGoldPercent * (100 + (event->value[0] + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP))) / 100;
				else
					iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP)) / 100;
			}
#else
			if (ecs::PlayerRuntime::IsPC((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)))
				iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP)) / 100;
#endif

			iGoldPercent = iGoldPercent * CHARACTER_MANAGER::instance().GetMobGoldDropRate(((pkAttacker) ? (pkAttacker)->GetEntityHandle() : entt::null)) / 100;

			// ADD_PREMIUM
			if (pkAttacker->GetPremiumRemainSeconds(PREMIUM_GOLD) > 0 ||
				pkAttacker->IsEquipUniqueGroup(UNIQUE_GROUP_LUCKY_GOLD))
				iGoldPercent += iGoldPercent;
			// END_OF_ADD_PREMIUM

			if (iGoldPercent > 100)
				iGoldPercent = 100;

			int iPercent;

			if (GetMobRank() >= MOB_RANK_BOSS)
				iPercent = ((iGoldPercent * PERCENT_LVDELTA_BOSS(ecs::PointSystem::GetLevel((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)), GetLevel())) / 100);
			else
				iPercent = ((iGoldPercent * PERCENT_LVDELTA(ecs::PointSystem::GetLevel((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)), GetLevel())) / 100);
			//int iPercent = CALCULATE_VALUE_LVDELTA(ecs::PointSystem::GetLevel((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)), GetLevel(), iGoldPercent);

			if (number(1, 100) > iPercent)
				return;

			int iGoldMultipler = 1;

			if (1 == number(1, 50000)) // 1/50000 Ȯ  10
				iGoldMultipler *= 10;
			else if (1 == number(1, 10000)) // 1/10000 Ȯ  5
				iGoldMultipler *= 5;

			//
			if (ecs::PointSystem::Get((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_GOLD_DOUBLE_BONUS))
				if (number(1, 100) <= ecs::PointSystem::Get((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), POINT_GOLD_DOUBLE_BONUS))
					iGoldMultipler *= 2;

			//
			// ---------     ----------
			//
			if (test_server)
				ecs::ChatSystem::Send((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), CHAT_TYPE_PARTY, "gold_mul %d rate %d", iGoldMultipler, CHARACTER_MANAGER::instance().GetMobGoldAmountRate(((pkAttacker) ? (pkAttacker)->GetEntityHandle() : entt::null)));

			//
			// ---------   ó -------------
			//
			int iGold10DropPct = 100;
#ifdef ENABLE_EVENT_MANAGER
			const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(YANG_DROP_EVENT, ecs::PlayerRuntime::GetEmpire((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)));
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
						LOG_INFO("iGold {}", iGold);
					iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(((pkAttacker) ? (pkAttacker)->GetEntityHandle() : entt::null)) / 100;
					iGold *= iGoldMultipler;

					if (iGold == 0)
					{
						continue;
					}

					if (test_server)
					{
						LOG_TRACE("Drop Moeny MobGoldAmountRate {} {}", CHARACTER_MANAGER::instance().GetMobGoldAmountRate(((pkAttacker) ? (pkAttacker)->GetEntityHandle() : entt::null)), iGoldMultipler);
						LOG_TRACE("Drop Money gold {} GoldMin {} GoldMax {}", iGold, GetMobTable().dwGoldMax, GetMobTable().dwGoldMax);
					}

#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93
					pkAttacker->GiveGold(iGold);
					iTotalGold += iGold;
#else
					const entt::entity gold = ItemSystem::CreateItemEcs(1, iGold);
					if (ItemSystem::IsValidItem(gold))
					{
						pos.x = GetX() + ((number(-14, 14) + number(-14, 14)) * 23);
						pos.y = GetY() + ((number(-14, 14) + number(-14, 14)) * 23);
						if (ItemSystem::PlaceItemOnGroundLegacyBoundary(
								gold, GetMapIndex(), pos, 300))
							iTotalGold += iGold;
						else
							ItemSystem::DestroyItemEntityEcs(gold, "GOLD_DROP_PLACE_FAIL");
					}
#endif
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
					iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(((pkAttacker) ? (pkAttacker)->GetEntityHandle() : entt::null)) / 100;
					iGold *= iGoldMultipler;

					if (iGold == 0)
					{
						continue;
					}

#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93
					pkAttacker->GiveGold(iGold);
					iTotalGold += iGold;
#else
					const entt::entity gold = ItemSystem::CreateItemEcs(1, iGold);
					if (ItemSystem::IsValidItem(gold))
					{
						pos.x = GetX() + (number(-7, 7) * 20);
						pos.y = GetY() + (number(-7, 7) * 20);
						if (ItemSystem::PlaceItemOnGroundLegacyBoundary(
								gold, GetMapIndex(), pos, 300))
							iTotalGold += iGold;
						else
							ItemSystem::DestroyItemEntityEcs(gold, "GOLD_DROP_PLACE_FAIL");
					}
#endif

				}
			}
			else
			{
				//
				// Ϲ
				//
				int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);
				iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(((pkAttacker) ? (pkAttacker)->GetEntityHandle() : entt::null)) / 100;
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
						else
						{
#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93
							pkAttacker->GiveGold(splitGold);
#else
							const entt::entity gold = ItemSystem::CreateItemEcs(1, splitGold);
							if (ItemSystem::IsValidItem(gold))
							{
								pos.x = GetX() + (number(-7, 7) * 20);
								pos.y = GetY() + (number(-7, 7) * 20);
								if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
										gold, GetMapIndex(), pos, 300))
									ItemSystem::DestroyItemEntityEcs(
										gold, "GOLD_DROP_PLACE_FAIL");
							}
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
	if (pAttacker && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) && IsPC() && GetMapIndex() == 1)
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
		const entt::entity attackerEntity = (pAttacker ? pAttacker->GetEntityHandle() : entt::null);
		const bool hasWeapon = ItemSystem::IsValidItem(
			ItemSystem::GetWearItem(attackerEntity, WEAR_WEAPON));
		if (AffectSystem::IsAffectFlag(attackerEntity, AFF_GWIGUM) && !hasWeapon)
		{
			AffectSystem::RemoveAffect(attackerEntity, SKILL_GWIGEOM);
			return false;
		}

		if (AffectSystem::IsAffectFlag(attackerEntity, AFF_GEOMGYEONG) && !hasWeapon)
		{
			AffectSystem::RemoveAffect(attackerEntity, SKILL_GEOMKYUNG);
			return false;
		}

	}

	if ((IsPC() && IsAffectFlag(AFF_REVIVE_INVISIBLE)) || (pAttacker && (ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) && AffectSystem::IsAffectFlag((pAttacker ? pAttacker->GetEntityHandle() : entt::null), AFF_REVIVE_INVISIBLE))))
		return false;

#ifdef ENABLE_NEWSTUFF
	if (pAttacker && IsStone() && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
	{
		if (GetEmpire() && GetEmpire() == ecs::PlayerRuntime::GetEmpire((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
		{
			SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
			return false;
		}
	}
#endif

	if (DAMAGE_TYPE_MAGIC == type)
	{
		dam = (int)((float)dam * (100 + (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_MAGIC_ATT_BONUS_PER) + ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100.f + 0.5f);
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
	if (pAttacker && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) && GetMapIndex() == 1 && (IsMonster() || IsStone()))
	{
#ifdef DISABLE_DAMAGE_TYPE_NORMAL_RANGE_EVENT_MAP



		const entt::entity weapon = ItemSystem::GetWearItem(
			(pAttacker ? pAttacker->GetEntityHandle() : entt::null), WEAR_WEAPON);
		const TItemTable* weaponProto = ItemSystem::GetItemProto(weapon);
		if (weaponProto && weaponProto->bSubType == WEAPON_BOW)
		{

			SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
			return false;
		}
#endif // !DISABLE_DAMAGE_TYPE_NORMAL_RANGE_EVENT_MAP
		const int64_t fixed_dam = 100000;

		// [1] Regisztrljuk a sebzst a dropphoz
		const entt::entity eAttacker = pAttacker ? pAttacker->GetEntityHandle() : entt::null;
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
			int iCriticalPct = ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_CRITICAL_PCT);

			if (!IsPC()) {
				iCriticalPct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);
				iCriticalPct += ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_PVM_CRITICAL_PCT);
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
					NetworkSyncSystem::BroadcastEffect(g_registry, GetEntityHandle(), SE_CRITICAL);

					if (IsAffectFlag(AFF_MANASHIELD))
					{
						RemoveAffect(AFF_MANASHIELD);
					}
				}
			}

			//
			int iPenetratePct = ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_PENETRATE_PCT);

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
						ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 257, "%d", GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100);
					}
#endif
					dam += GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100;

					if (IsAffectFlag(AFF_MANASHIELD))
					{
						RemoveAffect(AFF_MANASHIELD);
					}
#ifdef ENABLE_EFFECT_PENETRATE
					NetworkSyncSystem::BroadcastEffect(g_registry, GetEntityHandle(), SE_PENETRATE);
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
					ecs::ChatSystem::SendNew((pAttacker ? pAttacker->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 95, "%s#%d", GetName(), GetPoint(POINT_BLOCK));
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 95, "%s#%d", ecs::PlayerRuntime::GetName((pAttacker ? pAttacker->GetEntityHandle() : entt::null)).data(), ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_BLOCK));
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
					ecs::ChatSystem::SendNew((pAttacker ? pAttacker->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 96, "%s#%d", GetName(), GetPoint(POINT_DODGE));
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 96, "%s#%d", ecs::PlayerRuntime::GetName((pAttacker ? pAttacker->GetEntityHandle() : entt::null)).data(), ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_DODGE));
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
			if (pAttacker && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) && IsPC())
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
			int iCriticalPct = ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_CRITICAL_PCT);

			if (!IsPC()) {
				iCriticalPct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);
				iCriticalPct += ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_PVM_CRITICAL_PCT);
			}

			if (iCriticalPct)
			{
				//ũƼ   .
				iCriticalPct -= GetPoint(POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					NetworkSyncSystem::BroadcastEffect(g_registry, GetEntityHandle(), SE_CRITICAL);
				}
			}

			//
			int iPenetratePct = ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_PENETRATE_PCT);

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
					NetworkSyncSystem::BroadcastEffect(g_registry, GetEntityHandle(), SE_PENETRATE);
#endif
				}
			}

#ifdef ENABLE_BUG_FIXES
			if (int64_t iStealHP_ptr = ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_HP)) {
				if (number(1, 100) <= iStealHP_ptr) {
					int64_t iHP = std::min((int64_t)dam, std::max((int64_t)0, GetHP())) * ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_HP) / 100;


					if ((pAttacker->GetHP() > 0) && (pAttacker->GetHP() + iHP < ecs::PointSystem::GetMaxHP((pAttacker ? pAttacker->GetEntityHandle() : entt::null))) && (GetHP() > 0) && (iHP > 0)) {
						CreateFly(FLY_HP_MEDIUM, pAttacker);
						ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HP, iHP);
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

			if (int64_t iStealSP_ptr = ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_SP)) {
				if (IsPC() && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null))) {
					if (number(1, 100) <= iStealSP_ptr) {
						int64_t iSP = std::min((int64_t)dam, std::max((int64_t)0, GetSP())) * ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_SP) / 100;


						if ((pAttacker->GetSP() > 0) && (pAttacker->GetSP() + iSP < ecs::PointSystem::GetMaxSP((pAttacker ? pAttacker->GetEntityHandle() : entt::null))) && (GetSP() > 0) && (iSP > 0))
						{
							CreateFly(FLY_SP_MEDIUM, pAttacker);
							ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_SP, iSP);
							PointChange(POINT_SP, -iSP);
						}
					}
				}
			}
#else
			// HP ƿ
			if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_HP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iHP = MIN(dam, MAX(0, iCurHP)) * ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_HP) / 100;

					if (iHP > 0 && GetHP() >= iHP)
					{
						CreateFly(FLY_HP_SMALL, pAttacker);
						ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HP, iHP);
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
			if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_SP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iCur;

					if (IsPC())
						iCur = iCurSP;
					else
						iCur = iCurHP;

					int iSP = MIN(dam, MAX(0, iCur)) * ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_SP) / 100;

					if (iSP > 0 && iCur >= iSP)
					{
						CreateFly(FLY_SP_SMALL, pAttacker);
						ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_SP, iSP);

						if (IsPC())
							PointChange(POINT_SP, -iSP);
					}
				}
			}
#endif

			//  ƿ
			if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_GOLD))
			{
				if (number(1, 100) <= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_STEAL_GOLD))
				{
					int iAmount = number(1, GetLevel());
					ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_GOLD, iAmount);
					DBManager::instance().SendMoneyLog(MONEY_LOG_MISC, 1, iAmount);
				}
			}

#ifdef ENABLE_BUG_FIXES
			int iAbsoHP_ptr = ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HIT_HP_RECOVERY);
			if (iAbsoHP_ptr > 0) {
				if (number(1, 100) <= iAbsoHP_ptr) {
					int iHPAbso = std::min(dam, GetHP()) * ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HIT_HP_RECOVERY) / 100;
					if ((pAttacker->GetHP() > 0) && (pAttacker->GetHP() + iHPAbso < ecs::PointSystem::GetMaxHP((pAttacker ? pAttacker->GetEntityHandle() : entt::null))) && (GetHP() > 0) && (iHPAbso > 0)) {
						CreateFly(FLY_HP_SMALL, pAttacker);
						ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HP, iHPAbso);
					}
				}
			}

			int64_t iAbsoSP_ptr = ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HIT_SP_RECOVERY);
			if (iAbsoSP_ptr > 0) {
				if (number(1, 100) <= iAbsoSP_ptr) {
					int64_t iSPAbso = std::min(dam, GetSP()) * ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HIT_SP_RECOVERY) / 100;
					if ((pAttacker->GetSP() > 0) && (pAttacker->GetSP() + iSPAbso < ecs::PointSystem::GetMaxSP((pAttacker ? pAttacker->GetEntityHandle() : entt::null))) && (GetSP() > 0) && (iSPAbso > 0)) {
						CreateFly(FLY_SP_SMALL, pAttacker);
						ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_SP, iSPAbso);
					}
				}
			}
#else
			// ĥ  HPȸ
			if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HIT_HP_RECOVERY) && number(0, 4) > 0) // 80% Ȯ
			{
				int i = ((iCurHP >= 0) ? MIN(dam, iCurHP) : dam) * ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HIT_HP_RECOVERY) / 100; //@fixme107

				if (i)
				{
					CreateFly(FLY_HP_SMALL, pAttacker);
					ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HP, i);
				}
			}

			// ĥ  SPȸ
			if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HIT_SP_RECOVERY) && number(0, 4) > 0) // 80% Ȯ
			{
				int i = ((iCurHP >= 0) ? MIN(dam, iCurHP) : dam) * ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_HIT_SP_RECOVERY) / 100; //@fixme107

				if (i)
				{
					CreateFly(FLY_SP_SMALL, pAttacker);
					ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_SP, i);
				}
			}
#endif

			//   ش.
			if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_MANA_BURN_PCT))
			{
				if (number(1, 100) <= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_MANA_BURN_PCT))
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
			if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DAMAGE_BONUS))
				dam = dam * (100 + ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;
#ifdef ENABLE_MEDI_PVM
			if (IsNPC())
				dam = dam * (100 + ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_MEDI_PVM)) / 100;
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
			const int64_t skillBonus = ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_SKILL_DAMAGE_BONUS);
			if (skillBonus)
				dam = dam * (100 + skillBonus) / 100;
		}

		int64_t def = GetPoint(POINT_SKILL_DEFEND_BONUS);
		def = std::clamp<int64_t>(def, 0, 100);

		if (pAttacker && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) && IsPC())
			def = (def * 75 + 50) / 100;

		dam = dam * (100 - def) / 100;


		if (pAttacker && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) && IsNPC())
		{
			const int64_t normalRef = CalcReferenceBasicHitDamage(pAttacker, this);
			if (normalRef > 0)
			{
				int64_t minSkillDam = normalRef * 10;

				//const int64_t skillBonus = std::max<int64_t>(0, ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_SKILL_DAMAGE_BONUS));
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
		if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_MALL_ATTBONUS) > 0)
		{
			int64_t add_dam = std::min((int64_t)300, dam * pAttacker->GetLimitPoint(POINT_MALL_ATTBONUS) / 100);
			dam += add_dam;
		}

		if (ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
		{
			int iEmpire = ecs::PlayerRuntime::GetEmpire((pAttacker ? pAttacker->GetEntityHandle() : entt::null));
			int32_t lMapIndex = ecs::PlayerRuntime::GetMapIndex((pAttacker ? pAttacker->GetEntityHandle() : entt::null));
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
					ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_SP, -iDrain);
				else
				{
					int iSP = pAttacker->GetSP();
					ecs::PointSystem::Change((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_SP, -iSP);
				}
			}

		}
		else if (pAttacker->IsGuardNPC())
		{
						if (auto* flags = RuntimeFlags(GetEntityHandle()))
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
	if (pAttacker && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
	{
		int iDmgPct = CHARACTER_MANAGER::instance().GetUserDamageRate(((pAttacker) ? (pAttacker)->GetEntityHandle() : entt::null));
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
		if (!IsPC() && pAttacker && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
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
						LPPARTY party = ecs::SocialSystem::GetParty((pAttacker ? pAttacker->GetEntityHandle() : entt::null));
						if (party)
						{
							if (party->GetLeaderPID() == ecs::PlayerRuntime::GetPlayerID((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
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

		if (pAttacker && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) && pAttacker->IsSkillHit() && IsPC())
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
				ecs::PlayerRuntime::GetPlayerID((pAttacker ? pAttacker->GetEntityHandle() : entt::null))
			);
			CheckLeaderboardSkillMobChanges();
			if (GetMapIndex() == 41) {
				CHARACTER_MANAGER::instance().for_each_pc([](LegacyCharHandle ch) {
					ch->SendLeaderboardDataSkillMob(ch);
					});


			}



			ecs::ChatSystem::Send((pAttacker ? pAttacker->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Skill damage recorded: %d vs %s", dam, GetName());
		}
#endif
		if (test_server)
		{
			int iTmpPercent = 0; // @fixme136
			if (GetMaxHP() >= 0)
				iTmpPercent = (GetHP() * 100) / GetMaxHP();

			if (pAttacker)
			{
				ecs::ChatSystem::Send((pAttacker ? pAttacker->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "-> %s, DAM %d HP %d(%d%%) %s%s",
					GetName(),
					dam,
					GetHP(),
					iTmpPercent,
					IsCritical ? "crit " : "",
					IsPenetrate ? "pene " : "",
					IsDeathBlow ? "deathblow " : "");
			}

			ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_PARTY, "<- %s, DAM %d HP %d(%d%%) %s%s",
				pAttacker ? ecs::PlayerRuntime::GetName((pAttacker ? pAttacker->GetEntityHandle() : entt::null)).data() : nullptr,
				dam,
				GetHP(),
				iTmpPercent,
				IsCritical ? "crit " : "",
				IsPenetrate ? "pene " : "",
				IsDeathBlow ? "deathblow " : "");
		}

#ifdef ENABLE_RANKING
		if (ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null))) {
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
		if (!IsPC() && pAttacker && ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
		{
			pAttacker->SetQuestDamage(GetRaceNum(), dam);
			pAttacker->SetQuestNPCID(GetPacketVID());
			quest::CQuestManager::instance().QuestDamage(ecs::PlayerRuntime::GetPlayerID((pAttacker ? pAttacker->GetEntityHandle() : entt::null)), GetRaceNum());
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
		const entt::entity eAttacker = pAttacker ? pAttacker->GetEntityHandle() : entt::null;
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
		if (pAttacker && !ecs::PlayerRuntime::IsNPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
			m_dwKillerPID = ecs::PlayerRuntime::GetPlayerID((pAttacker ? pAttacker->GetEntityHandle() : entt::null));
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

		if (pAttacker && !ecs::PlayerRuntime::IsNPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)))
			m_dwKillerPID = ecs::PlayerRuntime::GetPlayerID((pAttacker ? pAttacker->GetEntityHandle() : entt::null));
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
//	//	ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "Nincs leaderboard adat.");
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
//	//	ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "Nincs leaderboard adat.");
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

void CHARACTER::UseArrow(entt::entity pkArrow, uint32_t dwArrowCount)
{
	int iCount = ItemSystem::GetItemCount(pkArrow);
	uint32_t dwVnum = ItemSystem::GetItemVnum(pkArrow);
#if !defined(__INFINITE_ARROW__)
	iCount = iCount - MIN(iCount, dwArrowCount);
#endif
	ItemSystem::SetItemCountEcs(pkArrow, iCount);

	if (iCount == 0)
	{
		const entt::entity newArrow = ItemSystem::FindSpecifyItem(
			GetEntityHandle(), dwVnum
#ifdef ENABLE_EXTRA_INVENTORY
			, false
#endif
		);

		LOG_INFO("UseArrow : FindSpecifyItem {} entity {}", dwVnum,
			static_cast<uint32_t>(newArrow));

		if (ItemSystem::IsValidItem(newArrow))
			ItemSystem::EquipItemEcs(GetEntityHandle(), newArrow);
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
		if (!battle_is_attackable((m_me ? m_me->GetEntityHandle() : entt::null), (pkVictim ? pkVictim->GetEntityHandle() : entt::null)))
			return;

		if (ecs::PlayerRuntime::IsNPC((m_me ? m_me->GetEntityHandle() : entt::null)))
		{
			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX((m_me ? m_me->GetEntityHandle() : entt::null)) - ecs::PlayerRuntime::GetX((pkVictim ? pkVictim->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((m_me ? m_me->GetEntityHandle() : entt::null)) - ecs::PlayerRuntime::GetY((pkVictim ? pkVictim->GetEntityHandle() : entt::null))) > 5000)
				return;
		}

		entt::entity pkBow = entt::null, pkArrow = entt::null;

		switch (m_bType)
		{
		case 0: // ϹȰ
		{
			int iDam = 0;

			if (ecs::PlayerRuntime::IsPC((m_me ? m_me->GetEntityHandle() : entt::null)))
			{
				if (m_me->GetJob() != JOB_ASSASSIN)
					return;

				if (0 == m_me->GetArrowAndBow(&pkBow, &pkArrow))
					return;

				if (m_me->GetSkillGroup() != 0)
					if (!ecs::PlayerRuntime::IsNPC((m_me ? m_me->GetEntityHandle() : entt::null)) && m_me->GetSkillGroup() != 2)
					{
						if (m_me->GetSP() < 5)
							return;

						ecs::PointSystem::Change((m_me ? m_me->GetEntityHandle() : entt::null), POINT_SP, -5);
					}

				iDam = CalcArrowDamage((m_me ? m_me->GetEntityHandle() : entt::null), (pkVictim ? pkVictim->GetEntityHandle() : entt::null), pkBow, pkArrow);
				m_me->UseArrow(pkArrow, 1);

#ifdef ENABLE_ANTICHEAT
				if (IS_SPEED_HACK((m_me ? m_me->GetEntityHandle() : entt::null), (pkVictim ? pkVictim->GetEntityHandle() : entt::null), get_dword_time())) {
					iDam = 0;
				}
#endif
			}
			else
				iDam = CalcMeleeDamage((m_me ? m_me->GetEntityHandle() : entt::null), (pkVictim ? pkVictim->GetEntityHandle() : entt::null));

			NormalAttackAffect((m_me ? m_me->GetEntityHandle() : entt::null), (pkVictim ? pkVictim->GetEntityHandle() : entt::null));

			//   (nyl vdelem)
			int32_t lValue = ecs::PointSystem::Get((pkVictim ? pkVictim->GetEntityHandle() : entt::null), POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get((m_me ? m_me->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get((m_me ? m_me->GetEntityHandle() : entt::null), POINT_IRR_WEAPON_DEFENSE);
#endif

			if (lValue < 0)   lValue = 0;
			if (lValue > 100) lValue = 100;

			iDam = (int)((int64_t)iDam * (100 - lValue) / 100);
			//iDam = (int)((int64_t)iDam * (100 - lValue) * 20 / 10000);

#ifdef ENABLE_SOUL_SYSTEM // Arrow ninja
			iDam += m_me->GetSoulItemDamage(pkVictim, iDam, RED_SOUL);
#endif

			//LOG_INFO(0, "%s arrow %s dam %d", ecs::PlayerRuntime::GetName((m_me ? m_me->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data(), iDam);

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

			if (ecs::PlayerRuntime::IsPC((m_me ? m_me->GetEntityHandle() : entt::null)))
				return;

			iDam = CalcMagicDamage((m_me ? m_me->GetEntityHandle() : entt::null), (pkVictim ? pkVictim->GetEntityHandle() : entt::null));

			NormalAttackAffect((m_me ? m_me->GetEntityHandle() : entt::null), (pkVictim ? pkVictim->GetEntityHandle() : entt::null));

			//
//#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
//						const int resist_magic = MINMAX(0, ecs::PointSystem::Get((pkVictim ? pkVictim->GetEntityHandle() : entt::null), POINT_RESIST_MAGIC), 100);
//						const int resist_magic_reduction = MINMAX(0, (m_me->GetJob()==JOB_SURA) ? ecs::PointSystem::Get((m_me ? m_me->GetEntityHandle() : entt::null), POINT_RESIST_MAGIC_REDUCTION)/2 : ecs::PointSystem::Get((m_me ? m_me->GetEntityHandle() : entt::null), POINT_RESIST_MAGIC_REDUCTION), 50);
//						const int total_res_magic = MINMAX(0, resist_magic - resist_magic_reduction, 100);
//						iDam = iDam * (100 - total_res_magic) / 100;
//#else
			iDam = iDam * (100 - (int)(ecs::PointSystem::Get((pkVictim ? pkVictim->GetEntityHandle() : entt::null), POINT_RESIST_MAGIC) / 2)) / 100;
			//#endif

									//LOG_INFO(0, "%s arrow %s dam %d", ecs::PlayerRuntime::GetName((m_me ? m_me->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data(), iDam);

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

				LOG_INFO("{} kwankeyok {}", ecs::PlayerRuntime::GetName((m_me ? m_me->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data());
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

				LOG_INFO("{} gigung {}", ecs::PlayerRuntime::GetName((m_me ? m_me->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data());
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

				LOG_INFO("{} hwajo {}", ecs::PlayerRuntime::GetName((m_me ? m_me->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data());
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

				LOG_TRACE("{} horse_wildattack {}", ecs::PlayerRuntime::GetName((m_me ? m_me->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data());
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

			LOG_INFO("{} - Skill {} -> {}", ecs::PlayerRuntime::GetName((m_me ? m_me->GetEntityHandle() : entt::null)).data(), m_bType, ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data());
			m_me->ComputeSkill(m_bType, pkVictim);
		}
		break;

		case SKILL_CHAIN:
		{
			m_me->OnMove(true);
			pkVictim->OnMove();

			if (pkVictim->CanBeginFight())
				pkVictim->BeginFight(m_me);

			LOG_INFO("{} - Skill {} -> {}", ecs::PlayerRuntime::GetName((m_me ? m_me->GetEntityHandle() : entt::null)).data(), m_bType, ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data());
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
		  pdw[0] = ecs::PlayerRuntime::GetPacketVID((m_me ? m_me->GetEntityHandle() : entt::null));
		  pdw[1] = ecs::PlayerRuntime::GetPacketVID((pkVictim ? pkVictim->GetEntityHandle() : entt::null));

		  event_create(budong_event_func, dwEI, PASSES_PER_SEC(1));
		  }
		  break;*/

		default:
			LOG_ERROR("CFuncShoot: I don't know this type [{}] of range attack.", (int)m_bType);
			break;
#ifdef ENABLE_NINJA_SANGONG_X30_RAZOR93
		case SKILL_SANGONG:
		{
			if (ecs::PlayerRuntime::IsStone((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) || pkVictim->GetMobRank() >= 4 || ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)))
			{
				int iDam = CalcMeleeDamage((m_me ? m_me->GetEntityHandle() : entt::null), (pkVictim ? pkVictim->GetEntityHandle() : entt::null));

				if (m_me->GetJob() == JOB_ASSASSIN &&
					(ecs::PlayerRuntime::IsStone((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) || pkVictim->GetMobRank() >= 4 || ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 136))
				{
					int multiplier = 36; // alap multiplier


					if (ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 331)
					{
						iDam = 0;
					}
					else
					{

						if (ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 8055)
						{
							multiplier = 34;
						}
						else if (ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 6193)
						{
							multiplier = 20;
						}
						else if (ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 8010 ||
							ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 8020 ||
							ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 180 ||
							ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 181 ||
							ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 182)
						{
							multiplier = 20;
						}
						else if (ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 180 ||
							ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 181 ||
							ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 182
							)
						{
							multiplier = 100;
						}
						else if (ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 4582 ||
							ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 4583 ||
							ecs::PlayerRuntime::GetRaceNum((pkVictim ? pkVictim->GetEntityHandle() : entt::null)) == 4584
							)
						{
							multiplier = 80	;
						}
						iDam *= multiplier;

					}
				}

				pkVictim->Damage(m_me, iDam, DAMAGE_TYPE_NORMAL);


				if (ecs::PlayerRuntime::IsPC((pkVictim ? pkVictim->GetEntityHandle() : entt::null)))
				{
					m_me->OnMove(true);
					pkVictim->OnMove();

					if (pkVictim->CanBeginFight())
						pkVictim->BeginFight(m_me);

					LOG_INFO("{} - Skill {} -> {}", ecs::PlayerRuntime::GetName((m_me ? m_me->GetEntityHandle() : entt::null)).data(), m_bType, ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data());
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
	LOG_INFO("Shoot {} type {} flyTargets.size {}", GetName(), bType, m_vec_dwFlyTargets.size());

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
		pack.dwTargetVID = ecs::PlayerRuntime::GetPacketVID((pkVictim ? pkVictim->GetEntityHandle() : entt::null));
		pack.x = ecs::PlayerRuntime::GetX((pkVictim ? pkVictim->GetEntityHandle() : entt::null));
		pack.y = ecs::PlayerRuntime::GetY((pkVictim ? pkVictim->GetEntityHandle() : entt::null));

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

	LOG_INFO("FlyTarget {} vid {} x {} y {}", GetName(), pack.dwTargetVID, pack.x, pack.y);
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

		if (AffectSystem::IsAffectFlag((pAttacker ? pAttacker->GetEntityHandle() : entt::null), AFF_EUNHYUNG) ||
			AffectSystem::IsAffectFlag((pAttacker ? pAttacker->GetEntityHandle() : entt::null), AFF_INVISIBILITY) ||
			AffectSystem::IsAffectFlag((pAttacker ? pAttacker->GetEntityHandle() : entt::null), AFF_REVIVE_INVISIBLE))
			continue;

		float fDist = DISTANCE_APPROX(ecs::PlayerRuntime::GetX((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) - ecs::PlayerRuntime::GetX((pkChr ? pkChr->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) - ecs::PlayerRuntime::GetY((pkChr ? pkChr->GetEntityHandle() : entt::null)));

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
		battle_end(GetEntityHandle());
	}
	else
	{
		const entt::entity eVictim = (pkVictim ? pkVictim->GetEntityHandle() : entt::null);
		if (m_eVictim != eVictim)
			MonsterLog("  : %s", ecs::PlayerRuntime::GetName((pkVictim ? pkVictim->GetEntityHandle() : entt::null)).data());

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
	if (RuntimeFlags(GetEntityHandle()) && IS_SET(RuntimeFlags(GetEntityHandle())->instantFlag, INSTANT_FLAG_STUN))
		return true;

	return false;
}

EVENTFUNC(StunEvent)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);

	if (info == nullptr)
	{
		LOG_ERROR("StunEvent> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	// Phase 10: WRITES_STATE - deferred until ECS component covers m_pkStunEvent
	ch->m_pkStunEvent = nullptr;
	const entt::entity e = (ch ? ch->GetEntityHandle() : entt::null);
	if (e != entt::null && g_registry.valid(e))
	{
		if (g_registry.all_of<ecs::StunTag>(e))
			g_registry.remove<ecs::StunTag>(e);
		if (auto* status = g_registry.try_get<ecs::StatusFlags>(e))
			status->isStunned = false;
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		g_dispatcher.trigger(ecs::EvStunBegin { e, 3000u });
	}
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

	LOG_INFO("{}: Stun {}", GetName(), static_cast<const void*>(this));

	PointChange(POINT_HP_RECOVERY, -GetPoint(POINT_HP_RECOVERY));
	PointChange(POINT_SP_RECOVERY, -GetPoint(POINT_SP_RECOVERY));

	CloseMyShop();

	event_cancel(&m_pkRecoveryEvent); // ȸ ̺Ʈ δ.

	TPacketGCStun pack;
	pack.header = HEADER_GC_STUN;
	pack.vid = GetPacketVID();
	PacketAround(&pack, sizeof(pack));

		if (auto* flags = RuntimeFlags(GetEntityHandle()))
		SET_BIT(flags->instantFlag, INSTANT_FLAG_STUN);
	const entt::entity e = GetEntityHandle();
	if (e != entt::null && g_registry.valid(e))
	{
		g_registry.emplace_or_replace<ecs::StunTag>(e);
		if (auto* status = g_registry.try_get<ecs::StatusFlags>(e))
			status->isStunned = true;
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}

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
	if (!pkAttacker || !ecs::PlayerRuntime::IsPC((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)))
		return;

	if (iDamage <= 0)
		iDamage = 1;

	const entt::entity eAttacker = pkAttacker ? pkAttacker->GetEntityHandle() : entt::null;
	if (eAttacker == entt::null)
		return;

	TDamageMap::iterator it = m_map_kDamage.find(eAttacker);
	if (it == m_map_kDamage.end())
		m_map_kDamage.insert(TDamageMap::value_type(eAttacker, TBattleInfo(iDamage, 0)));
	else
		it->second.iTotalDamage += iDamage;

	// hogy Dead() vissza tudja keresni a killert, ha kell
	m_dwKillerPID = ecs::PlayerRuntime::GetPlayerID((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null));
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
	if (IsPC() == true || (ecs::PlayerRuntime::IsPC((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) == true && pAttacker->GetTarget() == this))
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

		if (ecs::PlayerRuntime::GetDesc((pAttacker ? pAttacker->GetEntityHandle() : entt::null)) != nullptr)
		{
			ecs::PlayerRuntime::GetDesc((pAttacker ? pAttacker->GetEntityHandle() : entt::null))->Packet(&damageInfo, sizeof(TPacketGCDamageInfo));
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
	if (!ch || !ecs::PlayerRuntime::IsStone((ch ? ch->GetEntityHandle() : entt::null)) || ecs::PointSystem::GetMaxHP((ch ? ch->GetEntityHandle() : entt::null)) <= 0)
		return;

	const int iPercent = (ch->GetHP() * 100) / ecs::PointSystem::GetMaxHP((ch ? ch->GetEntityHandle() : entt::null));
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

	for (int step = ecs::PointSystem::GetMaxSP((ch ? ch->GetEntityHandle() : entt::null)) + 1; step <= wantStep; ++step)
	{
		ch->SetMaxSP(step);
		ch->SendMovePacket(FUNC_ATTACK, 0, ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)), 0);

		CHARACTER_MANAGER::instance().SelectStone(ch);

		if (step == 10 || step == 9)
			CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ecs::PlayerRuntime::GetMapIndex((ch ? ch->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) - 1500, ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) - 1500, ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) + 1500, ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) + 1500);
		else if (step == 8 || step == 7 || step == 6 || step == 3 || step == 1)
			CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ecs::PlayerRuntime::GetMapIndex((ch ? ch->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) - 1000, ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) - 1000, ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) + 1000, ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) + 1000);
		else if (step == 5 || step == 4 || step == 2)
			CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ecs::PlayerRuntime::GetMapIndex((ch ? ch->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) - 500, ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) - 500, ecs::PlayerRuntime::GetX((ch ? ch->GetEntityHandle() : entt::null)) + 500, ecs::PlayerRuntime::GetY((ch ? ch->GetEntityHandle() : entt::null)) + 500);

		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}

	NetworkSyncSystem::UpdatePacket((ch ? ch->GetEntityHandle() : entt::null));
}
#endif
static int64_t CalcReferenceBowHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim)
{
	if (!pAttacker || !pVictim)
		return 0;

	entt::entity pkBow = entt::null;
	entt::entity pkArrow = entt::null;

	if (0 == pAttacker->GetArrowAndBow(&pkBow, &pkArrow))
		return 0;

	int64_t dam = CalcArrowDamage((pAttacker ? pAttacker->GetEntityHandle() : entt::null), (pVictim ? pVictim->GetEntityHandle() : entt::null), pkBow, pkArrow);
	if (dam <= 0)
		return 0;

	int32_t lValue = ecs::PointSystem::Get((pVictim ? pVictim->GetEntityHandle() : entt::null), POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
	lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
	lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_IRR_WEAPON_DEFENSE);
#endif

	if (lValue < 0)
		lValue = 0;
	if (lValue > 100)
		lValue = 100;

	dam = dam * (100 - lValue) / 100;

#ifdef ENABLE_SOUL_SYSTEM
	dam += pAttacker->GetSoulItemDamage(pVictim, dam, RED_SOUL);
#endif

	if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DAMAGE_BONUS))
		dam = dam * (100 + ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

#ifdef ENABLE_MEDI_PVM
	if (ecs::PlayerRuntime::IsNPC((pVictim ? pVictim->GetEntityHandle() : entt::null)))
		dam = dam * (100 + ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_MEDI_PVM)) / 100;
#endif

	dam = dam * (100 - std::min((int64_t)99, ecs::PointSystem::Get((pVictim ? pVictim->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;

	return std::max<int64_t>(0, dam);
}

static int64_t CalcReferenceBasicHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim)
{
	if (!pAttacker || !pVictim)
		return 0;

	int64_t dam = 0;

	const entt::entity weapon = ItemSystem::GetWearItem(
		(pAttacker ? pAttacker->GetEntityHandle() : entt::null), WEAR_WEAPON);
	if (ItemSystem::IsValidItem(weapon) &&
		ItemSystem::GetItemType(weapon) == ITEM_WEAPON &&
		ItemSystem::GetItemSubType(weapon) == WEAPON_BOW)
		dam = CalcReferenceBowHitDamage(pAttacker, pVictim);
	else
		dam = CalcReferenceNormalHitDamage(pAttacker, pVictim);

	if (dam <= 0)
		return 0;

	const int64_t skillBonus = std::max<int64_t>(0, ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_SKILL_DAMAGE_BONUS));
	if (skillBonus)
		dam = dam * (100 + skillBonus) / 100;

	return dam;
}
static int64_t CalcReferenceNormalHitDamage(LPCHARACTER pAttacker, LPCHARACTER pVictim)
{
	if (!pAttacker || !pVictim)
		return 0;

	int64_t dam = CalcMeleeDamage((pAttacker ? pAttacker->GetEntityHandle() : entt::null), (pVictim ? pVictim->GetEntityHandle() : entt::null));
	if (dam <= 0)
		return 0;

	const entt::entity weapon = ItemSystem::GetWearItem(
		(pAttacker ? pAttacker->GetEntityHandle() : entt::null), WEAR_WEAPON);
	if (ItemSystem::IsValidItem(weapon))
	{
		int32_t lValue = 0;

		switch (ItemSystem::GetItemSubType(weapon))
		{
		case WEAPON_SWORD:
			lValue = ecs::PointSystem::Get((pVictim ? pVictim->GetEntityHandle() : entt::null), POINT_RESIST_SWORD);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_SPADA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_TWO_HANDED:
			lValue = ecs::PointSystem::Get((pVictim ? pVictim->GetEntityHandle() : entt::null), POINT_RESIST_TWOHAND);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_SPADONE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_DAGGER:
#ifdef ENABLE_WOLFMAN_CHARACTER
		case WEAPON_CLAW:
#endif
			lValue = ecs::PointSystem::Get((pVictim ? pVictim->GetEntityHandle() : entt::null), POINT_RESIST_DAGGER);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_PUGNALE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_BELL:
			lValue = ecs::PointSystem::Get((pVictim ? pVictim->GetEntityHandle() : entt::null), POINT_RESIST_BELL);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_CAMPANA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_FAN:
			lValue = ecs::PointSystem::Get((pVictim ? pVictim->GetEntityHandle() : entt::null), POINT_RESIST_FAN);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_VENTAGLIO);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_BOW:
			lValue = ecs::PointSystem::Get((pVictim ? pVictim->GetEntityHandle() : entt::null), POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_IRR_WEAPON_DEFENSE);
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

	if (ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DAMAGE_BONUS))
		dam = dam * (100 + ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

#ifdef ENABLE_MEDI_PVM
	if (ecs::PlayerRuntime::IsNPC((pVictim ? pVictim->GetEntityHandle() : entt::null)))
		dam = dam * (100 + ecs::PointSystem::Get((pAttacker ? pAttacker->GetEntityHandle() : entt::null), POINT_ATTBONUS_MEDI_PVM)) / 100;
#endif

	dam = dam * (100 - std::min((int64_t)99, ecs::PointSystem::Get((pVictim ? pVictim->GetEntityHandle() : entt::null), POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;

	return std::max<int64_t>(0, dam);
}

bool CHARACTER::IsAggressive() const
{
	return IS_SET(GetAIFlag(), AIFLAG_AGGRESSIVE) || AIHelpers::IsAggressive(GetEntityHandle());
}

void CHARACTER::SetAggressive()
{
		if (auto* flags = RuntimeFlags(GetEntityHandle()))
		SET_BIT(flags->aiFlag, AIFLAG_AGGRESSIVE);
	AIHelpers::SetAggressive(GetEntityHandle(), true);
}

bool CHARACTER::IsCoward() const
{
	return IS_SET(GetAIFlag(), AIFLAG_COWARD) || AIHelpers::IsCoward(GetEntityHandle());
}

void CHARACTER::SetCoward()
{
		if (auto* flags = RuntimeFlags(GetEntityHandle()))
		SET_BIT(flags->aiFlag, AIFLAG_COWARD);
	AIHelpers::SetCoward(GetEntityHandle(), true);
}

bool CHARACTER::IsBerserker() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_BERSERK))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(GetEntityHandle()))
		return flags->isBerserk;

	return false;
}

bool CHARACTER::IsStoneSkinner() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_STONESKIN))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(GetEntityHandle()))
		return flags->isStoneSkinner;

	return false;
}

bool CHARACTER::IsGodSpeeder() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_GODSPEED))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(GetEntityHandle()))
		return flags->isGodSpeed;

	return false;
}

bool CHARACTER::IsDeathBlower() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_DEATHBLOW))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(GetEntityHandle()))
		return flags->isDeathBlower;

	return false;
}

bool CHARACTER::IsReviver() const
{
	if (IS_SET(GetAIFlag(), AIFLAG_REVIVE))
		return true;

	if (auto* flags = AIHelpers::TryGetFlags(GetEntityHandle()))
		return flags->isReviver;

	return false;
}

void CHARACTER::SetNoAttackShinsu()
{
		if (auto* flags = RuntimeFlags(GetEntityHandle()))
		SET_BIT(flags->aiFlag, AIFLAG_NOATTACKSHINSU);
	AIHelpers::SetNoAttackShinsu(GetEntityHandle(), true);
}

bool CHARACTER::IsNoAttackShinsu() const
{
	return IS_SET(GetAIFlag(), AIFLAG_NOATTACKSHINSU) || AIHelpers::IsNoAttackShinsu(GetEntityHandle());
}

void CHARACTER::SetNoAttackChunjo()
{
		if (auto* flags = RuntimeFlags(GetEntityHandle()))
		SET_BIT(flags->aiFlag, AIFLAG_NOATTACKCHUNJO);
	AIHelpers::SetNoAttackChunjo(GetEntityHandle(), true);
}

bool CHARACTER::IsNoAttackChunjo() const
{
	return IS_SET(GetAIFlag(), AIFLAG_NOATTACKCHUNJO) || AIHelpers::IsNoAttackChunjo(GetEntityHandle());
}

void CHARACTER::SetNoAttackJinno()
{
		if (auto* flags = RuntimeFlags(GetEntityHandle()))
		SET_BIT(flags->aiFlag, AIFLAG_NOATTACKJINNO);
	AIHelpers::SetNoAttackJinno(GetEntityHandle(), true);
}

bool CHARACTER::IsNoAttackJinno() const
{
	return IS_SET(GetAIFlag(), AIFLAG_NOATTACKJINNO) || AIHelpers::IsNoAttackJinno(GetEntityHandle());
}

void CHARACTER::SetAttackMob()
{
		if (auto* flags = RuntimeFlags(GetEntityHandle()))
		SET_BIT(flags->aiFlag, AIFLAG_ATTACKMOB);
	AIHelpers::SetAttackMob(GetEntityHandle(), true);
}

bool CHARACTER::IsAttackMob() const
{
	return IS_SET(GetAIFlag(), AIFLAG_ATTACKMOB) || AIHelpers::IsAttackMob(GetEntityHandle());
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
	CombatSystem::SetComboSequence(GetEntityHandle(), seq);
}

uint8_t CHARACTER::GetComboSequence() const
{
	return CombatSystem::GetComboSequence(GetEntityHandle());
}

void CHARACTER::SetLastComboTime(uint32_t time)
{
	CombatSystem::SetLastComboTime(GetEntityHandle(), time);
}

uint32_t CHARACTER::GetLastComboTime() const
{
	return CombatSystem::GetLastComboTime(GetEntityHandle());
}

void CHARACTER::SetValidComboInterval(int interval)
{
	CombatSystem::SetValidComboInterval(GetEntityHandle(), interval);
}

int CHARACTER::GetValidComboInterval() const
{
	return CombatSystem::GetValidComboInterval(GetEntityHandle());
}

uint8_t CHARACTER::GetComboIndex() const
{
	return CombatSystem::GetComboIndex(GetEntityHandle());
}

void CHARACTER::IncreaseComboHackCount(int k)
{
	m_iComboHackCount += k;

	if (m_iComboHackCount >= 10)
	{
		if (GetDesc())
			if (GetDesc()->DelayedDisconnect(number(2, 7)))
			{
				LOG_INFO("COMBO_HACK_DISCONNECT: {} count: {}", GetName(), m_iComboHackCount);
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
		if (auto* flags = RuntimeFlags(ch->GetEntityHandle()))
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

		if (!ecs::PlayerRuntime::GetDesc((pkChr ? pkChr->GetEntityHandle() : entt::null)))
		{
			LOG_ERROR("{} {} does not have desc", ecs::PlayerRuntime::GetName((pkChr ? pkChr->GetEntityHandle() : entt::null)).data(), static_cast<const void*>(get_pointer(pkChr)));
			abort();
		}

		ecs::PlayerRuntime::GetDesc((pkChr ? pkChr->GetEntityHandle() : entt::null))->Packet(&p, sizeof(TPacketGCTarget));
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
	p.dwVID = ecs::PlayerRuntime::GetPacketVID((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null));

#ifdef __VIEW_TARGET_PLAYER_HP__
		if ((ecs::PointSystem::GetMaxHP((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) <= 0))
		{
			p.bHPPercent = 0;
#ifdef __VIEW_TARGET_DECIMAL_HP__
			p.iMinHP = 0;
			p.iMaxHP = 0;
#endif
		}
		else if (ecs::PlayerRuntime::IsPC((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) && !m_pkChrTarget->IsPolymorphed())
		{
			p.bHPPercent = MINMAX(0, m_pkChrTarget->GetHPPct(), 100);
#ifdef __VIEW_TARGET_DECIMAL_HP__
			p.iMinHP = m_pkChrTarget->GetHP();
			p.iMaxHP = ecs::PointSystem::GetMaxHP((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null));
#endif
		}
#else
		if ((ecs::PlayerRuntime::IsPC((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) && !m_pkChrTarget->IsPolymorphed()) || (ecs::PointSystem::GetMaxHP((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) <= 0))
			p.bHPPercent = 0;
#endif
		else
		{
			if (ecs::PlayerRuntime::GetRaceNum((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) == 20101 ||
				ecs::PlayerRuntime::GetRaceNum((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) == 20102 ||
				ecs::PlayerRuntime::GetRaceNum((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) == 20103 ||
				ecs::PlayerRuntime::GetRaceNum((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) == 20104 ||
				ecs::PlayerRuntime::GetRaceNum((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) == 20105 ||
				ecs::PlayerRuntime::GetRaceNum((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) == 20106 ||
				ecs::PlayerRuntime::GetRaceNum((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) == 20107 ||
				ecs::PlayerRuntime::GetRaceNum((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) == 20108 ||
				ecs::PlayerRuntime::GetRaceNum((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) == 20109)
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
				if (ecs::PointSystem::GetMaxHP((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) <= 0)
				{
					p.bHPPercent = 0;
					p.iMinHP = 0;
					p.iMaxHP = 0;
				}
				else
				{
					p.bHPPercent = std::min((m_pkChrTarget->GetHP() * 100) / ecs::PointSystem::GetMaxHP((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)), (int64_t)100);
					p.iMinHP = m_pkChrTarget->GetHP();
					p.iMaxHP = ecs::PointSystem::GetMaxHP((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null));
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
				if (ecs::PointSystem::GetMaxHP((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)) <= 0)
					p.bHPPercent = 0;
				else
					p.bHPPercent = MINMAX(0, (m_pkChrTarget->GetHP() * 100) / ecs::PointSystem::GetMaxHP((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)), 100);
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
		if (ecs::PlayerRuntime::IsPC((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null))) {
			const entt::entity item = ItemSystem::GetWearItem(
				(m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null), WEAR_PENDANT);
			if (ItemSystem::IsValidItem(item)) {
				uint32_t vnum = ItemSystem::GetItemVnum(item);
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
		else if (m_pkChrTarget->IsMonster() || ecs::PlayerRuntime::IsStone((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null))) {
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

		if (!ecs::PlayerRuntime::GetDesc((pkChr ? pkChr->GetEntityHandle() : entt::null)))
		{
			LOG_ERROR("{} {} does not have desc", ecs::PlayerRuntime::GetName((pkChr ? pkChr->GetEntityHandle() : entt::null)).data(), static_cast<const void*>(get_pointer(pkChr)));
			abort();
		}

		ecs::PlayerRuntime::GetDesc((pkChr ? pkChr->GetEntityHandle() : entt::null))->Packet(&p, sizeof(TPacketGCTarget));
	}
}

void CHARACTER::CheckTarget()
{
	if (!m_pkChrTarget)
		return;

	if (DISTANCE_APPROX(GetX() - ecs::PlayerRuntime::GetX((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null)), GetY() - ecs::PlayerRuntime::GetY((m_pkChrTarget ? m_pkChrTarget->GetEntityHandle() : entt::null))) >= 4800)
		SetTarget(nullptr);
}

bool CHARACTER::IsChangeAttackPosition(LPCHARACTER target) const
{
	if (!IsNPC())
		return true;

	uint32_t dwChangeTime = AI_CHANGE_ATTACK_POISITION_TIME_NEAR;

	if (DISTANCE_APPROX(GetX() - ecs::PlayerRuntime::GetX((target ? target->GetEntityHandle() : entt::null)), GetY() - ecs::PlayerRuntime::GetY((target ? target->GetEntityHandle() : entt::null))) >
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

			LOG_INFO("WAEGU move to {} {} (far)", iDestX, iDestY);
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
		LOG_INFO("{} {} A÷±âÇI°í µ13A°!AÚ! {} {}", GetName(), static_cast<const void*>(this), x, y);

	if (GetParty())
		GetParty()->SendMessage(this, PM_RETURN, x, y);

	return true;
}

bool CHARACTER::Follow(LPCHARACTER pkChr, float fMinDistance)
{
	if (IsPC())
	{
		LOG_ERROR("CHARACTER::Follow : PC cannot use this method", GetName());
		return false;
	}

	if (IS_SET(GetAIFlag(), AIFLAG_NOMOVE))
	{
		if (ecs::PlayerRuntime::IsPC((pkChr ? pkChr->GetEntityHandle() : entt::null)))
		{
			if (!GetParty() || !GetParty()->GetLeader() || GetParty()->GetLeader() == this)
			{
				if (get_dword_time() - m_pkMobInst->m_dwLastAttackedTime >= 15000)
				{
					if (m_pkMobData->m_table.wAttackRange < DISTANCE_APPROX(ecs::PlayerRuntime::GetX((pkChr ? pkChr->GetEntityHandle() : entt::null)) - GetX(), ecs::PlayerRuntime::GetY((pkChr ? pkChr->GetEntityHandle() : entt::null)) - GetY()))
						if (Return())
							return true;
				}
			}
		}
		return false;
	}

	int32_t x = ecs::PlayerRuntime::GetX((pkChr ? pkChr->GetEntityHandle() : entt::null));
	int32_t y = ecs::PlayerRuntime::GetY((pkChr ? pkChr->GetEntityHandle() : entt::null));

	if (ecs::PlayerRuntime::IsPC((pkChr ? pkChr->GetEntityHandle() : entt::null)))
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
	if (HasMoveState(pkChr ? pkChr->GetEntityHandle() : entt::null) &&
		GetMobBattleType() != BATTLE_TYPE_RANGE &&
		GetMobBattleType() != BATTLE_TYPE_MAGIC &&
		false == IsPet() && false == IsNewPet()
#else
	if (HasMoveState(pkChr ? pkChr->GetEntityHandle() : entt::null) &&
		GetMobBattleType() != BATTLE_TYPE_RANGE &&
		GetMobBattleType() != BATTLE_TYPE_MAGIC &&
		false == IsPet()
#endif
		)
	{
		float rot = pkChr->GetRotation();
		float rot_delta = GetDegreeDelta(rot, GetDegreeFromPositionXY(GetX(), GetY(), ecs::PlayerRuntime::GetX((pkChr ? pkChr->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((pkChr ? pkChr->GetEntityHandle() : entt::null))));

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
