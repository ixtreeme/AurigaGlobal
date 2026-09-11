#include "../../stdafx.h"
#include "MovementSystem.hpp"
#include "ViewSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "AcceSystem.hpp"
#include "AffectSystem.hpp"
#include "PointSystem.hpp"
#include "InventorySystem.hpp"
#include "SocialSystem.hpp"
#include "QuestSystem.hpp"
#include "NetworkSyncSystem.hpp"

#include "CombatSystem.hpp"
#include "SessionSystem.hpp"
#include "SkillSystem.hpp"
#include "MountSystem.hpp"

#include <algorithm>
#include <cmath>
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

// Stun schedules this; the body is further down with the other event handlers.
EVENTFUNC(StunEvent);

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

// The mob-table half of berserk and godspeed. CHARACTER read the AI flag word
// and then the AIFlags component; both are reachable from the entity, so the
// test moves here whole.
// Nearest of the attackers that have hurt this character. The damage map is
// already keyed by entity, so the walk never leaves entity handles: the old
// version resolved a character for every candidate and one more for the answer.
// Resolving `self` stays, because the map is still a CHARACTER member.
entt::entity GetNearestVictim(entt::entity attacker, entt::entity from)
{
    LPCHARACTER self = LegacyCharOf(attacker);
    if (!self)
        return entt::null;

    const entt::entity origin = (from != entt::null && g_registry.valid(from)) ? from : attacker;

    float nearest = 99999.0f;
    entt::entity victim = entt::null;

    for (const auto& [candidate, damage] : DamageLedgerOf(attacker).entries) {
        if (candidate == entt::null || !g_registry.valid(candidate))
            continue;

        if (AffectSystem::IsAffectFlag(candidate, AFF_EUNHYUNG) ||
            AffectSystem::IsAffectFlag(candidate, AFF_INVISIBILITY) ||
            AffectSystem::IsAffectFlag(candidate, AFF_REVIVE_INVISIBLE))
            continue;

        const float distance = DISTANCE_APPROX(
            ecs::PlayerRuntime::GetX(candidate) - ecs::PlayerRuntime::GetX(origin),
            ecs::PlayerRuntime::GetY(candidate) - ecs::PlayerRuntime::GetY(origin));

        if (distance < nearest) {
            victim = candidate;
            nearest = distance;
        }
    }

    return victim;
}

ecs::MobInstanceState* MobState(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;
    return g_registry.try_get<ecs::MobInstanceState>(e);
}

const ecs::MobInstanceState* MobStateConst(entt::entity e)
{
    return MobState(e);
}

bool IsBerserk(entt::entity e)
{
    const auto* state = MobState(e);
    return state && state->isBerserk;
}

void SetBerserk(entt::entity e, bool value)
{
    if (auto* state = MobState(e))
        state->isBerserk = value;
}

bool IsGodSpeed(entt::entity e)
{
    const auto* state = MobState(e);
    return state && state->isGodSpeed;
}

void SetGodSpeed(entt::entity e, bool value)
{
    auto* state = MobState(e);
    if (!state)
        return;

    state->isGodSpeed = value;

    // Godspeed pins attack speed while it lasts and hands the prototype
    // value back when it ends.
    const TMobTable* table = ecs::PlayerRuntime::GetMobTable(e);
    ecs::PointSystem::Set(e, POINT_ATT_SPEED, value ? 250 : (table ? table->sAttackSpeed : 0));
}

bool IsRevive(entt::entity e)
{
    const auto* state = MobState(e);
    return state && state->isRevive;
}

void SetRevive(entt::entity e, bool value)
{
    if (auto* state = MobState(e))
        state->isRevive = value;
}

// Two readings Follow needs, phrased so the caller never holds the state.
uint32_t GetLastAttackedTime(entt::entity e)
{
    const auto* state = MobState(e);
    return state ? state->lastAttackedTime : 0;
}

int32_t DistanceFromLastAttacked(entt::entity e)
{
    const auto* state = MobState(e);
    if (!state)
        return 0;
    return DISTANCE_APPROX(state->lastAttackedX - ecs::PlayerRuntime::GetX(e),
                           state->lastAttackedY - ecs::PlayerRuntime::GetY(e));
}

void SetLastAttacked(entt::entity e, uint32_t when)
{
    auto* state = MobState(e);
    if (!state)
        return;

    state->lastAttackedTime = when;
    state->lastAttackedX = ecs::PlayerRuntime::GetX(e);
    state->lastAttackedY = ecs::PlayerRuntime::GetY(e);
    state->lastAttackedZ = ecs::PlayerRuntime::GetZ(e);
}

bool IsBerserker(entt::entity e)
{
    if (IS_SET(ecs::PlayerRuntime::GetAIFlag(e), AIFLAG_BERSERK))
        return true;
    const auto* flags = AIHelpers::TryGetFlags(e);
    return flags && flags->isBerserk;
}

bool IsGodSpeeder(entt::entity e)
{
    if (IS_SET(ecs::PlayerRuntime::GetAIFlag(e), AIFLAG_GODSPEED))
        return true;
    const auto* flags = AIHelpers::TryGetFlags(e);
    return flags && flags->isGodSpeed;
}

// Who this mob is guarding: the stone it spawned from, else its party leader.
// Entity in, entity out - the caller no longer needs a character to ask.
entt::entity GetStone(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return entt::null;
    const auto* owner = g_registry.try_get<ecs::StoneOwner>(e);
    if (!owner || owner->stone == entt::null || !g_registry.valid(owner->stone))
        return entt::null;
    return owner->stone;
}

entt::entity GetProtege(entt::entity e)
{
    if (const entt::entity stone = GetStone(e); stone != entt::null)
        return stone;
    // The party leader is still a CHARACTER-side lookup; SocialSystem owns it.
    return ecs::SocialSystem::GetPartyLeader(e);
}

// Run away: pick a reachable bearing at increasing distance and walk it.
void CowardEscape(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    const int distances[4] = {500, 1000, 3000, 5000};
    for (int band = 2; band >= 0; --band) {
        for (int attempt = 0; attempt < 8; ++attempt) {
            ecs::MovementSystem::SetRotation(e, number(0, 359));

            float fx = 0.0f, fy = 0.0f;
            const float dist = number(distances[band], distances[band + 1]);
            GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(e), dist, &fx, &fy);

            const int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(e);
            const int32_t x = ecs::PlayerRuntime::GetX(e);
            const int32_t y = ecs::PlayerRuntime::GetY(e);

            bool blocked = false;
            for (int step = 1; step <= 100; ++step) {
                if (!ecs::IsMovablePosition(mapIndex, x + static_cast<int>(fx) * step / 100,
                        y + static_cast<int>(fy) * step / 100)) {
                    blocked = true;
                    break;
                }
            }
            if (blocked)
                continue;

            AIHelpers::SetStateDuration(e, PASSES_PER_SEC(1));

            const int destX = x + static_cast<int>(fx);
            const int destY = y + static_cast<int>(fy);
            if (ecs::MovementSystem::Goto(e, destX, destY))
                ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 0, 0, 0, 0);

            LOG_INFO("WAEGU move to {} {} (far)", destX, destY);
            return;
        }
    }
}

bool CanBeginFight(entt::entity e)
{
    if (!ecs::MovementSystem::CanMove(e))
        return false;

    return ecs::PlayerRuntime::GetPosition(e) == POS_STANDING && !IsDead(e) && !IsStun(e);
}

void BeginFight(entt::entity attacker, entt::entity victim)
{
    SetVictim(attacker, victim);
    ecs::PlayerRuntime::SetPosition(attacker, POS_FIGHTING);
    AIHelpers::SetNextStatePulse(attacker, 1);
}

#ifdef __DEFENSE_WAVE__
// Was a const CHARACTER method that never touched this - a vnum range test.
// Spelling kept as it was so it still greps against the original.
static bool IsDefanceWaweMastAttackMob(int32_t vnum)
{
    return (vnum >= 3401 && vnum <= 3405) || (vnum >= 3601 && vnum <= 3605) ||
           (vnum >= 3950 && vnum <= 3964);
}
#endif

// Whether a party leader may summon this member: high leadership always, or
// middling leadership shortly after the member died. The window compares a
// millisecond clock against a 180 offset, so it is 180ms wide rather than the
// 180s the surrounding code implies - preserved as it stands.
bool CanSummon(entt::entity e, int iLeaderShip)
{
    if (iLeaderShip >= 20)
        return true;
    if (iLeaderShip < 12)
        return false;
    const auto* dead = (e != entt::null && g_registry.valid(e))
        ? g_registry.try_get<ecs::LastDeadTime>(e) : nullptr;
    return dead && (dead->value + 180) > get_dword_time();
}

bool GetDeadByMonster(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    const auto* state = g_registry.try_get<ecs::DeadByMonster>(e);
    return state && state->value;
}

void SetDeadByMonster(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::DeadByMonster>(e).value = value;
}

uint32_t GetKillerPID(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* killer = g_registry.try_get<ecs::KillerPID>(e);
    return killer ? killer->value : 0;
}

void SetKillerPID(entt::entity e, uint32_t pid)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::KillerPID>(e).value = pid;
}

bool IsUndying(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    const auto* state = g_registry.try_get<ecs::UndyingState>(e);
    return state && state->value;
}

void SetUndying(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::UndyingState>(e).value = value;
}

bool GetInvincible(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    const auto* state = g_registry.try_get<ecs::InvincibleState>(e);
    return state && state->value;
}

// Returns whether the write landed. The CHARACTER version returned a
// constant 1, and callers used it as "did I have a target"; a handle that
// has gone now answers false instead of pretending.
bool SetInvincible(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    g_registry.get_or_emplace<ecs::InvincibleState>(e).value = value;
    return true;
}

void IncreaseMobRigHP(entt::entity e, int32_t amount)
{
    ecs::PointSystem::Change(e, POINT_HP_REGEN,
        ecs::PointSystem::Get(e, POINT_HP_REGEN) + amount, true);
}

void CreateFly(entt::entity attacker, uint8_t flyType, entt::entity victim)
{
    TPacketGCCreateFly pack;
    pack.bHeader = HEADER_GC_CREATE_FLY;
    pack.bType = flyType;
    pack.dwStartVID = ecs::PlayerRuntime::GetPacketVID(attacker);
    pack.dwEndVID = ecs::PlayerRuntime::GetPacketVID(victim);

    ecs::ViewSystem::PacketView(attacker, &pack, sizeof(TPacketGCCreateFly));
}

uint32_t GetSkipComboAttackByTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;
    const auto* skip = g_registry.try_get<ecs::ComboSkipUntil>(e);
    return skip ? skip->value : 0;
}

int GetMaxAggro(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return -100;
    const auto* aggro = g_registry.try_get<ecs::AggroState>(e);
    return aggro ? aggro->maxAggro : -100;
}

void SetMaxAggro(entt::entity e, int value)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::AggroState>(e).maxAggro = value;
}

// Who this character should be hitting, decided by aggro. The damage map is
// keyed by entity, so the search never leaves a handle: the old version
// resolved a character for every entry just to test distance and death.
void ChangeVictimByAggro(entt::entity self, int newAggro, entt::entity newVictim)
{
    if (self == entt::null || !g_registry.valid(self))
        return;

    // A victim chosen less than three seconds ago is left alone.
    if (get_dword_time() - GetVictimSetTime(self) < 3000)
        return;

    const entt::entity current = GetVictim(self);

    const auto adopt = [&](entt::entity candidate, int aggro) {
        // A handle that reached here through a damage report can have been
        // retired since; adopting it would leave the mob swinging at a slot
        // that now belongs to somebody else, or to nobody.
        if (candidate == entt::null || !g_registry.valid(candidate))
            return;
        SetMaxAggro(self, aggro);
#ifdef __DEFENSE_WAVE__
        if (IsDefanceWaweMastAttackMob(ecs::PlayerRuntime::GetRaceNum(self)))
            return;
#endif
        SetVictim(self, candidate);
        AIHelpers::SetStateDuration(self, 1);
    };

    if (newVictim != current) {
        // Somebody else: they only take over by out-aggroing the ceiling.
        if (GetMaxAggro(self) < newAggro)
            adopt(newVictim, newAggro);
        return;
    }

    // The current victim reporting in. A higher figure just raises the
    // ceiling; a lower one means somebody in the damage map may now be
    // angrier, so look for the angriest one still alive and within reach.
    if (GetMaxAggro(self) < newAggro) {
        SetMaxAggro(self, newAggro);
        return;
    }

    LPCHARACTER owner = LegacyCharOf(self);
    if (!owner)
        return;

    const int32_t x = ecs::PlayerRuntime::GetX(self);
    const int32_t y = ecs::PlayerRuntime::GetY(self);

    entt::entity best = entt::null;
    int bestAggro = newAggro;

    for (const auto& [candidate, battle] : DamageLedgerOf(self).entries) {
        if (battle.aggro <= bestAggro)
            continue;
        if (candidate == entt::null || !g_registry.valid(candidate) || IsDead(candidate))
            continue;
        if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(candidate) - x,
                            ecs::PlayerRuntime::GetY(candidate) - y) >= 5000)
            continue;

        best = candidate;
        bestAggro = battle.aggro;
    }

    if (best != entt::null)
        adopt(best, bestAggro);
}

void SetVictim(entt::entity attacker, entt::entity victim)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, attacker))
        return;
    const auto target = victim != attacker && ecs::Invariants::HasAnyTypeTag(g_registry, victim)
        ? victim : entt::null;
    auto& state = g_registry.get_or_emplace<ecs::CombatTarget>(attacker, entt::null, get_dword_time() - 3000);
    state.target = target;
    if (target != entt::null)
        state.setTime = get_dword_time();
    else
        battle_end(attacker);
}

entt::entity GetVictim(entt::entity attacker)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, attacker))
        return entt::null;
    const auto* state = g_registry.try_get<ecs::CombatTarget>(attacker);
    return state && ecs::Invariants::HasAnyTypeTag(g_registry, state->target)
        ? state->target : entt::null;
}

uint32_t GetVictimSetTime(entt::entity attacker)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, attacker))
        return 0;
    const auto* state = g_registry.try_get<ecs::CombatTarget>(attacker);
    return state ? state->setTime : get_dword_time() - 3000;
}

uint32_t GetLastAttackTime(entt::entity e)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e))
        return 0;
    const auto* state = g_registry.try_get<ecs::AttackCooldown>(e);
    return state ? state->lastAttackTime : get_dword_time() - 20000;
}

void SetLastAttackTime(entt::entity e, uint32_t time)
{
    if (ecs::Invariants::HasAnyTypeTag(g_registry, e))
        g_registry.get_or_emplace<ecs::AttackCooldown>(e).lastAttackTime = time;
}

bool IsSkillHit(entt::entity e)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e))
        return false;
    const auto* state = g_registry.try_get<ecs::SkillHitState>(e);
    return state && state->value;
}

void SetSkillHit(entt::entity e, bool value)
{
    if (ecs::Invariants::HasAnyTypeTag(g_registry, e))
        g_registry.get_or_emplace<ecs::SkillHitState>(e).value = value;
}

namespace {
const ecs::MobDataRef* MobData(entt::entity e)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e))
        return nullptr;
    const auto* mob = g_registry.try_get<ecs::MobDataRef>(e);
    return mob && mob->data ? mob : nullptr;
}
}

uint32_t GetMobDamageMin(entt::entity e)
{
    const auto* mob = MobData(e);
    return mob ? mob->data->m_table.dwDamageRange[0] : 0;
}

uint32_t GetMobDamageMax(entt::entity e)
{
    const auto* mob = MobData(e);
    return mob ? mob->data->m_table.dwDamageRange[1] : 0;
}

float GetMobDamageMultiplier(entt::entity e)
{
    const auto* mob = MobData(e);
    if (!mob)
        return 1.0f;
    // AIFlags may lag AI changes; MobInstanceState is what SetBerserk writes.
    const float multiplier = mob->data->m_table.fDamMultiply *
        (IsBerserk(e) ? 2.0f : 1.0f);
    return std::isfinite(multiplier) && multiplier >= 0 ? multiplier : 1.0f;
}

uint8_t GetMobBattleType(entt::entity e)
{
    const auto* mob = MobData(e);
    return mob ? mob->data->m_table.bBattleType : BATTLE_TYPE_MELEE;
}

uint16_t GetMobAttackRange(entt::entity e)
{
    const auto* mob = MobData(e);
    if (!mob)
        return 0;
    const auto& table = mob->data->m_table;
    int64_t range = table.wAttackRange;
    if (table.bBattleType == BATTLE_TYPE_RANGE || table.bBattleType == BATTLE_TYPE_MAGIC)
    {
#ifdef __DEFENSE_WAVE__
        const auto race = ecs::PlayerRuntime::GetRaceNum(e);
        if (race == 3960 || race == 3961 || race == 3962)
            range += ecs::PointSystem::Get(e, POINT_BOW_DISTANCE) + 4000;
#else
        range += ecs::PointSystem::Get(e, POINT_BOW_DISTANCE);
#endif
    }
#ifdef __DEFENSE_WAVE__
    else
    {
        const auto race = ecs::PlayerRuntime::GetRaceNum(e);
        if ((race >= 3950 && race <= 3955 && race != 3953) ||
            (race >= 3601 && race <= 3605 && race != 3602))
            range += 300;
    }
#endif
    return static_cast<uint16_t>(std::clamp<int64_t>(range, 0, UINT16_MAX));
}

bool CanFight(entt::entity e)
{
    return ecs::PlayerRuntime::GetPosition(e) >= POS_FIGHTING;
}

// The brief untouchable window after a revive or a rescue.
void ReviveInvisible(entt::entity e, int iDur)
{
    AffectSystem::AddAffect(e, AFFECT_REVIVE_INVISIBLE, POINT_NONE, 0,
        AFF_REVIVE_INVISIBLE, iDur, 0, true);
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



uint32_t GetAlignment(entt::entity e)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e)) return 0;
    const auto* state = g_registry.try_get<ecs::CombatStats>(e);
    return state ? state->alignment : 0;
}

uint32_t GetRealAlignment(entt::entity e)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e)) return 0;
    const auto* state = g_registry.try_get<ecs::CombatStats>(e);
    return state ? state->realAlignment : 0;
}

uint8_t GetAlignmentGrade(entt::entity e)
{
    const uint32_t alignment = GetRealAlignment(e) / 10;
    static constexpr uint32_t ceilings[] {
        4999,14999,19999,29999,49999,74999,99999,124999,174999,249999,
        499999,749999,999999,1499999,2499999,2999999,3499999,3999999,4499999,4999999
    };
    return static_cast<uint8_t>(std::lower_bound(std::begin(ceilings), std::end(ceilings), alignment) - std::begin(ceilings));
}

void UpdateAlignment(entt::entity e, int64_t amount)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e)) return;
    const auto oldGrade = GetAlignmentGrade(e);
    const auto oldVisibleAlignment = GetAlignment(e) / 10;
    uint64_t revision;
    {
        auto& state = g_registry.get_or_emplace<ecs::CombatStats>(e);
        // Clamp the delta before adding: signed reductions and INT64_MIN/MAX
        // must neither wrap to the maximum rank nor overflow the accumulator.
        const int64_t current = std::min(state.realAlignment, MAX_ALIGNMENT);
        const auto next = static_cast<uint32_t>(std::clamp<int64_t>(
            current + std::clamp<int64_t>(amount, -static_cast<int64_t>(MAX_ALIGNMENT), MAX_ALIGNMENT),
            0, MAX_ALIGNMENT));
        if (state.alignment == next && state.realAlignment == next) return;
        state.alignment = state.realAlignment = next;
        revision = ++state.alignmentRevision;
    }
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    if (oldGrade != GetAlignmentGrade(e)) ecs::PointSystem::Compute(e);
    // Compute can destroy the owner or perform a newer alignment update.
    // Never publish an obsolete outer operation (including an ABA update).
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e)) return;
    const auto* state = g_registry.try_get<ecs::CombatStats>(e);
    if (!state || state->alignmentRevision != revision) return;
    if (oldVisibleAlignment != state->alignment / 10)
        NetworkSyncSystem::BroadcastCharAdditionalInfo(g_registry, e);
}

void SetKillerMode(entt::entity e, bool isOn)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e) || IsKillerMode(e) == isOn) return;
    g_registry.get_or_emplace<ecs::StatusFlags>(e).isKillerMode = isOn;
    g_registry.get_or_emplace<ecs::CombatStats>(e).killerModePulse = static_cast<uint32_t>(thecore_pulse());
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    LOG_INFO("SetKillerMode Update {}[{}]", ecs::PlayerRuntime::GetName(e).data(), ecs::PlayerRuntime::GetPlayerID(e));
    NetworkSyncSystem::UpdatePacket(e);
}

bool IsKillerMode(entt::entity e)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e)) return false;
    const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
    return status && status->isKillerMode;
}

void UpdateKillerMode(entt::entity e)
{
    if (!IsKillerMode(e)) return;
    const auto* state = g_registry.try_get<ecs::CombatStats>(e);
    if (!state) return;
    const uint32_t elapsed = static_cast<uint32_t>(thecore_pulse()) - state->killerModePulse;
    const uint64_t timeout = static_cast<uint64_t>(std::max(passes_per_sec, 1)) * 30;
    if (elapsed >= timeout) SetKillerMode(e, false);
}

void SetPKMode(entt::entity e, uint8_t mode)
{
    if (mode >= PK_MODE_MAX_NUM || !ecs::Invariants::HasAnyTypeTag(g_registry, e)) return;
    if (mode == PK_MODE_GUILD && !ecs::SocialSystem::GetGuild(e)) mode = PK_MODE_FREE;
    {
        auto& state = g_registry.get_or_emplace<ecs::CombatStats>(e);
        if (state.pkMode == mode) return;
        state.pkMode = mode;
    }
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    LOG_INFO("PK_MODE: {} {}", ecs::PlayerRuntime::GetName(e).data(), mode);
    NetworkSyncSystem::UpdatePacket(e);
}

uint8_t GetPKMode(entt::entity e)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e)) return PK_MODE_PROTECT;
    const auto* state = g_registry.try_get<ecs::CombatStats>(e);
    return state ? state->pkMode : PK_MODE_PROTECT;
}


float GetAttackMultiplier(entt::entity e)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e)) return 1.0f;
    const auto* state = g_registry.try_get<ecs::CombatStats>(e);
    return state ? state->attackMultiplier : 1.0f;
}

void SetAttackMultiplier(entt::entity e, float multiplier)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e) || !std::isfinite(multiplier) || multiplier < 0) return;
    g_registry.get_or_emplace<ecs::CombatStats>(e).attackMultiplier = multiplier;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

float GetDamageMultiplier(entt::entity e)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e)) return 1.0f;
    const auto* state = g_registry.try_get<ecs::CombatStats>(e);
    return state ? state->damageMultiplier : 1.0f;
}

void SetDamageMultiplier(entt::entity e, float multiplier)
{
    if (!ecs::Invariants::HasAnyTypeTag(g_registry, e) || !std::isfinite(multiplier) || multiplier < 0) return;
    g_registry.get_or_emplace<ecs::CombatStats>(e).damageMultiplier = multiplier;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}


void SendLeaderboardData(entt::entity e)
{
	LPDESC desc = ecs::PlayerRuntime::GetDesc(e);
	if (!desc)
		return;

	// SQL lek?dez? top 10 j??osra
	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
		"SELECT name, level, r5, r8 FROM player.player ORDER BY r5 DESC LIMIT 10"));

	if (!pMsg || !pMsg->Get() || !pMsg->Get()->pSQLResult)
		return;

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

	desc->Packet(&p, sizeof(p));


}

void SendLeaderboardDataGuild(entt::entity e)
{
	LPDESC desc = ecs::PlayerRuntime::GetDesc(e);
	if (!desc)
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

	desc->Packet(&p, sizeof(p));
}

// Neither of these two reads a character; they were static CHARACTER members.
// CheckLeaderboardSkillMobChanges keeps its own last-seen top ten in a function
// static, so it is one board for the server however it is reached.
std::vector<LeaderboardEntry> FetchTop10SkillMob()
{
	std::vector<LeaderboardEntry> list;
	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
		"SELECT name, level, skill_victim, map1_skillmob "
		"FROM player.player ORDER BY map1_skillmob DESC LIMIT 10"));

	if (!pMsg || !pMsg->Get() || !pMsg->Get()->pSQLResult)
		return list;

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

void CheckLeaderboardSkillMobChanges()
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

void SendLeaderboardDataSkillMob(entt::entity e, entt::entity viewerEntity)
{
	if (!g_registry.valid(e) || !g_registry.valid(viewerEntity) || !ecs::PlayerRuntime::GetDesc(viewerEntity))
		return;

	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(
		"SELECT name, level, map1_skillmob, skill_victim "
		"FROM player.player ORDER BY map1_skillmob DESC LIMIT 10"));

	if (!pMsg || !pMsg->Get() || !pMsg->Get()->pSQLResult) return;
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

	TPacketGCLeaderboardNews p {};
	p.header = HEADER_GC_LEADERBOARD_NEWS;
	strlcpy(p.data, result.c_str(), sizeof(p.data));

	if (g_registry.valid(viewerEntity))
		if (auto* desc = ecs::PlayerRuntime::GetDesc(viewerEntity); desc && desc->GetEntity() == viewerEntity)
			desc->Packet(&p, sizeof(p));
}

bool IsDeathBlow(entt::entity e)
{
    // The legacy method dereferenced m_pkMobData unguarded, so calling it on
    // anything without mob data was a crash. Absent data is false here.
    if (e == entt::null || !g_registry.valid(e))
        return false;

    const auto* mob = g_registry.try_get<ecs::MobDataRef>(e);
    if (!mob || !mob->data)
        return false;

    return number(1, 100) <= mob->data->m_table.bDeathBlowPoint;
}

bool IsDeathBlower(entt::entity e)
{
    // Both branches of the legacy method, kept in order: the live AI flag
    // first, then the AIFlags bit EntityFactory derives from it at spawn.
    if (e == entt::null || !g_registry.valid(e))
        return false;

    if (const auto* runtime = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e))
        if (IS_SET(runtime->aiFlag, AIFLAG_DEATHBLOW))
            return true;

    const auto* flags = AIHelpers::TryGetFlags(e);
    return flags && flags->isDeathBlower;
}

} // namespace CombatSystem

// char_battle.cpp slice BE1 moved into CombatSystem.cpp

struct FuncForgetMyAttacker
{
	entt::entity m_character;
	explicit FuncForgetMyAttacker(entt::entity character) : m_character(character) {}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			const entt::entity ch = ent->GetEntityHandle();
			const entt::entity candidate = ch;
			if (ecs::PlayerRuntime::IsPC(candidate))
				return;
			if (CombatSystem::GetVictim(candidate) == m_character)
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
			if (CombatSystem::GetVictim(candidate) != entt::null)
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
			if (CombatSystem::GetVictim(candidate) != entt::null)
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
			if (CombatSystem::GetVictim(candidate) != entt::null && CombatSystem::GetVictim(candidate) != m_character)
				return;
			if (CombatSystem::GetMobAttackRange(candidate) > 150)
			{
				int iNewRange = 150;//(int)(CombatSystem::GetMobAttackRange(candidate) * 0.2);
				if (iNewRange < 150)
					iNewRange = 150;

				AffectSystem::AddAffect(candidate, AFFECT_BOW_DISTANCE, POINT_BOW_DISTANCE, iNewRange - CombatSystem::GetMobAttackRange(candidate), AFF_NONE, 3 * 60, 0, false);
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
			ecs::MovementSystem::Goto(candidate, tx, ty);
			ecs::MovementSystem::CalculateMoveDuration(candidate);

			NetworkSyncSystem::BroadcastSyncPacket(g_registry, candidate);
		}
	}
};


namespace CombatSystem {

void ForgetMyAttacker(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    FuncForgetMyAttacker f(e);
    ecs::ForEachAround(g_registry, e, f);
    ReviveInvisible(e, 5);
}

void AggregateMonster(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    FuncAggregateMonster f(e);
    ecs::ForEachAround(g_registry, e, f);
}

void AggregateMonsterPlus(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    FuncAggregateMonsterPlus f(e);
    ecs::ForEachAround(g_registry, e, f);
}

void AttractRanger(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    FuncAttractRanger f(e);
    ecs::ForEachAround(g_registry, e, f);
}

void PullMonster(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    FuncPullMonster f(e);
    ecs::ForEachAround(g_registry, e, f);
}

} // namespace CombatSystem

// char_battle.cpp slice BE2a moved into CombatSystem.cpp

