#include "stdafx.h"
#include "horse_rider.h"
#include "utils.h"
#include "config.h"
#include "char.h"
#include "arena.h"
#include "questmanager.h"
#include "ecs/Registry.hpp"
#include "ecs/EventDispatcher.hpp"
#include "ecs/events.hpp"
#include "ecs/components/pet_mount_components.hpp"
#include "ecs/components/dirty_components.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/ChatSystem.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/SkillSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include <algorithm>
#include <limits>
#include <utility>

THorseStat c_aHorseStat[HORSE_MAX_LEVEL+1] =
/*
   int iMinLevel;	// ž���� �� �ִ� �ּ� ����
   int iNPCRace;
   int iMaxHealth;	// ���� �ִ� ü��
   int iMaxStamina;	// ���� �ִ� ���׹̳�
   int iST;
   int iDX;
   int iHT;
   int iIQ;
   int iDamMean;
   int iDamMin;
   int iDamMax;
   int iDef;
 */
{//	pclvl	lo		h	s	st	dx  ht  iq      dm  dmax vedelem
	{  0,	0,	1,	1,	0,	0,	0,	0,	0,	0,	0,	0  },
	{ 25,	20101,	3,	4,	26,	1,	18,	9,	54,	43,	64,	20 },	// 1 (�ʱ�)
	{ 25,	20101,	4,	4,	27,	36,	18,	9,	55,	44,	66,	33 },
	{ 25,	20101,	5,	5,	28,	38,	19,	9,	56,	44,	67,	33 },
	{ 25,	20101,	7,	5,	29,	39,	19,	10,	57,	45,	68,	34 },
	{ 25,	20101,	8,	6,	30,	40,	20,	10,	58,	46,	69,	34 },
	{ 25,	20101,	9,	6,	31,	41,	21,	10,	59,	47,	70,	35 },
	{ 25,	20101,	11,	7,	32,	42,	21,	11,	60,	48,	72,	36 },
	{ 25,	20101,	12,	7,	33,	44,	22,	11,	61,	48,	73,	36 },
	{ 25,	20101,	13,	8,	34,	45,	22,	11,	62,	49,	74,	37 },
	{ 25,	20101,	15,	10,	35,	46,	23,	12,	63,	50,	75,	37 },
	{ 35,	20104,	18,	30,	40,	53,	27,	13,	69,	55,	82,	41 },	// 11 (�߱�)
	{ 35,	20104,	19,	35,	41,	54,	27,	14,	70,	56,	84,	42 },
	{ 35,	20104,	21,	40,	42,	56,	28,	14,	71,	56,	85,	42 },
	{ 35,	20104,	22,	50,	43,	57,	28,	14,	72,	57,	86,	43 },
	{ 35,	20104,	24,	55,	44,	58,	29,	15,	73,	58,	87,	43 },
	{ 35,	20104,	25,	60,	44,	59,	30,	15,	74,	59,	88,	44 },
	{ 35,	20104,	27,	65,	45,	60,	30,	15,	75,	60,	90,	45 },
	{ 35,	20104,	28,	70,	46,	62,	31,	15,	76,	60,	91,	45 },
	{ 35,	20104,	30,	80,	47,	63,	31,	16,	77,	61,	92,	46 },
	{ 35,	20104,	32,	100,	48,	64,	32,	16,	78,	62,	93,	46 },
	{ 50,	20107,	35,	120,	53,	71,	36,	18,	84,	67,	100,	50 },	// 21 (���)
	{ 50,	20107,	36,	125,	55,	74,	37,	18,	86,	68,	103,	51 },
	{ 50,	20107,	37,	130,	57,	76,	38,	19,	88,	70,	105,	52 },
	{ 50,	20107,	38,	135,	59,	78,	39,	20,	90,	72,	108,	54 },
	{ 50,	20107,	40,	140,	60,	80,	40,	20,	91,	72,	109,	54 },
	{ 50,	20107,	42,	145,	61,	81,	40,	20,	92,	73,	110,	55 },
	{ 50,	20107,	44,	150,	62,	83,	42,	21,	94,	75,	112,	56 },
	{ 50,	20107,	46,	160,	63,	84,	42,	21,	95,	76,	114,	57 },
	{ 50,	20107,	48,	170,	65,	87,	43,	22,	97,	77,	116,	58 },
	{ 50,	20107,	50,	200,	67,	89,	45,	22,	99,	79,	118,	59 }
};


namespace {
constexpr uint32_t HealthInterval = 3 * 24 * 60 * 60;
constexpr int ConsumeInterval = 6 * 60;
constexpr int RegenInterval = 12 * 60;

ecs::HorseRuntime* Horse(entt::entity rider) {
    return g_registry.valid(rider) ? g_registry.try_get<ecs::HorseRuntime>(rider) : nullptr;
}
// Commit only the native flag here. Publishing a dirty tag before the timer
// transaction is complete would let observers re-enter half a riding change.
bool SetRidingFlag(entt::entity rider, bool riding) {
    auto* state = g_registry.valid(rider) ? g_registry.try_get<ecs::MountState>(rider) : nullptr;
    if (!state) return false;
    state->horseRiding = riding;
    return true;
}
void RemoveHorseTimers(entt::registry&, entt::entity rider) { MountSystem::StopHorseTimers(rider); }
bool EnsureHorse(entt::entity rider) {
    if (!ecs::PlayerRuntime::IsPC(rider)) return false;
    struct Installed {};
    if (!g_registry.ctx().contains<Installed>()) {
        g_registry.on_destroy<ecs::HorseRuntime>().connect<&RemoveHorseTimers>();
        g_registry.ctx().emplace<Installed>();
    }
    // Do not use emplace's trailing get() after a construction callback.
    if (!g_registry.all_of<ecs::HorseRuntime>(rider))
        g_registry.insert<ecs::HorseRuntime>(&rider, &rider + 1);
    if (!ecs::PlayerRuntime::IsPC(rider) || !Horse(rider)) return false;
    if (!g_registry.all_of<ecs::MountState>(rider))
        g_registry.insert<ecs::MountState>(&rider, &rider + 1);
    return ecs::PlayerRuntime::IsPC(rider) &&
        g_registry.all_of<ecs::HorseRuntime, ecs::MountState>(rider);
}
uint32_t Now() { return static_cast<uint32_t>(std::clamp<int64_t>(get_global_time(), 0, std::numeric_limits<uint32_t>::max())); }
uint32_t NextHealthDrop() {
    return static_cast<uint32_t>(std::min<uint64_t>(uint64_t(Now()) + HealthInterval,
        std::numeric_limits<uint32_t>::max()));
}
bool Alive(entt::entity rider) {
    return ecs::PlayerRuntime::IsPC(rider) && MountSystem::GetHorseLevel(rider) > 0 &&
        MountSystem::GetHorseHealth(rider) > 0;
}
void Dirty(entt::entity rider) {
    if (g_registry.valid(rider) && !g_registry.all_of<ecs::DirtyTag>(rider))
        g_registry.insert<ecs::DirtyTag>(&rider, &rider + 1);
}
EVENTINFO(horserider_info) { entt::entity rider {entt::null}; };
EVENTFUNC(HorseStaminaConsume);
EVENTFUNC(HorseStaminaRegen);

bool StartTimer(entt::entity rider, bool consume) {
    if (!Alive(rider) || MountSystem::IsHorseRiding(rider) != consume) return false;
    auto* state = Horse(rider);
    if (!state) return false;
    if ((consume ? state->consume : state->regen) && !(consume ? state->regen : state->consume))
        return true;
    const uint64_t revision = state->timerRevision + 1;
    MountSystem::StopHorseTimers(rider);
    const auto ready = [&] {
        const auto* current = Horse(rider);
        return Alive(rider) && current && current->timerRevision == revision &&
            !current->regen && !current->consume && MountSystem::IsHorseRiding(rider) == consume;
    };
    if (!ready()) return false;
    auto* info = AllocEventInfo<horserider_info>(); info->rider = rider;
    auto timer = event_create(consume ? HorseStaminaConsume : HorseStaminaRegen, info,
        PASSES_PER_SEC(consume ? ConsumeInterval : RegenInterval));
    if (!timer || !ready()) { event_cancel(&timer); return false; }
    state = Horse(rider);
    (consume ? state->consume : state->regen) = std::move(timer);
    return true;
}
bool OwnsTimer(entt::entity rider, LPEVENT event, bool consume) {
    const auto* state = Horse(rider);
    return state && (consume ? state->consume : state->regen) == event;
}
int32_t Tick(LPEVENT event, bool consume) {
    const auto* info = event ? dynamic_cast<horserider_info*>(event->info) : nullptr;
    if (!info) return 0;
    const auto rider = info->rider;
    if (!OwnsTimer(rider, event, consume)) return 0;
    if (!Alive(rider) || MountSystem::IsHorseRiding(rider) != consume) {
        auto* state = Horse(rider);
        (consume ? state->consume : state->regen).reset();
        return 0;
    }
    MountSystem::ChangeHorseStamina(rider, consume ? -1 : 1);
    if (!OwnsTimer(rider, event, consume)) return 0;
    MountSystem::CheckHorseHealthDropTime(rider);
    if (!OwnsTimer(rider, event, consume)) return 0;
    if (consume) g_dispatcher.trigger(ecs::EvHorseStaminaConsume {rider});
    else g_dispatcher.trigger(ecs::EvHorseStaminaRegen {rider});
    if (!OwnsTimer(rider, event, consume)) return 0;
    if (!Alive(rider) || MountSystem::IsHorseRiding(rider) != consume ||
        (consume ? MountSystem::GetHorseStamina(rider) == 0 :
            MountSystem::GetHorseStamina(rider) >= MountSystem::GetHorseMaxStamina(rider))) {
        auto* state = Horse(rider);
        (consume ? state->consume : state->regen).reset();
        return 0;
    }
    return PASSES_PER_SEC(consume ? ConsumeInterval : RegenInterval);
}
EVENTFUNC(HorseStaminaConsume) { return Tick(event, true); }
EVENTFUNC(HorseStaminaRegen) { return Tick(event, false); }
} // namespace