#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
#endif
// char_battle.cpp slice BE3 moved into CombatSystem.cpp

#ifdef LEADERBOARD_RAZOR93


#ifdef LEADERBOARD_RAZOR93
#endif


#ifdef LEADERBOARD_RAZOR93

#endif

#endif

// char_battle.cpp slice BE2b moved into CombatSystem.cpp

namespace CombatSystem {

void DistributeHP(entt::entity victim, entt::entity killer)
{
	// The body below the dungeon test was removed long ago; what is left does
	// nothing whichever way the test goes. Carried over as it stands.
	if (ecs::SocialSystem::GetDungeon(killer)) //  ΰʴ´
		return;
}

ecs::DamageLedger& DamageLedgerOf(entt::entity e)
{
    static ecs::DamageLedger stale;
    if (e == entt::null || !g_registry.valid(e))
    {
        // A handle that no longer names a character gets a scratch ledger, so
        // callers that write through the reference need not check first.
        stale.entries.clear();
        return stale;
    }
    return g_registry.get_or_emplace<ecs::DamageLedger>(e);
}

void ClearDamageLedger(entt::entity e)
{
    if (auto* ledger = e != entt::null && g_registry.valid(e)
            ? g_registry.try_get<ecs::DamageLedger>(e) : nullptr)
        ledger->entries.clear();
}

// Aggro bookkeeping for one hit: the figure is weighted by how the damage
// was delivered, the standing victim gets a loyalty bonus, and the running
// total is what decides whether the target changes.
void UpdateAggrPointEx(entt::entity self, entt::entity attacker, uint8_t rawType,
    int dam, ecs::BattleContribution& info)
{
    const EDamageType type = static_cast<EDamageType>(rawType);
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
	if (attacker == GetVictim(self))
		dam = (int)(dam * 1.2f);

	info.aggro += dam;

	if (info.aggro < 0)
		info.aggro = 0;

	//LOG_INFO(0, "UpdateAggrPointEx for %s by %s dam %d total %d", ecs::PlayerRuntime::GetName(self), ecs::PlayerRuntime::GetName(eAttacker).data(), dam, total);
	if (ecs::SocialSystem::GetParty(self) && dam > 0 && type != DAMAGE_TYPE_SPECIAL)
	{
		LPPARTY pParty = ecs::SocialSystem::GetParty(self);

		//     ϴ
		int iPartyAggroDist = dam;

		if (pParty->GetLeaderPID() == ecs::PlayerRuntime::GetPacketVID(self))
			iPartyAggroDist /= 2;
		else
			iPartyAggroDist /= 3;

		pParty->SendMessage(self, PM_AGGRO_INCREASE, iPartyAggroDist, ecs::PlayerRuntime::GetPacketVID(attacker));
	}

	CombatSystem::ChangeVictimByAggro(self, info.aggro, attacker);
}

} // namespace CombatSystem

namespace CombatSystem {

// One attacker's running total against this character.
void UpdateAggrPoint(entt::entity e, entt::entity attacker, EDamageType type, int dam)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	if (CombatSystem::IsDead(e) || CombatSystem::IsStun(e))
		return;

	const entt::entity eAttacker = attacker;
	if (eAttacker == entt::null)
		return;

	std::map<entt::entity, ecs::BattleContribution>::iterator it = CombatSystem::DamageLedgerOf(e).entries.find(eAttacker);

	if (it == CombatSystem::DamageLedgerOf(e).entries.end())
	{
		CombatSystem::DamageLedgerOf(e).entries.insert(std::map<entt::entity, ecs::BattleContribution>::value_type(eAttacker, ecs::BattleContribution(0, dam)));
		it = CombatSystem::DamageLedgerOf(e).entries.find(eAttacker);
	}

	CombatSystem::UpdateAggrPointEx(e, attacker, type, dam, it->second);
}

} // namespace CombatSystem

// char_battle.cpp slice BD2b moved into CombatSystem.cpp

static uint32_t __GetPartyExpNP(const uint32_t level);
static uint32_t AdjustExpByLevel_Combat(const LegacyCharHandle ch, const uint32_t exp);

namespace CombatSystem {

void Stun(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	// CloseMyShop has no entity form yet; it is its own migration.
	LPCHARACTER self = ecs::LegacyCharOf(e);
	if (!self)
		return;

	if (CombatSystem::IsStun(e))
		return;

	if (CombatSystem::IsDead(e))
		return;

	if (!ecs::PlayerRuntime::IsPC(e))
	{
		if (LPPARTY party = ecs::SocialSystem::GetParty(e))
			party->SendMessage(e, PM_ATTACKED_BY, 0, 0);
	}

	LOG_INFO("{}: Stun {}", ecs::PlayerRuntime::GetName(e).data(), static_cast<uint32_t>(e));

	ecs::PointSystem::Change(e, POINT_HP_RECOVERY, -ecs::PointSystem::Get(e, POINT_HP_RECOVERY));
	ecs::PointSystem::Change(e, POINT_SP_RECOVERY, -ecs::PointSystem::Get(e, POINT_SP_RECOVERY));

	ecs::SocialSystem::CloseMyShop(e);

	ecs::PlayerRuntime::CancelCharEvent(e, ecs::PlayerRuntime::CharEvent::Recovery); // ȸ ̺Ʈ δ.

	TPacketGCStun pack;
	pack.header = HEADER_GC_STUN;
	pack.vid = ecs::PlayerRuntime::GetPacketVID(e);
	ecs::ViewSystem::PacketView(e, &pack, sizeof(pack));

		if (auto* flags = RuntimeFlags(e))
		SET_BIT(flags->instantFlag, INSTANT_FLAG_STUN);
	if (g_registry.valid(e))
	{
		g_registry.emplace_or_replace<ecs::StunTag>(e);
		if (auto* status = g_registry.try_get<ecs::StatusFlags>(e))
			status->isStunned = true;
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}

	if (ecs::PlayerRuntime::GetCharEvent(e, ecs::PlayerRuntime::CharEvent::Stun))
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = e;

	ecs::PlayerRuntime::SetCharEvent(e, ecs::PlayerRuntime::CharEvent::Stun,
		event_create(StunEvent, info, PASSES_PER_SEC(3)));
}

} // namespace CombatSystem


#define ENABLE_NEWEXP_CALCULATION
#ifdef ENABLE_NEWEXP_CALCULATION
#define NEW_GET_LVDELTA(me, victim) aiPercentByDeltaLev[MINMAX(0, (victim + 15) - me, MAX_EXP_DELTA_OF_LEV - 1)]
typedef long double rate_t;
static void GiveExp(entt::entity fromEntity, entt::entity toEntity, int iExp)
{
	// The marriage bonus, the mount vnum, the pet system, the unique-group test
	// and the PC-bang flag have no entity form yet; each is its own migration
	// and they share this one resolve.
	LPCHARACTER to = ecs::LegacyCharOf(toEntity);
	if (!to)
		return;
	if (test_server && iExp < 0)
	{
		ecs::ChatSystem::Send(toEntity, CHAT_TYPE_INFO, "exp(%d) overflow", iExp);
		return;
	}
	// decrease/increase exp based on player<>mob level
	rate_t lvFactor = static_cast<rate_t>(NEW_GET_LVDELTA(ecs::PointSystem::GetLevel(toEntity), ecs::PointSystem::GetLevel(fromEntity))) / 100.0L;
	iExp *= lvFactor;
	// start calculating rate exp bonus
	int iBaseExp = iExp;
	rate_t rateFactor = 100;

	rateFactor += CPrivManager::instance().GetPriv(toEntity, PRIV_EXP_PCT);
	if (ItemSystem::IsEquipUniqueItem(toEntity, UNIQUE_ITEM_LARBOR_MEDAL))
		rateFactor += 20;
	if (ecs::PlayerRuntime::GetMapIndex(toEntity) >= 660000 && ecs::PlayerRuntime::GetMapIndex(toEntity) < 670000)
		rateFactor += 20;
#ifdef NEW_POINT_EXP_DOUBLE_BONUS_RAZOR93


	int expDoubleBonus = ecs::PointSystem::Get(toEntity, POINT_EXP_DOUBLE_BONUS);

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
	if (ecs::PointSystem::Get(toEntity, POINT_EXP_DOUBLE_BONUS))
		if (number(1, 100) <= ecs::PointSystem::Get(toEntity, POINT_EXP_DOUBLE_BONUS))
			rateFactor += 30;
#endif
	if (ItemSystem::IsEquipUniqueItem(toEntity, UNIQUE_ITEM_DOUBLE_EXP))
		rateFactor += 50;

	switch (to->GetMountVnum())
	{
	case 20110:
	case 20111:
	case 20112:
	case 20113:
		if (ItemSystem::IsEquipUniqueItem(toEntity, 71115) || ItemSystem::IsEquipUniqueItem(toEntity, 71117) || ItemSystem::IsEquipUniqueItem(toEntity, 71119) ||
			ItemSystem::IsEquipUniqueItem(toEntity, 71121))
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

	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(toEntity, PREMIUM_EXP) > 0)
		rateFactor += 50;
	if (to->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_EXP))
		rateFactor += 50;
	if (ecs::PointSystem::Get(toEntity, POINT_PC_BANG_EXP_BONUS) > 0)
	{
		if (ecs::PlayerRuntime::IsPCBang(toEntity))
			rateFactor += ecs::PointSystem::Get(toEntity, POINT_PC_BANG_EXP_BONUS);
	}
	rateFactor += to->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS);
	rateFactor += ecs::PointSystem::Get(toEntity, POINT_RAMADAN_CANDY_BONUS_EXP);
	rateFactor += ecs::PointSystem::Get(toEntity, POINT_MALL_EXPBONUS);
	// useless (never used except for china intoxication) = always 100
	rateFactor = rateFactor * static_cast<rate_t>(CHARACTER_MANAGER::instance().GetMobExpRate(toEntity)) / 100.0L;
	// apply calculated rate bonus
	iExp *= (rateFactor / 100.0L);
	if (test_server)
		ecs::ChatSystem::Send(toEntity, CHAT_TYPE_INFO, "base_exp(%d) * rate(%Lf) = exp(%d)", iBaseExp, rateFactor / 100.0L, iExp);
	// you can get at maximum only 10% of the total required exp at once (so, you need to kill at least 10 mobs to level up) (useless)
	iExp = std::min(ecs::PlayerRuntime::GetNextExp(toEntity) / 10, (uint32_t)iExp);
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
		ecs::ChatSystem::Send(toEntity, CHAT_TYPE_INFO, "exp+minGNE+adjust(%d)", iExp);
	// set
	ecs::PointSystem::Change(toEntity, POINT_EXP, iExp, true);
	CombatSystem::CreateFly(fromEntity, FLY_EXP, toEntity);
	// marriage
	{
		const entt::entity you = ecs::SocialSystem::GetMarryPartner(toEntity);
		if (you != entt::null)
		{
			// sometimes, this overflows
			uint32_t dwUpdatePoint = (2000.0L / ecs::PointSystem::GetLevel(toEntity) / ecs::PointSystem::GetLevel(toEntity) / 3) * iExp;

			if (ecs::PlayerRuntime::GetPremiumRemainSeconds(toEntity, PREMIUM_MARRIAGE_FAST) > 0 ||
				ecs::PlayerRuntime::GetPremiumRemainSeconds(you, PREMIUM_MARRIAGE_FAST) > 0)
				dwUpdatePoint *= 3;

			marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID(toEntity));

			// DIVORCE_NULL_BUG_FIX
			if (pMarriage && pMarriage->IsNear())
				pMarriage->Update(dwUpdatePoint);
			// END_OF_DIVORCE_NULL_BUG_FIX
		}
	}
}
#else
static void GiveExp(entt::entity fromEntity, entt::entity toEntity, int iExp)
{
	// The marriage bonus, the mount vnum, the pet system, the unique-group test
	// and the PC-bang flag have no entity form yet; each is its own migration
	// and they share this one resolve.
	LPCHARACTER to = ecs::LegacyCharOf(toEntity);
	if (!to)
		return;
	//  ġ
	iExp = CALCULATE_VALUE_LVDELTA(ecs::PointSystem::GetLevel(toEntity), ecs::PointSystem::GetLevel(fromEntity), iExp);

	int iBaseExp = iExp;

	// , ȸ ġ ̺Ʈ
#ifdef ENABLE_EVENT_MANAGER
	const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(EXP_EVENT, ecs::PlayerRuntime::GetEmpire(toEntity));
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
		if (ItemSystem::IsEquipUniqueItem(toEntity, UNIQUE_ITEM_LARBOR_MEDAL))
			iExp += iExp * 20 / 100;

		// Ÿ ġ ʽ
		if (ecs::PlayerRuntime::GetMapIndex(toEntity) >= 660000 && ecs::PlayerRuntime::GetMapIndex(toEntity) < 670000)
			iExp += iExp * 20 / 100; // 1.2 (20%)

		//  ġ ι Ӽ
		if (ecs::PointSystem::Get(toEntity, POINT_EXP_DOUBLE_BONUS))
			if (number(1, 100) <= ecs::PointSystem::Get(toEntity, POINT_EXP_DOUBLE_BONUS))
				iExp += iExp * 30 / 100; // 1.3 (30%)

		//   (2ð¥)
		if (ItemSystem::IsEquipUniqueItem(toEntity, UNIQUE_ITEM_DOUBLE_EXP))
			iExp += iExp * 50 / 100;

		switch (to->GetMountVnum())
		{
		case 20110:
		case 20111:
		case 20112:
		case 20113:
			if (ItemSystem::IsEquipUniqueItem(toEntity, 71115) || ItemSystem::IsEquipUniqueItem(toEntity, 71117) || ItemSystem::IsEquipUniqueItem(toEntity, 71119) ||
				ItemSystem::IsEquipUniqueItem(toEntity, 71121))
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
		if (ecs::PlayerRuntime::GetPremiumRemainSeconds(toEntity, PREMIUM_EXP) > 0)
		{
			iExp += (iExp * 50 / 100);
		}

		if (to->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_EXP) == true)
		{
			iExp += (iExp * 50 / 100);
		}

		// PC  ġ ʽ
		if (ecs::PointSystem::Get(toEntity, POINT_PC_BANG_EXP_BONUS) > 0)
		{
			if (to->IsPCBang() == true)
				iExp += (iExp * ecs::PointSystem::Get(toEntity, POINT_PC_BANG_EXP_BONUS) / 100);
		}

		// ȥ ʽ
		iExp += iExp * to->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS) / 100;
	}

	iExp += (iExp * ecs::PointSystem::Get(toEntity, POINT_RAMADAN_CANDY_BONUS_EXP) / 100);
	iExp += (iExp * ecs::PointSystem::Get(toEntity, POINT_MALL_EXPBONUS) / 100);

	if (test_server)
	{
		LOG_INFO("Bonus Exp : Ramadan Candy: {} MallExp: {}", ecs::PointSystem::Get(toEntity, POINT_RAMADAN_CANDY_BONUS_EXP), ecs::PointSystem::Get(toEntity, POINT_MALL_EXPBONUS));
	}

	// ȹ  2005.04.21  85%
	iExp = iExp * CHARACTER_MANAGER::instance().GetMobExpRate(toEntity) / 100;

	// ġ ѹ ȹ淮
	iExp = MIN(ecs::PlayerRuntime::GetNextExp(toEntity) / 10, iExp);

	if (test_server)
	{
		if (quest::CQuestManager::instance().GetEventFlag("exp_bonus_log") && iBaseExp > 0)
			ecs::ChatSystem::Send(toEntity, CHAT_TYPE_INFO, "exp bonus %d%%", (iExp - iBaseExp) * 100 / iBaseExp);
		ecs::ChatSystem::Send(toEntity, CHAT_TYPE_INFO, "exp(%d) base_exp(%d)", iExp, iBaseExp);
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

	ecs::PointSystem::Change(toEntity, POINT_EXP, iExp, true);
	CombatSystem::CreateFly(fromEntity, FLY_EXP, toEntity);

	{
		const entt::entity you = ecs::SocialSystem::GetMarryPartner(toEntity);
		// κΰ  Ƽ̸ ݽ
		if (you != entt::null)
		{
			// 1 100%
			uint32_t dwUpdatePoint = 2000 * iExp / ecs::PointSystem::GetLevel(toEntity) / ecs::PointSystem::GetLevel(toEntity) / 3;

			if (ecs::PlayerRuntime::GetPremiumRemainSeconds(toEntity, PREMIUM_MARRIAGE_FAST) > 0 ||
				ecs::PlayerRuntime::GetPremiumRemainSeconds(you, PREMIUM_MARRIAGE_FAST) > 0)
				dwUpdatePoint = (uint32_t)(dwUpdatePoint * 3);

			marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID(toEntity));

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

		explicit FPartyTotaler(entt::entity center)
			: total(0), member_count(0),
			  x(ecs::PlayerRuntime::GetX(center)), y(ecs::PlayerRuntime::GetY(center))
		{
		};

		void operator () (entt::entity chEntity)
		{
			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(chEntity) - x, ecs::PlayerRuntime::GetY(chEntity) - y) <= PARTY_DEFAULT_RANGE)
			{
				total += __GetPartyExpNP(ecs::PointSystem::GetLevel(chEntity));

				++member_count;
			}
		}
	};

	struct FPartyDistributor
	{
		int		total;
		entt::entity	c;
		int		x, y;
		uint32_t		_iExp;
		int		m_iMode;
		int		m_iMemberCount;

		FPartyDistributor(entt::entity center, int member_count, int total, uint32_t iExp, int iMode)
			: total(total), c(center),
			  x(ecs::PlayerRuntime::GetX(center)), y(ecs::PlayerRuntime::GetY(center)),
			  _iExp(iExp), m_iMode(iMode), m_iMemberCount(member_count)
		{
			if (m_iMemberCount == 0)
				m_iMemberCount = 1;
		};

		void operator () (entt::entity chEntity)
		{
			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(chEntity) - x, ecs::PlayerRuntime::GetY(chEntity) - y) <= PARTY_DEFAULT_RANGE)
			{
				uint32_t iExp2 = 0;

				switch (m_iMode)
				{
				case PARTY_EXP_DISTRIBUTION_NON_PARITY:
					iExp2 = (uint32_t)(_iExp * (float)__GetPartyExpNP(ecs::PointSystem::GetLevel(chEntity)) / total);
					break;

				case PARTY_EXP_DISTRIBUTION_PARITY:
					iExp2 = _iExp / m_iMemberCount;
					break;

				default:
					LOG_ERROR("Unknown party exp distribution mode {}", m_iMode);
					return;
				}

				GiveExp(c, chEntity, iExp2);
			}
		}
	};
}

typedef struct SDamageInfo
{
	int iDam;
	entt::entity pAttacker;
	LPPARTY pParty;

	void Clear()
	{
		pAttacker = entt::null;
		pParty = nullptr;
	}

	inline void Distribute(entt::entity chEntity, int iExp)
	{
		if (pAttacker != entt::null)
			GiveExp(chEntity, pAttacker, iExp);
		else if (pParty)
		{
			NPartyExpDistribute::FPartyTotaler f(chEntity);
			pParty->ForEachOnlineMember(f);

			if (pParty->IsPositionNearLeader(chEntity))
				iExp = iExp * (100 + pParty->GetExpBonusPercent()) / 100;

			NPartyExpDistribute::FPartyDistributor fDist(chEntity, f.member_count, f.total, iExp, pParty->GetExpDistributionMode());
			pParty->ForEachOnlineMember(fDist);
		}
	}
} TDamageInfo;

namespace CombatSystem {

// Splitting the kill experience over everyone in the ledger, and naming
// the one that did the most damage.
entt::entity DistributeExp(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return entt::null;

	int iExpToDistribute = ecs::PlayerRuntime::GetExp(e);

	if (iExpToDistribute <= 0)
		return entt::null;

	uint64_t	iTotalDam = 0;
	entt::entity pkChrMostAttacked = entt::null;
	uint64_t iMostDam = 0;

	typedef std::vector<TDamageInfo> TDamageInfoTable;
	TDamageInfoTable damage_info_table;
	std::map<LPPARTY, TDamageInfo> map_party_damage;

	damage_info_table.reserve(CombatSystem::DamageLedgerOf(e).entries.size());

	std::map<entt::entity, ecs::BattleContribution>::iterator it = CombatSystem::DamageLedgerOf(e).entries.begin();

	// ϴ    ɷ . (50m)
	while (it != CombatSystem::DamageLedgerOf(e).entries.end())
	{
		const entt::entity eAttacker = it->first;
		uint64_t iDam = it->second.totalDamage;

		++it;


		// NPC ⵵ ϳ? -.-;
		if (!LegacyCharOf(eAttacker) || ecs::PlayerRuntime::IsNPC(eAttacker) || DISTANCE_APPROX(ecs::PlayerRuntime::GetX(e) - ecs::PlayerRuntime::GetX(eAttacker), ecs::PlayerRuntime::GetY(e) - ecs::PlayerRuntime::GetY(eAttacker)) > 5000)
			continue;

		iTotalDam += iDam;
		if (pkChrMostAttacked == entt::null || iDam > iMostDam)
		{
			pkChrMostAttacked = eAttacker;
			iMostDam = iDam;
		}

		if (ecs::SocialSystem::GetParty(eAttacker))
		{
			std::map<LPPARTY, TDamageInfo>::iterator it = map_party_damage.find(ecs::SocialSystem::GetParty(eAttacker));
			if (it == map_party_damage.end())
			{
				TDamageInfo di;
				di.iDam = iDam;
				di.pAttacker = entt::null;
				di.pParty = ecs::SocialSystem::GetParty(eAttacker);
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
			di.pAttacker = eAttacker;
			di.pParty = nullptr;

			//LOG_INFO(0, "__ pq_damage %s %d", ecs::PlayerRuntime::GetName(eAttacker).data(), iDam);
			//pq_damage.push(di);
			damage_info_table.push_back(di);
		}
	}

	for (std::map<LPPARTY, TDamageInfo>::iterator it = map_party_damage.begin(); it != map_party_damage.end(); ++it)
	{
		damage_info_table.push_back(it->second);
		//LOG_INFO(0, "__ pq_damage_party [%u] %d", it->second.pParty->GetLeaderPID(), it->second.iDam);
	}

	ecs::PlayerRuntime::SetExp(e, 0);
	//CombatSystem::ClearDamageLedger(e);

	if (iTotalDam == 0)	//  ذ 0̸
		return entt::null;

	// Half of the experience goes to the stone that spawned this mob.
	if (const entt::entity stone = CombatSystem::GetStone(e); stone != entt::null)
	{
		const int iExp = iExpToDistribute >> 1;
		ecs::PlayerRuntime::SetExp(stone, ecs::PlayerRuntime::GetExp(stone) + iExp);
		iExpToDistribute -= iExp;
	}

	LOG_TRACE("{} total exp: {}, damage_info_table.size() == {}, TotalDam {}", ecs::PlayerRuntime::GetName(e).data(), iExpToDistribute, damage_info_table.size(), iTotalDam);
	//LOG_INFO(1, "%s total exp: %d, pq_damage.size() == %d, TotalDam %d",
	//ecs::PlayerRuntime::GetName(e).data(), iExpToDistribute, pq_damage.size(), iTotalDam);

	if (damage_info_table.empty())
		return entt::null;

	//      HP ȸ Ѵ.
	CombatSystem::DistributeHP(e, pkChrMostAttacked);	//  ý

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
			LOG_ERROR("DistributeExp percent over 1.0 (fPercent {} name {})", fPercent, ecs::PlayerRuntime::GetName(di->pAttacker).data());
			fPercent = 1.0f;
		}

		iExp += (int)(iExpToDistribute * fPercent);

		//LOG_INFO(0, "%s given exp percent %.1f + 20 dam %d", ecs::PlayerRuntime::GetName(e).data(), fPercent * 100.0f, di.iDam);
#ifdef DISABLE_EXP_FROM_STONES_RAZOR93
		if (ecs::PlayerRuntime::IsStone(e)) // razor93
		{
			//NEM HIVJA MEG A di->Distribute(e, iExp);
		}
		else
		{
			di->Distribute(e, iExp);//HA NEM STNONE AKKOR IGEN
		}
#else
		const int race = ecs::PlayerRuntime::GetRaceNum(e);
		if (race == 8010 || race == 8020 || race == 8738 || race == 8739 || race == 8740 || race == 4811 || race == 4812 || race == 4813 || race == 4814 || race == 4815
			|| race == 8821 || race == 8822 || race == 8823 || race == 8824
			)
			return pkChrMostAttacked; // seggbe
		di->Distribute(e, iExp);
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
				LOG_ERROR("DistributeExp percent over 1.0 (fPercent {} name {})", fPercent, ecs::PlayerRuntime::GetName(di.pAttacker).data());
				fPercent = 1.0f;
			}

			//LOG_INFO(0, "%s given exp percent %.1f dam %d", ecs::PlayerRuntime::GetName(e).data(), fPercent * 100.0f, di.iDam);
			di.Distribute(e, (int)(iExpToDistribute * fPercent));
		}
	}

	return pkChrMostAttacked;
}

} // namespace CombatSystem

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
	const entt::entity chEntity = ch ? info->entity : entt::null;

	if (ch == nullptr)
	{
		LOG_ERROR("DEAD_EVENT: cannot find char pointer with MOB entity({})", static_cast<uint32_t>(info->entity));
		return 0;
	}

	ecs::PlayerRuntime::SetCharEvent(chEntity, ecs::PlayerRuntime::CharEvent::Dead, nullptr);
	{
		const entt::entity victimEntity = chEntity;
		if (victimEntity != entt::null)
			g_dispatcher.trigger(ecs::EvCharDead { entt::null, victimEntity });
	}

	if (!ecs::PlayerRuntime::IsPC(chEntity))
	{
		if (ch->IsMonster() == true)
		{
			if (CombatSystem::IsRevive(chEntity) == false && ecs::SocialSystem::HasReviverInParty(chEntity) == true)
			{
				ecs::PlayerRuntime::SetPosition(chEntity, POS_STANDING);
				ecs::PlayerRuntime::SetHP(chEntity, ecs::PointSystem::GetMaxHP(chEntity));

				ecs::ViewSystem::ViewReencode(chEntity);

				CombatSystem::SetAggressive(chEntity);
				CombatSystem::SetRevive(chEntity, true);

				return 0;
			}
		}

		M2_DESTROY_CHARACTER(ch);
	}

	return 0;
}


namespace CombatSystem {

void Dead(entt::entity victim, entt::entity killer, bool immediate)
{
	if (victim == entt::null || !g_registry.valid(victim))
		return;
	// FakePlayers are normally excluded from death handling, but LostCastle clones must die.
	//if (IsFakePlayer() && !CLostCastleDungeon::instance().IsCloneVID(GetVID()))
	//	return;

	if (IsDead(victim))
		return;

	if (CombatSystem::GetInvincible(victim))
		return;

	// LostCastle klonoknak nincs mob_proto (m_pkMobData == nullptr),
	// ezert a normal !ecs::PlayerRuntime::IsPC(victim) reward/resurrection ag GetMobTable()-t hivna es crashelne.
	// Itt egy safe halal pipeline + return.
	//if (IsFakePlayer() && CLostCastleDungeon::instance().IsCloneVID(GetVID()))
	//{
	//	if (!hasKiller && m_dwKillerPID)
	//		hasKiller = CHARACTER_MANAGER::instance().FindByPID(m_dwKillerPID);

	//	m_dwKillerPID = 0;

	//	if (auto* flags = RuntimeFlags(victim))
	//		SET_BIT(flags->instantFlag, INSTANT_FLAG_NO_REWARD);

	//	ecs::PlayerRuntime::SetPosition(victim, POS_DEAD);
	//	AffectSystem::ClearAffect(victim, true);
	//	ClearSync();
	//	event_cancel(&m_pkStunEvent);

	//	if (hasKiller && ecs::PlayerRuntime::IsPC((hasKiller ? hasKiller->GetEntityHandle() : entt::null)))
	//		CLostCastleDungeon::instance().OnMobKilled((hasKiller ? hasKiller->GetEntityHandle() : entt::null), victim);

	//	TPacketGCDead pack;
	//	pack.header = HEADER_GC_DEAD;
	//	pack.vid = ecs::PlayerRuntime::GetPacketVID(victim);
	//	ecs::ViewSystem::PacketView(victim, &pack, sizeof(pack));

	//	if (auto* flags = RuntimeFlags(victim))
	//		REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_STUN);

	//	if (ecs::SocialSystem::GetDungeon(victim))
	//		ecs::SocialSystem::GetDungeon(victim)->DeadCharacter(this);

	//	if (m_pkDeadEvent)
	//		event_cancel(&m_pkDeadEvent);

	//	SCharDeadEventInfo* pEventInfo = AllocEventInfo<SCharDeadEventInfo>();
	//	pEventInfo->vid = GetVID();
	//	m_pkDeadEvent = event_create(dead_event, pEventInfo, immediate ? 1 : PASSES_PER_SEC(1));
	//	return;
	//}

	if (ecs::PlayerRuntime::IsPC(victim))
	{
		if (MountSystem::IsHorseRiding(victim)) {
			MountSystem::StopRiding(victim);
		}
		else if (MountSystem::GetMountVnum(victim)) {
			AffectSystem::RemoveAffect(victim, AFFECT_MOUNT_BONUS);
			MountSystem::SetMountVnum(victim, 0);
			ItemSystem::UnEquipSpecialRideUniqueItem(victim);
			NetworkSyncSystem::UpdatePacket(victim);
		}
	}

	if (ecs::PlayerRuntime::IsMonster(victim) || ecs::PlayerRuntime::IsStone(victim))
	{
		LPDUNGEON dungeon = ecs::SocialSystem::GetDungeon(victim);
		if (dungeon)
		{
			dungeon->DecMonster();
		}
	}

#ifdef ENABLE_EVENT_MANAGER
	// Map1 mass-spawn wave tracking (Tanaka / Golden Frog)
	if (ecs::PlayerRuntime::IsMonster(victim) && ecs::PlayerRuntime::GetMapIndex(victim) == 1)
	{
		const uint32_t vnum = ecs::PlayerRuntime::GetRaceNum(victim);
		if (vnum == 5000u || vnum == 124u)
			Map1MassSpawnEvent_OnMobDead(ecs::PlayerRuntime::GetPacketVID(victim));
	}
#endif


	// A killer that left only a player id behind is looked up again, and the
	// entity handle has to come back with it. Everything below asks about the
	// killer through the handle, so a killer recovered as a pointer alone read
	// as "no killer at all": no PvP bookkeeping, no guild war kill, no log.
	if ((killer == entt::null || !g_registry.valid(killer)) && GetKillerPID(victim))
	{
		if (LPCHARACTER recovered = CHARACTER_MANAGER::instance().FindByPID(GetKillerPID(victim)))
			killer = recovered->GetEntityHandle();
	}

	const bool hasKiller = killer != entt::null && g_registry.valid(killer);

	CombatSystem::SetKillerPID(victim, 0); // ݵ ʱȭ ؾ DO NOT DELETE THIS LINE UNLESS YOU ARE 1000000% SURE

	bool isAgreedPVP = false;
	bool isUnderGuildWar = false;
	bool isDuel = false;

	if (hasKiller && ecs::PlayerRuntime::IsPC(killer))
	{
		if (const auto* killerTarget = g_registry.try_get<ecs::SelectedTarget>(killer);
			killerTarget && killerTarget->target == victim)
			CombatSystem::SetTarget(killer, entt::null);

		isAgreedPVP = CPVPManager::instance().Dead(victim, ecs::PlayerRuntime::GetPlayerID(killer));
		isDuel = CArenaManager::instance().OnDead(killer, victim);
#ifdef ENABLE_PVP_ADVANCED
		if (isAgreedPVP || isDuel)
		{
			const char* szTableStaticPvP[] = { BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT };

			int betMoneyDead = ecs::PlayerRuntime::GetQuestFlag(victim, szTableStaticPvP[8]);
			int betMoneyKiller = ecs::QuestSystem::GetFlag(killer, szTableStaticPvP[8]);

			if (betMoneyDead > 0 && betMoneyKiller > 0)
			{
				ecs::PointSystem::Change(killer, POINT_GOLD, betMoneyDead * 2, true);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(killer, CHAT_TYPE_INFO, 515, "%d", betMoneyDead);
#endif
			}

			for (unsigned int i = 0; i < _countof(szTableStaticPvP); i++) {
				char pkCh_Buf[CHAT_MAX_LEN + 1], pkKiller_Buf[CHAT_MAX_LEN + 1];

				snprintf(pkCh_Buf, sizeof(pkCh_Buf), "BINARY_Duel_Delete");
				snprintf(pkKiller_Buf, sizeof(pkKiller_Buf), "BINARY_Duel_Delete");

				ecs::ChatSystem::Send(victim, CHAT_TYPE_COMMAND, pkCh_Buf);
				ecs::PlayerRuntime::SetQuestFlag(victim, szTableStaticPvP[i], 0);

				ecs::ChatSystem::Send(killer, CHAT_TYPE_COMMAND, pkKiller_Buf);
				ecs::QuestSystem::SetFlag(killer, szTableStaticPvP[i], 0);
			}
		}
#endif

		if (ecs::PlayerRuntime::IsPC(victim))
		{
			CGuild* g1 = ecs::SocialSystem::GetGuild(victim);
			CGuild* g2 = ecs::SocialSystem::GetGuild(killer);

			if (g1 && g2)
				if (g1->UnderWar(g2->GetID()))
					isUnderGuildWar = true;

			ecs::PlayerRuntime::SetQuestNPCID(killer, ecs::PlayerRuntime::GetPacketVID(victim));
			quest::CQuestManager::instance().Kill(ecs::PlayerRuntime::GetPlayerID(killer), quest::QUEST_NO_NPC);
			// Guild kill bookkeeping is still pointer-shaped; CGuildManager is its
			// own migration. One resolve, named.
			if (LPCHARACTER legacyKiller = ecs::LegacyCharOf(killer))
			{
				if (LPCHARACTER legacyVictim = ecs::LegacyCharOf(victim))
					CGuildManager::instance().Kill(legacyKiller, legacyVictim);
			}
		}
	}

#ifdef ENABLE_QUEST_DIE_EVENT
	//if (ecs::PlayerRuntime::IsPC(victim))
	//{
	//	if (hasKiller)
	//		ecs::PlayerRuntime::SetQuestNPCID(victim, hasKiller->GetVID());
	//	// quest::CQuestManager::instance().Die(ecs::PlayerRuntime::GetPlayerID(victim), quest::QUEST_NO_NPC);
	//	quest::CQuestManager::instance().Die(ecs::PlayerRuntime::GetPlayerID(victim), (hasKiller)?ecs::PlayerRuntime::GetRaceNum((hasKiller ? hasKiller->GetEntityHandle() : entt::null)):quest::QUEST_NO_NPC);
	//}
	if (ecs::PlayerRuntime::IsPC(victim))
	{
		if (hasKiller) {
			ecs::PlayerRuntime::SetQuestNPCID(victim, ecs::PlayerRuntime::GetPacketVID(killer));
		}

		quest::CQuestManager::instance().Die(ecs::PlayerRuntime::GetPlayerID(victim), (hasKiller) ? ecs::PlayerRuntime::GetRaceNum(killer) : quest::QUEST_NO_NPC);
	}
#endif

#ifdef ENABLE_RANKING
	if ((ecs::PlayerRuntime::IsPC(victim))) {
		if (((isAgreedPVP) || (isDuel)) && (hasKiller)) {
			ecs::PlayerRuntime::SetRankPoints(victim, 1, ecs::PlayerRuntime::GetRankPoints(killer, 1) + 1);
			ecs::PlayerRuntime::SetRankPoints(killer, 0, ecs::PlayerRuntime::GetRankPoints(killer, 0) + 1);
		}
		else if (isUnderGuildWar) {
			ecs::PlayerRuntime::SetRankPoints(killer, 2, ecs::PlayerRuntime::GetRankPoints(killer, 2) + 1);
		}
	}

	if (hasKiller) {
		if (ecs::PlayerRuntime::IsPC(killer)) {
			if (ecs::PlayerRuntime::IsStone(victim)) {
				if (hasKiller)
					ecs::PlayerRuntime::SetRankPoints(killer, 5, ecs::PlayerRuntime::GetRankPoints(killer, 5) + 1);
			}
			else if (ecs::PlayerRuntime::IsMonster(victim)) {
				if (ecs::PlayerRuntime::GetMobRank(victim) >= MOB_RANK_BOSS)
					ecs::PlayerRuntime::SetRankPoints(killer, 7, ecs::PlayerRuntime::GetRankPoints(killer, 7) + 1);
				else
					ecs::PlayerRuntime::SetRankPoints(killer, 6, ecs::PlayerRuntime::GetRankPoints(killer, 6) + 1);
			}
		}
	}
#endif

	/*
		if (hasKiller &&
				!isAgreedPVP &&
				!isUnderGuildWar &&
				ecs::PlayerRuntime::IsPC(victim) &&
				!isDuel)
		{
			if (GetGMLevel() == GM_PLAYER || test_server)
			{
				ItemDropPenalty(victim, killer);
			}
		}
	*/

#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
	if (ecs::PlayerRuntime::IsPC(victim)) {
#ifdef ENABLE_01092021
		if (hasKiller && !ecs::PlayerRuntime::IsPC(killer)) {
			CombatSystem::SetTarget(killer, entt::null);
		}
#endif
		AffectSystem::ClearAffectSkills(victim);
	}
#endif
	ecs::PlayerRuntime::SetPosition(victim, POS_DEAD);
	AffectSystem::ClearAffect(victim, true);

	if (hasKiller && ecs::PlayerRuntime::IsPC(victim))
	{
		if (!ecs::PlayerRuntime::IsPC(killer))
		{
#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
			CombatSystem::SetDeadByMonster(victim, true);
#endif

			LOG_TRACE("DEAD: {} {} WITH PENALTY", ecs::PlayerRuntime::GetName(victim).data(), static_cast<uint32_t>(victim));
						if (auto* flags = RuntimeFlags(victim))
				SET_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);
			LogManager::instance().CharLog(victim, ecs::PlayerRuntime::GetRaceNum(killer), "DEAD_BY_NPC", ecs::PlayerRuntime::GetName(killer).data());
		}
		else
		{
#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
			CombatSystem::SetDeadByMonster(victim, false);
#endif
			LOG_TRACE("DEAD_BY_PC: {} {} KILLER {} {}", ecs::PlayerRuntime::GetName(victim).data(), static_cast<uint32_t>(victim), ecs::PlayerRuntime::GetName(killer).data(), static_cast<uint32_t>(killer));
						if (auto* flags = RuntimeFlags(victim))
				REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);

			if (ecs::PlayerRuntime::GetEmpire(victim) != ecs::PlayerRuntime::GetEmpire(killer))
			{
				int64_t iEP = std::min(ecs::PointSystem::Get(victim, POINT_EMPIRE_POINT), ecs::PointSystem::Get(killer, POINT_EMPIRE_POINT));

				ecs::PointSystem::Change(victim, POINT_EMPIRE_POINT, -(iEP / 10));
				ecs::PointSystem::Change(killer, POINT_EMPIRE_POINT, iEP / 5);


				char buf[256];
				snprintf(buf, sizeof(buf),
					"%d %u %d %s %d %u %d %s",
					ecs::PlayerRuntime::GetEmpire(victim), GetAlignment(victim), GetPKMode(victim), ecs::PlayerRuntime::GetName(victim).data(),
					ecs::PlayerRuntime::GetEmpire(killer), GetAlignment(killer), GetPKMode(killer), ecs::PlayerRuntime::GetName(killer).data());

				LogManager::instance().CharLog(victim, ecs::PlayerRuntime::GetPlayerID(killer), "DEAD_BY_PC", buf);
			}
			else
			{
//				if (!isAgreedPVP && !isUnderGuildWar && !IsKillerMode() /*&& GetAlignment(victim) >= 0*/ && !isDuel)
//				{
//					int iNoPenaltyProb = 0;
//
//					if (GetAlignment(killer) >= 0)	// 1/3 percent down
//						iNoPenaltyProb = 33;
//					else				// 4/5 percent down
//						iNoPenaltyProb = 20;
//
//					if (number(1, 100) < iNoPenaltyProb) {
//#ifdef TEXTS_IMPROVEMENT
//						ecs::ChatSystem::SendNew((hasKiller ? hasKiller->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 413, "");
//#endif
//					}
//					else {
//						if (ecs::SocialSystem::GetParty((hasKiller ? hasKiller->GetEntityHandle() : entt::null)))
//						{
//							FPartyAlignmentCompute f(-20000, ecs::PlayerRuntime::GetX((hasKiller ? hasKiller->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY((hasKiller ? hasKiller->GetEntityHandle() : entt::null)));
//							ecs::SocialSystem::GetParty((hasKiller ? hasKiller->GetEntityHandle() : entt::null))->ForEachOnlineMember(f);
//
//							if (f.m_iCount == 0)
//								CombatSystem::UpdateAlignment(hasKiller->GetEntityHandle(), -20000);
//							else
//							{
//								0, "ALIGNMENT PARTY count %d amount %d", f.m_iCount, f.m_iAmount);
//
//								f.m_iStep = 1;
//								ecs::SocialSystem::GetParty((hasKiller ? hasKiller->GetEntityHandle() : entt::null))->ForEachOnlineMember(f);
//							}
//						}
//						else
//							CombatSystem::UpdateAlignment(hasKiller->GetEntityHandle(), -20000);
//					}
//				}

				char buf[256];
				snprintf(buf, sizeof(buf),
					"%d %u %d %s %d %u %d %s",
					ecs::PlayerRuntime::GetEmpire(victim), GetAlignment(victim), GetPKMode(victim), ecs::PlayerRuntime::GetName(victim).data(),
					ecs::PlayerRuntime::GetEmpire(killer), GetAlignment(killer), GetPKMode(killer), ecs::PlayerRuntime::GetName(killer).data());

				LogManager::instance().CharLog(victim, ecs::PlayerRuntime::GetPlayerID(killer), "DEAD_BY_PC", buf);
			}

#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(killer);
			if (bBattlePassId)
			{
				uint32_t dwToKillCount, dwMinLevel;
				uint32_t dwLevel = ecs::PointSystem::GetLevel(victim);
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, PLAYER_KILL, &dwMinLevel, &dwToKillCount))
				{
#ifdef ENABLE_BATTLE_PASS_SECURITY_KILL
					if ((ecs::PlayerRuntime::GetDesc(victim)->GetHostName() != ecs::PlayerRuntime::GetDesc(killer)->GetHostName()) && CBattlePass::instance().IsEligibleForPlayerKill(ecs::PlayerRuntime::GetPlayerID(killer), ecs::PlayerRuntime::GetPlayerID(victim)))
					{
						if (dwLevel >= dwMinLevel && ecs::PlayerRuntime::GetMissionProgress(killer, PLAYER_KILL, bBattlePassId) < dwToKillCount)
						{
							ecs::PlayerRuntime::UpdateMissionProgress(killer, PLAYER_KILL, bBattlePassId, 1, dwToKillCount);
							CBattlePass::instance().RegisterPlayerKill(ecs::PlayerRuntime::GetPlayerID(killer), ecs::PlayerRuntime::GetPlayerID(victim));
						}
					}
#else
					if (dwLevel >= dwMinLevel && ecs::PlayerRuntime::GetMissionProgress(killer, PLAYER_KILL, bBattlePassId) < dwToKillCount)
						ecs::PlayerRuntime::UpdateMissionProgress(killer, PLAYER_KILL, bBattlePassId, 1, dwToKillCount);
#endif
				}
			}
			if (hasKiller && ecs::PlayerRuntime::IsPC(killer) && ecs::PlayerRuntime::IsPC(victim))
			{
				const char* szMapName;
				switch (ecs::PlayerRuntime::GetMapIndex(victim))
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
					int iRankPoints = ecs::PlayerRuntime::GetRankPoints(killer, 0); // PvP rangpont
					snprintf(szMsg, sizeof(szMsg),
						"|cff00ff00%s|r has killed |cffff0000%s|r Map: %s, PVP-Mode: DUEL (Winned duels: %d)",
						ecs::PlayerRuntime::GetName(killer).data(), ecs::PlayerRuntime::GetName(victim).data(), szMapName, iRankPoints);
				}
				else
				{
					snprintf(szMsg, sizeof(szMsg),
						"|cff00ff00%s|r has killed |cffff0000%s|r Map: %s, PVP-Mode: FREE!",
						ecs::PlayerRuntime::GetName(killer).data(), ecs::PlayerRuntime::GetName(victim).data(), szMapName);
				}

				BroadcastNotice(szMsg);
			}


#endif
		}
	}
	else
	{
		LOG_TRACE("DEAD: {} {}", ecs::PlayerRuntime::GetName(victim).data(), static_cast<uint32_t>(victim));
				if (auto* flags = RuntimeFlags(victim))
			REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);
	}

	NetworkSyncSystem::ClearSync(victim);

	//LOG_INFO(1, "stun cancel %s[%d]", ecs::PlayerRuntime::GetName(victim).data(), (uint32_t)GetVID());
	ecs::PlayerRuntime::CancelCharEvent(victim, ecs::PlayerRuntime::CharEvent::Stun); //  ̺Ʈ δ.

	if (ecs::PlayerRuntime::IsPC(victim))
	{
		g_registry.get_or_emplace<ecs::LastDeadTime>(victim).value = get_dword_time();
		//SetKillerMode(victim, hasKiller && ecs::PlayerRuntime::IsPC((hasKiller ? hasKiller->GetEntityHandle() : entt::null)));
		SetKillerMode(victim, false);
		ecs::PlayerRuntime::GetDesc(victim)->SetPhase(PHASE_DEAD);
	}
	else
	{
		// 忡 ݹ ʹ   Ѵ.
		if (!(RuntimeFlags(victim) && IS_SET(RuntimeFlags(victim)->instantFlag, INSTANT_FLAG_NO_REWARD)))
		{
			if (!(hasKiller && ecs::PlayerRuntime::IsPC(killer) && ecs::SocialSystem::GetGuild(killer) && ecs::SocialSystem::GetGuild(killer)->UnderAnyWar(GUILD_WAR_TYPE_FIELD)))
			{
				// Ȱϴ ʹ   ʴ´.
				const TMobTable* mobTable = ecs::PlayerRuntime::GetMobTable(victim);
				if (mobTable && mobTable->dwResurrectionVnum)
				{
					// DUNGEON_MONSTER_REBIRTH_BUG_FIX
					auto* chResurrect = CHARACTER_MANAGER::instance().SpawnMob(mobTable->dwResurrectionVnum, ecs::PlayerRuntime::GetMapIndex(victim), ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim), ecs::PlayerRuntime::GetZ(victim), true, (int)ecs::PlayerRuntime::GetRotation(victim));
					if (ecs::SocialSystem::GetDungeon(victim) && chResurrect)
					{
						ecs::SocialSystem::SetDungeon(chResurrect->GetEntityHandle(), ecs::SocialSystem::GetDungeon(victim));
					}
					// END_OF_DUNGEON_MONSTER_REBIRTH_BUG_FIX

					Reward(victim, false);
				}
				else if (CombatSystem::IsRevive(victim) == true)
				{
					Reward(victim, false);
				}
				else
				{
					Reward(victim, true); // Drops gold, item, etc..
				}

				// Rewarding runs quest triggers and item drops, either of which can
				// destroy this mob outright.
				if (!g_registry.valid(victim))
					return;
			}
			else
			{
				if (auto& notice = g_registry.get_or_emplace<ecs::GuildWarNoticeTime>(killer);
					notice.value < get_dword_time())
				{
					notice.value = get_dword_time() + 60000;
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(killer, CHAT_TYPE_INFO, 147, "");
#endif
				}
			}
		}
	}

	// BOSS_KILL_LOG
	if (ecs::PlayerRuntime::GetMobRank(victim) >= MOB_RANK_BOSS && hasKiller && ecs::PlayerRuntime::IsPC(killer))
	{
		char buf[51];
		snprintf(buf, sizeof(buf), "%d %ld", g_bChannel, ecs::PlayerRuntime::GetMapIndex(killer));
		if (ecs::PlayerRuntime::IsStone(victim))
			LogManager::instance().CharLog(killer, ecs::PlayerRuntime::GetRaceNum(victim), "STONE_KILL", buf);
		else
			LogManager::instance().CharLog(killer, ecs::PlayerRuntime::GetRaceNum(victim), "BOSS_KILL", buf);
	}
	// END_OF_BOSS_KILL_LOG

	TPacketGCDead pack;
	pack.header = HEADER_GC_DEAD;
	pack.vid = ecs::PlayerRuntime::GetPacketVID(victim);
	ecs::ViewSystem::PacketView(victim, &pack, sizeof(pack));

		if (auto* flags = RuntimeFlags(victim))
		REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_STUN);

	// ÷̾ ĳ̸
	if (ecs::PlayerRuntime::GetDesc(victim) != nullptr) {
		//
		// Ŭ̾Ʈ Ʈ Ŷ ٽ .
		//
		for (const auto& affect : AffectSystem::Snapshot(victim))
			if (affect)
				SendAffectAddPacket(ecs::PlayerRuntime::GetDesc(victim), affect.get());
	}

	//
	// Dead ̺Ʈ ,
	//
	// Dead ̺Ʈ    Ŀ Destroy ǵ ָ,
	// PC  3 ִٰ    ش. 3  κ
	//   , ⼭    ޴´.
	if (isDuel == false)
	{
		if (ecs::PlayerRuntime::GetCharEvent(victim, ecs::PlayerRuntime::CharEvent::Dead))
		{
			LOG_TRACE("DEAD_EVENT_CANCEL: {} {} {}", ecs::PlayerRuntime::GetName(victim).data(), static_cast<uint32_t>(victim), static_cast<const void*>(get_pointer(
				ecs::PlayerRuntime::GetCharEvent(victim, ecs::PlayerRuntime::CharEvent::Dead))));
			ecs::PlayerRuntime::CancelCharEvent(victim, ecs::PlayerRuntime::CharEvent::Dead);
		}

		if (ecs::PlayerRuntime::IsStone(victim))
		{
			// Every mob the stone spawned dies here, and each of those deaths
			// comes back through this function.
			ClearStone(victim);
			if (!g_registry.valid(victim))
				return;
		}

		// The dungeon may destroy the character it is told about, so nothing
		// below may touch the victim without asking again.
		if (LPDUNGEON dungeon = ecs::SocialSystem::GetDungeon(victim))
		{
			dungeon->DeadCharacter(victim);

			if (!g_registry.valid(victim))
				return;
		}

		if (!ecs::PlayerRuntime::IsPC(victim))
		{
			SCharDeadEventInfo* pEventInfo = AllocEventInfo<SCharDeadEventInfo>();
			pEventInfo->entity = victim;

			if (CombatSystem::IsRevive(victim) == false && ecs::SocialSystem::HasReviverInParty(victim) == true)
			{
				ecs::PlayerRuntime::SetCharEvent(victim, ecs::PlayerRuntime::CharEvent::Dead,
					event_create(dead_event, pEventInfo, immediate ? 1 : PASSES_PER_SEC(1)));
			}
#ifdef __DEFENSE_WAVE__
			else if (ecs::PlayerRuntime::GetRaceNum(victim) >= 3950 && ecs::PlayerRuntime::GetRaceNum(victim) <= 3964)
			{
				ecs::PlayerRuntime::SetCharEvent(victim, ecs::PlayerRuntime::CharEvent::Dead,
					event_create(dead_event, pEventInfo, immediate ? 1 : PASSES_PER_SEC(1)));
			}
#endif
			else
			{
				ecs::PlayerRuntime::SetCharEvent(victim, ecs::PlayerRuntime::CharEvent::Dead,
					event_create(dead_event, pEventInfo, immediate ? 1 : PASSES_PER_SEC(1)));
			}

			LOG_TRACE("DEAD_EVENT_CREATE: {} {} {}", ecs::PlayerRuntime::GetName(victim).data(), static_cast<uint32_t>(victim), static_cast<const void*>(get_pointer(
				ecs::PlayerRuntime::GetCharEvent(victim, ecs::PlayerRuntime::CharEvent::Dead))));
		}
	}

	ExchangeSystem::Cancel(victim);

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (AttrTransfer_is_open(victim) == true)
	{
		AttrTransfer_close(victim);
	}
#endif

	if (ecs::SessionSystem::IsCubeOpen(victim))
		ecs::SessionSystem::SetCubeNPC(victim, entt::null);

#ifdef ENABLE_ACCE_SYSTEM
	if (ecs::PlayerRuntime::IsPC(victim))
		if (LPCHARACTER windows = ecs::LegacyCharOf(victim))
			ecs::AcceSystem::Close(windows->GetEntityHandle());
#endif

	// The personal shop window and the safebox each close through CHARACTER
	// still; each is its own migration, and they share one resolve here.
	if (ecs::PlayerRuntime::IsPC(victim))
	{
		CShopManager::instance().StopShopping(victim);
		if (LPCHARACTER windows = ecs::LegacyCharOf(victim))
		{
			ecs::SocialSystem::CloseMyShop(windows->GetEntityHandle());
			ecs::SessionSystem::CloseSafebox(windows->GetEntityHandle());
		}
	}
}

} // namespace CombatSystem

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
        if (tick < attackCooldown.lastCombatPulse || (tick - attackCooldown.lastCombatPulse) < attackPeriod) {
            return;
        }

        auto& victimHealth = reg.get<ecs::Health>(combatTarget.target);
        const int32_t damage = 1;
        victimHealth.current = std::max<int32_t>(0, victimHealth.current - damage);
        attackCooldown.lastCombatPulse = tick;

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

namespace CombatSystem {

// One swing. Attacker and victim are handles the whole way through; the old
// version resolved the victim at the top and then dereferenced it twice
// without ever checking it, which is what this step exists to stop.
bool Attack(entt::entity attacker, entt::entity victim, uint8_t attackType)
{
    if (attacker == entt::null || !g_registry.valid(attacker))
        return false;
    if (victim == entt::null || !g_registry.valid(victim))
        return false;

#ifdef ENABLE_BUG_FIXES
    if (ecs::SocialSystem::GetMyShop(victim))
        return false;
#endif

    if (test_server)
        LOG_TRACE("[TEST_SERVER] Attack : {} type {}, MobBattleType {}",
            ecs::PlayerRuntime::GetName(attacker), attackType,
            (!ecs::PlayerRuntime::IsPC(attacker) && GetMobBattleType(attacker))
                ? GetMobAttackRange(attacker) : 0);

    if (!ecs::MovementSystem::CanMove(attacker))
        return false;

#ifdef ENABLE_ANTICHEAT
    if (LPSECTREE own = ecs::PlayerRuntime::GetSectree(attacker),
        theirs = ecs::PlayerRuntime::GetSectree(victim); own && theirs) {
        if (own->IsAttr(ecs::PlayerRuntime::GetX(attacker), ecs::PlayerRuntime::GetY(attacker), ATTR_BANPK) ||
            theirs->IsAttr(ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim), ATTR_BANPK)) {
            if (LPDESC desc = ecs::PlayerRuntime::GetDesc(attacker)) {
                LogManager::instance().HackLog("ANTISAFEZONE", attacker);
                desc->DelayedDisconnect(3);
            }
        }
    }
#endif

    if (!battle_is_attackable(attacker, victim))
        return false;

    const uint32_t now = get_dword_time();

    if (ecs::PlayerRuntime::IsPC(attacker)) {
#ifdef ENABLE_ANTICHEAT
        if (IS_SPEED_HACK(attacker, victim, now))
            return false;
#endif
        if (attackType == 0 && now < GetSkipComboAttackByTime(attacker))
            return false;
    }

    NetworkSyncSystem::SetSyncOwner(victim, attacker);

    if (CanBeginFight(victim))
        BeginFight(victim, attacker);

    int result = BATTLE_NONE;