namespace MountSystem {
int GetHorseLevel(entt::entity rider) {
    const auto* state = Horse(rider);
    return state ? std::min<int>(state->level, HORSE_MAX_LEVEL) : 0;
}
int GetHorseHealth(entt::entity rider) { const auto* state = Horse(rider); return state ? state->health : 0; }
int GetHorseStamina(entt::entity rider) { const auto* state = Horse(rider); return state ? state->stamina : 0; }
int GetHorseMaxHealth(entt::entity rider) { return Horse(rider) ? c_aHorseStat[GetHorseLevel(rider)].iMaxHealth : 0; }
int GetHorseMaxStamina(entt::entity rider) { return Horse(rider) ? c_aHorseStat[GetHorseLevel(rider)].iMaxStamina : 0; }
int GetHorseArmor(entt::entity rider) { return c_aHorseStat[GetHorseLevel(rider)].iArmor; }
int GetHorseGrade(entt::entity rider) {
    // Preserve this server's fixed grade rule. Changing it would change skill
    // permissions and horse quest behavior, independently of the ECS migration.
    return Horse(rider) ? 2 : 0;
}
bool CanUseHorseSkill(entt::entity rider) { return IsRiding(rider) && GetHorseGrade(rider) == 3; }

void StopHorseTimers(entt::entity rider) {
    auto* state = Horse(rider);
    if (!state) return;
    ++state->timerRevision;
    auto regen = std::exchange(state->regen, {});
    auto consume = std::exchange(state->consume, {});
    // No component reference survives cancellation.
    event_cancel(&regen);
    event_cancel(&consume);
}
void LoadHorseData(entt::entity rider, const THorseInfo& data, uint32_t logoffSeconds) {
    const THorseInfo snapshot = data;
    if (!EnsureHorse(rider)) return;
    StopHorseTimers(rider);
    if (!ecs::PlayerRuntime::IsPC(rider) ||
        !g_registry.all_of<ecs::HorseRuntime, ecs::MountState>(rider)) return;
    // Hydration starts with no published mount model; EnterHorse restores it.
    g_registry.get<ecs::MountState>(rider).mountVnum = 0;
    auto& state = *Horse(rider);
    state.level = static_cast<uint8_t>(std::min<int>(snapshot.bLevel, HORSE_MAX_LEVEL));
    const auto& stats = c_aHorseStat[state.level];
    state.health = static_cast<int16_t>(std::clamp<int>(snapshot.sHealth, 0, stats.iMaxHealth));
    const int64_t recovered = state.level ? logoffSeconds / RegenInterval : 0;
    state.stamina = static_cast<int16_t>(std::clamp<int64_t>(int64_t(snapshot.sStamina) + recovered, 0, stats.iMaxStamina));
    state.healthDropTime = snapshot.dwHorseHealthDropTime;
    SetRidingFlag(rider, snapshot.bRiding && state.level && state.health && state.stamina);
}
THorseInfo StoreHorseData(entt::entity rider) {
    THorseInfo data {};
    if (const auto* state = Horse(rider)) {
        data.bLevel = static_cast<uint8_t>(GetHorseLevel(rider));
        data.sHealth = state->health; data.sStamina = state->stamina;
        data.dwHorseHealthDropTime = state->healthDropTime;
        data.bRiding = IsHorseRiding(rider) ? 1 : 0;
    }
    return data;
}
void SetHorseLevel(entt::entity rider, int level) {
    if (!EnsureHorse(rider)) return;
    auto& state = *Horse(rider);
    state.level = static_cast<uint8_t>(std::clamp(level, 0, HORSE_MAX_LEVEL));
    state.health = static_cast<int16_t>(c_aHorseStat[state.level].iMaxHealth);
    state.stamina = static_cast<int16_t>(c_aHorseStat[state.level].iMaxStamina);
    state.healthDropTime = NextHealthDrop();
    if (!state.level) {
        if (IsHorseRiding(rider)) StopRiding(rider);
        StopHorseTimers(rider);
    }
    if (!Horse(rider)) return;
    SkillSystem::SetSkillLevel(rider, SKILL_HORSE, GetHorseLevel(rider));
    if (!Horse(rider)) return;
    SendHorseInfo(rider);
    if (!Horse(rider)) return;
    Dirty(rider);
    if (!Horse(rider)) return;
    ecs::PointSystem::Compute(rider);
    if (Horse(rider)) SkillSystem::SendSkillLevelPacket(rider);
}
void EnterHorse(entt::entity rider) {
    if (!Alive(rider)) return;
    if (IsHorseRiding(rider)) {
        SetRidingFlag(rider, false);
        StartRiding(rider);
    } else StartTimer(rider, false);
    CheckHorseHealthDropTime(rider, false);
}
void ChangeHorseStamina(entt::entity rider, int64_t delta, bool send) {
    auto* state = Horse(rider);
    if (!state) return;
    // Clamp the delta first so hostile GM input cannot overflow addition.
    delta = std::clamp<int64_t>(delta, -GetHorseMaxStamina(rider), GetHorseMaxStamina(rider));
    state->stamina = static_cast<int16_t>(std::clamp<int64_t>(state->stamina + delta, 0, GetHorseMaxStamina(rider)));
    if (!state->stamina && IsHorseRiding(rider)) StopRiding(rider);
    if (send && Horse(rider)) SendHorseInfo(rider);
}
void ChangeHorseHealth(entt::entity rider, int64_t delta, bool send) {
    auto* state = Horse(rider);
    if (!state) return;
    delta = std::clamp<int64_t>(delta, -GetHorseMaxHealth(rider), GetHorseMaxHealth(rider));
    state->health = static_cast<int16_t>(std::clamp<int64_t>(state->health + delta, 0, GetHorseMaxHealth(rider)));
    if (state->level && !state->health) HorseDie(rider);
    if (send && Horse(rider)) SendHorseInfo(rider);
}
void CheckHorseHealthDropTime(entt::entity rider, bool send) {
    auto* state = Horse(rider);
    if (!state || !state->level || state->healthDropTime >= Now()) return;
    const uint64_t drops = (uint64_t(Now()) - state->healthDropTime + HealthInterval - 1) / HealthInterval;
    state->healthDropTime = static_cast<uint32_t>(std::min<uint64_t>(
        uint64_t(state->healthDropTime) + drops * HealthInterval, std::numeric_limits<uint32_t>::max()));
    ChangeHorseHealth(rider, -static_cast<int64_t>(drops), send);
}
void FeedHorse(entt::entity rider) {
    if (!Alive(rider)) return;
    Horse(rider)->healthDropTime = NextHealthDrop();
    ChangeHorseHealth(rider, 1);
    if (Alive(rider)) ChangeHorseStamina(rider, 1);
}
void HorseDie(entt::entity rider) {
    if (!Horse(rider)) return;
    ChangeHorseStamina(rider, -GetHorseStamina(rider));
    if (!Horse(rider)) return;
    StopHorseTimers(rider);
    if (Horse(rider)) SummonHorse(rider, false);
}
bool ReviveHorse(entt::entity rider) {
    if (!Horse(rider) || GetHorseLevel(rider) <= 0 || GetHorseHealth(rider) > 0) return false;
    auto& state = *Horse(rider);
    state.health = static_cast<int16_t>(GetHorseMaxHealth(rider));
    state.stamina = static_cast<int16_t>(GetHorseMaxStamina(rider));
    state.healthDropTime = NextHealthDrop();
    StartTimer(rider, false);
    if (!Horse(rider)) return true;
    SummonHorse(rider, false);
    if (Horse(rider)) SummonHorse(rider, true);
    if (Horse(rider)) Dirty(rider);
    return true;
}
bool StartRiding(entt::entity rider) {
    if (!ecs::PlayerRuntime::IsPC(rider) || IsRiding(rider)) return false;
#if defined(BLOCK_RIDING_INSIDE_WAR) || defined(ENABLE_NEWSTUFF)
    if (ecs::SocialSystem::GetWarMap(rider)
#ifndef BLOCK_RIDING_INSIDE_WAR
        && g_NoMountAtGuildWar
#endif
    ) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 852, "");
#endif
        AffectSystem::RemoveAffect(rider, AFFECT_MOUNT);
        if (g_registry.valid(rider)) AffectSystem::RemoveAffect(rider, AFFECT_MOUNT_BONUS);
        return false;
    }