    if (attackType == 0) {
        switch (GetMobBattleType(attacker)) {
        case BATTLE_TYPE_MELEE:
        case BATTLE_TYPE_POWER:
        case BATTLE_TYPE_TANKER:
        case BATTLE_TYPE_SUPER_POWER:
        case BATTLE_TYPE_SUPER_TANKER:
            result = battle_melee_attack(attacker, victim);
            break;

        case BATTLE_TYPE_RANGE:
        case BATTLE_TYPE_MAGIC: {
            // FlyTarget and Shoot are the projectile pipeline and still take a
            // character; the shot type is the only thing that differs here.
            const uint8_t shotType = GetMobBattleType(attacker) == BATTLE_TYPE_RANGE ? 0 : 1;
            LPCHARACTER shooter = LegacyCharOf(attacker);
            if (!shooter) {
                result = BATTLE_NONE;
                break;
            }
            FlyTarget(attacker, ecs::PlayerRuntime::GetPacketVID(victim),
                ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim),
                HEADER_CG_FLY_TARGETING);
            result = Shoot(attacker, shotType) ? BATTLE_DAMAGE : BATTLE_NONE;
            break;
        }

        default:
            LOG_ERROR("Unhandled battle type {}", GetMobBattleType(attacker));
            result = BATTLE_NONE;
            break;
        }
    } else {
        if (ecs::PlayerRuntime::IsPC(attacker)) {
            const uint32_t sinceSkill = now - SkillSystem::GetLastSkillTime(attacker);
            if (sinceSkill > 1500) {
                LOG_INFO("HACK: Too long skill using term. Name({}) PID({}) delta({})",
                    ecs::PlayerRuntime::GetName(attacker),
                    ecs::PlayerRuntime::GetPlayerID(attacker), sinceSkill);
                return false;
            }
        }

        LOG_TRACE("Attack call ComputeSkill {} {}", attackType, ecs::PlayerRuntime::GetName(victim));
        // ComputeSkill is the skill pipeline and is its own migration.
        LPCHARACTER caster = LegacyCharOf(attacker);
        result = caster ? caster->ComputeSkill(attackType, victim) : BATTLE_NONE;
    }

    if (result != BATTLE_DAMAGE && result != BATTLE_DEAD)
        return false;

    // The swing can have retired either side by now - a killing blow destroys
    // the victim, and a callback on the way can take the attacker with it.
    if (g_registry.valid(attacker))
        ecs::MovementSystem::OnMove(attacker, true);
    if (g_registry.valid(victim))
        ecs::MovementSystem::OnMove(victim);

    // Only a PC clears its own victim; for a mob the state machine does it.
    if (result == BATTLE_DEAD && g_registry.valid(attacker) && ecs::PlayerRuntime::IsPC(attacker))
        SetVictim(attacker, entt::null);

    return true;
}

} // namespace CombatSystem

// char_battle.cpp slice BD1 moved into CombatSystem.cpp

namespace CombatSystem {

// The soul-point share a party member gets for a kill nearby.
void DistributeSP(entt::entity e, entt::entity killer, int iMethod)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	// GetLastMoveTime has no entity form yet; it is its own
	// migration.
	LPCHARACTER self = ecs::LegacyCharOf(e);
	if (!self)
		return;

	LPCHARACTER pkKiller = ecs::LegacyCharOf(killer);
	if (ecs::PlayerRuntime::GetSP(killer) >= ecs::PointSystem::GetMaxSP(killer))
		return;

	bool bAttacking = (get_dword_time() - CombatSystem::GetLastAttackTime(e)) < 3000;
	bool bMoving = (get_dword_time() - ecs::MovementSystem::GetLastMoveTime(e)) < 3000;

	if (iMethod == 1)
	{
		int num = number(0, 3);

		if (!num)
		{
			int iLvDelta = ecs::PointSystem::GetLevel(e) - ecs::PointSystem::GetLevel(killer);
			int iAmount = 0;

			if (iLvDelta >= 5)
				iAmount = 10;
			else if (iLvDelta >= 0)
				iAmount = 6;
			else if (iLvDelta >= -3)
				iAmount = 2;

			if (iAmount != 0)
			{
				iAmount += (iAmount * ecs::PointSystem::Get(killer, POINT_SP_REGEN)) / 100;

				if (iAmount >= 11)
					CombatSystem::CreateFly(e, FLY_SP_BIG, killer);
				else if (iAmount >= 7)
					CombatSystem::CreateFly(e, FLY_SP_MEDIUM, killer);
				else
					CombatSystem::CreateFly(e, FLY_SP_SMALL, killer);

				ecs::PointSystem::Change(killer, POINT_SP, iAmount);
			}
		}
	}
	else
	{
		if (ecs::PlayerRuntime::GetJob(killer) == JOB_SHAMAN || (ecs::PlayerRuntime::GetJob(killer) == JOB_SURA && pkKiller->GetSkillGroup() == 2))
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + ecs::PointSystem::GetMaxSP(e) / 100;
			else if (bMoving)
				iAmount = 3 + ecs::PointSystem::GetMaxSP(e) * 2 / 100;
			else
				iAmount = 10 + ecs::PointSystem::GetMaxSP(e) * 3 / 100; //

			iAmount += (iAmount * ecs::PointSystem::Get(killer, POINT_SP_REGEN)) / 100;
			ecs::PointSystem::Change(killer, POINT_SP, iAmount);
		}
		else
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + ecs::PointSystem::GetMaxSP(killer) / 200;
			else if (bMoving)
				iAmount = 2 + ecs::PointSystem::GetMaxSP(killer) / 100;
			else
			{
				//
				if (ecs::PlayerRuntime::GetHP(killer) < ecs::PointSystem::GetMaxHP(killer))
					iAmount = 2 + (ecs::PointSystem::GetMaxSP(killer) / 100); //   á
				else
					iAmount = 9 + (ecs::PointSystem::GetMaxSP(killer) / 100); // ⺻
			}

			iAmount += (iAmount * ecs::PointSystem::Get(killer, POINT_SP_REGEN)) / 100;
			ecs::PointSystem::Change(killer, POINT_SP, iAmount);
		}
	}
}

} // namespace CombatSystem


// char_battle.cpp slice BD2a helper surface duplicated into CombatSystem.cpp

static uint32_t __GetPartyExpNP(const uint32_t level)
{
	if (!level || level > PLAYER_EXP_TABLE_MAX)
		return 14000;
	return party_exp_distribute_table[level];
}


static uint32_t AdjustExpByLevel_Combat(const LegacyCharHandle ch, const uint32_t exp)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (PLAYER_MAX_LEVEL_CONST < ecs::PointSystem::GetLevel(chEntity))
	{
		double ret = 0.95;
		double factor = 0.1;

		for (int64_t i = 0; i < ecs::PointSystem::GetLevel(chEntity) - 100; ++i)
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


namespace CombatSystem {

// The experience a death costs.
void DeathPenalty(entt::entity e, uint8_t bTown)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	// CloseAcce has no entity form yet; it is its own
	// migration.
	LPCHARACTER self = ecs::LegacyCharOf(e);
	if (!self)
		return;

	LOG_INFO("DEATH_PERNALY_CHECK({}) town({})", ecs::PlayerRuntime::GetName(e).data(), bTown);

	Cube_close(self);
#ifdef __ATTR_TRANSFER_SYSTEM__
	AttrTransfer_close(e);
#endif
#ifdef ENABLE_ACCE_SYSTEM
	ecs::AcceSystem::Close(e);
#endif

	if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
	{
		return;
	}

	if (ecs::PointSystem::GetLevel(e) < 10) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 412, "");
#endif
		return;
	}

	if (number(0, 2) == 1) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 412, "");
#endif
		return;
	}

	if (RuntimeFlags(e) && IS_SET(RuntimeFlags(e)->instantFlag, INSTANT_FLAG_DEATH_PENALTY))
	{
				if (auto* flags = RuntimeFlags(e))
			REMOVE_BIT(flags->instantFlag, INSTANT_FLAG_DEATH_PENALTY);

		// NO_DEATH_PENALTY_BUG_FIX
		if (!bTown) //   ڸ Ȱø  ȣ Ѵ. ( ͽô ġ гƼ )
		{
			if (AffectSystem::FindAffect(e, AFFECT_NO_DEATH_PENALTY))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 384, "");
#endif
				AffectSystem::RemoveAffect(e, AFFECT_NO_DEATH_PENALTY);
				return;
			}
		}
		// END_OF_NO_DEATH_PENALTY_BUG_FIX

		int iLoss = ((ecs::PlayerRuntime::GetNextExp(e) * __GetExpLossPerc(ecs::PointSystem::GetLevel(e))) / 100);

		iLoss = std::min(800000, iLoss);

		if (bTown)
			iLoss = 0;

		if (ItemSystem::IsEquipUniqueItem(e, UNIQUE_ITEM_TEARDROP_OF_GODNESS))
			iLoss /= 2;

		LOG_INFO("DEATH_PENALTY({}) EXP_LOSS: {} percent {}%", ecs::PlayerRuntime::GetName(e).data(), iLoss, __GetExpLossPerc(ecs::PointSystem::GetLevel(e)));

		ecs::PointSystem::Change(e, POINT_EXP, -iLoss, true);
	}
}

} // namespace CombatSystem


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

namespace CombatSystem {

// What a death scatters on the ground.
void ItemDropPenalty(entt::entity e, entt::entity killer)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
	if (ecs::PlayerRuntime::GetGMLevel(e) > GM_PLAYER) {
		return;
	}
#endif

	if (ecs::SocialSystem::GetMyShop(e))
		return;

	if (ecs::PointSystem::GetLevel(e) < 50)
		return;

	if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
	{
		return;
	}

	struct TItemDropPenalty* table = &aItemDropPenalty_kor[0];

	if (ecs::PointSystem::GetLevel(e) < 10)
		return;

	uint8_t iAlignIndex;

	if (CombatSystem::GetRealAlignment(e)		<= 4999)		iAlignIndex = 0;
	else if (CombatSystem::GetRealAlignment(e) <= 14999)		iAlignIndex = 1;
	else if (CombatSystem::GetRealAlignment(e) <= 19999)		iAlignIndex = 2;
	else if (CombatSystem::GetRealAlignment(e) <= 29999)		iAlignIndex = 3;
	else if (CombatSystem::GetRealAlignment(e) <= 49999)		iAlignIndex = 4;
	else if (CombatSystem::GetRealAlignment(e) <= 74999)		iAlignIndex = 5;
	else if (CombatSystem::GetRealAlignment(e) <= 99999)		iAlignIndex = 6;
	else if (CombatSystem::GetRealAlignment(e) <= 124999)		iAlignIndex = 7;
	else if (CombatSystem::GetRealAlignment(e) <= 174999)		iAlignIndex = 8;
	else if (CombatSystem::GetRealAlignment(e) <= 249999)		iAlignIndex = 9;
	else if (CombatSystem::GetRealAlignment(e) <= 499999)		iAlignIndex = 10;
	else if (CombatSystem::GetRealAlignment(e) <= 749999)		iAlignIndex = 11;
	else if (CombatSystem::GetRealAlignment(e) <= 999999)		iAlignIndex = 12;
	else if (CombatSystem::GetRealAlignment(e) <= 1499999)		iAlignIndex = 13;
	else if (CombatSystem::GetRealAlignment(e) <= 2499999)		iAlignIndex = 14;
	else if (CombatSystem::GetRealAlignment(e) == 2500000)		iAlignIndex = 15;
	else return;

	std::vector<std::pair<entt::entity, int>> vec_item;
	const entt::entity ownerEntity = e;
	int	i;
	bool isDropAllEquipments = false;

	TItemDropPenalty& r = table[iAlignIndex];
	LOG_INFO("{} align {} inven_pct {} equip_pct {}", ecs::PlayerRuntime::GetName(e).data(), iAlignIndex, r.iInventoryPct, r.iEquipmentPct);

	bool bDropInventory = r.iInventoryPct >= number(1, 1000);
	bool bDropEquipment = r.iEquipmentPct >= number(1, 100);
	bool bDropAntiDropUniqueItem = false;

	if ((bDropInventory || bDropEquipment) && ItemSystem::IsEquipUniqueItem(e, UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY))
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

				InventorySystem::SyncQuickslot(e, QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
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

				InventorySystem::SyncQuickslot(e, QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
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
			InventorySystem::SyncQuickslot(e, QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE1, 255);
			if (ItemSystem::RemoveItemEcs(unique1))
				vec_item.emplace_back(unique1, EQUIPMENT);
		}

		const entt::entity unique2 = ItemSystem::GetWearItem(ownerEntity, WEAR_UNIQUE2);
		if (ItemSystem::IsValidItem(unique2) &&
			ItemSystem::GetItemVnum(unique2) == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			InventorySystem::SyncQuickslot(e, QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE2, 255);
			if (ItemSystem::RemoveItemEcs(unique2))
				vec_item.emplace_back(unique2, EQUIPMENT);
		}
	}

	{
		PIXEL_POSITION pos;
		pos.x = ecs::PlayerRuntime::GetX(e);
		pos.y = ecs::PlayerRuntime::GetY(e);

		unsigned int i;

		for (i = 0; i < vec_item.size(); ++i)
		{
			const entt::entity item = vec_item[i].first;
			if (!ItemSystem::IsValidItem(item))
				continue;
			int window = vec_item[i].second;

			if (!ItemSystem::PlaceItemOnGround(
					item, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
				continue;

			LOG_INFO("DROP_ITEM_PK: {} {} {} from {}",
				ItemSystem::GetItemName(item), pos.x, pos.y, ecs::PlayerRuntime::GetName(e).data());
			LogManager::instance().ItemLogEntity(
				e, item, "DEAD_DROP",
				(window == INVENTORY) ? "INVENTORY" :
				((window == EQUIPMENT) ? "EQUIPMENT" : ""));

			pos.x = ecs::PlayerRuntime::GetX(e) + number(-7, 7) * 20;
			pos.y = ecs::PlayerRuntime::GetY(e) + number(-7, 7) * 20;
		}
	}
}

} // namespace CombatSystem


// char_battle.cpp slice BC3a helper surface duplicated into CombatSystem.cpp

#ifdef ENABLE_DROP_INSTANT_INVENTORY
static void __UpdateBattlePassCollectProgress(LegacyCharHandle ch, uint32_t dwItemVnum, uint32_t dwCount)
{
#ifdef ENABLE_BATTLE_PASS
	if (!ch || !dwCount)
		return;

	const uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(ch->GetEntityHandle());
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

			if (ecs::PlayerRuntime::GetMissionProgress(ch->GetEntityHandle(), dwMissionType, bBattlePassId) >= dwNeedCount)
				return;

			ecs::PlayerRuntime::UpdateMissionProgress(ch->GetEntityHandle(), dwMissionType, bBattlePassId, dwCount, dwNeedCount);
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

static void __GiveRewardItemToCharacterOrDrop(entt::entity chEntity, entt::entity victim, entt::entity itemEntity, const PIXEL_POSITION& pos, bool bTrackBattlePass)
{
	// The auto-give and the battle-pass progress still take the character.
	LPCHARACTER ch = ecs::LegacyCharOf(chEntity);
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

	if (!ItemSystem::PlaceItemOnGround(
			itemEntity,
			ecs::PlayerRuntime::GetMapIndex(victim),
			pos, 300))
		return;

	if (ch && CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(chEntity)) == false)
		ItemSystem::SetGroundOwnership(
			itemEntity, chEntity, 60);

	LOG_INFO("DROP_ITEM: {} {} {} from {}", ItemSystem::GetItemName(itemEntity),
		pos.x, pos.y, ecs::PlayerRuntime::GetName(victim).data());
}
#endif


#ifdef ENABLE_RARE_DROP_NOTICE_RAZOR93
static std::string MakeItemLink(entt::entity item, entt::entity killer, entt::entity mob)
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
	if (ecs::PlayerRuntime::GetDesc(killer))
		lang = ecs::PlayerRuntime::GetDesc(killer)->GetLanguage();


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
		killer != entt::null ? ecs::PlayerRuntime::GetName(killer).data() : "Player",
		mob != entt::null ? ecs::PlayerRuntime::GetName(mob).data() : "Mob",
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

namespace CombatSystem {

// Everything a kill hands out: the experience, the gold and the drops.
void Reward(entt::entity e, bool bItemDrop)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	//PROF_UNIT puReward("Reward");
	const entt::entity attacker = DistributeExp(e);
	if (attacker == entt::null)
		return;

	// CreateDropItem, MakeItemLink, __GiveRewardItemToCharacterOrDrop and the
	// party dice roll still take the characters; each is its own migration and
	// they share these two resolves.
	LPCHARACTER self = ecs::LegacyCharOf(e);
	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);
	if (!self || !pkAttacker)
		return;


	if (!ecs::PlayerRuntime::IsPC(e) && !self->GetMobData())
	{
		LOG_ERROR("Reward: NULL mob data (vid={} race={} name={} map={} x={} y={} attacker={})", ecs::PlayerRuntime::GetPacketVID(e), ecs::PlayerRuntime::GetRaceNum(e), ecs::PlayerRuntime::GetName(e).data(), ecs::PlayerRuntime::GetMapIndex(e), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), pkAttacker ? ecs::PlayerRuntime::GetName(attacker).data() : "<null>");
		CombatSystem::ClearDamageLedger(e);
		return;
	}
	//PROF_UNIT pu1("r1");
	if (ecs::PlayerRuntime::IsPC(attacker))
	{
		if ((ecs::PointSystem::GetLevel(e) - ecs::PointSystem::GetLevel(attacker)) >= -10)
		{
			/*if (CombatSystem::GetRealAlignment(pkAttacker->GetEntityHandle()) < 0) // trsra: minden gyilkols 2 pontot ad
			{
				if (ItemSystem::IsEquipUniqueItem(pkAttacker->GetEntityHandle(), UNIQUE_ITEM_FASTER_ALIGNMENT_UP_BY_KILL))
					CombatSystem::UpdateAlignment(pkAttacker->GetEntityHandle(), 14);
				else
					CombatSystem::UpdateAlignment(pkAttacker->GetEntityHandle(), 7);
			}
			else*/
				CombatSystem::UpdateAlignment(pkAttacker->GetEntityHandle(), 2);
		}

		ecs::PlayerRuntime::SetQuestNPCID(pkAttacker->GetEntityHandle(), ecs::PlayerRuntime::GetPacketVID(e));
		quest::CQuestManager::instance().Kill(ecs::PlayerRuntime::GetPlayerID(attacker), ecs::PlayerRuntime::GetRaceNum(e));
		CHARACTER_MANAGER::instance().KillLog(ecs::PlayerRuntime::GetRaceNum(e));
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
		COrcsDungeon::instance().OnMobKilled(attacker, e);
		CTritonTempleDungeon::instance().OnMobKilled(attacker, e);
		CValentineDungeon::instance().OnMobKilled(attacker, e);
		CRuneDungeon::instance().OnMobKilled(attacker, e);
		CPyramidDungeonRazor93::instance().OnMobKilled(attacker, e);
		CNightmareDungeonRazor93::instance().OnMobKilled(attacker, e);
		//CLostCastleDungeon::instance().OnMobKilled((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null), e);
		CHalloween2022Dungeon::instance().OnMobKilled(attacker, e);
		CVikingDungeon::instance().OnMobKilled(attacker, e);
		CEasterDungeon::instance().OnMobKilled(attacker, e);
#endif

#ifdef ENABLE_BATTLE_PASS
		uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(pkAttacker->GetEntityHandle());
		if (bBattlePassId)
		{
			uint32_t dwMonsterVnum, dwToKillCount;
			if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, MONSTER_KILL, &dwMonsterVnum, &dwToKillCount))
			{
				if (dwMonsterVnum == ecs::PlayerRuntime::GetRaceNum(e) && ecs::PlayerRuntime::GetMissionProgress(pkAttacker->GetEntityHandle(), MONSTER_KILL, bBattlePassId) < dwToKillCount)
					ecs::PlayerRuntime::UpdateMissionProgress(pkAttacker->GetEntityHandle(), MONSTER_KILL, bBattlePassId, 1, dwToKillCount);
			}
		}
#endif

		if (!number(0, 9))
		{
			if (ecs::PointSystem::Get(attacker, POINT_KILL_HP_RECOVERY))
			{
				int iHP = ecs::PointSystem::GetMaxHP(attacker) * ecs::PointSystem::Get(attacker, POINT_KILL_HP_RECOVERY) / 100;
				ecs::PointSystem::Change(attacker, POINT_HP, iHP);
				CombatSystem::CreateFly(e, FLY_HP_SMALL, attacker);
			}

			if (ecs::PointSystem::Get(attacker, POINT_KILL_SP_RECOVER))
			{
				int iSP = ecs::PointSystem::GetMaxSP(attacker) * ecs::PointSystem::Get(attacker, POINT_KILL_SP_RECOVER) / 100;
				ecs::PointSystem::Change(attacker, POINT_SP, iSP);
				CombatSystem::CreateFly(e, FLY_SP_SMALL, attacker);
			}
		}
	}
	//pu1.Pop();

#ifdef ENABLE_BLOCK_MULTIFARM
	if (AffectSystem::FindAffect(attacker, AFFECT_DROP_BLOCK, APPLY_NONE)) {
		return;
	}
#endif

	if (!bItemDrop)
		return;

	PIXEL_POSITION pos { ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e),
		ecs::PlayerRuntime::GetZ(e) };

	if (!ecs::GetMovablePosition(ecs::PlayerRuntime::GetMapIndex(e), pos.x, pos.y, pos))
		return;

	//
	//
	//
	//PROF_UNIT pu2("r2");
	if (test_server)
		LOG_TRACE("Drop money : Attacker {}", ecs::PlayerRuntime::GetName(attacker).data());
	RewardGold(e, attacker);
	//pu2.Pop();

	//
	//
	//
	//PROF_UNIT pu3("r3");
	entt::entity itemEntity = entt::null;

	std::vector<entt::entity> s_vec_item;
	s_vec_item.clear();

	if (ITEM_MANAGER::instance().CreateDropItem(self, pkAttacker, s_vec_item))
	{

#ifdef ENABLE_RARE_DROP_NOTICE_RAZOR93
		for (const entt::entity dropItem : s_vec_item)
		{
			if (verjema_szadba_ixtreeme.find(ItemSystem::GetItemVnum(dropItem)) != verjema_szadba_ixtreeme.end())
			{
		std::string message = MakeItemLink(dropItem, attacker, e);
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

		if (ecs::SocialSystem::GetDungeon(e) && pkAttacker && ecs::PlayerRuntime::IsPC(attacker) && !s_vec_item.empty())
		{
			const long lMapIndex = ecs::PlayerRuntime::GetMapIndex(e); // a megolt mob mapindexe

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
				if (ecs::SocialSystem::GetParty(attacker)) // CSAK partyra
				{
					CDungeon* pDungeon = ecs::SocialSystem::GetDungeon(e);

					// csak akkor, ha a killer ugyanebben a dungeon instance-ben van
					if (ecs::SocialSystem::GetDungeon(pkAttacker->GetEntityHandle()) == pDungeon)
					{
						// --- helper: HWID|HOST kulcs ugyanugy, ahogy nalad masutt is ---
						auto MakeHwidHostKey = [&](entt::entity ch) -> std::string
							{
								if (ch == entt::null || !ecs::PlayerRuntime::IsPC(ch) || !ecs::PlayerRuntime::GetDesc(ch))
									return std::string();

								DESC* d = ecs::PlayerRuntime::GetDesc(ch);
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
						std::unordered_map<std::string, entt::entity> mapWinnerByKey;
						mapWinnerByKey.reserve(16);

						pDungeon->ForEachMember([&](entt::entity mch)
							{
								const entt::entity mchEntity = mch;
								if (mch == entt::null || !ecs::PlayerRuntime::IsPC(mchEntity) || !ecs::PlayerRuntime::GetDesc(mchEntity))
									return;

								// ugyanabban a dungeon instance-ben kell legyen
								if (ecs::SocialSystem::GetDungeon(mchEntity) != pDungeon)
									return;

								//   ugyanazon a mapindexen legyen (INSTANCE) -> NINCS hibas normalizalas
								if (ecs::PlayerRuntime::GetMapIndex(mchEntity) != lMapIndex)
									return;

								// ugyanabban a partyban legyen
								if (ecs::SocialSystem::GetParty(mchEntity) != ecs::SocialSystem::GetParty(attacker))
									return;

								std::string key = MakeHwidHostKey(mch);

								// ha nincs hwid/host, fallback: account (ne kapjon duplan)
								if (key.empty())
									key = "ACC:" + std::to_string(ecs::PlayerRuntime::GetDesc(mchEntity)->GetAccountTable().id);

								auto it = mapWinnerByKey.find(key);
								if (it == mapWinnerByKey.end())
								{
									mapWinnerByKey.emplace(std::move(key), mch);
									return;
								}

								// dupe HWID|HOST: a legtobb dmg-et okozo kap
								uint64_t dmgNew = 0;
								uint64_t dmgOld = 0;

								auto itNew = CombatSystem::DamageLedgerOf(e).entries.find(mchEntity);
								if (itNew != CombatSystem::DamageLedgerOf(e).entries.end())
									dmgNew = itNew->second.totalDamage;

								auto itOld = CombatSystem::DamageLedgerOf(e).entries.find(it->second);
								if (itOld != CombatSystem::DamageLedgerOf(e).entries.end())
									dmgOld = itOld->second.totalDamage;

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
								const entt::entity rchEntity = kv.second;

								if (rchEntity == entt::null || !ecs::PlayerRuntime::IsPC(rchEntity) || !ecs::PlayerRuntime::GetDesc(rchEntity))
									continue;

								PIXEL_POSITION mpos = pos;

								// kis eltolas, hogy ne 1 pontra essen minden
								mpos.x = number(-7, 7) * 20 + ecs::PlayerRuntime::GetX(e);
								mpos.y = number(-7, 7) * 20 + ecs::PlayerRuntime::GetY(e);

								for (const auto& di : drops)
								{
									const entt::entity newItem =
										ITEM_MANAGER::instance().CreateItem(di.vnum, di.count);
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
										__GiveRewardItemToCharacterOrDrop(rchEntity, e, newItem, mpos, true);
									}
									else
									{
										if (!ItemSystem::PlaceItemOnGround(
												newItem, lMapIndex, mpos, 300))
										{
											ItemSystem::DestroyItemEntityEcs(
												newItem, "SHARED_DROP_PLACE_FAIL");
											continue;
										}

										if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(rchEntity)) == false)
											ItemSystem::SetGroundOwnership(
												newItem, rchEntity);
									}
#else
									if (!ItemSystem::PlaceItemOnGround(
											newItem, lMapIndex, mpos, 300))
									{
										ItemSystem::DestroyItemEntityEcs(
											newItem, "SHARED_DROP_PLACE_FAIL");
										continue;
									}

									if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(rchEntity)) == false)
										ItemSystem::SetGroundOwnership(
											newItem, rchEntity);
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
					CombatSystem::ClearDamageLedger(e);
					return;
				}

				const bool bKeepGroundDrop = false;

				if (bInstantRewardToInventory && !bKeepGroundDrop)
				{
					__GiveRewardItemToCharacterOrDrop(attacker, e, itemEntity, pos, true);
				}
				else
				{
					if (!ItemSystem::PlaceItemOnGround(
							itemEntity, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
					{
						LOG_ERROR("failed to place single drop entity {}",
							static_cast<uint32_t>(itemEntity));
						CombatSystem::ClearDamageLedger(e);
						return;
					}

					if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(attacker)) == false)
					{
						ItemSystem::SetGroundOwnership(
							itemEntity, attacker);
					}

					LOG_INFO("DROP_ITEM: {} {} {} from {}",
						ItemSystem::GetItemName(itemEntity), pos.x, pos.y, ecs::PlayerRuntime::GetName(e).data());
				}

				pos.x = number(-7, 7) * 20;
				pos.y = number(-7, 7) * 20;
				pos.x += ecs::PlayerRuntime::GetX(e);
				pos.y += ecs::PlayerRuntime::GetY(e);
			}
			else
			{
				int iItemIdx = s_vec_item.size() - 1;

				std::priority_queue<std::pair<uint64_t, entt::entity> > pq;

				uint64_t total_dam = 0;

				for (std::map<entt::entity, ecs::BattleContribution>::iterator it = CombatSystem::DamageLedgerOf(e).entries.begin(); it != CombatSystem::DamageLedgerOf(e).entries.end(); ++it)
				{
					uint64_t iDamage = it->second.totalDamage;
					if (iDamage > 0)
					{
						if (ecs::PlayerRuntime::IsValid(it->first))
						{
							pq.push(std::make_pair(iDamage, it->first));
							total_dam += iDamage;
						}
					}
				}

				std::vector<entt::entity> v;

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

						if (!ItemSystem::PlaceItemOnGround(
								itemEntity, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
							continue;

						if (pkAttacker && CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(attacker)) == false)
							ItemSystem::SetGroundOwnership(
								itemEntity, attacker);

						LOG_INFO("DROP_ITEM: {} {} {} by {}",
							ItemSystem::GetItemName(itemEntity), pos.x, pos.y, ecs::PlayerRuntime::GetName(e).data());

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += ecs::PlayerRuntime::GetX(e);
						pos.y += ecs::PlayerRuntime::GetY(e);
					}
				}
				else
				{
					std::vector<entt::entity>::iterator it = v.begin();

					while (iItemIdx >= 0)
					{
						itemEntity = s_vec_item[iItemIdx--];

						if (!ItemSystem::IsValidItem(itemEntity))
						{
							LOG_ERROR("item null in vector idx {}", iItemIdx + 1);
							continue;
						}

						entt::entity owner = *it;

						if (LPPARTY ownerParty = ecs::SocialSystem::GetParty(owner))
							owner = ownerParty->GetNextOwnership(owner, ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e));

						++it;

						if (it == v.end())
							it = v.begin();

						const bool bKeepGroundDrop = false;

						if (bInstantRewardToInventory && !bKeepGroundDrop)
						{
							__GiveRewardItemToCharacterOrDrop(owner, e, itemEntity, pos, true);
						}
						else
						{
							if (!ItemSystem::PlaceItemOnGround(
									itemEntity, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
								continue;

							if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(owner)) == false)
							{
								ItemSystem::SetGroundOwnership(
									itemEntity, owner);
							}

							LOG_INFO("DROP_ITEM: {} {} {} by {}",
								ItemSystem::GetItemName(itemEntity), pos.x, pos.y, ecs::PlayerRuntime::GetName(e).data());
						}

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += ecs::PlayerRuntime::GetX(e);
						pos.y += ecs::PlayerRuntime::GetY(e);
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
					CombatSystem::ClearDamageLedger(e);
					return;
				}
				if (!ItemSystem::PlaceItemOnGround(
						itemEntity, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
				{
					LOG_ERROR("failed to place single ground drop entity {}",
						static_cast<uint32_t>(itemEntity));
					CombatSystem::ClearDamageLedger(e);
					return;
				}

				if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(attacker)) == false)
				{
					ItemSystem::SetGroundOwnership(
						itemEntity, attacker);
				}

				pos.x = number(-7, 7) * 20;
				pos.y = number(-7, 7) * 20;
				pos.x += ecs::PlayerRuntime::GetX(e);
				pos.y += ecs::PlayerRuntime::GetY(e);

				LOG_INFO("DROP_ITEM: {} {} {} from {}",
					ItemSystem::GetItemName(itemEntity), pos.x, pos.y, ecs::PlayerRuntime::GetName(e).data());
			}
			else
			{
				int iItemIdx = s_vec_item.size() - 1;

				std::priority_queue<std::pair<uint64_t, entt::entity> > pq;

				uint64_t total_dam = 0;

				for (std::map<entt::entity, ecs::BattleContribution>::iterator it = CombatSystem::DamageLedgerOf(e).entries.begin(); it != CombatSystem::DamageLedgerOf(e).entries.end(); ++it)
				{
					uint64_t iDamage = it->second.totalDamage;
					if (iDamage > 0)
					{
						if (ecs::PlayerRuntime::IsValid(it->first))
						{
							pq.push(std::make_pair(iDamage, it->first));
							total_dam += iDamage;
						}
					}
				}

				std::vector<entt::entity> v;

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

						if (!ItemSystem::PlaceItemOnGround(
								itemEntity, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
							continue;

						if (pkAttacker && CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(attacker)) == false)
							ItemSystem::SetGroundOwnership(
								itemEntity, attacker);

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += ecs::PlayerRuntime::GetX(e);
						pos.y += ecs::PlayerRuntime::GetY(e);

						LOG_INFO("DROP_ITEM: {} {} {} by {}",
							ItemSystem::GetItemName(itemEntity), pos.x, pos.y, ecs::PlayerRuntime::GetName(e).data());
					}
				}
				else
				{
					std::vector<entt::entity>::iterator it = v.begin();

					while (iItemIdx >= 0)
					{
						itemEntity = s_vec_item[iItemIdx--];

						if (!ItemSystem::IsValidItem(itemEntity))
						{
							LOG_ERROR("item null in vector idx {}", iItemIdx + 1);
							continue;
						}

						if (!ItemSystem::PlaceItemOnGround(
								itemEntity, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
							continue;

						entt::entity owner = *it;

						if (LPPARTY ownerParty = ecs::SocialSystem::GetParty(owner))
							owner = ownerParty->GetNextOwnership(owner, ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e));

						++it;

						if (it == v.end())
							it = v.begin();

						if (CBattleArena::instance().IsBattleArenaMap(ecs::PlayerRuntime::GetMapIndex(owner)) == false)
						{
							ItemSystem::SetGroundOwnership(
								itemEntity, owner);
						}

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += ecs::PlayerRuntime::GetX(e);
						pos.y += ecs::PlayerRuntime::GetY(e);

						LOG_INFO("DROP_ITEM: {} {} {} by {}",
							ItemSystem::GetItemName(itemEntity), pos.x, pos.y, ecs::PlayerRuntime::GetName(e).data());
					}
				}
			}

#endif
		}
	}

	CombatSystem::ClearDamageLedger(e);
}

} // namespace CombatSystem


// char_battle.cpp slice BC2 moved into CombatSystem.cpp

namespace CombatSystem {

// The gold a kill drops, and who it belongs to.
void RewardGold(entt::entity e, entt::entity attacker)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	// The mob table and the drop helpers still take the characters; each is
	// its own migration and they share these two resolves.
	LPCHARACTER self = ecs::LegacyCharOf(e);
	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);

	if (!self || !pkAttacker || !ecs::PlayerRuntime::IsPC(attacker))
		return;

	if (!self->GetMobData())
	{
		LOG_ERROR("RewardGold: NULL mob data (vid={} race={} name={} map={} x={} y={} attacker={})", ecs::PlayerRuntime::GetPacketVID(e), ecs::PlayerRuntime::GetRaceNum(e), ecs::PlayerRuntime::GetName(e).data(), ecs::PlayerRuntime::GetMapIndex(e), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), pkAttacker ? ecs::PlayerRuntime::GetName(attacker).data() : "<null>");
		return;
	}
	if (pkAttacker && ecs::PlayerRuntime::IsPC(attacker)) {
		if (ecs::PlayerRuntime::IsStone(e)) {
#ifdef ENABLE_ANTICHEAT
			if (ecs::PlayerRuntime::GetMapIndex(attacker) < 1000) {
				pkAttacker->ProcessCheatCheck(get_global_time());
			}
#endif
#ifdef DISABLE_GOLD_DROP_FROM_TAKAKA
			if (ecs::PlayerRuntime::GetRaceNum(e) >= TANAKA) {
				return;
			}
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
			if (AffectSystem::FindAffect(attacker, AFFECT_DROP_BLOCK, APPLY_NONE)) {
				return;
			}
#endif

			bool drop = true;
			int mylvl = ecs::PointSystem::GetLevel(attacker), targetlvl = ecs::PointSystem::GetLevel(e);
			if (mylvl > targetlvl) {
				drop = mylvl - targetlvl <= 15 ? true : false;
			}

			if (drop) {
				int64_t gold = number((*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMin, (*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMax);

				if (gold <= 0) {
					return;
				}

				if (ecs::PointSystem::Get(attacker, POINT_MALL_GOLDBONUS)) {
					gold += (gold * ecs::PointSystem::Get(attacker, POINT_MALL_GOLDBONUS) / 100);
				}

				ecs::PointSystem::Change(attacker, POINT_GOLD, gold, true);
			}
		}
		else {
#ifdef ENABLE_BLOCK_MULTIFARM
			if (AffectSystem::FindAffect(attacker, AFFECT_DROP_BLOCK, APPLY_NONE)) {
				return;
			}
#endif

			// ADD_PREMIUM
			bool isAutoLoot =
				(ecs::PlayerRuntime::GetPremiumRemainSeconds(pkAttacker->GetEntityHandle(), PREMIUM_AUTOLOOT) > 0 ||
					pkAttacker->IsEquipUniqueGroup(UNIQUE_GROUP_AUTOLOOT))
				? true : false; // 3
			// END_OF_ADD_PREMIUM

			PIXEL_POSITION pos;

			if (!isAutoLoot)
				if (!ecs::GetMovablePosition(ecs::PlayerRuntime::GetMapIndex(e), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), pos))
					return;

			int iTotalGold = 0;
			//
			// ---------   Ȯ  ----------
			//
			int iGoldPercent = MobRankStats[ecs::PlayerRuntime::GetMobRank(e)].iGoldPercent;

			if (ecs::PlayerRuntime::IsPC(attacker))
				iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(attacker, PRIV_GOLD_DROP)) / 100;

#ifdef ENABLE_EVENT_MANAGER
			if (ecs::PlayerRuntime::IsPC(attacker))
			{
				const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(YANG_DROP_EVENT, ecs::PlayerRuntime::GetEmpire(attacker));
				if (event != nullptr)
					iGoldPercent = iGoldPercent * (100 + (event->value[0] + CPrivManager::instance().GetPriv(attacker, PRIV_GOLD_DROP))) / 100;
				else
					iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(attacker, PRIV_GOLD_DROP)) / 100;
			}
#else
			if (ecs::PlayerRuntime::IsPC(attacker))
				iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP)) / 100;
#endif

			iGoldPercent = iGoldPercent * CHARACTER_MANAGER::instance().GetMobGoldDropRate(attacker) / 100;

			// ADD_PREMIUM
			if (ecs::PlayerRuntime::GetPremiumRemainSeconds(pkAttacker->GetEntityHandle(), PREMIUM_GOLD) > 0 ||
				pkAttacker->IsEquipUniqueGroup(UNIQUE_GROUP_LUCKY_GOLD))
				iGoldPercent += iGoldPercent;
			// END_OF_ADD_PREMIUM

			if (iGoldPercent > 100)
				iGoldPercent = 100;

			int iPercent;

			if (ecs::PlayerRuntime::GetMobRank(e) >= MOB_RANK_BOSS)
				iPercent = ((iGoldPercent * PERCENT_LVDELTA_BOSS(ecs::PointSystem::GetLevel(attacker), ecs::PointSystem::GetLevel(e))) / 100);
			else
				iPercent = ((iGoldPercent * PERCENT_LVDELTA(ecs::PointSystem::GetLevel(attacker), ecs::PointSystem::GetLevel(e))) / 100);
			//int iPercent = CALCULATE_VALUE_LVDELTA(ecs::PointSystem::GetLevel((pkAttacker ? pkAttacker->GetEntityHandle() : entt::null)), ecs::PointSystem::GetLevel(e), iGoldPercent);

			if (number(1, 100) > iPercent)
				return;

			int iGoldMultipler = 1;

			if (1 == number(1, 50000)) // 1/50000 Ȯ  10
				iGoldMultipler *= 10;
			else if (1 == number(1, 10000)) // 1/10000 Ȯ  5
				iGoldMultipler *= 5;

			//
			if (ecs::PointSystem::Get(attacker, POINT_GOLD_DOUBLE_BONUS))
				if (number(1, 100) <= ecs::PointSystem::Get(attacker, POINT_GOLD_DOUBLE_BONUS))
					iGoldMultipler *= 2;

			//
			// ---------     ----------
			//
			if (test_server)
				ecs::ChatSystem::Send(attacker, CHAT_TYPE_PARTY, "gold_mul %d rate %d", iGoldMultipler, CHARACTER_MANAGER::instance().GetMobGoldAmountRate(attacker));

			//
			// ---------   ó -------------
			//
			int iGold10DropPct = 100;
#ifdef ENABLE_EVENT_MANAGER
			const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(YANG_DROP_EVENT, ecs::PlayerRuntime::GetEmpire(attacker));
			if (event != nullptr)
				iGold10DropPct = (iGold10DropPct * 100) / (100 + event->value[0] + CPrivManager::instance().GetPriv(attacker, PRIV_GOLD10_DROP));
			else
				iGold10DropPct = (iGold10DropPct * 100) / (100 + CPrivManager::instance().GetPriv(attacker, PRIV_GOLD10_DROP));
#else
			iGold10DropPct = (iGold10DropPct * 100) / (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD10_DROP));