#endif
    if (CombatSystem::IsDead(rider) || AffectSystem::IsPolymorphed(rider)) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, CombatSystem::IsDead(rider) ? 356 : 355, "");
#endif
        return false;
    }
    const auto armor = ItemSystem::GetWearItem(rider, WEAR_BODY);
    const auto armorVnum = ItemSystem::GetItemVnum(armor);
    if (ItemSystem::IsValidItem(armor) && armorVnum >= 11901 && armorVnum <= 11904) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 410, "");
#endif
        return false;
    }
    if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(rider))) return false;
    if (!Alive(rider) || GetHorseStamina(rider) <= 0) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO,
            GetHorseLevel(rider) <= 0 ? 333 : GetHorseHealth(rider) <= 0 ? 335 : 334, "");
#endif
        return false;
    }
    const auto summoned = GetSummonedHorse(rider);
    const auto vnum = summoned != entt::null ? ecs::PlayerRuntime::GetRaceNum(summoned) : GetMyHorseVnum(rider);
    if (!SetRidingFlag(rider, true)) return false;
    if (!StartTimer(rider, true)) { if (Horse(rider)) SetRidingFlag(rider, false); return false; }
    const auto revision = Horse(rider)->timerRevision;
    const auto receipt = [&] { const auto* s = Horse(rider); return s && s->timerRevision == revision && IsHorseRiding(rider); };
    SendHorseInfo(rider);
    if (receipt()) SummonHorse(rider, false);
    if (receipt()) SetMountVnum(rider, vnum);
    if (receipt()) Dirty(rider);
    return true;
}
bool StopRiding(entt::entity rider) {
    if (!Horse(rider) || !IsHorseRiding(rider)) return false;
    const auto oldVnum = GetMountVnum(rider);
    SetRidingFlag(rider, false);
    if (!Horse(rider)) return true;
    StartTimer(rider, false);
    if (!Horse(rider)) return true;
    const auto revision = Horse(rider)->timerRevision;
    const auto receipt = [&] { const auto* s = Horse(rider); return s && s->timerRevision == revision && !IsHorseRiding(rider); };
    quest::CQuestManager::instance().Unmount(ecs::PlayerRuntime::GetPlayerID(rider));
    if (!receipt()) return true;
    if (!CombatSystem::IsDead(rider) && !CombatSystem::IsStun(rider)) {
        SetMountVnum(rider, 0);
        if (receipt()) SummonHorse(rider, true, false, oldVnum);
    } else {
        if (auto* state = g_registry.try_get<ecs::MountState>(rider)) state->mountVnum = 0;
        ecs::PointSystem::Compute(rider);
        if (receipt()) NetworkSyncSystem::UpdatePacket(rider);
    }
    for (const auto point : {POINT_ST, POINT_DX, POINT_HT, POINT_IQ})
        if (receipt()) ecs::PointSystem::Change(rider, point, 0);
    if (receipt()) Dirty(rider);
    return true;
}
void ClearHorseInfo(entt::entity rider) {
    if (!g_registry.valid(rider)) return;
    const bool hide = !IsHorseRiding(rider);
    SetSummonedHorse(rider, entt::null);
    if (!g_registry.valid(rider)) return;
    if (hide) {
        if (auto* state = g_registry.try_get<ecs::MountState>(rider))
            state->sendHorseLevel = state->sendHorseHealthGrade = state->sendHorseStaminaGrade = 0;
    }
    Dirty(rider);
    if (hide && g_registry.valid(rider)) ecs::ChatSystem::Send(rider, CHAT_TYPE_COMMAND, "hide_horse_state");
}
void SendHorseInfo(entt::entity rider) {
    if (!Horse(rider) || (GetSummonedHorse(rider) == entt::null && !IsHorseRiding(rider))) return;
    auto* state = g_registry.try_get<ecs::MountState>(rider);
    if (!state) return;
    const int hp = GetHorseHealth(rider), maxHP = GetHorseMaxHealth(rider);
    const int stamina = GetHorseStamina(rider), maxStamina = GetHorseMaxStamina(rider);
    const int healthGrade = !hp ? 0 : hp * 10 <= maxHP * 3 ? 1 : hp * 10 <= maxHP * 7 ? 2 : 3;
    const int staminaGrade = stamina * 10 <= maxStamina ? 0 : stamina * 10 <= maxStamina * 3 ? 1 :
        stamina * 10 <= maxStamina * 7 ? 2 : 3;
    const int level = GetHorseLevel(rider);
    if (state->sendHorseLevel == level && state->sendHorseHealthGrade == healthGrade &&
        state->sendHorseStaminaGrade == staminaGrade) return;
    state->sendHorseLevel = level; state->sendHorseHealthGrade = healthGrade; state->sendHorseStaminaGrade = staminaGrade;
    Dirty(rider);
    if (Horse(rider)) ecs::ChatSystem::Send(rider, CHAT_TYPE_COMMAND, "horse_state %d %d %d", level, healthGrade, staminaGrade);
}
} // namespace MountSystem