#endif

			// MOB_RANK BOSS   ź
			if (ecs::PlayerRuntime::GetMobRank(e) >= MOB_RANK_BOSS && !ecs::PlayerRuntime::IsStone(e) && (*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMax != 0)
			{
				if (1 == number(1, iGold10DropPct))
					iGoldMultipler *= 10; // 1% Ȯ  10

				int iSplitCount = number(25, 35);

				for (int i = 0; i < iSplitCount; ++i)
				{
					int iGold = number((*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMin, (*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMax) / iSplitCount;
					if (test_server)
						LOG_INFO("iGold {}", iGold);
					iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(attacker) / 100;
					iGold *= iGoldMultipler;

					if (iGold == 0)
					{
						continue;
					}

					if (test_server)
					{
						LOG_TRACE("Drop Moeny MobGoldAmountRate {} {}", CHARACTER_MANAGER::instance().GetMobGoldAmountRate(attacker), iGoldMultipler);
						LOG_TRACE("Drop Money gold {} GoldMin {} GoldMax {}", iGold, (*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMax, (*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMax);
					}

#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93
					ItemSystem::GiveGold(pkAttacker->GetEntityHandle(), iGold);
					iTotalGold += iGold;
#else
					const entt::entity gold = ITEM_MANAGER::instance().CreateItem(1, iGold);
					if (ItemSystem::IsValidItem(gold))
					{
						pos.x = ecs::PlayerRuntime::GetX(e) + ((number(-14, 14) + number(-14, 14)) * 23);
						pos.y = ecs::PlayerRuntime::GetY(e) + ((number(-14, 14) + number(-14, 14)) * 23);
						if (ItemSystem::PlaceItemOnGround(
								gold, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
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
					int iGold = number((*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMin, (*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMax);
					iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(attacker) / 100;
					iGold *= iGoldMultipler;

					if (iGold == 0)
					{
						continue;
					}

#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93
					ItemSystem::GiveGold(pkAttacker->GetEntityHandle(), iGold);
					iTotalGold += iGold;
#else
					const entt::entity gold = ITEM_MANAGER::instance().CreateItem(1, iGold);
					if (ItemSystem::IsValidItem(gold))
					{
						pos.x = ecs::PlayerRuntime::GetX(e) + (number(-7, 7) * 20);
						pos.y = ecs::PlayerRuntime::GetY(e) + (number(-7, 7) * 20);
						if (ItemSystem::PlaceItemOnGround(
								gold, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
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
				int iGold = number((*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMin, (*ecs::PlayerRuntime::GetMobTable(e)).dwGoldMax);
				iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(attacker) / 100;
				iGold *= iGoldMultipler;

				int iSplitCount;

				if (iGold >= 3)
					iSplitCount = number(1, 3);
				else if (ecs::PlayerRuntime::GetMobRank(e) >= MOB_RANK_BOSS)
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
							ItemSystem::GiveGold(pkAttacker->GetEntityHandle(), splitGold);
						}
						else
						{
#ifdef ENABLE_YANG_INSTANT_INVENTORY_RAZOR93
							ItemSystem::GiveGold(pkAttacker->GetEntityHandle(), splitGold);
#else
							const entt::entity gold = ITEM_MANAGER::instance().CreateItem(1, splitGold);
							if (ItemSystem::IsValidItem(gold))
							{
								pos.x = ecs::PlayerRuntime::GetX(e) + (number(-7, 7) * 20);
								pos.y = ecs::PlayerRuntime::GetY(e) + (number(-7, 7) * 20);
								if (!ItemSystem::PlaceItemOnGround(
										gold, ecs::PlayerRuntime::GetMapIndex(e), pos, 300))
									ItemSystem::DestroyItemEntityEcs(
										gold, "GOLD_DROP_PLACE_FAIL");
							}
#endif
						}
					}
				}
			}
		}

		//DBManager::instance().SendMoneyLog(MONEY_LOG_MONSTER, ecs::PlayerRuntime::GetRaceNum(e), iTotalGold);
	}
}

} // namespace CombatSystem

// char_battle.cpp slice BB2b moved into CombatSystem.cpp

#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
static void ProcessStoneSpawnStep(entt::entity stone);

static int64_t CalcReferenceBasicHitDamage(entt::entity attacker, entt::entity victim);

namespace CombatSystem {

// The damage pipeline. Victim and attacker are handles the whole way through.
// The receiver used to be the thing being hit, so every bare accessor in here
// was a read on the victim; that is what the entity in each call is.
bool Damage(entt::entity victim, entt::entity attacker, int64_t dam, uint8_t damageType)
{
    if (victim == entt::null || !g_registry.valid(victim))
        return false;

    const EDamageType type = static_cast<EDamageType>(damageType);

    // The damage map that decides drops and experience is still a CHARACTER
    // member, so the victim is resolved once here and nowhere else below. That
    // map is its own migration.
    LPCHARACTER book = LegacyCharOf(victim);
    if (!book)
        return false;

	LPCHARACTER pkAttacker = ecs::LegacyCharOf(attacker);
#ifdef DISABLE_PC_ATTACK_PC_ON_MAPIDEX1
	if (pkAttacker && ecs::PlayerRuntime::IsPC(attacker) && ecs::PlayerRuntime::IsPC(victim) && ecs::PlayerRuntime::GetMapIndex(victim) == 1)
		return false;
#endif
	if (CombatSystem::GetInvincible(victim))
		return false;

#ifdef __NEWPET_SYSTEM__
	if (ecs::PlayerRuntime::IsImmortal(victim))
		return false;
#endif

	if (pkAttacker)
	{
		const entt::entity attackerEntity = attacker;
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

	if ((ecs::PlayerRuntime::IsPC(victim) && AffectSystem::IsAffectFlag(victim, AFF_REVIVE_INVISIBLE)) || (pkAttacker && (ecs::PlayerRuntime::IsPC(attacker) && AffectSystem::IsAffectFlag(attacker, AFF_REVIVE_INVISIBLE))))
		return false;

#ifdef ENABLE_NEWSTUFF
	if (pkAttacker && ecs::PlayerRuntime::IsStone(victim) && ecs::PlayerRuntime::IsPC(attacker))
	{
		if (ecs::PlayerRuntime::GetEmpire(victim) && ecs::PlayerRuntime::GetEmpire(victim) == ecs::PlayerRuntime::GetEmpire(attacker))
		{
			CombatSystem::SendDamagePacket(victim, attacker, 0, DAMAGE_BLOCK);
			return false;
		}
	}
#endif

	if (DAMAGE_TYPE_MAGIC == type)
	{
		dam = (int)((float)dam * (100 + (ecs::PointSystem::Get(attacker, POINT_MAGIC_ATT_BONUS_PER) + ecs::PointSystem::Get(attacker, POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100.f + 0.5f);
	}

	// Ÿ ƴ   ó
	if (type != DAMAGE_TYPE_NORMAL && type != DAMAGE_TYPE_NORMAL_RANGE)
	{
		if (AffectSystem::IsAffectFlag(victim, AFF_TERROR))
		{
			int pct = SkillSystem::GetSkillPower(victim, SKILL_TERROR) / 400;

			if (number(1, 100) <= pct)
				return false;
		}
	}
#ifdef ENABLE_MAX_100K_DMG_ON_EVENT_MAP_RAZOR93
	if (pkAttacker && ecs::PlayerRuntime::IsPC(attacker) && ecs::PlayerRuntime::GetMapIndex(victim) == 1 && (ecs::PlayerRuntime::IsMonster(victim) || ecs::PlayerRuntime::IsStone(victim)))
	{
#ifdef DISABLE_DAMAGE_TYPE_NORMAL_RANGE_EVENT_MAP


		const entt::entity weapon = ItemSystem::GetWearItem(
			attacker, WEAR_WEAPON);
		const TItemTable* weaponProto = ItemSystem::GetItemProto(weapon);
		if (weaponProto && weaponProto->bSubType == WEAPON_BOW)
		{

			CombatSystem::SendDamagePacket(victim, attacker, 0, DAMAGE_BLOCK);
			return false;
		}
#endif // !DISABLE_DAMAGE_TYPE_NORMAL_RANGE_EVENT_MAP
		const int64_t fixed_dam = 100000;

		// [1] Regisztrljuk a sebzst a dropphoz
		const entt::entity eAttacker = attacker;
		if (eAttacker == entt::null)
			return false;

		auto& damageMap = CombatSystem::DamageLedgerOf(victim).entries;
		auto it = damageMap.find(eAttacker);
		if (it == damageMap.end())
		{
			damageMap.insert(std::make_pair(
				eAttacker,
				ecs::BattleContribution(fixed_dam, 0)
			));
		}
		else
		{
			it->second.totalDamage += fixed_dam;
		}


		CombatSystem::SendDamagePacket(victim, attacker, fixed_dam, DAMAGE_NORMAL);


		if (ecs::PlayerRuntime::GetHP(victim) <= fixed_dam)
		{
			ecs::PlayerRuntime::SetHP(victim, 0);
			CombatSystem::Dead(victim, attacker);
			return true; // nem megy tovbb
		}
		else
		{
			ecs::PointSystem::Change(victim, POINT_HP, -fixed_dam, false);
			return false; // nem megy tovbb
		}
	}
#endif


	int iCurHP = ecs::PlayerRuntime::GetHP(victim);
	int iCurSP = ecs::PlayerRuntime::GetSP(victim);

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
		if (pkAttacker)
		{
			// ũƼ
			int iCriticalPct = ecs::PointSystem::Get(attacker, POINT_CRITICAL_PCT);

			if (!ecs::PlayerRuntime::IsPC(victim)) {
				iCriticalPct += pkAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);
				iCriticalPct += ecs::PointSystem::Get(attacker, POINT_PVM_CRITICAL_PCT);
			}

			if (iCriticalPct)
			{
				if (iCriticalPct >= 10) // 10 ũ 5% + (4 1% ),  ġ 50̸ 20%
					iCriticalPct = 5 + (iCriticalPct - 10) / 4;
				else // 10  ܼ  , 10 = 5%
					iCriticalPct /= 2;

				//ũƼ   .
				iCriticalPct -= ecs::PointSystem::Get(victim, POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					NetworkSyncSystem::BroadcastEffect(g_registry, victim, SE_CRITICAL);

					if (AffectSystem::IsAffectFlag(victim, AFF_MANASHIELD))
					{
						AffectSystem::RemoveAffect(victim, AFF_MANASHIELD);
					}
				}
			}

			//
			int iPenetratePct = ecs::PointSystem::Get(attacker, POINT_PENETRATE_PCT);

			if (!ecs::PlayerRuntime::IsPC(victim))
				iPenetratePct += pkAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);


			if (iPenetratePct)
			{
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(SKILL_RESIST_PENETRATE);

					if (nullptr != pkSk)
					{
						pkSk->SetPointVar("k", 1.0f * SkillSystem::GetSkillPower(victim, SKILL_RESIST_PENETRATE) / 100.0f);

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
				iPenetratePct -= ecs::PointSystem::Get(victim, POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;
#ifdef TEXTS_IMPROVEMENT
					if (test_server) {
						ecs::ChatSystem::SendNew(victim, CHAT_TYPE_INFO, 257, "%d", ecs::PointSystem::Get(victim, POINT_DEF_GRADE) * (100 + ecs::PointSystem::Get(victim, POINT_DEF_BONUS)) / 100);
					}
#endif
					dam += ecs::PointSystem::Get(victim, POINT_DEF_GRADE) * (100 + ecs::PointSystem::Get(victim, POINT_DEF_BONUS)) / 100;

					if (AffectSystem::IsAffectFlag(victim, AFF_MANASHIELD))
					{
						AffectSystem::RemoveAffect(victim, AFF_MANASHIELD);
					}
#ifdef ENABLE_EFFECT_PENETRATE
					NetworkSyncSystem::BroadcastEffect(g_registry, victim, SE_PENETRATE);
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
			if (ecs::PointSystem::Get(victim, POINT_BLOCK) && number(1, 100) <= ecs::PointSystem::Get(victim, POINT_BLOCK))
			{
#ifdef TEXTS_IMPROVEMENT
				if (test_server) {
					ecs::ChatSystem::SendNew(attacker, CHAT_TYPE_INFO, 95, "%s#%d", ecs::PlayerRuntime::GetName(victim), ecs::PointSystem::Get(victim, POINT_BLOCK));
					ecs::ChatSystem::SendNew(victim, CHAT_TYPE_INFO, 95, "%s#%d", ecs::PlayerRuntime::GetName(attacker).data(), ecs::PointSystem::Get(attacker, POINT_BLOCK));
				}
#endif
				CombatSystem::SendDamagePacket(victim, attacker, 0, DAMAGE_BLOCK);
				return false;
			}
		}
		else if (type == DAMAGE_TYPE_NORMAL_RANGE)
		{
			// Ÿ Ÿ
			if (ecs::PointSystem::Get(victim, POINT_DODGE) && number(1, 100) <= ecs::PointSystem::Get(victim, POINT_DODGE))
			{
#ifdef TEXTS_IMPROVEMENT
				if (test_server) {
					ecs::ChatSystem::SendNew(attacker, CHAT_TYPE_INFO, 96, "%s#%d", ecs::PlayerRuntime::GetName(victim), ecs::PointSystem::Get(victim, POINT_DODGE));
					ecs::ChatSystem::SendNew(victim, CHAT_TYPE_INFO, 96, "%s#%d", ecs::PlayerRuntime::GetName(attacker).data(), ecs::PointSystem::Get(attacker, POINT_DODGE));
				}
#endif
				CombatSystem::SendDamagePacket(victim, attacker, 0, DAMAGE_DODGE);
				return false;
			}
		}

#ifndef ENABLE_NO_MALUS_JEONGWIHON
		if (AffectSystem::IsAffectFlag(victim, AFF_JEONGWIHON))
			dam = (int)(dam * (100 + SkillSystem::GetSkillPower(victim, SKILL_JEONGWI) * 25 / 100) / 100);
#endif

		if (AffectSystem::IsAffectFlag(victim, AFF_TERROR))
			dam = (int)(dam * (95 - SkillSystem::GetSkillPower(victim, SKILL_TERROR) / 5) / 100);

		//if (AffectSystem::IsAffectFlag(victim, AFF_HOSIN))
		//	dam = dam * (100 - ecs::PointSystem::Get(victim, POINT_RESIST_NORMAL_DAMAGE)) / 100;
		if (AffectSystem::IsAffectFlag(victim, AFF_HOSIN))
		{
			int32_t resist = ecs::PointSystem::Get(victim, POINT_RESIST_NORMAL_DAMAGE);

			// clamp 0..100
			if (resist < 0) resist = 0;
			if (resist > 100) resist = 100;

			// PvP: csak fele hasson
			if (pkAttacker && ecs::PlayerRuntime::IsPC(attacker) && ecs::PlayerRuntime::IsPC(victim))
				resist = (resist + 1) / 2; // kerekítve: 1->1, 2->1, 3->2...
			if (pkAttacker && ecs::PlayerRuntime::IsMonster(attacker) && ecs::PlayerRuntime::IsPC(victim))
				resist = (resist + 1) / 2; // kerekítve: 1->1, 2->1, 3->2...
			dam = dam * (100 - resist) / 100;
		}
		//
		//  Ӽ
		//
		if (pkAttacker)
		{
			if (type == DAMAGE_TYPE_NORMAL)
			{
				// ݻ
				if (ecs::PointSystem::Get(victim, POINT_REFLECT_MELEE))
				{
					int reflectDamage = dam * ecs::PointSystem::Get(victim, POINT_REFLECT_MELEE) / 100;

					// NOTE: ڰ IMMUNE_REFLECT Ӽ ִٸ ݻ縦  ϴ
					// ƴ϶ 1/3  ؼ  ȹ û.
					if (AffectSystem::IsImmune(attacker, IMMUNE_REFLECT))
						reflectDamage = int(reflectDamage / 3.0f + 0.5f);

					CombatSystem::Damage(attacker, victim, reflectDamage, DAMAGE_TYPE_SPECIAL);
				}
			}

			// ũƼ
			int iCriticalPct = ecs::PointSystem::Get(attacker, POINT_CRITICAL_PCT);

			if (!ecs::PlayerRuntime::IsPC(victim)) {
				iCriticalPct += pkAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);
				iCriticalPct += ecs::PointSystem::Get(attacker, POINT_PVM_CRITICAL_PCT);
			}

			if (iCriticalPct)
			{
				//ũƼ   .
				iCriticalPct -= ecs::PointSystem::Get(victim, POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					NetworkSyncSystem::BroadcastEffect(g_registry, victim, SE_CRITICAL);
				}
			}

			//
			int iPenetratePct = ecs::PointSystem::Get(attacker, POINT_PENETRATE_PCT);

			if (!ecs::PlayerRuntime::IsPC(victim))
				iPenetratePct += pkAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

			{
				CSkillProto* pkSk = CSkillManager::instance().Get(SKILL_RESIST_PENETRATE);

				if (nullptr != pkSk)
				{
					pkSk->SetPointVar("k", 1.0f * SkillSystem::GetSkillPower(victim, SKILL_RESIST_PENETRATE) / 100.0f);

					iPenetratePct -= static_cast<int>(pkSk->kPointPoly.Eval());
				}
			}


			if (iPenetratePct)
			{

				//Ÿ   .
				iPenetratePct -= ecs::PointSystem::Get(victim, POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;
					dam += ecs::PointSystem::Get(victim, POINT_DEF_GRADE) * (100 + ecs::PointSystem::Get(victim, POINT_DEF_BONUS)) / 100;
#ifdef ENABLE_EFFECT_PENETRATE
					NetworkSyncSystem::BroadcastEffect(g_registry, victim, SE_PENETRATE);
#endif
				}
			}

#ifdef ENABLE_BUG_FIXES
			if (int64_t iStealHP_ptr = ecs::PointSystem::Get(attacker, POINT_STEAL_HP)) {
				if (number(1, 100) <= iStealHP_ptr) {
					int64_t iHP = std::min((int64_t)dam, std::max((int64_t)0, ecs::PlayerRuntime::GetHP(victim))) * ecs::PointSystem::Get(attacker, POINT_STEAL_HP) / 100;


					if ((ecs::PointSystem::Get(attacker, POINT_HP) > 0) && (ecs::PointSystem::Get(attacker, POINT_HP) + iHP < ecs::PointSystem::GetMaxHP(attacker)) && (ecs::PlayerRuntime::GetHP(victim) > 0) && (iHP > 0)) {
						CombatSystem::CreateFly(victim, FLY_HP_MEDIUM, attacker);
						ecs::PointSystem::Change(attacker, POINT_HP, iHP);
#if defined(ENABLE_DS_RUNE) || defined(ENABLE_MELEY_LAIR)
						int32_t racevnum = ecs::PlayerRuntime::GetRaceNum(victim);
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
							ecs::PointSystem::Change(victim, POINT_HP, -iHP);
						}
#else
						ecs::PointSystem::Change(victim, POINT_HP, -iHP);
#endif
					}
				}
			}

			if (int64_t iStealSP_ptr = ecs::PointSystem::Get(attacker, POINT_STEAL_SP)) {
				if (ecs::PlayerRuntime::IsPC(victim) && ecs::PlayerRuntime::IsPC(attacker)) {
					if (number(1, 100) <= iStealSP_ptr) {
						int64_t iSP = std::min((int64_t)dam, std::max((int64_t)0, ecs::PlayerRuntime::GetSP(victim))) * ecs::PointSystem::Get(attacker, POINT_STEAL_SP) / 100;


						if ((ecs::PointSystem::Get(attacker, POINT_SP) > 0) && (ecs::PointSystem::Get(attacker, POINT_SP) + iSP < ecs::PointSystem::GetMaxSP(attacker)) && (ecs::PlayerRuntime::GetSP(victim) > 0) && (iSP > 0))
						{
							CombatSystem::CreateFly(victim, FLY_SP_MEDIUM, attacker);
							ecs::PointSystem::Change(attacker, POINT_SP, iSP);
							ecs::PointSystem::Change(victim, POINT_SP, -iSP);
						}
					}
				}
			}
#else
			// HP ƿ
			if (ecs::PointSystem::Get(attacker, POINT_STEAL_HP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iHP = MIN(dam, MAX(0, iCurHP)) * ecs::PointSystem::Get(attacker, POINT_STEAL_HP) / 100;

					if (iHP > 0 && ecs::PlayerRuntime::GetHP(victim) >= iHP)
					{
						CombatSystem::CreateFly(victim, FLY_HP_SMALL, attacker);
						ecs::PointSystem::Change(attacker, POINT_HP, iHP);
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
							ecs::PointSystem::Change(victim, POINT_HP, -iHP);
						}
#else
						ecs::PointSystem::Change(victim, POINT_HP, -iHP);
#endif
					}
				}
			}

			// SP ƿ
			if (ecs::PointSystem::Get(attacker, POINT_STEAL_SP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iCur;

					if (ecs::PlayerRuntime::IsPC(victim))
						iCur = iCurSP;
					else
						iCur = iCurHP;

					int iSP = MIN(dam, MAX(0, iCur)) * ecs::PointSystem::Get(attacker, POINT_STEAL_SP) / 100;

					if (iSP > 0 && iCur >= iSP)
					{
						CombatSystem::CreateFly(victim, FLY_SP_SMALL, attacker);
						ecs::PointSystem::Change(attacker, POINT_SP, iSP);

						if (ecs::PlayerRuntime::IsPC(victim))
							ecs::PointSystem::Change(victim, POINT_SP, -iSP);
					}
				}
			}
#endif

			//  ƿ
			if (ecs::PointSystem::Get(attacker, POINT_STEAL_GOLD))
			{
				if (number(1, 100) <= ecs::PointSystem::Get(attacker, POINT_STEAL_GOLD))
				{
					int iAmount = number(1, ecs::PointSystem::GetLevel(victim));
					ecs::PointSystem::Change(attacker, POINT_GOLD, iAmount);
					DBManager::instance().SendMoneyLog(MONEY_LOG_MISC, 1, iAmount);
				}
			}

#ifdef ENABLE_BUG_FIXES
			int iAbsoHP_ptr = ecs::PointSystem::Get(attacker, POINT_HIT_HP_RECOVERY);
			if (iAbsoHP_ptr > 0) {
				if (number(1, 100) <= iAbsoHP_ptr) {
					int iHPAbso = std::min(dam, ecs::PlayerRuntime::GetHP(victim)) * ecs::PointSystem::Get(attacker, POINT_HIT_HP_RECOVERY) / 100;
					if ((ecs::PointSystem::Get(attacker, POINT_HP) > 0) && (ecs::PointSystem::Get(attacker, POINT_HP) + iHPAbso < ecs::PointSystem::GetMaxHP(attacker)) && (ecs::PlayerRuntime::GetHP(victim) > 0) && (iHPAbso > 0)) {
						CombatSystem::CreateFly(victim, FLY_HP_SMALL, attacker);
						ecs::PointSystem::Change(attacker, POINT_HP, iHPAbso);
					}
				}
			}

			int64_t iAbsoSP_ptr = ecs::PointSystem::Get(attacker, POINT_HIT_SP_RECOVERY);
			if (iAbsoSP_ptr > 0) {
				if (number(1, 100) <= iAbsoSP_ptr) {
					int64_t iSPAbso = std::min(dam, ecs::PlayerRuntime::GetSP(victim)) * ecs::PointSystem::Get(attacker, POINT_HIT_SP_RECOVERY) / 100;
					if ((ecs::PointSystem::Get(attacker, POINT_SP) > 0) && (ecs::PointSystem::Get(attacker, POINT_SP) + iSPAbso < ecs::PointSystem::GetMaxSP(attacker)) && (ecs::PlayerRuntime::GetSP(victim) > 0) && (iSPAbso > 0)) {
						CombatSystem::CreateFly(victim, FLY_SP_SMALL, attacker);
						ecs::PointSystem::Change(attacker, POINT_SP, iSPAbso);
					}
				}
			}
#else
			// ĥ  HPȸ
			if (ecs::PointSystem::Get(attacker, POINT_HIT_HP_RECOVERY) && number(0, 4) > 0) // 80% Ȯ
			{
				int i = ((iCurHP >= 0) ? MIN(dam, iCurHP) : dam) * ecs::PointSystem::Get(attacker, POINT_HIT_HP_RECOVERY) / 100; //@fixme107

				if (i)
				{
					CombatSystem::CreateFly(victim, FLY_HP_SMALL, attacker);
					ecs::PointSystem::Change(attacker, POINT_HP, i);
				}
			}

			// ĥ  SPȸ
			if (ecs::PointSystem::Get(attacker, POINT_HIT_SP_RECOVERY) && number(0, 4) > 0) // 80% Ȯ
			{
				int i = ((iCurHP >= 0) ? MIN(dam, iCurHP) : dam) * ecs::PointSystem::Get(attacker, POINT_HIT_SP_RECOVERY) / 100; //@fixme107

				if (i)
				{
					CombatSystem::CreateFly(victim, FLY_SP_SMALL, attacker);
					ecs::PointSystem::Change(attacker, POINT_SP, i);
				}
			}
#endif

			//   ش.
			if (ecs::PointSystem::Get(attacker, POINT_MANA_BURN_PCT))
			{
				if (number(1, 100) <= ecs::PointSystem::Get(attacker, POINT_MANA_BURN_PCT))
					ecs::PointSystem::Change(victim, POINT_SP, -50);
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
		if (pkAttacker) {
			if (ecs::PointSystem::Get(attacker, POINT_NORMAL_HIT_DAMAGE_BONUS))
				dam = dam * (100 + ecs::PointSystem::Get(attacker, POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;
#ifdef ENABLE_MEDI_PVM
			if (!ecs::PlayerRuntime::IsPC(victim))
				dam = dam * (100 + ecs::PointSystem::Get(attacker, POINT_ATTBONUS_MEDI_PVM)) / 100;
#endif
		}

		dam = dam * (100 - std::min((int64_t)99, ecs::PointSystem::Get(victim, POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;
	}
	break;
	case DAMAGE_TYPE_MELEE:
	case DAMAGE_TYPE_RANGE:
	case DAMAGE_TYPE_FIRE:
	case DAMAGE_TYPE_ICE:
	case DAMAGE_TYPE_ELEC:
	case DAMAGE_TYPE_MAGIC:
	{
		if (pkAttacker) {
			const int64_t skillBonus = ecs::PointSystem::Get(attacker, POINT_SKILL_DAMAGE_BONUS);
			if (skillBonus)
				dam = dam * (100 + skillBonus) / 100;
		}

		int64_t def = ecs::PointSystem::Get(victim, POINT_SKILL_DEFEND_BONUS);
		def = std::clamp<int64_t>(def, 0, 100);

		if (pkAttacker && ecs::PlayerRuntime::IsPC(attacker) && ecs::PlayerRuntime::IsPC(victim))
			def = (def * 75 + 50) / 100;

		dam = dam * (100 - def) / 100;


		if (pkAttacker && ecs::PlayerRuntime::IsPC(attacker) && !ecs::PlayerRuntime::IsPC(victim))
		{
			const int64_t normalRef = CalcReferenceBasicHitDamage(attacker, victim);
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
	if (AffectSystem::IsAffectFlag(victim, AFF_MANASHIELD))
	{
		// POINT_MANASHIELD  ۾
		int iDamageSPPart = dam / 3;
		int iDamageToSP = iDamageSPPart * ecs::PointSystem::Get(victim, POINT_MANASHIELD) / 100;
		int iSP = ecs::PlayerRuntime::GetSP(victim);

		// SP
		if (iDamageToSP <= iSP)
		{
			ecs::PointSystem::Change(victim, POINT_SP, -iDamageToSP);
			dam -= iDamageSPPart;
		}
		else
		{
			// ŷ ڶ ǰ  ￩ҋ
			ecs::PointSystem::Change(victim, POINT_SP, -ecs::PlayerRuntime::GetSP(victim));
			dam -= iSP * 100 / std::max(ecs::PointSystem::Get(victim, POINT_MANASHIELD), (int64_t)1);
		}
	}

	//
	// ü   ( )
	//
	//if (ecs::PointSystem::Get(victim, POINT_MALL_DEFBONUS) > 0)
	//{
	//	int64_t dec_dam = std::min((int64_t)200, dam * ecs::PointSystem::Get(victim, POINT_MALL_DEFBONUS) / 100);//razor93
	//	dam -= dec_dam;
	//}

	if (pkAttacker)
	{
		//
		// ü ݷ  ( )
		//
		if (ecs::PointSystem::Get(attacker, POINT_MALL_ATTBONUS) > 0)
		{
			int64_t add_dam = std::min((int64_t)300, dam * ecs::PointSystem::GetLimitPoint(attacker, POINT_MALL_ATTBONUS) / 100);
			dam += add_dam;
		}

		if (ecs::PlayerRuntime::IsPC(attacker))
		{
			int iEmpire = ecs::PlayerRuntime::GetEmpire(attacker);
			int32_t lMapIndex = ecs::PlayerRuntime::GetMapIndex(attacker);
			int iMapEmpire = ecs::GetEmpireFromMap(lMapIndex);

			// ٸ     10%
			if (iEmpire && iMapEmpire && iEmpire != iMapEmpire)
			{
				dam = dam * 9 / 10;
			}

			if (!ecs::PlayerRuntime::IsPC(victim) && ecs::PlayerRuntime::GetMonsterDrainSPPoint(victim))
			{
				int iDrain = ecs::PlayerRuntime::GetMonsterDrainSPPoint(victim);

				if (iDrain <= ecs::PointSystem::Get(attacker, POINT_SP))
					ecs::PointSystem::Change(attacker, POINT_SP, -iDrain);
				else
				{
					int iSP = ecs::PointSystem::Get(attacker, POINT_SP);
					ecs::PointSystem::Change(attacker, POINT_SP, -iSP);
				}
			}

		}
		else if (ecs::PlayerRuntime::IsGuardNPC(attacker))
		{
						if (auto* flags = RuntimeFlags(victim))
				SET_BIT(flags->instantFlag, INSTANT_FLAG_NO_REWARD);
			CombatSystem::Stun(victim);
			return true;
		}
	}
	//puAttr.Pop();

	if (!ecs::PlayerRuntime::GetSectree(victim) || ecs::PlayerRuntime::GetSectree(victim)->IsAttr(ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim), ATTR_BANPK))
		return false;

	if (!ecs::PlayerRuntime::IsPC(victim))
	{
		if (ecs::SocialSystem::GetPartyLeader(victim) != entt::null)
			CombatSystem::SetLastAttacked(ecs::SocialSystem::GetPartyLeader(victim), get_dword_time());
		else
			CombatSystem::SetLastAttacked(victim, get_dword_time());
	}

	if (CombatSystem::IsStun(victim))
	{
		CombatSystem::Dead(victim, attacker);
		return true;
	}

	if (CombatSystem::IsDead(victim))
		return true;

	//    ʵ .
	if (type == DAMAGE_TYPE_POISON)
	{
		if (ecs::PlayerRuntime::GetHP(victim) - dam <= 0)
		{
			dam = ecs::PlayerRuntime::GetHP(victim) - 1;
		}
	}
	// ------------------------
	//  ̾
	// -----------------------
	if (pkAttacker && ecs::PlayerRuntime::IsPC(attacker))
	{
		int iDmgPct = CHARACTER_MANAGER::instance().GetUserDamageRate(attacker);
		dam = dam * iDmgPct / 100;
	}

	// STONE SKIN :
	if (ecs::PlayerRuntime::IsMonster(victim) && CombatSystem::IsStoneSkinner(victim))
	{
		if (ecs::PlayerRuntime::GetHPPct(victim) < (*ecs::PlayerRuntime::GetMobTable(victim)).bStoneSkinPoint)
			dam /= 2;
	}

	//PROF_UNIT puRest1("Rest1");
	if (pkAttacker)
	{
		// DEATH BLOW : Ȯ  4  (!?  ̺Ʈ  ͸ )
		if (ecs::PlayerRuntime::IsMonster(attacker) && CombatSystem::IsDeathBlower(attacker))
		{
			if (CombatSystem::IsDeathBlow(attacker))
			{
				if (number(1, 4) == ecs::PlayerRuntime::GetJob(victim))
				{
					IsDeathBlow = true;
					dam = dam * 4;
				}
			}
		}

		uint8_t damageFlag = 0;

		if (type == DAMAGE_TYPE_POISON)
			damageFlag = DAMAGE_POISON;
		else
			damageFlag = DAMAGE_NORMAL;

		if (IsCritical == true)
			damageFlag |= DAMAGE_CRITICAL;

		if (IsPenetrate == true)
			damageFlag |= DAMAGE_PENETRATE;


		//
		float damMul = CombatSystem::GetDamageMultiplier(victim);
		float tempDam = dam;
		dam = tempDam * damMul + 0.5f;

#ifdef ENABLE_BATTLE_PASS
		if (dam > 0)
		{
			uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(attacker);
			if (bBattlePassId)
			{
				if (ecs::PlayerRuntime::IsPC(victim))
				{
					uint32_t dwMinLevel, dwDamage;
					uint32_t dwLevel = ecs::PointSystem::GetLevel(victim);
					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, PLAYER_DAMAGE, &dwMinLevel, &dwDamage))
					{
						if (!ecs::PlayerRuntime::IsCompletedMission(attacker, PLAYER_DAMAGE))
						{
							uint32_t dwDam = dam;
							if (dwLevel >= dwMinLevel && ecs::PlayerRuntime::GetMissionProgress(victim, PLAYER_DAMAGE, bBattlePassId) < dwDam)
							{
								ecs::PlayerRuntime::UpdateMissionProgress(attacker, PLAYER_DAMAGE, bBattlePassId, dwDam, dwDamage);
							}
						}
					}
				}
				else
				{
					uint32_t dwMonsterVnum, dwDamage;
					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, MONSTER_DAMAGE, &dwMonsterVnum, &dwDamage))
					{
						uint32_t dwRaceNum = ecs::PlayerRuntime::GetRaceNum(victim);
						if (!ecs::PlayerRuntime::IsCompletedMission(attacker, MONSTER_DAMAGE))
						{
							uint32_t dwDam = dam;
							if (dwMonsterVnum == dwRaceNum && ecs::PlayerRuntime::GetMissionProgress(victim, MONSTER_DAMAGE, bBattlePassId) < dwDam)
							{
								ecs::PlayerRuntime::UpdateMissionProgress(attacker, MONSTER_DAMAGE, bBattlePassId, dwDam, dwDamage);
							}
						}
					}
				}
			}
		}
#endif

#if defined(ENABLE_DS_RUNE) || defined(ENABLE_MELEY_LAIR)
		if (!ecs::PlayerRuntime::IsPC(victim) && pkAttacker && ecs::PlayerRuntime::IsPC(attacker))
		{
			int32_t racevnum = ecs::PlayerRuntime::GetRaceNum(victim);
			LPDUNGEON dungeon = ecs::SocialSystem::GetDungeon(victim);
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
							int32_t per = (ecs::PointSystem::GetMaxHP(victim) / 100) * 60;
							if (ecs::PlayerRuntime::GetHP(victim) - dam <= per)
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

								if (ecs::PlayerRuntime::GetHP(victim) > per)
								{
									ecs::PointSystem::Change(victim, POINT_HP, -(ecs::PlayerRuntime::GetHP(victim) - per), false);
								}
								else
								{
									ecs::PointSystem::Change(victim, POINT_HP, (per - ecs::PlayerRuntime::GetHP(victim)), false);
								}

								CombatSystem::SetInvincible(victim, true);
								return false;
							}
						}
						else if (step == 2)
						{
							int32_t per = (ecs::PointSystem::GetMaxHP(victim) / 100) * 20;
							if (ecs::PlayerRuntime::GetHP(victim) - dam <= per)
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

								if (ecs::PlayerRuntime::GetHP(victim) > per)
								{
									ecs::PointSystem::Change(victim, POINT_HP, -(ecs::PlayerRuntime::GetHP(victim) - per), false);
								}
								else
								{
									ecs::PointSystem::Change(victim, POINT_HP, (per - ecs::PlayerRuntime::GetHP(victim)), false);
								}

								CombatSystem::SetAttackMultiplier(victim, 2.0f);
								CombatSystem::SetDamageMultiplier(victim, 2.0f);
								CombatSystem::SetInvincible(victim, true);
								return false;
							}
						}
					}
					else if (type == 3 && step == 0)
					{
						LPPARTY party = ecs::SocialSystem::GetParty(attacker);
						if (party)
						{
							if (party->GetLeaderPID() == ecs::PlayerRuntime::GetPlayerID(attacker))
							{
								int32_t per = (ecs::PointSystem::GetMaxHP(victim) / 100) * 70;
								if (ecs::PlayerRuntime::GetHP(victim) - dam <= per)
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
							int32_t per = (ecs::PointSystem::GetMaxHP(victim) / 100) * 50;
							if (ecs::PlayerRuntime::GetHP(victim) - dam <= per)
							{
								dungeon->SetFlag("step", 1);
								dungeon->SpawnRegen("data/dungeon/rune/regen8.txt");

								dungeon->Notice(907, "");
								dungeon->Notice(906, "");

								if (ecs::PlayerRuntime::GetHP(victim) > per)
								{
									ecs::PointSystem::Change(victim, POINT_HP, -(ecs::PlayerRuntime::GetHP(victim) - per), false);
								}
								else
								{
									ecs::PointSystem::Change(victim, POINT_HP, (per - ecs::PlayerRuntime::GetHP(victim)), false);
								}

								CombatSystem::IncreaseMobRigHP(victim, 20);
								CombatSystem::SetInvincible(victim, true);
								return false;
							}
						}
						else if (step == 2)
						{
							int32_t per = (ecs::PointSystem::GetMaxHP(victim) / 100) * 10;
							if (ecs::PlayerRuntime::GetHP(victim) - dam <= per)
							{
								dungeon->SetFlag("step", 3);
								dungeon->SpawnRegen("data/dungeon/rune/regen9.txt");

								dungeon->Notice(905, "");
								dungeon->Notice(906, "");

								if (ecs::PlayerRuntime::GetHP(victim) > per)
								{
									ecs::PointSystem::Change(victim, POINT_HP, -(ecs::PlayerRuntime::GetHP(victim) - per), false);
								}
								else
								{
									ecs::PointSystem::Change(victim, POINT_HP, (per - ecs::PlayerRuntime::GetHP(victim)), false);
								}

								CombatSystem::SetAttackMultiplier(victim, 2.0f);
								CombatSystem::SetDamageMultiplier(victim, 2.0f);
								CombatSystem::SetInvincible(victim, true);
								return false;
							}
						}
					}

					if (itakehp != 0)
					{
						ecs::PointSystem::Change(victim, POINT_HP, -itakehp);
					}
				}
#endif
#if defined(ENABLE_MELEY_LAIR)
				if (racevnum == 6118)
				{
					int32_t vid = ecs::PlayerRuntime::GetPacketVID(victim);
					if (vid == dungeon->GetFlag("statue_vid1") || vid == dungeon->GetFlag("statue_vid2") || vid == dungeon->GetFlag("statue_vid3") || vid == dungeon->GetFlag("statue_vid4"))
					{
						int32_t floor = dungeon->GetFlag("floor");
						if (floor >= 1 && floor < 5)
						{
							int32_t per = (ecs::PointSystem::GetMaxHP(victim) / 100) * 75;
							if (ecs::PlayerRuntime::GetHP(victim) - dam <= per)
							{
								dungeon->SetFlag("floor", floor + 1);

								if (ecs::PlayerRuntime::GetHP(victim) > per)
								{
									ecs::PointSystem::Change(victim, POINT_HP, -(ecs::PlayerRuntime::GetHP(victim) - per), false);
								}
								else
								{
									ecs::PointSystem::Change(victim, POINT_HP, (per - ecs::PlayerRuntime::GetHP(victim)), false);
								}

								CombatSystem::SetInvincible(victim, true);

								if (!AffectSystem::FindAffect(victim, AFFECT_STATUE))
								{
									AffectSystem::AddAffect(victim, AFFECT_STATUE, POINT_NONE, 0, AFF_STATUE1, 3600, 0, true);
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
							int32_t per = (ecs::PointSystem::GetMaxHP(victim) / 100) * 50;
							if (ecs::PlayerRuntime::GetHP(victim) - dam <= per)
							{
								dungeon->SetFlag("floor", floor + 1);

								if (ecs::PlayerRuntime::GetHP(victim) > per)
								{
									ecs::PointSystem::Change(victim, POINT_HP, -(ecs::PlayerRuntime::GetHP(victim) - per), false);
								}
								else
								{
									ecs::PointSystem::Change(victim, POINT_HP, (per - ecs::PlayerRuntime::GetHP(victim)), false);
								}

								CombatSystem::SetInvincible(victim, true);

								if (!AffectSystem::FindAffect(victim, AFFECT_STATUE))
								{
									AffectSystem::AddAffect(victim, AFFECT_STATUE, POINT_NONE, 0, AFF_STATUE2, 3600, 0, true);
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
							int32_t per = (ecs::PointSystem::GetMaxHP(victim) / 100) * 5;
							if (ecs::PlayerRuntime::GetHP(victim) - dam <= per)
							{
								dungeon->SetFlag("floor", floor + 1);

								if (ecs::PlayerRuntime::GetHP(victim) > per)
								{
									ecs::PointSystem::Change(victim, POINT_HP, -(ecs::PlayerRuntime::GetHP(victim) - per), false);
								}
								else
								{
									ecs::PointSystem::Change(victim, POINT_HP, (per - ecs::PlayerRuntime::GetHP(victim)), false);
								}

								CombatSystem::SetInvincible(victim, true);

								if (!AffectSystem::FindAffect(victim, AFFECT_STATUE))
								{
									AffectSystem::AddAffect(victim, AFFECT_STATUE, POINT_NONE, 0, AFF_STATUE3, 3600, 0, true);
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
						ecs::PointSystem::Change(victim, POINT_HP, -itakehp);
					}
				}
#endif
			}
		}
#endif

		if (pkAttacker)
			CombatSystem::SendDamagePacket(victim, attacker, dam, damageFlag);
#ifdef LEADERBOARD_RAZOR93

		if (pkAttacker && ecs::PlayerRuntime::IsPC(attacker) && CombatSystem::IsSkillHit(attacker) && ecs::PlayerRuntime::IsPC(victim))
		{
			char szVictimEsc[CHARACTER_NAME_MAX_LEN * 2 + 1];
			DBManager::instance().EscapeString(szVictimEsc, sizeof(szVictimEsc), ecs::PlayerRuntime::GetName(victim).data(),
				strnlen(ecs::PlayerRuntime::GetName(victim).data(), CHARACTER_NAME_MAX_LEN));

			DBManager::instance().DirectQuery(
				"UPDATE player.player "
				"SET "
				"    skill_victim = IF(%d > map1_skillmob, '%s', skill_victim), "
				"    map1_skillmob = GREATEST(map1_skillmob, %d) "
				"WHERE id = %d",
				dam,
				szVictimEsc,
				dam,
				ecs::PlayerRuntime::GetPlayerID(attacker)
			);
			CheckLeaderboardSkillMobChanges();
			if (ecs::PlayerRuntime::GetMapIndex(victim) == 41) {
				CHARACTER_MANAGER::instance().for_each_pc([](LegacyCharHandle ch) {
					CombatSystem::SendLeaderboardDataSkillMob(ch->GetEntityHandle(), (ch ? ch->GetEntityHandle() : entt::null));
					});


			}


			ecs::ChatSystem::Send(attacker, CHAT_TYPE_INFO, "Skill damage recorded: %d vs %s", dam, ecs::PlayerRuntime::GetName(victim));
		}
#endif
		if (test_server)
		{
			int iTmpPercent = 0; // @fixme136
			if (ecs::PointSystem::GetMaxHP(victim) >= 0)
				iTmpPercent = (ecs::PlayerRuntime::GetHP(victim) * 100) / ecs::PointSystem::GetMaxHP(victim);

			if (pkAttacker)
			{
				ecs::ChatSystem::Send(attacker, CHAT_TYPE_INFO, "-> %s, DAM %d HP %d(%d%%) %s%s",
					ecs::PlayerRuntime::GetName(victim),
					dam,
					ecs::PlayerRuntime::GetHP(victim),
					iTmpPercent,
					IsCritical ? "crit " : "",
					IsPenetrate ? "pene " : "",
					IsDeathBlow ? "deathblow " : "");
			}

			ecs::ChatSystem::Send(victim, CHAT_TYPE_PARTY, "<- %s, DAM %d HP %d(%d%%) %s%s",
				pkAttacker ? ecs::PlayerRuntime::GetName(attacker).data() : nullptr,
				dam,
				ecs::PlayerRuntime::GetHP(victim),
				iTmpPercent,
				IsCritical ? "crit " : "",
				IsPenetrate ? "pene " : "",
				IsDeathBlow ? "deathblow " : "");
		}

#ifdef ENABLE_RANKING
		if (ecs::PlayerRuntime::IsPC(attacker)) {
			if (ecs::PlayerRuntime::IsPC(victim)) {
				switch (type) {
				case DAMAGE_TYPE_NORMAL:
				case DAMAGE_TYPE_NORMAL_RANGE: {
					if (dam > ecs::PlayerRuntime::GetRankPoints(attacker, 3))
						ecs::PlayerRuntime::SetRankPoints(attacker, 3, dam);
				}
											 break;
				case DAMAGE_TYPE_MELEE:
				case DAMAGE_TYPE_RANGE:
				case DAMAGE_TYPE_FIRE:
				case DAMAGE_TYPE_ICE:
				case DAMAGE_TYPE_ELEC:
				case DAMAGE_TYPE_MAGIC: {
					if (dam > ecs::PlayerRuntime::GetRankPoints(attacker, 4))
						ecs::PlayerRuntime::SetRankPoints(attacker, 4, dam);
				}
									  break;
				default:
					break;
				}
			}
			else if (ecs::PlayerRuntime::IsMonster(victim)) {
				if (ecs::PlayerRuntime::GetMobRank(victim) >= MOB_RANK_BOSS) {
					switch (type) {
					case DAMAGE_TYPE_NORMAL:
					case DAMAGE_TYPE_NORMAL_RANGE: {
						if (dam > ecs::PlayerRuntime::GetRankPoints(attacker, 8))
							ecs::PlayerRuntime::SetRankPoints(attacker, 8, dam);
					}
												 break;
					case DAMAGE_TYPE_MELEE:
					case DAMAGE_TYPE_RANGE:
					case DAMAGE_TYPE_FIRE:
					case DAMAGE_TYPE_ICE:
					case DAMAGE_TYPE_ELEC:
					case DAMAGE_TYPE_MAGIC: {
						if (dam > ecs::PlayerRuntime::GetRankPoints(attacker, 9))
							ecs::PlayerRuntime::SetRankPoints(attacker, 9, dam);
					}
										  break;
					default:
						break;
					}
				}
				else if (!ecs::PlayerRuntime::IsStone(victim)) {
					switch (type) {
					case DAMAGE_TYPE_NORMAL:
					case DAMAGE_TYPE_NORMAL_RANGE: {
						if (dam > ecs::PlayerRuntime::GetRankPoints(attacker, 18))
							ecs::PlayerRuntime::SetRankPoints(attacker, 18, dam);
					}
												 break;
					case DAMAGE_TYPE_MELEE:
					case DAMAGE_TYPE_RANGE:
					case DAMAGE_TYPE_FIRE:
					case DAMAGE_TYPE_ICE:
					case DAMAGE_TYPE_ELEC:
					case DAMAGE_TYPE_MAGIC: {
						if (dam > ecs::PlayerRuntime::GetRankPoints(attacker, 19))
							ecs::PlayerRuntime::SetRankPoints(attacker, 19, dam);
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
	if (!IsUndying(victim))
	{
#ifdef __DUNGEON_INFO_SYSTEM__
		if (!ecs::PlayerRuntime::IsPC(victim) && pkAttacker && ecs::PlayerRuntime::IsPC(attacker))
		{
			pkAttacker->SetQuestDamage(ecs::PlayerRuntime::GetRaceNum(victim), dam);
			ecs::PlayerRuntime::SetQuestNPCID(attacker, ecs::PlayerRuntime::GetPacketVID(victim));
			quest::CQuestManager::instance().QuestDamage(ecs::PlayerRuntime::GetPlayerID(attacker), ecs::PlayerRuntime::GetRaceNum(victim));
		}
#endif

		if (ecs::PlayerRuntime::GetHP(victim) - dam <= 0) // @fixme137
			dam = ecs::PlayerRuntime::GetHP(victim);

		ecs::PointSystem::Change(victim, POINT_HP, -dam, false);
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
		if (ecs::PlayerRuntime::IsStone(victim))
			ProcessStoneSpawnStep(victim);
#endif
	}

	//puRest1.Pop();

	//PROF_UNIT puRest2("Rest2");
	if (pkAttacker && dam > 0 && !ecs::PlayerRuntime::IsPC(victim))
	{
		//PROF_UNIT puRest20("Rest20");
		const entt::entity eAttacker = attacker;
		std::map<entt::entity, ecs::BattleContribution>::iterator it = CombatSystem::DamageLedgerOf(victim).entries.end();
		if (eAttacker != entt::null)
		{
			it = CombatSystem::DamageLedgerOf(victim).entries.find(eAttacker);

			if (it == CombatSystem::DamageLedgerOf(victim).entries.end())
			{
				CombatSystem::DamageLedgerOf(victim).entries.insert(
					std::map<entt::entity, ecs::BattleContribution>::value_type(eAttacker, ecs::BattleContribution(dam, 0)));
				it = CombatSystem::DamageLedgerOf(victim).entries.find(eAttacker);
			}
			else
			{
				it->second.totalDamage += dam;
			}
		}
		//puRest20.Pop();

		//PROF_UNIT puRest21("Rest21");
#ifdef __DEFENSE_WAVE__
		if (ecs::PlayerRuntime::GetRaceNum(victim) != 20434)
		{
			ecs::PlayerRuntime::StartRecoveryEvent(victim);
		}
#else
		ecs::PlayerRuntime::StartRecoveryEvent(victim);
#endif
		//puRest21.Pop();

		//PROF_UNIT puRest22("Rest22");
		if (it != CombatSystem::DamageLedgerOf(victim).entries.end())
			CombatSystem::UpdateAggrPointEx(victim, attacker, type, dam, it->second);
		//puRest22.Pop();
	}
	//puRest2.Pop();

	//PROF_UNIT puRest3("Rest3");

#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
	if (ecs::PlayerRuntime::GetHP(victim) <= 0)
	{
		if (pkAttacker && !ecs::PlayerRuntime::IsNPC(attacker))
			SetKillerPID(victim, ecs::PlayerRuntime::GetPlayerID(attacker));
		else
			SetKillerPID(victim, 0);

		if (!ecs::PlayerRuntime::IsPC(victim))
		{
			CombatSystem::Dead(victim, attacker, true);
			return true;
		}


		CombatSystem::Stun(victim);
	}

#else

	if (ecs::PlayerRuntime::GetHP(victim) <= 0)
	{
		CombatSystem::Stun(victim);

		if (pkAttacker && !ecs::PlayerRuntime::IsNPC(attacker))
			SetKillerPID(victim, ecs::PlayerRuntime::GetPlayerID(attacker));
		else
			SetKillerPID(victim, 0);
	}
#endif
#ifdef __DEFENSE_WAVE__
	if (ecs::PlayerRuntime::GetRaceNum(victim) == 20434)
	{
		LPDUNGEON dungeon = ecs::SocialSystem::GetDungeon(victim);
		if (dungeon)
		{
			dungeon->UpdateMastHP();
			// A dungeon with no registered mast dereferenced null here; a destroyed
			// one reads no health and counts as fallen.
			const entt::entity mast = dungeon->GetMast();
			if (mast != entt::null && ecs::PlayerRuntime::GetHP(mast) <= 0)
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
} // namespace CombatSystem
#endif
static int64_t CalcReferenceBowHitDamage(entt::entity attacker, entt::entity victim);
static int64_t CalcReferenceBasicHitDamage(entt::entity attacker, entt::entity victim);
static int64_t CalcReferenceNormalHitDamage(entt::entity attacker, entt::entity victim);

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

namespace CombatSystem {

// The arrow and the bow a shot will spend, and the spending of them.
int GetArrowAndBow(entt::entity e, entt::entity* ppkBow, entt::entity* ppkArrow, int iArrowCount)
{
	const entt::entity bow = ItemSystem::GetWearItem(e, WEAR_WEAPON);
	if (!ItemSystem::IsValidItem(bow))
	{
		return 0;
	}

	const TItemTable* bowProto = ItemSystem::GetItemProto(bow);
	if (!bowProto || bowProto->bSubType != WEAPON_BOW)
	{
		return 0;
	}

	const entt::entity arrow = ItemSystem::GetWearItem(e, WEAR_ARROW);
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

void UseArrow(entt::entity e, entt::entity pkArrow, uint32_t dwArrowCount)
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
			e, dwVnum
#ifdef ENABLE_EXTRA_INVENTORY
			, false
#endif
		);

		LOG_INFO("UseArrow : FindSpecifyItem {} entity {}", dwVnum,
			static_cast<uint32_t>(newArrow));

		if (ItemSystem::IsValidItem(newArrow))
			ItemSystem::EquipItemEcs(e, newArrow);
	}
}

} // namespace CombatSystem

class CFuncShoot
{
public:
	entt::entity	m_me;
	uint8_t		m_bType;
	bool		m_bSucceed;

	CFuncShoot(entt::entity shooter, uint8_t bType) : m_me(shooter), m_bType(bType), m_bSucceed(false)
	{
	}

	// ComputeSkill and GetSoulItemDamage have no entity form yet; each is its
	// own migration and they share this one resolve.
	LPCHARACTER Self() const { return ecs::LegacyCharOf(m_me); }

	void operator () (uint32_t dwTargetVID)
	{
		const entt::entity me = m_me;
		if (m_bType > 1)
		{
			if (g_bSkillDisable)
				return;


		}

		auto* pkVictim = CHARACTER_MANAGER::instance().Find(dwTargetVID);
		const entt::entity victim = pkVictim ? pkVictim->GetEntityHandle() : entt::null;


		if (!pkVictim)
			return;

		if (m_bType > 1)
			SkillSystem::SetSkillMainTarget(me, m_bType, victim);

		//  Ұ
		if (!battle_is_attackable(me, victim))
			return;

		if (ecs::PlayerRuntime::IsNPC(me))
		{
			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(me) - ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(me) - ecs::PlayerRuntime::GetY(victim)) > 5000)
				return;
		}

		entt::entity pkBow = entt::null, pkArrow = entt::null;

		switch (m_bType)
		{
		case 0: // ϹȰ
		{
			int iDam = 0;

			if (ecs::PlayerRuntime::IsPC(me))
			{
				if (ecs::PlayerRuntime::GetJob(me) != JOB_ASSASSIN)
					return;

				if (0 == CombatSystem::GetArrowAndBow(me, &pkBow, &pkArrow))
					return;

				if (SkillSystem::GetSkillGroup(me) != 0)
					if (!ecs::PlayerRuntime::IsNPC(me) && SkillSystem::GetSkillGroup(me) != 2)
					{
						if (ecs::PlayerRuntime::GetSP(me) < 5)
							return;

						ecs::PointSystem::Change(me, POINT_SP, -5);
					}

				iDam = CalcArrowDamage(me, victim, pkBow, pkArrow);
				CombatSystem::UseArrow(me, pkArrow, 1);

#ifdef ENABLE_ANTICHEAT
				if (IS_SPEED_HACK(me, victim, get_dword_time())) {
					iDam = 0;
				}
#endif
			}
			else
				iDam = CalcMeleeDamage(me, victim);

			NormalAttackAffect(me, victim);

			//   (nyl vdelem)
			int32_t lValue = ecs::PointSystem::Get(victim, POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get(me, POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get(me, POINT_IRR_WEAPON_DEFENSE);
#endif

			if (lValue < 0)   lValue = 0;
			if (lValue > 100) lValue = 100;

			iDam = (int)((int64_t)iDam * (100 - lValue) / 100);
			//iDam = (int)((int64_t)iDam * (100 - lValue) * 20 / 10000);

#ifdef ENABLE_SOUL_SYSTEM // Arrow ninja
			iDam += Self()->GetSoulItemDamage(victim, iDam, RED_SOUL);
#endif

			//LOG_INFO(0, "%s arrow %s dam %d", ecs::PlayerRuntime::GetName(me).data(), ecs::PlayerRuntime::GetName(victim).data(), iDam);

			ecs::MovementSystem::OnMove(me, true);
			ecs::MovementSystem::OnMove(victim);

			if (CombatSystem::CanBeginFight(victim))
				CombatSystem::BeginFight(victim, me);

			CombatSystem::Damage(victim, me, iDam, DAMAGE_TYPE_NORMAL_RANGE);
			// Ÿġ
		}
		break;


		case 1: // Ϲ
		{
			int iDam;

			if (ecs::PlayerRuntime::IsPC(me))
				return;

			iDam = CalcMagicDamage(me, victim);

			NormalAttackAffect(me, victim);

			//
//#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
//						const int resist_magic = MINMAX(0, ecs::PointSystem::Get(victim, POINT_RESIST_MAGIC), 100);
//						const int resist_magic_reduction = MINMAX(0, (ecs::PlayerRuntime::GetJob(me)==JOB_SURA) ? ecs::PointSystem::Get(me, POINT_RESIST_MAGIC_REDUCTION)/2 : ecs::PointSystem::Get(me, POINT_RESIST_MAGIC_REDUCTION), 50);
//						const int total_res_magic = MINMAX(0, resist_magic - resist_magic_reduction, 100);
//						iDam = iDam * (100 - total_res_magic) / 100;
//#else
			iDam = iDam * (100 - (int)(ecs::PointSystem::Get(victim, POINT_RESIST_MAGIC) / 2)) / 100;
			//#endif

									//LOG_INFO(0, "%s arrow %s dam %d", ecs::PlayerRuntime::GetName(me).data(), ecs::PlayerRuntime::GetName(victim).data(), iDam);

			ecs::MovementSystem::OnMove(me, true);
			ecs::MovementSystem::OnMove(victim);

			if (CombatSystem::CanBeginFight(victim))
				CombatSystem::BeginFight(victim, me);

			CombatSystem::Damage(victim, me, iDam, DAMAGE_TYPE_MAGIC);
			// Ÿġ
		}
		break;

		case SKILL_YEONSA:	//
		{
			//int iUseArrow = 2 + (SkillSystem::GetSkillPower(me, SKILL_YEONSA) *6/100);
			int iUseArrow = 1;

			// Ż ϴ°
			{
				if (iUseArrow == CombatSystem::GetArrowAndBow(me, &pkBow, &pkArrow, iUseArrow))
				{
					ecs::MovementSystem::OnMove(me, true);
					ecs::MovementSystem::OnMove(victim);

					if (CombatSystem::CanBeginFight(victim))
						CombatSystem::BeginFight(victim, me);

					Self()->ComputeSkill(m_bType, victim);
					CombatSystem::UseArrow(me, pkArrow, iUseArrow);

					if (CombatSystem::IsDead(victim))
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

			if (iUseArrow == CombatSystem::GetArrowAndBow(me, &pkBow, &pkArrow, iUseArrow))
			{
				ecs::MovementSystem::OnMove(me, true);
				ecs::MovementSystem::OnMove(victim);

				if (CombatSystem::CanBeginFight(victim))
					CombatSystem::BeginFight(victim, me);

				LOG_INFO("{} kwankeyok {}", ecs::PlayerRuntime::GetName(me).data(), ecs::PlayerRuntime::GetName(victim).data());
				Self()->ComputeSkill(m_bType, victim);
				CombatSystem::UseArrow(me, pkArrow, iUseArrow);
			}
		}
		break;

		case SKILL_GIGUNG:
		{
			int iUseArrow = 1;
			if (iUseArrow == CombatSystem::GetArrowAndBow(me, &pkBow, &pkArrow, iUseArrow))
			{
				ecs::MovementSystem::OnMove(me, true);
				ecs::MovementSystem::OnMove(victim);

				if (CombatSystem::CanBeginFight(victim))
					CombatSystem::BeginFight(victim, me);

				LOG_INFO("{} gigung {}", ecs::PlayerRuntime::GetName(me).data(), ecs::PlayerRuntime::GetName(victim).data());
				Self()->ComputeSkill(m_bType, victim);
				CombatSystem::UseArrow(me, pkArrow, iUseArrow);
			}
		}

		break;
		case SKILL_HWAJO:
		{
			int iUseArrow = 1;
			if (iUseArrow == CombatSystem::GetArrowAndBow(me, &pkBow, &pkArrow, iUseArrow))
			{
				ecs::MovementSystem::OnMove(me, true);
				ecs::MovementSystem::OnMove(victim);

				if (CombatSystem::CanBeginFight(victim))
					CombatSystem::BeginFight(victim, me);

				LOG_INFO("{} hwajo {}", ecs::PlayerRuntime::GetName(me).data(), ecs::PlayerRuntime::GetName(victim).data());
				Self()->ComputeSkill(m_bType, victim);
				CombatSystem::UseArrow(me, pkArrow, iUseArrow);
			}
		}

		break;

		case SKILL_HORSE_WILDATTACK_RANGE:
		{
			int iUseArrow = 1;
			if (iUseArrow == CombatSystem::GetArrowAndBow(me, &pkBow, &pkArrow, iUseArrow))
			{
				ecs::MovementSystem::OnMove(me, true);
				ecs::MovementSystem::OnMove(victim);

				if (CombatSystem::CanBeginFight(victim))
					CombatSystem::BeginFight(victim, me);

				LOG_TRACE("{} horse_wildattack {}", ecs::PlayerRuntime::GetName(me).data(), ecs::PlayerRuntime::GetName(victim).data());
				Self()->ComputeSkill(m_bType, victim);
				CombatSystem::UseArrow(me, pkArrow, iUseArrow);
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
			ecs::MovementSystem::OnMove(me, true);
			ecs::MovementSystem::OnMove(victim);

			if (CombatSystem::CanBeginFight(victim))
				CombatSystem::BeginFight(victim, me);

			LOG_INFO("{} - Skill {} -> {}", ecs::PlayerRuntime::GetName(me).data(), m_bType, ecs::PlayerRuntime::GetName(victim).data());
			Self()->ComputeSkill(m_bType, victim);
		}
		break;

		case SKILL_CHAIN:
		{
			ecs::MovementSystem::OnMove(me, true);
			ecs::MovementSystem::OnMove(victim);

			if (CombatSystem::CanBeginFight(victim))
				CombatSystem::BeginFight(victim, me);

			LOG_INFO("{} - Skill {} -> {}", ecs::PlayerRuntime::GetName(me).data(), m_bType, ecs::PlayerRuntime::GetName(victim).data());
			Self()->ComputeSkill(m_bType, victim);

			// TODO     ϱ
		}
		break;
#ifndef ENABLE_BUG_FIXES
		case SKILL_YONGBI:
		{
			ecs::MovementSystem::OnMove(me, true);
		}
		break;
#endif
		/*case SKILL_BUDONG:
		  {
		  ecs::MovementSystem::OnMove(me, true);
		  ecs::MovementSystem::OnMove(victim);

		  uint32_t * pdw;
		  uint32_t dwEI = AllocEventInfo(sizeof(uint32_t) * 2, &pdw);
		  pdw[0] = ecs::PlayerRuntime::GetPacketVID(me);
		  pdw[1] = ecs::PlayerRuntime::GetPacketVID(victim);

		  event_create(budong_event_func, dwEI, PASSES_PER_SEC(1));
		  }
		  break;*/

		default:
			LOG_ERROR("CFuncShoot: I don't know this type [{}] of range attack.", (int)m_bType);
			break;
#ifdef ENABLE_NINJA_SANGONG_X30_RAZOR93
		case SKILL_SANGONG:
		{
			if (ecs::PlayerRuntime::IsStone(victim) || ecs::PlayerRuntime::GetMobRank(victim) >= 4 || ecs::PlayerRuntime::GetRaceNum(victim))
			{
				int iDam = CalcMeleeDamage(me, victim);

				if (ecs::PlayerRuntime::GetJob(me) == JOB_ASSASSIN &&
					(ecs::PlayerRuntime::IsStone(victim) || ecs::PlayerRuntime::GetMobRank(victim) >= 4 || ecs::PlayerRuntime::GetRaceNum(victim) == 136))
				{
					int multiplier = 36; // alap multiplier


					if (ecs::PlayerRuntime::GetRaceNum(victim) == 331)
					{
						iDam = 0;
					}
					else
					{

						if (ecs::PlayerRuntime::GetRaceNum(victim) == 8055)
						{
							multiplier = 34;
						}
						else if (ecs::PlayerRuntime::GetRaceNum(victim) == 6193)
						{
							multiplier = 20;
						}
						else if (ecs::PlayerRuntime::GetRaceNum(victim) == 8010 ||
							ecs::PlayerRuntime::GetRaceNum(victim) == 8020 ||
							ecs::PlayerRuntime::GetRaceNum(victim) == 180 ||
							ecs::PlayerRuntime::GetRaceNum(victim) == 181 ||
							ecs::PlayerRuntime::GetRaceNum(victim) == 182)
						{
							multiplier = 20;
						}
						else if (ecs::PlayerRuntime::GetRaceNum(victim) == 180 ||
							ecs::PlayerRuntime::GetRaceNum(victim) == 181 ||
							ecs::PlayerRuntime::GetRaceNum(victim) == 182
							)
						{
							multiplier = 100;
						}
						else if (ecs::PlayerRuntime::GetRaceNum(victim) == 4582 ||
							ecs::PlayerRuntime::GetRaceNum(victim) == 4583 ||
							ecs::PlayerRuntime::GetRaceNum(victim) == 4584
							)
						{
							multiplier = 80	;
						}
						iDam *= multiplier;

					}
				}

				CombatSystem::Damage(victim, me, iDam, DAMAGE_TYPE_NORMAL);


				if (ecs::PlayerRuntime::IsPC(victim))
				{
					ecs::MovementSystem::OnMove(me, true);
					ecs::MovementSystem::OnMove(victim);

					if (CombatSystem::CanBeginFight(victim))
						CombatSystem::BeginFight(victim, me);

					LOG_INFO("{} - Skill {} -> {}", ecs::PlayerRuntime::GetName(me).data(), m_bType, ecs::PlayerRuntime::GetName(victim).data());
					Self()->ComputeSkill(m_bType, victim);
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

#ifdef LEADERBOARD_RAZOR93
#endif


// char_battle.cpp slice BB1 moved into CombatSystem.cpp

EVENTFUNC(StunEvent)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);

	if (info == nullptr)
	{
		LOG_ERROR("StunEvent> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = ecs::LegacyCharOf(info->ch);

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	const entt::entity e = info->ch;
	ecs::PlayerRuntime::SetCharEvent(e, ecs::PlayerRuntime::CharEvent::Stun, nullptr);
	if (e != entt::null && g_registry.valid(e))
	{
		if (g_registry.all_of<ecs::StunTag>(e))
			g_registry.remove<ecs::StunTag>(e);
		if (auto* status = g_registry.try_get<ecs::StatusFlags>(e))
			status->isStunned = false;
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		g_dispatcher.trigger(ecs::EvStunBegin { e, 3000u });
	}
	CombatSystem::Dead(e);
	return 0;
}

namespace CombatSystem {

// Loading a shot, and letting it go.
void FlyTarget(entt::entity e, uint32_t dwTargetVID, int32_t x, int32_t y, uint8_t bHeader)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	const entt::entity pkVictim = CHARACTER_MANAGER::instance().FindEntity(dwTargetVID);
	TPacketGCFlyTargeting pack;

	//pack.bHeader	= HEADER_GC_FLY_TARGETING;
	pack.bHeader = (bHeader == HEADER_CG_FLY_TARGETING) ? HEADER_GC_FLY_TARGETING : HEADER_GC_ADD_FLY_TARGETING;
	pack.dwShooterVID = ecs::PlayerRuntime::GetPacketVID(e);

	if (pkVictim != entt::null)
	{
		pack.dwTargetVID = ecs::PlayerRuntime::GetPacketVID(pkVictim);
		pack.x = ecs::PlayerRuntime::GetX(pkVictim);
		pack.y = ecs::PlayerRuntime::GetY(pkVictim);

		auto& targets = g_registry.get_or_emplace<ecs::FlyTargets>(e);
		if (bHeader == HEADER_CG_FLY_TARGETING)
			targets.primary = dwTargetVID;
		else
			targets.list.push_back(dwTargetVID);
	}
	else
	{
		pack.dwTargetVID = 0;
		pack.x = x;
		pack.y = y;
	}

	LOG_INFO("FlyTarget {} vid {} x {} y {}", ecs::PlayerRuntime::GetName(e).data(), pack.dwTargetVID, pack.x, pack.y);
	ecs::ViewSystem::PacketView(e, &pack, sizeof(pack), e);
}

bool Shoot(entt::entity e, uint8_t bType)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;

	auto& targets = g_registry.get_or_emplace<ecs::FlyTargets>(e);
	LOG_INFO("Shoot {} type {} flyTargets.size {}", ecs::PlayerRuntime::GetName(e).data(), bType, targets.list.size());

	if (!ecs::MovementSystem::CanMove(e))
	{
		return false;
	}

	CFuncShoot f(e, bType);

	if (targets.primary != 0)
	{
		const uint32_t single = targets.primary;
		targets.primary = 0;
		f(single);
	}

	// The functor can retire the shooter, and with it the component.
	std::vector<uint32_t> queued;
	if (auto* still = g_registry.valid(e) ? g_registry.try_get<ecs::FlyTargets>(e) : nullptr)
		queued.swap(still->list);

	f = std::for_each(queued.begin(), queued.end(), f);

	return f.m_bSucceed;
}

} // namespace CombatSystem

namespace CombatSystem {

void DetermineDropMetinStone(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& drop = g_registry.get_or_emplace<ecs::MetinStoneDrop>(e);

#ifdef ENABLE_NEWSTUFF
	if (g_NoDropMetinStone)
	{
		drop.vnum = 0;
		return;
	}
#endif

	static const uint32_t c_adwMetin[] =
	{
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
	uint32_t stone_num = ecs::PlayerRuntime::GetRaceNum(e);
	int idx = std::lower_bound(aStoneDrop, aStoneDrop + STONE_INFO_MAX_NUM, stone_num) - aStoneDrop;
	if (idx >= STONE_INFO_MAX_NUM || aStoneDrop[idx].dwMobVnum != stone_num)
	{
		drop.vnum = 0;
	}
	else
	{
		const SStoneDropInfo& info = aStoneDrop[idx];
		drop.pct = info.iDropPct;
		{
			drop.vnum = c_adwMetin[number(0, sizeof(c_adwMetin) / sizeof(uint32_t) - 1)];
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
					drop.vnum += 100;
				}
			}
		}
	}
}

uint32_t GetDropMetinStoneVnum(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	const auto* drop = g_registry.try_get<ecs::MetinStoneDrop>(e);
	return drop ? drop->vnum : 0;
}

uint8_t GetDropMetinStonePct(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	const auto* drop = g_registry.try_get<ecs::MetinStoneDrop>(e);
	return drop ? drop->pct : 0;
}

} // namespace CombatSystem


struct FuncSetLastAttacked
{
	FuncSetLastAttacked(uint32_t dwTime) : m_dwTime(dwTime)
	{
	}

	void operator () (LegacyCharHandle ch)
	{
		CombatSystem::SetLastAttacked(ch->GetEntityHandle(), m_dwTime);
	}

	uint32_t m_dwTime;
};

#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93


#endif

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

static int64_t CalcReferenceNormalHitDamage(entt::entity attacker, entt::entity victim);
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
static void ProcessStoneSpawnStep(entt::entity stone)
{
	if (stone == entt::null || !g_registry.valid(stone) || !ecs::PlayerRuntime::IsStone(stone) || ecs::PointSystem::GetMaxHP(stone) <= 0)
		return;

	const int iPercent = (ecs::PlayerRuntime::GetHP(stone) * 100) / ecs::PointSystem::GetMaxHP(stone);
	const uint32_t dwVnum = number(
		MIN((*ecs::PlayerRuntime::GetMobTable(stone)).sAttackSpeed, (*ecs::PlayerRuntime::GetMobTable(stone)).sMovingSpeed),
		MAX((*ecs::PlayerRuntime::GetMobTable(stone)).sAttackSpeed, (*ecs::PlayerRuntime::GetMobTable(stone)).sMovingSpeed));

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

	for (int step = ecs::PointSystem::GetMaxSP(stone) + 1; step <= wantStep; ++step)
	{
		ecs::PlayerRuntime::SetMaxSP(stone, step);
		ecs::MovementSystem::SendMovePacket(stone, FUNC_ATTACK, 0, ecs::PlayerRuntime::GetX(stone), ecs::PlayerRuntime::GetY(stone), 0);

		CHARACTER_MANAGER::instance().SelectStone(stone);

		if (step == 10 || step == 9)
			CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ecs::PlayerRuntime::GetMapIndex(stone), ecs::PlayerRuntime::GetX(stone) - 1500, ecs::PlayerRuntime::GetY(stone) - 1500, ecs::PlayerRuntime::GetX(stone) + 1500, ecs::PlayerRuntime::GetY(stone) + 1500);
		else if (step == 8 || step == 7 || step == 6 || step == 3 || step == 1)
			CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ecs::PlayerRuntime::GetMapIndex(stone), ecs::PlayerRuntime::GetX(stone) - 1000, ecs::PlayerRuntime::GetY(stone) - 1000, ecs::PlayerRuntime::GetX(stone) + 1000, ecs::PlayerRuntime::GetY(stone) + 1000);
		else if (step == 5 || step == 4 || step == 2)
			CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, ecs::PlayerRuntime::GetMapIndex(stone), ecs::PlayerRuntime::GetX(stone) - 500, ecs::PlayerRuntime::GetY(stone) - 500, ecs::PlayerRuntime::GetX(stone) + 500, ecs::PlayerRuntime::GetY(stone) + 500);

		CHARACTER_MANAGER::instance().SelectStone(entt::null);
	}

	NetworkSyncSystem::UpdatePacket(stone);
}
#endif
static int64_t CalcReferenceBowHitDamage(entt::entity attacker, entt::entity victim)
{
	if (attacker == entt::null || !g_registry.valid(attacker) ||
		victim == entt::null || !g_registry.valid(victim))
		return 0;

	entt::entity pkBow = entt::null;
	entt::entity pkArrow = entt::null;

	if (0 == CombatSystem::GetArrowAndBow(attacker, &pkBow, &pkArrow))
		return 0;

	int64_t dam = CalcArrowDamage(attacker, victim, pkBow, pkArrow);
	if (dam <= 0)
		return 0;

	int32_t lValue = ecs::PointSystem::Get(victim, POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
	lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
	lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif

	if (lValue < 0)
		lValue = 0;
	if (lValue > 100)
		lValue = 100;

	dam = dam * (100 - lValue) / 100;

#ifdef ENABLE_SOUL_SYSTEM
	if (LPCHARACTER souled = ecs::LegacyCharOf(attacker))
		dam += souled->GetSoulItemDamage(victim, dam, RED_SOUL);
#endif

	if (ecs::PointSystem::Get(attacker, POINT_NORMAL_HIT_DAMAGE_BONUS))
		dam = dam * (100 + ecs::PointSystem::Get(attacker, POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

#ifdef ENABLE_MEDI_PVM
	if (ecs::PlayerRuntime::IsNPC(victim))
		dam = dam * (100 + ecs::PointSystem::Get(attacker, POINT_ATTBONUS_MEDI_PVM)) / 100;
#endif

	dam = dam * (100 - std::min((int64_t)99, ecs::PointSystem::Get(victim, POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;

	return std::max<int64_t>(0, dam);
}

static int64_t CalcReferenceBasicHitDamage(entt::entity attacker, entt::entity victim)
{
	if (attacker == entt::null || !g_registry.valid(attacker) ||
		victim == entt::null || !g_registry.valid(victim))
		return 0;

	int64_t dam = 0;

	const entt::entity weapon = ItemSystem::GetWearItem(
		attacker, WEAR_WEAPON);
	if (ItemSystem::IsValidItem(weapon) &&
		ItemSystem::GetItemType(weapon) == ITEM_WEAPON &&
		ItemSystem::GetItemSubType(weapon) == WEAPON_BOW)
		dam = CalcReferenceBowHitDamage(attacker, victim);
	else
		dam = CalcReferenceNormalHitDamage(attacker, victim);

	if (dam <= 0)
		return 0;

	const int64_t skillBonus = std::max<int64_t>(0, ecs::PointSystem::Get(attacker, POINT_SKILL_DAMAGE_BONUS));
	if (skillBonus)
		dam = dam * (100 + skillBonus) / 100;

	return dam;
}
static int64_t CalcReferenceNormalHitDamage(entt::entity attacker, entt::entity victim)
{
	if (attacker == entt::null || !g_registry.valid(attacker) ||
		victim == entt::null || !g_registry.valid(victim))
		return 0;

	int64_t dam = CalcMeleeDamage(attacker, victim);
	if (dam <= 0)
		return 0;

	const entt::entity weapon = ItemSystem::GetWearItem(
		attacker, WEAR_WEAPON);
	if (ItemSystem::IsValidItem(weapon))
	{
		int32_t lValue = 0;

		switch (ItemSystem::GetItemSubType(weapon))
		{
		case WEAPON_SWORD:
			lValue = ecs::PointSystem::Get(victim, POINT_RESIST_SWORD);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_SPADA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_TWO_HANDED:
			lValue = ecs::PointSystem::Get(victim, POINT_RESIST_TWOHAND);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_SPADONE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_DAGGER:
			lValue = ecs::PointSystem::Get(victim, POINT_RESIST_DAGGER);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_PUGNALE);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_BELL:
			lValue = ecs::PointSystem::Get(victim, POINT_RESIST_BELL);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_CAMPANA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_FAN:
			lValue = ecs::PointSystem::Get(victim, POINT_RESIST_FAN);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_VENTAGLIO);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
#endif
			break;

		case WEAPON_BOW:
			lValue = ecs::PointSystem::Get(victim, POINT_RESIST_BOW);
#ifdef ENABLE_NEW_BONUS_TALISMAN
			lValue -= ecs::PointSystem::Get(attacker, POINT_ATTBONUS_IRR_FRECCIA);
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
			lValue -= ecs::PointSystem::Get(attacker, POINT_IRR_WEAPON_DEFENSE);
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

	dam = static_cast<int64_t>(CombatSystem::GetAttackMultiplier(attacker) * static_cast<double>(dam) + 0.5);

#ifdef ENABLE_SOUL_SYSTEM
	if (LPCHARACTER souled = ecs::LegacyCharOf(attacker))
		dam += souled->GetSoulItemDamage(victim, dam, RED_SOUL);
#endif

	if (ecs::PointSystem::Get(attacker, POINT_NORMAL_HIT_DAMAGE_BONUS))
		dam = dam * (100 + ecs::PointSystem::Get(attacker, POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

#ifdef ENABLE_MEDI_PVM
	if (ecs::PlayerRuntime::IsNPC(victim))
		dam = dam * (100 + ecs::PointSystem::Get(attacker, POINT_ATTBONUS_MEDI_PVM)) / 100;
#endif

	dam = dam * (100 - std::min((int64_t)99, ecs::PointSystem::Get(victim, POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;

	return std::max<int64_t>(0, dam);
}

namespace CombatSystem {

void SetAggressive(entt::entity e)
{
	if (auto* flags = RuntimeFlags(e))
		SET_BIT(flags->aiFlag, AIFLAG_AGGRESSIVE);

	AIHelpers::SetAggressive(e, true);
}

} // namespace CombatSystem

namespace CombatSystem {

void ResetChatCounter(entt::entity e)
{
	if (g_registry.valid(e))
		g_registry.get_or_emplace<ecs::InteractionCounters>(e) = {};
}

uint8_t GetChatCounter(entt::entity e)
{
	const auto* counters = g_registry.valid(e) ? g_registry.try_get<ecs::InteractionCounters>(e) : nullptr;
	return counters ? counters->chat : 0;
}

uint8_t IncreaseChatCounter(entt::entity e)
{
	return g_registry.valid(e) ? ++g_registry.get_or_emplace<ecs::InteractionCounters>(e).chat : 0;
}

void ResetMountCounter(entt::entity e)
{
	if (g_registry.valid(e))
		g_registry.get_or_emplace<ecs::InteractionCounters>(e).mount = 0;
}

uint8_t GetMountCounter(entt::entity e)
{
	const auto* counters = g_registry.valid(e) ? g_registry.try_get<ecs::InteractionCounters>(e) : nullptr;
	return counters ? counters->mount : 0;
}

uint8_t IncreaseMountCounter(entt::entity e)
{
	return g_registry.valid(e) ? ++g_registry.get_or_emplace<ecs::InteractionCounters>(e).mount : 0;
}

} // namespace CombatSystem

namespace CombatSystem {

// Attaching a mob to a stone, or detaching it. Leaving a stone now leaves its
// spawn list as well; the CHARACTER version only ever inserted, so a mob that
// moved between stones stayed in the first one's list forever.
void SetStone(entt::entity e, entt::entity stone)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    if (const auto* owner = g_registry.try_get<ecs::StoneOwner>(e);
        owner && owner->stone != entt::null && owner->stone != stone && g_registry.valid(owner->stone))
    {
        if (auto* previous = g_registry.try_get<ecs::StoneSpawns>(owner->stone))
            std::erase(previous->members, e);
    }

    if (stone == entt::null || !g_registry.valid(stone))
    {
        g_registry.remove<ecs::StoneOwner>(e);
        return;
    }

    g_registry.emplace_or_replace<ecs::StoneOwner>(e, stone);
    auto& spawns = g_registry.get_or_emplace<ecs::StoneSpawns>(stone);
    if (std::find(spawns.members.begin(), spawns.members.end(), e) == spawns.members.end())
        spawns.members.push_back(e);
}

// Kills everything this stone spawned, then detaches this mob from its own
// stone. The member list is taken by move first: every Dead() below comes back
// through here and erases from the very list the old std::for_each walked,
// which invalidated its iterator mid-iteration.
void ClearStone(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    if (auto* spawns = g_registry.try_get<ecs::StoneSpawns>(e))
    {
        std::vector<entt::entity> members;
        members.swap(spawns->members);

        for (const entt::entity member : members)
        {
            if (member == entt::null || !g_registry.valid(member))
                continue;
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
            if (auto* flags = RuntimeFlags(member))
                SET_BIT(flags->instantFlag, INSTANT_FLAG_NO_REWARD);
#endif
            Dead(member, entt::null);
            // Dead() can take the entity with it; nothing may touch it after.
            if (g_registry.valid(member))
                SetStone(member, entt::null);
        }
    }

    if (const auto* owner = g_registry.try_get<ecs::StoneOwner>(e))
    {
        if (owner->stone != entt::null && g_registry.valid(owner->stone))
        {
            if (auto* spawns = g_registry.try_get<ecs::StoneSpawns>(owner->stone))
                std::erase(spawns->members, e);
        }
        g_registry.remove<ecs::StoneOwner>(e);
    }
}

} // namespace CombatSystem

namespace CombatSystem {

void SendDamagePacket(entt::entity e, entt::entity attacker, int Damage, uint8_t DamageFlag)
{
	if (ecs::PlayerRuntime::GetDesc(e) != nullptr || (ecs::PlayerRuntime::IsPC(attacker) == true && GetSelectedTarget(attacker) == e))
	{
		TPacketGCDamageInfo damageInfo;
		memset(&damageInfo, 0, sizeof(TPacketGCDamageInfo));

		damageInfo.header = HEADER_GC_DAMAGE_INFO;
		damageInfo.dwVID = ecs::PlayerRuntime::GetPacketVID(e);
		damageInfo.flag = DamageFlag;
		damageInfo.damage = Damage;
#ifdef ENABLE_TARGET_DAMAGE_RAZOR93
		ecs::ViewSystem::PacketView(e, &damageInfo, sizeof(TPacketGCDamageInfo));
		return;
#endif

		if (ecs::PlayerRuntime::GetDesc(e) != nullptr)
		{
			ecs::PlayerRuntime::GetDesc(e)->Packet(&damageInfo, sizeof(TPacketGCDamageInfo));
		}

		if (ecs::PlayerRuntime::GetDesc(attacker) != nullptr)
		{
			ecs::PlayerRuntime::GetDesc(attacker)->Packet(&damageInfo, sizeof(TPacketGCDamageInfo));
		}

		if (ecs::PlayerRuntime::IsArenaObserverMode(e) == false && ecs::PlayerRuntime::GetArena(e) != nullptr) {
			ecs::PlayerRuntime::GetArena(e)->SendPacketToObserver(&damageInfo, sizeof(TPacketGCDamageInfo));
		}
	}
}

entt::entity GetSelectedTarget(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return entt::null;

    const auto* selected = g_registry.try_get<ecs::SelectedTarget>(e);
    if (!selected || selected->target == entt::null || !g_registry.valid(selected->target))
        return entt::null;

    return selected->target;
}

void BroadcastTargetPacket(entt::entity e)
{
	auto* selectedBy = g_registry.try_get<ecs::SelectedBy>(e);
	if (!selectedBy || selectedBy->selectors.empty())
		return;

	TPacketGCTarget p;

	p.header = HEADER_GC_TARGET;
	p.dwVID = ecs::PlayerRuntime::GetPacketVID(e);

#ifdef __VIEW_TARGET_DECIMAL_HP__
	if (ecs::PointSystem::GetMaxHP(e) <= 0)
	{
		p.bHPPercent = 0;
		p.iMinHP = 0;
		p.iMaxHP = 0;
	}
	else
	{
		p.bHPPercent = std::min((ecs::PlayerRuntime::GetHP(e) * 100) / ecs::PointSystem::GetMaxHP(e), (int64_t)100);
		p.iMinHP = ecs::PlayerRuntime::GetHP(e);
		p.iMaxHP = ecs::PointSystem::GetMaxHP(e);
	}
#else
	if (ecs::PlayerRuntime::GetDesc(e) != nullptr)
		p.bHPPercent = 0;
	else if (ecs::PointSystem::GetMaxHP(e) <= 0)
		p.bHPPercent = 0;
	else
		p.bHPPercent = MINMAX(0, ecs::PlayerRuntime::GetHPPct(e), 100);
#endif

	for (const entt::entity chr : selectedBy->selectors)
	{
		if (chr == entt::null || !g_registry.valid(chr))
			continue;

		// Legacy aborted on a missing descriptor here too; same reasoning as
		// ClearTarget - the set is filled by SetTarget, which never asked for
		// one.
		if (LPDESC desc = ecs::PlayerRuntime::GetDesc(chr))
			desc->Packet(&p, sizeof(TPacketGCTarget));
		else
			LOG_ERROR("BroadcastTargetPacket: selector {} has no desc",
				ecs::PlayerRuntime::GetName(chr).data());
	}
}

void ClearTarget(entt::entity e)
{
	if (auto* selected = g_registry.try_get<ecs::SelectedTarget>(e);
		selected && selected->target != entt::null)
	{
		g_registry.get_or_emplace<ecs::SelectedBy>(selected->target).selectors.erase(e);
		selected->target = entt::null;
	}

	TPacketGCTarget p;

	p.header = HEADER_GC_TARGET;
	p.dwVID = 0;
	p.bHPPercent = 0;
#ifdef __VIEW_TARGET_DECIMAL_HP__
	p.iMinHP = 0;
	p.iMaxHP = 0;
#endif

	auto* selectedBy = g_registry.try_get<ecs::SelectedBy>(e);
	if (!selectedBy)
		return;

	for (const entt::entity chr : selectedBy->selectors)
	{
		if (chr == entt::null || !g_registry.valid(chr))
			continue;

		g_registry.get_or_emplace<ecs::SelectedTarget>(chr).target = entt::null;

		// Legacy called abort() here. A selector is in this set because
		// SetTarget put it there, and SetTarget never required a descriptor,
		// so reaching one without is possible and is not worth killing the
		// server over.
		if (LPDESC desc = ecs::PlayerRuntime::GetDesc(chr))
			desc->Packet(&p, sizeof(TPacketGCTarget));
		else
			LOG_ERROR("ClearTarget: selector {} has no desc",
				ecs::PlayerRuntime::GetName(chr).data());
	}

	selectedBy->selectors.clear();
}

void SetTarget(entt::entity e, entt::entity target)
{
	auto& selected = g_registry.get_or_emplace<ecs::SelectedTarget>(e);
	if (selected.target == target)
		return;

	if (selected.target != entt::null && g_registry.valid(selected.target))
		g_registry.get_or_emplace<ecs::SelectedBy>(selected.target).selectors.erase(e);

	selected.target = target;

	TPacketGCTarget p;
	p.header = HEADER_GC_TARGET;

	if (target != entt::null && g_registry.valid(target))
	{
		g_registry.get_or_emplace<ecs::SelectedBy>(target).selectors.insert(e);
	p.dwVID = ecs::PlayerRuntime::GetPacketVID(target);

#ifdef __VIEW_TARGET_PLAYER_HP__
		if ((ecs::PointSystem::GetMaxHP(target) <= 0))
		{
			p.bHPPercent = 0;
#ifdef __VIEW_TARGET_DECIMAL_HP__
			p.iMinHP = 0;
			p.iMaxHP = 0;
#endif
		}
		else if (ecs::PlayerRuntime::IsPC(target) && !AffectSystem::IsPolymorphed(target))
		{
			p.bHPPercent = MINMAX(0, ecs::PlayerRuntime::GetHPPct(target), 100);
#ifdef __VIEW_TARGET_DECIMAL_HP__
			p.iMinHP = ecs::PlayerRuntime::GetHP(target);
			p.iMaxHP = ecs::PointSystem::GetMaxHP(target);
#endif
		}
#else
		if ((ecs::PlayerRuntime::IsPC(target) && !AffectSystem::IsPolymorphed(target)) || (ecs::PointSystem::GetMaxHP(target) <= 0))
			p.bHPPercent = 0;
#endif
		else
		{
			if (ecs::PlayerRuntime::GetRaceNum(target) == 20101 ||
				ecs::PlayerRuntime::GetRaceNum(target) == 20102 ||
				ecs::PlayerRuntime::GetRaceNum(target) == 20103 ||
				ecs::PlayerRuntime::GetRaceNum(target) == 20104 ||
				ecs::PlayerRuntime::GetRaceNum(target) == 20105 ||
				ecs::PlayerRuntime::GetRaceNum(target) == 20106 ||
				ecs::PlayerRuntime::GetRaceNum(target) == 20107 ||
				ecs::PlayerRuntime::GetRaceNum(target) == 20108 ||
				ecs::PlayerRuntime::GetRaceNum(target) == 20109)
			{
				const entt::entity owner = CombatSystem::GetVictim(target);

				if (owner != entt::null)
				{
					int iHorseHealth = MountSystem::GetHorseHealth(owner);
					int iHorseMaxHealth = MountSystem::GetHorseMaxHealth(owner);
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
				if (ecs::PointSystem::GetMaxHP(target) <= 0)
				{
					p.bHPPercent = 0;
					p.iMinHP = 0;
					p.iMaxHP = 0;
				}
				else
				{
					p.bHPPercent = std::min((ecs::PlayerRuntime::GetHP(target) * 100) / ecs::PointSystem::GetMaxHP(target), (int64_t)100);
					p.iMinHP = ecs::PlayerRuntime::GetHP(target);
					p.iMaxHP = ecs::PointSystem::GetMaxHP(target);
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
				if (ecs::PointSystem::GetMaxHP(target) <= 0)
					p.bHPPercent = 0;
				else
					p.bHPPercent = MINMAX(0, (ecs::PlayerRuntime::GetHP(target) * 100) / ecs::PointSystem::GetMaxHP(target), 100);
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
	if (target != entt::null && g_registry.valid(target)) {
		const entt::entity chrTarget = target;
		if (ecs::PlayerRuntime::IsPC(chrTarget)) {
			const entt::entity item = ItemSystem::GetWearItem(
				chrTarget, WEAR_PENDANT);
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
		else if (ecs::PlayerRuntime::IsMonster(target) || ecs::PlayerRuntime::IsStone(chrTarget)) {
			if (ecs::PlayerRuntime::IsRaceFlag(target, RACE_FLAG_ATT_ELEC)) {
				p.bElement = 1;
			}
			else if (ecs::PlayerRuntime::IsRaceFlag(target, RACE_FLAG_ATT_FIRE)) {
				p.bElement = 2;
			}
			else if (ecs::PlayerRuntime::IsRaceFlag(target, RACE_FLAG_ATT_ICE)) {
				p.bElement = 3;
			}
			else if (ecs::PlayerRuntime::IsRaceFlag(target, RACE_FLAG_ATT_WIND)) {
				p.bElement = 4;
			}
			else if (ecs::PlayerRuntime::IsRaceFlag(target, RACE_FLAG_ATT_EARTH)) {
				p.bElement = 5;
			}
			else if (ecs::PlayerRuntime::IsRaceFlag(target, RACE_FLAG_ATT_DARK)) {
				p.bElement = 6;
			}
		}
	}
#endif
	ecs::PlayerRuntime::GetDesc(e)->Packet(&p, sizeof(TPacketGCTarget));
}

void CheckTarget(entt::entity e)
{
	const auto* selected = g_registry.try_get<ecs::SelectedTarget>(e);
	if (!selected || selected->target == entt::null || !g_registry.valid(selected->target))
		return;

	const entt::entity chrTarget = selected->target;

	if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(e) - ecs::PlayerRuntime::GetX(chrTarget), ecs::PlayerRuntime::GetY(e) - ecs::PlayerRuntime::GetY(chrTarget)) >= 4800)
		SetTarget(e, entt::null);
}

} // namespace CombatSystem

namespace CombatSystem {

// Walk back to where this mob was last attacked. Everything it reads has an
// entity accessor now that the last-attacked position is a component.
bool Return(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;

    // CHARACTER::IsNPC was m_bCharType != CHAR_TYPE_PC, so "anything but a PC".
    if (ecs::PlayerRuntime::IsPC(e))
        return false;

    SetVictim(e, entt::null);

    const auto* mobState = MobStateConst(e);
    if (!mobState)
        return false;

    const int32_t x = mobState->lastAttackedX;
    const int32_t y = mobState->lastAttackedY;

    ecs::MovementSystem::SetRotationToXY(e, x, y);

    if (!ecs::MovementSystem::Goto(e, x, y))
        return false;

    ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 0, 0, 0, 0);

    if (test_server)
        LOG_INFO("{} returning to {} {}", ecs::PlayerRuntime::GetName(e), x, y);

    if (LPPARTY party = ecs::SocialSystem::GetParty(e))
        party->SendMessage(e, PM_RETURN, x, y);

    return true;
}

// The last of the mob-table AI flags that still lived on CHARACTER.
bool IsStoneSkinner(entt::entity e)
{
    if (IS_SET(ecs::PlayerRuntime::GetAIFlag(e), AIFLAG_STONESKIN))
        return true;

    const auto* flags = AIHelpers::TryGetFlags(e);
    return flags && flags->isStoneSkinner;
}

} // namespace CombatSystem

namespace CombatSystem {

bool IsChangeAttackPosition(entt::entity e, entt::entity target)
{
    // CHARACTER::IsNPC was "anything but a PC"; a PC always counts as ready.
    if (ecs::PlayerRuntime::IsPC(e))
        return true;

    uint32_t changeTime = AI_CHANGE_ATTACK_POISITION_TIME_NEAR;

    if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(e) - ecs::PlayerRuntime::GetX(target),
                        ecs::PlayerRuntime::GetY(e) - ecs::PlayerRuntime::GetY(target)) >
        AI_CHANGE_ATTACK_POISITION_DISTANCE + GetMobAttackRange(e))
        changeTime = AI_CHANGE_ATTACK_POISITION_TIME_FAR;

    const auto* timer = (e != entt::null && g_registry.valid(e))
        ? g_registry.try_get<ecs::AttackPositionTimer>(e) : nullptr;
    return get_dword_time() - (timer ? timer->lastChange : 0) > changeTime;
}

void SetChangeAttackPositionTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::AttackPositionTimer>(e).lastChange = get_dword_time();
}

void ResetChangeAttackPositionTime(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    g_registry.get_or_emplace<ecs::AttackPositionTimer>(e).lastChange =
        get_dword_time() - AI_CHANGE_ATTACK_POISITION_TIME_NEAR;
}

// Chase a target and stop at minDistance. The body is the one CHARACTER::Follow
// had; every reader it used has an entity accessor, so the only thing that
// changed is who is being asked.
bool Follow(entt::entity self, entt::entity target, float minDistance)
{
    if (self == entt::null || !g_registry.valid(self))
        return false;

    if (ecs::PlayerRuntime::IsPC(self)) {
        LOG_ERROR("Follow: PC cannot use this ({})", ecs::PlayerRuntime::GetName(self));
        return false;
    }

    const int32_t selfX = ecs::PlayerRuntime::GetX(self);
    const int32_t selfY = ecs::PlayerRuntime::GetY(self);

    // The mob leads its own party, or has none, and has been left alone long
    // enough to give up and walk back.
    const auto leadsOrHasNoParty = [&] {
        const entt::entity leader = ecs::SocialSystem::GetPartyLeader(self);
        return leader == entt::null || leader == self;
    };
    const auto idleLongEnough = [&] {
        return get_dword_time() - GetLastAttackedTime(self) >= 15000;
    };

    if (IS_SET(ecs::PlayerRuntime::GetAIFlag(self), AIFLAG_NOMOVE)) {
        if (ecs::PlayerRuntime::IsPC(target) && leadsOrHasNoParty() && idleLongEnough()) {
            const TMobTable* table = ecs::PlayerRuntime::GetMobTable(self);
            const int32_t gap = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(target) - selfX,
                                                ecs::PlayerRuntime::GetY(target) - selfY);
            if (table && table->wAttackRange < gap && Return(self))
                return true;
        }
        return false;
    }

    int32_t x = ecs::PlayerRuntime::GetX(target);
    int32_t y = ecs::PlayerRuntime::GetY(target);

    if (ecs::PlayerRuntime::IsPC(target) && leadsOrHasNoParty() && idleLongEnough()) {
        if (5000 < DistanceFromLastAttacked(self) && Return(self))
            return true;
    }

#ifndef ENABLE_BUG_FIXES
    if (ecs::PlayerRuntime::IsGuardNPC(self)) {
        if (5000 < DistanceFromLastAttacked(self) && Return(self))
            return true;
    }
#endif

    const uint8_t battleType = GetMobBattleType(self);
    const bool intercepts = HasMoveState(target) &&
        battleType != BATTLE_TYPE_RANGE &&
        battleType != BATTLE_TYPE_MAGIC &&
        !ecs::PlayerRuntime::IsPet(self)
#ifdef __NEWPET_SYSTEM__
        && !ecs::PlayerRuntime::IsNewPet(self)
#endif
        ;

    if (intercepts) {
        // Aim at where the target will be, not where it is.
        const float targetRotation = ecs::PlayerRuntime::GetRotation(target);
        const float rotationDelta = GetDegreeDelta(targetRotation,
            GetDegreeFromPositionXY(selfX, selfY,
                ecs::PlayerRuntime::GetX(target), ecs::PlayerRuntime::GetY(target)));

        const float targetSpeed = ecs::MovementSystem::GetMoveSpeed(target);
        const float ownSpeed = ecs::MovementSystem::GetMoveSpeed(self);

        const float gap = DISTANCE_SQRT(x - selfX, y - selfY);
        const float closingSpeed = ownSpeed - targetSpeed * cos(rotationDelta * M_PI / 180);

        if (closingSpeed >= 0.1f) {
            const float meetTime = gap / closingSpeed;
            if (meetTime * targetSpeed <= 100000.0f) {
                float estimateX = 0.0f;
                float estimateY = 0.0f;
                GetDeltaByDegree(targetRotation, meetTime * targetSpeed, &estimateX, &estimateY);

                x += static_cast<int32_t>(estimateX);
                y += static_cast<int32_t>(estimateY);

                const float projected = sqrt(((double)x - selfX) * (x - selfX) +
                                             ((double)y - selfY) * (y - selfY));
                if (gap < projected) {
                    x = static_cast<int32_t>(selfX + (x - selfX) * gap / projected);
                    y = static_cast<int32_t>(selfY + (y - selfY) * gap / projected);
                }
            }
        }
    }

    ecs::MovementSystem::SetRotationToXY(self, x, y);

    const float distance = DISTANCE_SQRT(x - selfX, y - selfY);
    if (distance <= minDistance)
        return false;

    float fx = 0.0f;
    float fy = 0.0f;

    if (IsChangeAttackPosition(self, target) &&
        ecs::PlayerRuntime::GetMobRank(self) < MOB_RANK_BOSS) {
        SetChangeAttackPositionTime(self);

        int retry = 16;
        int dx = 0;
        int dy = 0;
        const int rot = static_cast<int>(GetDegreeFromPositionXY(x, y, selfX, selfY));

        while (--retry) {
            if (distance < 500.0f)
                GetDeltaByDegree((rot + number(-90, 90) + number(-90, 90)) % 360, minDistance, &fx, &fy);
            else
                GetDeltaByDegree(number(0, 359), minDistance, &fx, &fy);

            dx = x + static_cast<int>(fx);
            dy = y + static_cast<int>(fy);

            LPSECTREE tree = ecs::SectorAt(ecs::PlayerRuntime::GetMapIndex(self), dx, dy);
            if (nullptr == tree)
                break;

            if (0 == (tree->GetAttribute(dx, dy) & (ATTR_BLOCK | ATTR_OBJECT)))
                break;
        }

        if (!ecs::MovementSystem::Goto(self, dx, dy))
            return false;
    } else {
        GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(self), distance - minDistance, &fx, &fy);

        if (!ecs::MovementSystem::Goto(self, selfX + static_cast<int>(fx), selfY + static_cast<int>(fy)))
            return false;
    }

    ecs::MovementSystem::SendMovePacket(self, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

} // namespace CombatSystem

