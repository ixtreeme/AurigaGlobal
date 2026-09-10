#include "../../stdafx.h"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"

#include "AffectSystem.hpp"
#include "QuestSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "MovementSystem.hpp"
#include "CombatSystem.hpp"
#include "MountSystem.hpp"
#include "VisibilitySystem.hpp"
#include "SkillSystem.hpp"
#include "SocialSystem.hpp"
#include "../EntityInvariants.hpp"
#include <unordered_set>

#include "../../affect.h"
#include "../../arena.h"
#include "../../buffer_manager.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../config.h"
#include "../../constants.h"
#include "../../desc.h"
#include "../../desc_client.h"
#include "../../battle.h"
#include "../../DragonSoul.h"
#include "DragonSoulSystem.hpp"
#include "../../guild.h"
#include "../../horsename_manager.h"
#include "../../item.h"
#include "../../locale_service.h"
#include "../../lua_incl.h"
#include "../../packet.h"
#include "../../questmanager.h"
#ifdef ENABLE_NEW_USE_POTION
#include "../../party.h"
#endif
#include "../../utils.h"
#include "../EntityFactory.hpp"
#include "ItemSystem.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/status_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/character_runtime_components.hpp"
#include "../components/transform_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include <Core/Logging.hpp>

EVENTFUNC(affect_event);

void SendAffectRemovePacket(LPDESC d, uint32_t pid, uint32_t type, uint8_t point);

namespace {

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);

const int poison_damage_rate[MOB_RANK_MAX_NUM] = {
    80, 50, 40, 30, 25, 1
};

int GetPoisonDamageRate(entt::entity character)
{
    int iRate = ecs::PlayerRuntime::IsPC(character)
        ? 50
        : poison_damage_rate[ecs::PlayerRuntime::GetMobRank(character)];
    iRate = MAX(0, iRate - ecs::PointSystem::Get(character, POINT_POISON_REDUCE));
    return iRate;
}

EVENTINFO(TPoisonEventInfo)
{
    entt::entity character { entt::null };
    entt::entity attacker { entt::null };
    int count;

    TPoisonEventInfo()
        : count(0)
    {
    }
};

EVENTFUNC(poison_event)
{
    TPoisonEventInfo* info = dynamic_cast<TPoisonEventInfo*>(event->info);
    if (info == nullptr) {
        LOG_ERROR("poison_event> <Factor> Null pointer");
        return 0;
    }

    const entt::entity character = info->character;
    if (character == entt::null || !g_registry.valid(character)) {
        return 0;
    }

    int dam = ecs::PointSystem::GetMaxHP(character) * GetPoisonDamageRate(character) / 1000;
    if (test_server) {
        ecs::ChatSystem::Send(character, CHAT_TYPE_NOTICE, "Poison Damage %d", dam);
    }

    if (character != entt::null) {
        g_dispatcher.trigger(ecs::EvPoisonApplied { character, dam });
    }

    if (CombatSystem::Damage(character, info->attacker, dam, DAMAGE_TYPE_POISON)) {
        if (auto* state = g_registry.try_get<ecs::AffectEventState>(character))
            state->poisonEvent = nullptr;
        return 0;
    }

    --info->count;
    if (info->count) {
        return PASSES_PER_SEC(3);
    }

    if (auto* state = g_registry.try_get<ecs::AffectEventState>(character))
        state->poisonEvent = nullptr;
    return 0;
}


EVENTINFO(TFireEventInfo)
{
    entt::entity character { entt::null };
    entt::entity attacker { entt::null };
    int count;
    int amount;

    TFireEventInfo()
        : count(0)
        , amount(0)
    {
    }
};

EVENTFUNC(fire_event)
{
    TFireEventInfo* info = dynamic_cast<TFireEventInfo*>(event->info);
    if (info == nullptr) {
        LOG_ERROR("fire_event> <Factor> Null pointer");
        return 0;
    }

    const entt::entity character = info->character;
    if (character == entt::null || !g_registry.valid(character)) {
        return 0;
    }

    int dam = info->amount;
    if (test_server) {
        ecs::ChatSystem::Send(character, CHAT_TYPE_NOTICE, "Fire Damage %d", dam);
    }

    if (character != entt::null) {
        g_dispatcher.trigger(ecs::EvFireApplied { character, dam });
    }

    if (CombatSystem::Damage(character, info->attacker, dam, DAMAGE_TYPE_FIRE)) {
        if (auto* state = g_registry.try_get<ecs::AffectEventState>(character))
            state->fireEvent = nullptr;
        return 0;
    }

    --info->count;
    if (info->count) {
        return PASSES_PER_SEC(3);
    }

    if (auto* state = g_registry.try_get<ecs::AffectEventState>(character))
        state->fireEvent = nullptr;
    return 0;
}

int poison_level_adjust[9] = {
    100, 90, 80, 70, 50, 30, 10, 5, 0
};


LegacyCharHandle LegacyCharOf(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return nullptr;
    }

    auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e);
    return legacy ? legacy->ptr : nullptr;
}

void MarkPoison(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return;
    }

    if (auto* sf = g_registry.try_get<ecs::StatusFlags>(e)) {
        sf->hasPoisoned = value;
    }

    if (value) {
        g_registry.emplace_or_replace<ecs::PoisonTag>(e);
    } else {
        g_registry.remove<ecs::PoisonTag>(e);
    }

    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void MarkBleeding(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return;
    }

    if (auto* sf = g_registry.try_get<ecs::StatusFlags>(e)) {
        sf->hasBled = value;
    }

    if (value) {
        g_registry.emplace_or_replace<ecs::BleedTag>(e);
    } else {
        g_registry.remove<ecs::BleedTag>(e);
    }

    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void MarkFire(entt::entity e, bool value)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return;
    }

    if (value) {
        g_registry.emplace_or_replace<ecs::FireTag>(e);
    } else {
        g_registry.remove<ecs::FireTag>(e);
    }

    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

ecs::AffectList* AffectState(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e) ||
        !ecs::Invariants::HasAnyTypeTag(g_registry, e))
        return nullptr;
    return g_registry.try_get<ecs::AffectList>(e);
}

uint64_t NextAffectToken()
{
    static uint64_t next = 0;
    return ++next;
}

uint64_t TouchAffectType(entt::entity e, uint32_t type)
{
    if (auto* state = AffectState(e))
        return state->mutationTokens[type] = NextAffectToken();
    return 0;
}

bool IsCurrentAffectType(entt::entity e, uint32_t type, uint64_t token)
{
    const auto* state = AffectState(e);
    if (!state || !token)
        return false;
    const auto it = state->mutationTokens.find(type);
    return it != state->mutationTokens.end() && it->second == token;
}

AffectSystem::AffectLease LastWireAffect(entt::entity e, uint32_t type, uint8_t apply)
{
    const auto* state = AffectState(e);
    if (state)
        for (auto it = state->affects.rbegin(); it != state->affects.rend(); ++it)
            if (*it && (*it)->dwType == type && (*it)->bApplyOn == apply)
                return *it;
    return {};
}

bool SameAffect(const CAffect& a, const CAffect& b)
{
    return a.dwType == b.dwType && a.bApplyOn == b.bApplyOn &&
        a.lApplyValue == b.lApplyValue && a.dwFlag == b.dwFlag &&
        a.lDuration == b.lDuration && a.lSPCost == b.lSPCost;
}

void PublishAffectRemoval(entt::entity e, const CAffect& value)
{
    // The wire/DB key is (type, apply), not the allocation address. Never erase
    // a replacement that a point/packet callback has already installed.
    const auto current = [&] {
        return AffectState(e) && !LastWireAffect(e, value.dwType, value.bApplyOn);
    };
    if (!current() || !ecs::PlayerRuntime::IsPC(e))
        return;
    if (db_clientdesc) {
        TPacketGDRemoveAffect packet{};
        packet.dwPID = ecs::PlayerRuntime::GetPlayerID(e);
        packet.dwType = value.dwType;
        packet.bApplyOn = value.bApplyOn;
        db_clientdesc->DBPacket(HEADER_GD_REMOVE_AFFECT, 0, &packet, sizeof(packet));
    }
    if (current())
        if (auto* desc = ecs::PlayerRuntime::GetDesc(e)) {
            TPacketGCAffectRemove packet{};
            packet.bHeader = HEADER_GC_AFFECT_REMOVE;
            packet.dwType = value.dwType;
            packet.bApplyOn = value.bApplyOn;
            desc->Packet(&packet, sizeof(packet));
        }
}

bool IsNoSaveAffect(uint32_t type)
{
    return type == AFFECT_WAR_FLAG || type == AFFECT_REVIVE_INVISIBLE ||
        (type >= AFFECT_PREMIUM_START && type <= AFFECT_PREMIUM_END);
}

} // namespace

namespace AffectSystem {

void ApplyFire(entt::entity target, entt::entity attacker, int amount, int count)
{
    if (target == entt::null || !g_registry.valid(target))
        return;

    auto& events = g_registry.get_or_emplace<ecs::AffectEventState>(target);
    if (events.fireEvent)
        return;

    MarkFire(target, true);
    AddAffect(target, AFFECT_FIRE, POINT_NONE, 0, AFF_FIRE, count * 3 + 1, 0, true);

    TFireEventInfo* info = AllocEventInfo<TFireEventInfo>();
    info->character = target;
    info->attacker = attacker;
    info->count = count;
    info->amount = amount;
    events.fireEvent = event_create(fire_event, info, 1);
}

void RemoveFire(entt::entity e)
{
    MarkFire(e, false);

    if (e == entt::null || !g_registry.valid(e)) {
        return;
    }

    RemoveAffect(e, AFFECT_FIRE);
    if (auto* events = g_registry.try_get<ecs::AffectEventState>(e))
        event_cancel(&events->fireEvent);
}

void ApplyPoison(entt::entity target, entt::entity attacker)
{
    if (target == entt::null || !g_registry.valid(target))
        return;

    auto& events = g_registry.get_or_emplace<ecs::AffectEventState>(target);
    auto& status = g_registry.get_or_emplace<ecs::StatusFlags>(target);
    if (events.poisonEvent)
        return;

    if (status.hasPoisoned && !ecs::PlayerRuntime::IsPC(target))
        return;


    const bool hasAttacker = attacker != entt::null && g_registry.valid(attacker);
    if (hasAttacker && ecs::PointSystem::GetLevel(attacker) < ecs::PointSystem::GetLevel(target)) {
        int delta = ecs::PointSystem::GetLevel(target) - ecs::PointSystem::GetLevel(attacker);
        if (delta > 8) {
            delta = 8;
        }

        if (number(1, 100) > poison_level_adjust[delta]) {
            return;
        }
    }

    MarkPoison(target, true);
    AddAffect(target, AFFECT_POISON, POINT_NONE, 0, AFF_POISON, POISON_LENGTH + 1, 0, true);

    TPoisonEventInfo* info = AllocEventInfo<TPoisonEventInfo>();
    info->character = target;
    info->attacker = attacker;
    info->count = 10;
    events.poisonEvent = event_create(poison_event, info, 1);

    if (test_server && hasAttacker) {
        char buf[256];
        snprintf(buf, sizeof(buf), "POISON %s -> %s", ecs::PlayerRuntime::GetName(attacker).data(), ecs::PlayerRuntime::GetName(target).data());
        ecs::ChatSystem::Send(attacker, CHAT_TYPE_INFO, "%s", buf);
    }
}

void RemovePoison(entt::entity e)
{
    MarkPoison(e, false);

    if (e == entt::null || !g_registry.valid(e)) {
        return;
    }

    RemoveAffect(e, AFFECT_POISON);
    if (auto* events = g_registry.try_get<ecs::AffectEventState>(e))
        event_cancel(&events->poisonEvent);
}

void ApplyBleeding(entt::entity, entt::entity)
{
}

void RemoveBleeding(entt::entity)
{
}

void CancelDamageEvents(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    if (auto* events = g_registry.try_get<ecs::AffectEventState>(e)) {
        event_cancel(&events->poisonEvent);
        event_cancel(&events->fireEvent);
    }
}

bool IsImmune(entt::entity e, uint32_t immuneFlag)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return false;
    }

    const auto* immunity = g_registry.try_get<ecs::ImmunityFlags>(e);
    if (!immunity || !IS_SET(immunity->flags, immuneFlag)) {
        if (test_server && ecs::PlayerRuntime::IsPC(e)) {
            const std::string name(ecs::PlayerRuntime::GetName(e));
            ecs::ChatSystem::Send(e, CHAT_TYPE_PARTY, "<IMMUNE_FAIL> (%s) NO_IMMUNE_FLAG", name.c_str());
        }
        return false;
    }

#ifdef ENABLE_IMMUNE_PERC
    int immune_pct = 90;
    int percent = number(1, 100);

    if (percent <= immune_pct)
#else
    if (true)
#endif
    {
        if (test_server && ecs::PlayerRuntime::IsPC(e)) {
            const std::string name(ecs::PlayerRuntime::GetName(e));
            ecs::ChatSystem::Send(e, CHAT_TYPE_PARTY, "<IMMUNE_SUCCESS> (%s)", name.c_str());
        }
        return true;
    }

    if (test_server && ecs::PlayerRuntime::IsPC(e)) {
        const std::string name(ecs::PlayerRuntime::GetName(e));
        ecs::ChatSystem::Send(e, CHAT_TYPE_PARTY, "<IMMUNE_FAIL> (%s)", name.c_str());
    }

    return false;
}

void ApplyMobAttribute(entt::entity target, const TMobTable* table)
{
    auto* ch = LegacyCharOf(target);
    if (!ch || !table) {
        return;
    }

    for (int i = 0; i < MOB_ENCHANTS_MAX_NUM; ++i) {
        if (table->cEnchants[i] != 0) {
            ecs::PointSystem::ApplyPoint(target, aiMobEnchantApplyIdx[i], table->cEnchants[i]);
        }
    }


    for (int i = 0; i < MOB_RESISTS_MAX_NUM; ++i) {
        if (table->cResists[i] != 0) {
            ecs::PointSystem::ApplyPoint(target, aiMobResistsApplyIdx[i], table->cResists[i]);
        }
    }


    if (target != entt::null && g_registry.valid(target)) {
        g_registry.emplace_or_replace<ecs::DirtyTag>(target);
    }
}

AffectLease Attach(entt::entity e, const CAffect& value)
{
    auto* state = AffectState(e);
    if (!state || value.bApplyOn >= POINT_MAX_NUM)
        return {};
    AffectLease affect(CAffect::Acquire(), &CAffect::Release);
    *affect = value;
    state->affects.push_back(affect);
    TouchAffectType(e, value.dwType);
    return affect;
}

AffectLease Lease(entt::entity e, const CAffect* affect)
{
    const auto* state = AffectState(e);
    if (!state || !affect)
        return {};
    for (const auto& entry : state->affects)
        if (entry.get() == affect)
            return entry;
    return {};
}

AffectLease Detach(entt::entity e, const CAffect* affect)
{
    auto lease = Lease(e, affect);
    if (lease) {
        AffectState(e)->affects.remove(lease);
        TouchAffectType(e, lease->dwType);
    }
    return lease;
}

std::vector<AffectLease> Snapshot(entt::entity e)
{
    const auto* state = AffectState(e);
    if (!state)
        return {};
    return {state->affects.begin(), state->affects.end()};
}

TAffectFlag GetFlags(entt::entity e)
{
    const auto* state = AffectState(e);
    return state ? state->flags : TAffectFlag{};
}

void SetFlag(entt::entity e, uint32_t flag, bool enabled)
{
    // Some affect types store an item ID in dwFlag. It is not a bit index.
    if (flag == 0 || flag >= AFF_BITS_MAX)
        return;
    if (auto* state = AffectState(e)) {
        if (enabled)
            state->flags.Set(static_cast<int>(flag));
        else
            state->flags.Reset(static_cast<int>(flag));
    }
}

bool IsLoaded(entt::entity e)
{
    const auto* state = AffectState(e);
    return state && state->isLoaded;
}

void SetLoaded(entt::entity e, bool loaded)
{
    if (auto* state = AffectState(e))
        state->isLoaded = loaded;
}

void ComputeAffect(entt::entity e, CAffect affect, bool add)
{
    if (!AffectState(e) || affect.bApplyOn >= POINT_MAX_NUM)
        return;
    if (add && affect.dwType >= GUILD_SKILL_START && affect.dwType <= GUILD_SKILL_END) {
        auto* guild = ecs::SocialSystem::GetGuild(e);
        if (!guild || !guild->UnderAnyWar())
            return;
    }

    SetFlag(e, affect.dwFlag, add);
    const int64_t value = affect.lApplyValue;
    ecs::PointSystem::Change(e, affect.bApplyOn, add ? value : -value);
    if (!AffectState(e))
        return;

    // Only these unmigrated skill timers still need a CHARACTER leaf.
    // No component references or borrowed affect pointers cross PointChange.
    if (affect.dwType == SKILL_MUYEONG) {
        if (auto* ch = LegacyCharOf(e)) {
            if (add) ch->StartMuyeongEvent();
            else ch->StopMuyeongEvent();
        }
    }
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
    if (affect.dwType == SKILL_GYEONGGONG) {
        if (auto* ch = LegacyCharOf(e)) {
            if (add) ch->StartGyeongGongEvent();
            else ch->StopGyeongGongEvent();
        }
    }
#endif
}

CAffect* FindAffect(entt::entity e, uint32_t type, uint8_t apply)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return nullptr;
    }

    auto* affectList = g_registry.try_get<ecs::AffectList>(e);
    if (!affectList) {
        return nullptr;
    }

    for (const auto& affect : affectList->affects) {
        if (!affect) {
            continue;
        }

        if (affect->dwType == type && (apply == APPLY_NONE || affect->bApplyOn == apply)) {
            return affect.get();
        }
    }

    return nullptr;
}

CAffect* FindAffect(entt::entity e, uint32_t type, uint8_t apply, int32_t value)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;
    const auto* affectList = g_registry.try_get<ecs::AffectList>(e);
    if (!affectList)
        return nullptr;
    for (const auto& affect : affectList->affects)
    {
        if (affect && affect->dwType == type && affect->bApplyOn == apply &&
            affect->lApplyValue == value)
            return affect.get();
    }
    return nullptr;
}

bool IsAffectFlag(entt::entity e, uint32_t flag)
{
    return flag > 0 && flag < AFF_BITS_MAX && GetFlags(e).IsSet(static_cast<int>(flag));
}

bool StartAffectEvent(entt::entity e)
{
    if (!AffectState(e))
        return false;
    auto& state = g_registry.get_or_emplace<ecs::AffectTickState>(e);
    if (state.timer)
        return true;
    // A pending reservation is not an installed timer. In particular, a nested
    // add must not grant an affect if the outer creation is later cancelled.
    if (state.startingToken)
        return false;
    const auto token = state.startingToken = NextAffectToken();
    struct StartGuard {
        entt::entity entity;
        uint64_t token;
        ~StartGuard() {
            if (!g_registry.valid(entity)) return;
            if (auto* current = g_registry.try_get<ecs::AffectTickState>(entity);
                current && current->startingToken == token)
                current->startingToken = 0;
        }
    } guard{e, token};
    auto* info = AllocEventInfo<char_event_info>();
    info->ch = e;
    auto timer = event_create(affect_event, info, passes_per_sec);
    // No registry references survive event creation, including nested start,
    // stop, owner destruction or replacement of the timer component.
    auto* current = AffectState(e) ? g_registry.try_get<ecs::AffectTickState>(e) : nullptr;
    if (!current || current->startingToken != token) {
        event_cancel(&timer);
        return false;
    }
    current->timer = std::move(timer);
    return static_cast<bool>(current->timer);
}

void StopAffectEvent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;
    if (auto* state = g_registry.try_get<ecs::AffectTickState>(e)) {
        state->startingToken = 0;
        auto timer = std::exchange(state->timer, {});
        event_cancel(&timer);
    }
}

bool AddAffect(entt::entity e, uint32_t type, uint8_t applyOn, int32_t applyValue,
               uint32_t flag, int32_t duration, int32_t spCost, bool overwrite,
               bool isCube)
{
    if (!AffectState(e) || applyOn >= POINT_MAX_NUM)
        return false;
    if (duration == 0)
        duration = 1;
    uint64_t token = TouchAffectType(e, type);
    const auto current = [&] { return IsCurrentAffectType(e, type, token); };

#ifdef ENABLE_BUG_FIXES
    if (type == AFFECT_POLYMORPH) {
        if (IsAffectFlag(e, AFF_GEOMGYEONG))
            RemoveAffect(e, SKILL_GEOMKYUNG);
        if (!current()) return false;
        if (IsAffectFlag(e, AFF_GWIGUM))
            RemoveAffect(e, SKILL_GWIGEOM);
        if (!current()) return false;
    }
#endif
#ifdef TEXTS_IMPROVEMENT
    if (type == AFFECT_BLOCK_CHAT && duration > 1) {
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 414, "%d", duration / 60);
        if (!current()) return false;
    }
#endif
    if (flag == AFF_STUN) {
        const auto* pos = g_registry.try_get<ecs::Position>(e);
        const auto* destination = g_registry.try_get<ecs::MovementDestination>(e);
        if (pos && destination && (pos->x != destination->x || pos->y != destination->y)) {
            if (const auto* runtime = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
                runtime && runtime->position == POS_FIGHTING)
                ecs::PlayerRuntime::SetPosition(e, POS_STANDING);
            if (!current()) return false;
            // Zero timing and remove the destination. A subsequent movement
            // initializes its own start; no m_posStart mirror write is needed.
            ecs::MovementSystem::SyncDestinationClear(e);
            if (!current()) return false;
            NetworkSyncSystem::BroadcastSyncPacket(g_registry, e);
            if (!current()) return false;
        }
    }

    // Reserve the timer before changing points/ownership. A scheduling failure
    // must not leave a granted affect without an expiry/recovery event.
    if (!StartAffectEvent(e) || !current()) return false;
    if (overwrite) {
        const auto old = Detach(e, FindAffect(e, type, isCube ? applyOn : APPLY_NONE));
        if (old) {
            token = TouchAffectType(e, type);
            const CAffect value = *old;
            ComputeAffect(e, value, false);
            PublishAffectRemoval(e, value);
            if (!current()) return false;
        }
    }
    const CAffect value{type, applyOn, applyValue, flag, duration, spCost};
    const auto lease = Attach(e, value);
    if (!lease)
        return false;
    const auto owned = [&] { return Lease(e, lease.get()) && SameAffect(*lease, value); };
    const auto publishable = [&] { return owned() && LastWireAffect(e, type, applyOn) == lease; };

    ComputeAffect(e, value, true);
    if (!owned()) return false;
    NetworkSyncSystem::UpdatePacket(e);
    if (!owned()) return false;
    if (!ecs::PlayerRuntime::IsPC(e))
        return true;

    // Publish DB first so reentrant replacements remain the last write on both
    // channels. Different apply slots can coexist; only a newer identical wire
    // key supersedes this record's publication.
    if (publishable() && !IsNoSaveAffect(type) && db_clientdesc) {
        TPacketGDAddAffect packet{};
        packet.dwPID = ecs::PlayerRuntime::GetPlayerID(e);
        packet.elem = {type, applyOn, applyValue, flag, duration, spCost};
        db_clientdesc->DBPacket(HEADER_GD_ADD_AFFECT, 0, &packet, sizeof(packet));
    }
    if (publishable())
        if (auto* desc = ecs::PlayerRuntime::GetDesc(e)) {
            CAffect packetValue = value;
            SendAffectAddPacket(desc, &packetValue);
        }
    return owned();
}

bool RemoveAffect(entt::entity e, uint32_t type)
{
    if (!AffectState(e))
        return false;
    TouchAffectType(e, type);
#ifdef TEXTS_IMPROVEMENT
    if (type == AFFECT_BLOCK_CHAT)
        ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 474, "");
#endif
    bool removed = false;
    // Finite batch: an effect added by a removal callback belongs to the next
    // operation, not to an unbounded Find/Remove loop.
    for (const auto& affect : Snapshot(e))
        if (affect && affect->dwType == type)
            removed = RemoveAffect(e, affect.get()) || removed;
    return removed;
}

bool RemoveAffect(entt::entity e, CAffect* affect)
{
    const auto lease = Detach(e, affect);
    if (!lease)
        return false;
    const CAffect value = *lease;
    ComputeAffect(e, value, false);
    if (!AffectState(e))
        return true;

    // Preserve the revive-invisibility/mount exceptions: their removal must
    // not reapply all the other buffs through a full point recalculation.
    if (value.dwType != AFFECT_REVIVE_INVISIBLE
#ifdef ENABLE_BUG_FIXES
        && value.dwType != AFFECT_MOUNT
#endif
    )
        ecs::PointSystem::Compute(e);
    else
        NetworkSyncSystem::UpdatePacket(e);
    if (!AffectState(e))
        return true;

    const auto hp = ecs::PointSystem::Get(e, POINT_HP);
    const auto maxHP = ecs::PointSystem::GetMaxHP(e);
    if (hp > maxHP)
        ecs::PointSystem::Change(e, POINT_HP, static_cast<int64_t>(maxHP) - hp);
    if (!AffectState(e))
        return true;
    const auto sp = ecs::PointSystem::Get(e, POINT_SP);
    const auto maxSP = ecs::PointSystem::GetMaxSP(e);
    if (sp > maxSP)
        ecs::PointSystem::Change(e, POINT_SP, static_cast<int64_t>(maxSP) - sp);
    if (!AffectState(e))
        return true;

    if (test_server)
        LOG_TRACE("AFFECT_REMOVE: {} (flag {} apply: {})",
            ecs::PlayerRuntime::GetName(e), value.dwFlag, static_cast<int>(value.bApplyOn));
    PublishAffectRemoval(e, value);
    return true;
}

void RemoveBadAffects(entt::entity e)
{
    if (!AffectState(e))
        return;
    RemovePoison(e);
    RemoveFire(e);
    RemoveAffect(e, AFFECT_STUN);
    RemoveAffect(e, AFFECT_SLOW);
    RemoveAffect(e, SKILL_TUSOK);
}

void RemoveGoodAffects(entt::entity e)
{
    constexpr uint32_t types[] = {
        AFFECT_MOV_SPEED, AFFECT_ATT_SPEED, AFFECT_STR, AFFECT_DEX,
        AFFECT_INT, AFFECT_CON, AFFECT_CHINA_FIREWORK, SKILL_JEONGWI,
        SKILL_GEOMKYUNG, SKILL_GYEONGGONG, SKILL_GWIGEOM, SKILL_TERROR,
        SKILL_JUMAGAP, SKILL_MANASHILED, SKILL_HOSIN, SKILL_REFLECT,
        SKILL_GICHEON, SKILL_KWAESOK, SKILL_JEUNGRYEOK, SKILL_CHUNKEON,
        SKILL_EUNHYUNG,
    };
    for (const auto type : types)
        RemoveAffect(e, type);
}

uint32_t GetBattlePassDeadline(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e)) return 0;
    const auto* state = g_registry.try_get<ecs::BattlePassTiming>(e);
    return state ? state->deadline : 0;
}

bool SetBattlePassDeadline(entt::entity e, uint32_t deadline)
{
    if (e == entt::null || !g_registry.valid(e) ||
        !ecs::Invariants::HasAnyTypeTag(g_registry, e)) return false;
    g_registry.get_or_emplace<ecs::BattlePassTiming>(e).deadline = deadline;
    return true;
}

int32_t GetBattlePassRemainingSeconds(entt::entity e)
{
    const int64_t remaining = static_cast<int64_t>(GetBattlePassDeadline(e)) - get_global_time();
    return static_cast<int32_t>(std::clamp<int64_t>(remaining, INT32_MIN, INT32_MAX));
}

bool ProcessAffect(entt::entity e)
{
    auto* state = AffectState(e);
    if (!state) return true;
    if (state->expiryToken) return state->affects.empty();
    const auto token = state->expiryToken = NextAffectToken();
    struct ExpiryGuard {
        entt::entity entity;
        uint64_t token;
        ~ExpiryGuard() {
            if (auto* state = AffectState(entity); state && state->expiryToken == token)
                state->expiryToken = 0;
        }
    } guard{e, token};
    const auto current = [&] {
        const auto* state = AffectState(e);
        return state && state->expiryToken == token;
    };
    const auto empty = [&] {
        const auto* state = AffectState(e);
        return !state || state->affects.empty();
    };
    const auto stillOwned = [&](const AffectLease& affect, const CAffect& value) {
        return current() && Lease(e, affect.get()) == affect && SameAffect(*affect, value);
    };
    bool changed = false;

    // Absolute deadlines are refreshed before the ordinary duration pass.
    // PREMIUM_MAX_NUM is a count, not a valid premium index.
    for (uint8_t i = 0; i < PREMIUM_MAX_NUM; ++i) {
        const uint32_t type = AFFECT_PREMIUM_START + i;
        const auto affect = Lease(e, FindAffect(e, type));
        if (!affect) continue;
        const CAffect value = *affect;
        const int64_t remaining = ecs::PlayerRuntime::GetPremiumRemainSeconds(e, i);
        if (!current()) return empty();
        if (!stillOwned(affect, value)) continue;
        if (remaining < 0) {
            RemoveAffect(e, type);
            changed = true;
            if (!current()) return empty();
        } else
            affect->lDuration = static_cast<int32_t>(std::min<int64_t>(remaining + 1, INT32_MAX));
    }
#ifdef ENABLE_VOTE_FOR_BONUS
    if (const auto affect = Lease(e, FindAffect(e, AFFECT_VOTEFORBONUS));
        affect && static_cast<int64_t>(affect->lDuration) <= get_global_time()) {
        RemoveAffect(e, AFFECT_VOTEFORBONUS);
        changed = true;
        if (!current()) return empty();
    }
#endif
#ifdef ENABLE_BATTLE_PASS
    if (const auto affect = Lease(e, FindAffect(e, AFFECT_BATTLE_PASS))) {
        const int64_t remaining = GetBattlePassRemainingSeconds(e);
        if (remaining < 0) {
            // Clear the old deadline before callbacks can install a new pass.
            SetBattlePassDeadline(e, 0);
            RemoveAffect(e, AFFECT_BATTLE_PASS);
            changed = true;
            if (!current()) return empty();
        } else
            affect->lDuration = static_cast<int32_t>(std::min<int64_t>(remaining + 1, INT32_MAX));
    }
#endif
    if (const auto affect = Lease(e, FindAffect(e, AFFECT_HAIR))) {
        const CAffect value = *affect;
        const bool expired = ecs::QuestSystem::GetFlag(e, "hair.limit_time") < get_global_time();
        if (!current()) return empty();
        if (stillOwned(affect, value)) {
            if (expired) {
                ecs::PlayerRuntime::SetPart(e, PART_HAIR, 0);
                if (!current()) return empty();
                if (stillOwned(affect, value)) RemoveAffect(e, static_cast<uint32_t>(AFFECT_HAIR));
                if (!current()) return empty();
            } else if (affect->lDuration < INT32_MAX)
                ++affect->lDuration;
        }
    }
    CHorseNameManager::instance().Validate(e);
    if (!current()) return empty();

    const auto oldFlags = GetFlags(e);
    const auto oldMoveSpeed = ecs::PointSystem::Get(e, POINT_MOV_SPEED);
    const auto oldAttackSpeed = ecs::PointSystem::Get(e, POINT_ATT_SPEED);
    struct Entry { AffectLease affect; CAffect value; };
    std::vector<Entry> batch;
    std::unordered_set<const CAffect*> seen;
    for (const auto& affect : Snapshot(e))
        if (affect && seen.insert(affect.get()).second)
            batch.push_back({affect, *affect});

    // New/replaced records during a point or packet callback wait for the next
    // pass. No registry/list references or borrowed records cross callbacks.
    for (const auto& [affect, value] : batch) {
        if (!current()) return empty();
        if (!stillOwned(affect, value)) continue;
        bool expired = false;
        if (value.dwType >= GUILD_SKILL_START && value.dwType <= GUILD_SKILL_END) {
            auto* guild = ecs::SocialSystem::GetGuild(e);
            expired = !guild || !guild->UnderAnyWar();
            if (!current()) return empty();
            if (!stillOwned(affect, value)) continue;
        }
        if (value.lSPCost > 0
#ifdef ENABLE_SOUL_SYSTEM
            && value.dwType != AFFECT_SOUL_RED && value.dwType != AFFECT_SOUL_BLUE
#endif
        ) {
            if (ecs::PointSystem::Get(e, POINT_SP) < value.lSPCost)
                expired = true;
            else {
                ecs::PointSystem::Change(e, POINT_SP, -static_cast<int64_t>(value.lSPCost));
                if (!current()) return empty();
                if (!stillOwned(affect, value)) continue;
            }
        }
        // Avoid decrementing INT32_MIN. Zero/negative loaded durations expire.
        if (affect->lDuration > 0) --affect->lDuration;
        if (affect->lDuration <= 0) expired = true;
        if (!expired) continue;

        const CAffect expiredValue = *affect;
        if (!Detach(e, affect.get())) continue;
        ComputeAffect(e, expiredValue, false);
        if (!current()) return empty();
        changed = true;
        PublishAffectRemoval(e, expiredValue);
        if (!current()) return empty();
    }
    if (changed) {
        if (oldFlags != GetFlags(e) || oldMoveSpeed != ecs::PointSystem::Get(e, POINT_MOV_SPEED) ||
            oldAttackSpeed != ecs::PointSystem::Get(e, POINT_ATT_SPEED)) {
            NetworkSyncSystem::UpdatePacket(e);
            if (!current()) return empty();
        }
        const auto hp = ecs::PointSystem::Get(e, POINT_HP);
        const auto maxHP = ecs::PointSystem::GetMaxHP(e);
        if (hp > maxHP) ecs::PointSystem::Change(e, POINT_HP, static_cast<int64_t>(maxHP) - hp);
        if (!current()) return empty();
        const auto sp = ecs::PointSystem::Get(e, POINT_SP);
        const auto maxSP = ecs::PointSystem::GetMaxSP(e);
        if (sp > maxSP) ecs::PointSystem::Change(e, POINT_SP, static_cast<int64_t>(maxSP) - sp);
        if (!current()) return empty();
    }
    return AffectState(e)->affects.empty();
}

void ClearAffectSkills(entt::entity e)
{
    if (auto* state = AffectState(e))
        state->skillAffects.clear();
}

void ClearAffect(entt::entity e, bool save)
{
    auto* ch = LegacyCharOf(e);
    if (!ch) {
        return;
    }

    ch->ClearAffect(save);

}

void RefreshAffect(entt::entity e)
{
    auto* state = AffectState(e);
    if (!state || state->refreshToken)
        return;

    // A unique pass token also detects remove/re-emplace of the component on
    // the same entity. Nested refresh must not recursively apply the bonuses.
    static uint64_t nextToken = 0;
    const uint64_t token = ++nextToken;
    state->refreshToken = token;
    struct RefreshGuard {
        entt::entity entity;
        uint64_t token;
        ~RefreshGuard() {
            if (auto* current = AffectState(entity); current && current->refreshToken == token)
                current->refreshToken = 0;
        }
    } guard{e, token};

    struct Entry { AffectLease lease; CAffect value; };
    std::vector<Entry> snapshot;
    std::unordered_set<const CAffect*> seen;
    for (const auto& affect : state->affects)
        if (affect && seen.insert(affect.get()).second)
            snapshot.push_back({affect, *affect});

    for (const auto& entry : snapshot) {
        const auto* current = AffectState(e);
        if (!current || current->refreshToken != token)
            return;
        // A callback may remove/replace the next affect. Keep its allocation
        // alive, but never apply an obsolete snapshot or a newly added entry.
        if (Lease(e, entry.lease.get()) && SameAffect(*entry.lease, entry.value))
            ComputeAffect(e, entry.value, true);
    }
}

bool IsPolymorphed(entt::entity e)
{
	return GetPolymorphVnum(e) != 0;
}

int GetPolymorphPower(entt::entity e)
{
    if (test_server)
    {
        const int value = quest::CQuestManager::instance().GetEventFlag("poly");
        if (value)
            return value;
    }

    return aiPolymorphPowerByLevel[MINMAX(0, SkillSystem::GetSkillLevel(e, SKILL_POLYMORPH), 40)];
}

bool IsPolyMaintainStat(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return false;
	const auto* state = g_registry.try_get<ecs::PolymorphState>(e);
	return state && state->maintainStat;
}

uint32_t GetPolymorphVnum(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	if (const auto* race = g_registry.try_get<ecs::RaceState>(e))
		return race->polymorphRace;
	const auto* state = g_registry.try_get<ecs::PolymorphState>(e);
	return state ? state->raceVnum : 0;
}

void SetPolymorph(entt::entity e, uint32_t raceVnum, bool maintainStats)
{
	if (e == entt::null || !g_registry.valid(e))
		return;
	if (raceVnum < JOB_MAX_NUM)
	{
		raceVnum = 0;
		maintainStats = false;
	}
	if (GetPolymorphVnum(e) == raceVnum)
		return;

	auto& race = g_registry.get_or_emplace<ecs::RaceState>(e);
	race.polymorphRace = raceVnum;
	auto& polymorph = g_registry.get_or_emplace<ecs::PolymorphState>(e);
	polymorph.raceVnum = raceVnum;
	polymorph.maintainStat = maintainStats;
	auto* status = g_registry.try_get<ecs::StatusFlags>(e);
	if (!status)
		status = &g_registry.emplace<ecs::StatusFlags>(e, ecs::StatusFlags {});
	status->isPolymorph = raceVnum != 0;

	LOG_INFO("POLYMORPH: {} race {}", ecs::PlayerRuntime::GetName(e), raceVnum);
	if (raceVnum != 0)
		MountSystem::StopRiding(e);

	status->isSpawnState = true;
	ecs::VisibilitySystem::Reencode(g_registry, e);
	status->isSpawnState = false;

	if (!maintainStats)
	{
		ecs::PointSystem::Change(e, POINT_ST, 0);
		ecs::PointSystem::Change(e, POINT_DX, 0);
		ecs::PointSystem::Change(e, POINT_IQ, 0);
		ecs::PointSystem::Change(e, POINT_HT, 0);
	}
	CombatSystem::SetValidComboInterval(e, 0);
	CombatSystem::SetComboSequence(e, 0);
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void UpdateAffect(entt::registry&, uint32_t)
{
    // Expiry is still scheduled by affect_event/ProcessAffect exactly once.
    // AffectList is authoritative now; no CHARACTER-to-ECS mirror pass exists.
}

} // namespace AffectSystem

void CHARACTER::AttackedByFire(entt::entity attacker, int amount, int count)
{
    AffectSystem::ApplyFire
        (GetEntityHandle(),
        attacker,
        amount,
        count);
}

void AffectSystem_Update(entt::registry& reg, uint32_t tick)
{
    AffectSystem::UpdateAffect(reg, tick);
}

// char_affect.cpp moved into AffectSystem.cpp


#define IS_NO_SAVE_AFFECT(type) ((type) == AFFECT_WAR_FLAG || (type) == AFFECT_REVIVE_INVISIBLE || ((type) >= AFFECT_PREMIUM_START && (type) <= AFFECT_PREMIUM_END))
#define IS_NO_CLEAR_ON_DEATH_AFFECT(type) ((type) == AFFECT_PVM_RACE || (type) == AFFECT_BLOCK_CHAT || ((type) >= 500 && (type) < 600) || ((type) >= 564 && (type) < 566) || ((type) >= NEW_AFFECT_BIOLOGIST_1 && (type) <= NEW_AFFECT_BIOLOGIST_16))
void SendAffectRemovePacket(LPDESC d, uint32_t pid, uint32_t type, uint8_t point)
{
	TPacketGCAffectRemove ptoc;
	ptoc.bHeader	= HEADER_GC_AFFECT_REMOVE;
	ptoc.dwType		= type;
	ptoc.bApplyOn	= point;
	d->Packet(&ptoc, sizeof(TPacketGCAffectRemove));

	TPacketGDRemoveAffect ptod;
	ptod.dwPID		= pid;
	ptod.dwType		= type;
	ptod.bApplyOn	= point;
	db_clientdesc->DBPacket(HEADER_GD_REMOVE_AFFECT, 0, &ptod, sizeof(ptod));
}

void SendAffectAddPacket(LPDESC d, CAffect * pkAff)
{
	TPacketGCAffectAdd ptoc;
	ptoc.bHeader		= HEADER_GC_AFFECT_ADD;
	ptoc.elem.dwType		= pkAff->dwType;
	ptoc.elem.bApplyOn		= pkAff->bApplyOn;
	ptoc.elem.lApplyValue	= pkAff->lApplyValue;
	ptoc.elem.dwFlag		= pkAff->dwFlag;
	ptoc.elem.lDuration		= pkAff->lDuration;
	ptoc.elem.lSPCost		= pkAff->lSPCost;
	d->Packet(&ptoc, sizeof(TPacketGCAffectAdd));
}
////////////////////////////////////////////////////////////////////
// Affect
CAffect * CHARACTER::FindAffect(uint32_t dwType, uint8_t bApply) const
{
    return AffectSystem::FindAffect(GetEntityHandle(), dwType, bApply);
}

std::vector<std::shared_ptr<CAffect>> CHARACTER::GetAffectContainer() const
{
    return AffectSystem::Snapshot(GetEntityHandle());
}

TAffectFlag CHARACTER::GetAffectFlags() const
{
    return AffectSystem::GetFlags(GetEntityHandle());
}

EVENTFUNC(affect_event)
{
    const auto* info = dynamic_cast<char_event_info*>(event->info);
    if (!info)
        return 0;
    const auto entity = info->ch;
    const auto matches = [&] {
        if (!g_registry.valid(entity)) return false;
        const auto* state = g_registry.try_get<ecs::AffectTickState>(entity);
        return state && state->timer == event;
    };
    if (!matches())
        return 0;
    if (!AffectState(entity)) {
        AffectSystem::StopAffectEvent(entity);
        return 0;
    }
    // The outer recovery/item/recall tick is still legacy; it delegates expiry
    // to native ProcessAffect. Timer ownership no longer depends on CHARACTER.
    auto* ch = LegacyCharOf(entity);
    if (!ch) {
        AffectSystem::StopAffectEvent(entity);
        return 0;
    }
    const bool repeat = ch->UpdateAffect();
    if (!matches())
        return 0;
    if (!repeat || !AffectState(entity)) {
        AffectSystem::StopAffectEvent(entity);
        return 0;
    }
    return passes_per_sec;
}

bool CHARACTER::UpdateAffect()
{
#ifdef ENABLE_BUG_FIXES
	if (!ItemSystem::IsValidItem(ItemSystem::GetWearItem(GetEntityHandle(), WEAR_WEAPON))) {
		if (IsAffectFlag(AFF_GEOMGYEONG)) {
			RemoveAffect(SKILL_GEOMKYUNG);
		}

		if (IsAffectFlag(AFF_GWIGUM)) {
			RemoveAffect(SKILL_GWIGEOM);
		}
	}
#endif

	// affect_event ���� ó���� ���� �ƴ�����, 1��¥�� �̺�Ʈ���� ó���ϴ� ����
	// �̰� ���̶� ���⼭ ���� ó���� �Ѵ�.
	if (GetPoint(POINT_HP_RECOVERY) > 0)
	{
		if (GetMaxHP() <= GetHP())
		{
			PointChange(POINT_HP_RECOVERY, -GetPoint(POINT_HP_RECOVERY));
		}
		else
		{
			int iVal = MIN(GetPoint(POINT_HP_RECOVERY), GetMaxHP() * 7 / 100);

			PointChange(POINT_HP, iVal);
			PointChange(POINT_HP_RECOVERY, -iVal);
		}
	}

	if (GetPoint(POINT_SP_RECOVERY) > 0)
	{
		if (GetMaxSP() <= ecs::PlayerRuntime::GetSP(GetEntityHandle()))
			PointChange(POINT_SP_RECOVERY, -GetPoint(POINT_SP_RECOVERY));
		else
		{
			int iVal = MIN(GetPoint(POINT_SP_RECOVERY), GetMaxSP() * 7 / 100);

			PointChange(POINT_SP, iVal);
			PointChange(POINT_SP_RECOVERY, -iVal);
		}
	}

	if (GetPoint(POINT_HP_RECOVER_CONTINUE) > 0)
	{
		PointChange(POINT_HP, GetPoint(POINT_HP_RECOVER_CONTINUE));
	}

	if (GetPoint(POINT_SP_RECOVER_CONTINUE) > 0)
	{
		PointChange(POINT_SP, GetPoint(POINT_SP_RECOVER_CONTINUE));
	}

	ItemSystem::AutoRecoveryItemProcess(GetEntityHandle(), AFFECT_AUTO_HP_RECOVERY);
	ItemSystem::AutoRecoveryItemProcess(GetEntityHandle(), AFFECT_AUTO_SP_RECOVERY);
#ifdef ENABLE_NEW_USE_POTION
	ItemSystem::AutoRecoveryItemProcess(GetEntityHandle(), AFFECT_AUTO_HP_RECOVERY2);
	ItemSystem::AutoRecoveryItemProcess(GetEntityHandle(), AFFECT_AUTO_SP_RECOVERY2);
#endif
#ifdef ENABLE_RECALL
	AutoRecallProcess();
#endif

	// ���׹̳� ȸ��
	if (GetMaxStamina() > GetStamina())
	{
		int iSec = (get_dword_time() - GetStopTime()) / 3000;
		if (iSec)
			PointChange(POINT_STAMINA, GetMaxStamina()/1);
	}


	// ProcessAffect�� affect�� ������ true�� �����Ѵ�.
    // Expiry callbacks can destroy this CHARACTER. Capture the entity before
    // entering the native pass and never read this again afterwards.
    const auto entity = GetEntityHandle();
    const bool empty = AffectSystem::ProcessAffect(entity);
    if (!AffectState(entity)) return false;
    if (empty && ecs::PointSystem::Get(entity, POINT_HP_RECOVERY) == 0 &&
        ecs::PointSystem::Get(entity, POINT_SP_RECOVERY) == 0 &&
        ecs::PointSystem::Get(entity, POINT_STAMINA) == ecs::PlayerRuntime::GetMaxStamina(entity)) {
        AffectSystem::StopAffectEvent(entity);
        return false;
    }
    return true;
}

void CHARACTER::StartAffectEvent()
{
    AffectSystem::StartAffectEvent(GetEntityHandle());
}

#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
void CHARACTER::SaveAffectSkills(uint32_t dwType, uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag, int32_t lDuration, int32_t lSPCost)
{
    if (auto* state = AffectState(GetEntityHandle()))
        state->skillAffects.push_back({dwType, bApplyOn, lApplyValue, dwFlag,
            lDuration, lSPCost, static_cast<uint32_t>(get_global_time())});
}

void CHARACTER::LoadAffectSkills()
{
    const auto entity = GetEntityHandle();
    auto* state = AffectState(entity);
    if (!state)
        return;
    // Consume the saved batch before callbacks; nested loads cannot replay it.
    auto saved = std::move(state->skillAffects);
    state->skillAffects.clear();
    for (const auto& affect : saved) {
        const int64_t remaining = static_cast<int64_t>(affect.lDuration) -
            (static_cast<int64_t>(get_global_time()) - affect.dwTime);
        if (remaining > 0 && remaining <= INT32_MAX)
            AffectSystem::AddAffect(entity, affect.dwType, affect.bApplyOn,
                affect.lApplyValue, affect.dwFlag, static_cast<int32_t>(remaining),
                affect.lSPCost, false);
        if (!AffectState(entity))
            return;
    }
}
#endif

void CHARACTER::ClearAffect(bool bSave)
{
    const auto entity = GetEntityHandle();
    auto* state = AffectState(entity);
    if (!state)
        return;
    for (auto& [type, token] : state->mutationTokens)
        token = NextAffectToken();

	for (const auto& lease : AffectSystem::Snapshot(entity))
	{
		if (!AffectSystem::Lease(entity, lease.get()))
			continue;
		CAffect* pkAff = lease.get();

		if (bSave)
		{
#ifdef ENABLE_SOUL_SYSTEM
			if ( pkAff->dwType == AFFECT_SOUL_RED || pkAff->dwType == AFFECT_SOUL_BLUE )
			{

				continue;
			}
#endif

			if ( IS_NO_CLEAR_ON_DEATH_AFFECT(pkAff->dwType) || IS_NO_SAVE_AFFECT(pkAff->dwType) )
			{

				continue;
			}
#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
			else if ((IsPC()) && (
				(pkAff->dwType == SKILL_JEONGWI) ||	// 3
				(pkAff->dwType == SKILL_GEOMKYUNG) ||	// 4
				(pkAff->dwType == SKILL_CHUNKEON) ||		// 19
				(pkAff->dwType == SKILL_GYEONGGONG) ||	// 49
				(pkAff->dwType == SKILL_GWIGEOM) ||		// 63
				(pkAff->dwType == SKILL_TERROR) ||		// 64
				(pkAff->dwType == SKILL_JUMAGAP) ||		// 65
				(pkAff->dwType == SKILL_MUYEONG) ||		// 78
				(pkAff->dwType == SKILL_MANASHILED) ||	// 79
				(pkAff->dwType == SKILL_HOSIN) ||			// 94
				(pkAff->dwType == SKILL_REFLECT) ||			// 95
				(pkAff->dwType == SKILL_GICHEON) ||		// 96
				(pkAff->dwType == SKILL_KWAESOK) ||		// 110
				(pkAff->dwType == SKILL_JEUNGRYEOK)		// 111
			))
			{
				SaveAffectSkills(pkAff->dwType, pkAff->bApplyOn, pkAff->lApplyValue, pkAff->dwFlag, pkAff->lDuration, pkAff->lSPCost);
				//continue;
			}
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
			else if ((pkAff->dwType == AFFECT_DROP_BLOCK) || (pkAff->dwType == AFFECT_DROP_UNBLOCK)) {

				continue;
			}
#endif

#ifdef __AUTO_QUQUE_ATTACK__
			if (pkAff->dwType == AFFECT_AUTO_METIN_FARM)
			{

				continue;
			}
#endif
#ifdef ENABLE_GUILD_ATTRIBUTE
			if (AFFECT_GUILD_ATTRIBUTE == pkAff->dwType)
			{

				continue;
			}
#endif
			if (IsPC())
			{
				SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
			}
		}

		if (AffectSystem::Detach(entity, pkAff))
			AffectSystem::ComputeAffect(entity, *lease, false);
		if (!AffectState(entity))
			return;
	}

	NetworkSyncSystem::UpdatePacket(entity);
	if (!AffectState(entity))
		return;

	CheckMaximumPoints();
	if (!AffectState(entity))
		return;

	if (AffectSystem::Snapshot(entity).empty())
		AffectSystem::StopAffectEvent(entity);
}

int CHARACTER::ProcessAffect()
{
    return AffectSystem::ProcessAffect(GetEntityHandle());
}

void CHARACTER::SaveAffect()
{
	TPacketGDAddAffect p;

	for (const auto& lease : GetAffectContainer())
	{
		const CAffect* pkAff = lease.get();
		if (IS_NO_SAVE_AFFECT(pkAff->dwType))
			continue;

		LOG_TRACE("AFFECT_SAVE: {} {} {} {}", pkAff->dwType, static_cast<int>(pkAff->bApplyOn), pkAff->lApplyValue, pkAff->lDuration);

		p.dwPID			= GetPlayerID();
		p.elem.dwType		= pkAff->dwType;
		p.elem.bApplyOn		= pkAff->bApplyOn;
		p.elem.lApplyValue	= pkAff->lApplyValue;
		p.elem.dwFlag		= pkAff->dwFlag;
		p.elem.lDuration	= pkAff->lDuration;
		p.elem.lSPCost		= pkAff->lSPCost;
		db_clientdesc->DBPacket(HEADER_GD_ADD_AFFECT, 0, &p, sizeof(p));
	}
}

EVENTINFO(load_affect_login_event_info)
{
	uint32_t pid;
	uint32_t count;
	char* data;

	load_affect_login_event_info()
	: pid( 0 )
	, count( 0 )
	, data( nullptr )
	{
	}
};

EVENTFUNC(load_affect_login_event)
{
	load_affect_login_event_info* info = dynamic_cast<load_affect_login_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("load_affect_login_event_info> <Factor> Null pointer");
		return 0;
	}

	uint32_t dwPID = info->pid;
	auto* ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);

	if (!ch)
	{
		M2_DELETE_ARRAY(info->data);
		info->data = nullptr;
		return 0;
	}

	const entt::entity character = ch->GetEntityHandle();
	LPDESC d = ecs::PlayerRuntime::GetDesc(character);

	if (!d)
	{
		M2_DELETE_ARRAY(info->data);
		info->data = nullptr;
		return 0;
	}

	if (d->IsPhase(PHASE_HANDSHAKE) ||
			d->IsPhase(PHASE_LOGIN) ||
			d->IsPhase(PHASE_SELECT) ||
			d->IsPhase(PHASE_DEAD) ||
			d->IsPhase(PHASE_LOADING))
	{
		return PASSES_PER_SEC(1);
	}
	else if (d->IsPhase(PHASE_CLOSE))
	{
		M2_DELETE_ARRAY(info->data);
		info->data = nullptr;
		return 0;
	}
	else if (d->IsPhase(PHASE_GAME))
	{
		LOG_INFO("Affect Load by Event");
		LOG_ERROR("AFFECT_EVENT_LOAD_BEGIN pid={} name={} count={} data={} ch={}",
			ecs::PlayerRuntime::GetPlayerID(character), ecs::PlayerRuntime::GetName(character).data(), info->count, static_cast<const void*>(info->data), static_cast<const void*>(ch));
		ch->LoadAffect(info->count, (TPacketAffectElement*)info->data);
		LOG_ERROR("AFFECT_EVENT_LOAD_END pid={} name={} count={} data={}",
			ecs::PlayerRuntime::GetPlayerID(character), ecs::PlayerRuntime::GetName(character).data(), info->count, static_cast<const void*>(info->data));
		LOG_ERROR("AFFECT_EVENT_DATA_DELETE_BEGIN pid={} data={}", ecs::PlayerRuntime::GetPlayerID(character), static_cast<const void*>(info->data));
		M2_DELETE_ARRAY(info->data);
		info->data = nullptr;
		LOG_ERROR("AFFECT_EVENT_DATA_DELETE_END pid={} data={}", ecs::PlayerRuntime::GetPlayerID(character), static_cast<const void*>(info->data));
		return 0;
	}
	else
	{
		LOG_ERROR("input_db.cpp:quest_login_event INVALID PHASE pid {}", ecs::PlayerRuntime::GetPlayerID(character));
		M2_DELETE_ARRAY(info->data);
		info->data = nullptr;
		return 0;
	}
}

#ifdef ENABLE_BIOLOGIST_UI
void CHARACTER::CheckBiologistReward() {
	int stat = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), "biologist.stat");
	if (stat > 0) {
		for (int i = 0; i < stat; i++) {
			if (FindAffect(biologistMissionInfo[i][14])) {
				continue;
			}

			if (biologistMissionInfo[i][11] == 0) {
				int j = 0;
				for (int w = 0; w < 4; w++) {
					j += 2;
					uint8_t bApplyOn = biologistMissionInfo[i][j + 1];
					int32_t lApplyValue = biologistMissionInfo[i][j + 2];
					if (bApplyOn == APPLY_NONE || lApplyValue == 0) {
						continue;
					} else {
						bApplyOn = aApplyInfo[bApplyOn].bPointType;
						AddAffect(biologistMissionInfo[i][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
					}
				}
			} else {
				uint8_t bApplyOn = biologistMissionInfo[i][7];
				int32_t lApplyValue = biologistMissionInfo[i][8];
				if (bApplyOn != APPLY_NONE || lApplyValue != 0) {
					bApplyOn = aApplyInfo[bApplyOn].bPointType;
					AddAffect(biologistMissionInfo[i][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
				}
			}
		}
	}
}
#endif

void CHARACTER::LoadAffect(uint32_t dwCount, TPacketAffectElement * pElements)
{
	const auto entity = GetEntityHandle();
	if (!AffectState(entity) || (dwCount && !pElements))
		return;
	AffectState(entity)->isLoaded = false;
	LPDESC desc = GetDesc();
	LOG_ERROR("LOAD_AFFECT_BEGIN pid={} name={} count={} elements={} desc={}",
		GetPlayerID(), GetName(), dwCount, static_cast<const void*>(pElements), static_cast<const void*>(desc));

	if (!desc)
		return;
	if (!desc->IsPhase(PHASE_GAME))
	{
		if (test_server)
			LOG_INFO("LOAD_AFFECT: Creating Event", GetName(), dwCount);

		load_affect_login_event_info* info = AllocEventInfo<load_affect_login_event_info>();

		info->pid = GetPlayerID();
		info->count = dwCount;
		info->data = M2_NEW char[sizeof(TPacketAffectElement) * dwCount];
		memcpy(info->data, pElements, sizeof(TPacketAffectElement) * dwCount);

		event_create(load_affect_login_event, info, PASSES_PER_SEC(1));

		LOG_ERROR("LOAD_AFFECT_REQUEUE pid={} name={} count={} data={}",
			GetPlayerID(), GetName(), dwCount, static_cast<const void*>(info->data));
		return;
	}

	LOG_ERROR("LOAD_AFFECT_CLEAR_BEGIN pid={} name={} existing_affects={}", GetPlayerID(), GetName(), GetAffectContainer().size());
	ClearAffect(true);
	LOG_ERROR("LOAD_AFFECT_CLEAR_END pid={} name={} remaining_affects={}", GetPlayerID(), GetName(), GetAffectContainer().size());

	if (test_server)
		LOG_INFO("LOAD_AFFECT: {} count {}", GetName(), dwCount);

	TAffectFlag afOld = GetAffectFlags();

	int64_t lMovSpd = GetPoint(POINT_MOV_SPEED);
	int64_t lAttSpd = GetPoint(POINT_ATT_SPEED);
	const entt::entity character = GetEntityHandle();

	for (uint32_t i = 0; i < dwCount; ++i, ++pElements)
	{
		////// �������� �ε������ʴ´�.
		////if (pElements->dwType == SKILL_MUYEONG)
		////	continue;
		if (AFFECT_AUTO_HP_RECOVERY == pElements->dwType || AFFECT_AUTO_SP_RECOVERY == pElements->dwType)
		{
			const entt::entity item = ItemSystem::FindItemByID(
				character, pElements->dwFlag);
			if (!ItemSystem::IsValidItem(item))
				continue;

			ItemSystem::LockItem(item);
		}
#ifdef ENABLE_NEW_USE_POTION
		else if (AFFECT_AUTO_HP_RECOVERY2 == pElements->dwType || AFFECT_AUTO_SP_RECOVERY2 == pElements->dwType)
		{
			const entt::entity item = ItemSystem::FindItemByID(
				character, pElements->dwFlag);
			if (!ItemSystem::IsValidItem(item))
				continue;

			ItemSystem::LockItem(item);
		}
		else if ((pElements->dwType >= AFFECT_NEW_POTION1) && (pElements->dwType <= AFFECT_NEW_POTION31))
		{
			const entt::entity item = ItemSystem::FindItemByID(
				character, pElements->dwFlag);
			if (ItemSystem::IsValidItem(item))
				ItemSystem::LockItem(item);
			else
				continue;
		}
		//else if (pElements->dwType == AFFECT_NEW_POTION31)
		//{
		//	LPPARTY party = GetParty();
		//	if ((!party) || (party && GetPlayerID() != party->GetLeaderPID())) {
		//		LPITEM item = FindItemByID(pElements->dwFlag);
		//		if (item) {
		//			item->Lock(false);
		//			item->SetSocket(1, 0);
		//			RemoveAffect(AFFECT_NEW_POTION31);
		//		} else {
		//			continue;
		//		}
		//	}
		//}
#endif
#ifdef ENABLE_RECALL
#ifdef __PET_SYSTEM__
		else if (pElements->dwType == AFFECT_RECALL1)
		{
			const entt::entity item = ItemSystem::FindItemByID(
				character, pElements->dwFlag);
			if (ItemSystem::IsValidItem(item))
				ItemSystem::LockItem(item);
			else
				continue;
		}
#endif
#ifdef __NEWPET_SYSTEM__
		else if (pElements->dwType == AFFECT_RECALL2)
		{
			const entt::entity item = ItemSystem::FindItemByID(
				character, pElements->dwFlag);
			if (ItemSystem::IsValidItem(item))
				ItemSystem::LockItem(item);
			else
				continue;
		}
#endif
#endif

#ifdef ENABLE_SOUL_SYSTEM
		if(pElements->dwType == AFFECT_SOUL_RED || pElements->dwType == AFFECT_SOUL_BLUE)
		{
			const entt::entity item = ItemSystem::FindItemByID(
				character, static_cast<uint32_t>(pElements->lSPCost));

			if (!ItemSystem::IsValidItem(item))
				continue;

			ItemSystem::LockItem(item);
		}
#endif

		if (pElements->bApplyOn >= POINT_MAX_NUM)
		{
			LOG_ERROR("invalid affect data {} ApplyOn {} ApplyValue {}", GetName(), static_cast<int>(pElements->bApplyOn), pElements->lApplyValue);
			continue;
		}

		if (test_server)
		{
			LOG_INFO("Load Affect : Affect {} {} {}", GetName(), pElements->dwType, static_cast<int>(pElements->bApplyOn));
		}

		auto lease = AffectSystem::Attach(entity, {pElements->dwType,
			pElements->bApplyOn, pElements->lApplyValue, pElements->dwFlag,
			pElements->lDuration, pElements->lSPCost});
		if (!lease)
			return;
		CAffect* pkAff = lease.get();

		SendAffectAddPacket(GetDesc(), pkAff);

		AffectSystem::ComputeAffect(entity, *lease, true);
		if (!AffectState(entity))
			return;
	}
	LOG_ERROR("LOAD_AFFECT_LOOP_END pid={} name={} loaded_affects={}", GetPlayerID(), GetName(), GetAffectContainer().size());

	if ( CArenaManager::instance().IsArenaMap(GetMapIndex()) == true )
	{
		RemoveGoodAffect();
	}

#ifndef ENABLE_01092021
	RemoveAffect(AFFECT_MOUNT);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	RemoveAffect(AFFECT_MOUNT_BONUS);
	if (GetMapIndex() != 113 && CArenaManager::instance().IsArenaMap(GetMapIndex()) == false) {
		CheckMount();
	}
#endif
#endif

	if (afOld != GetAffectFlags() || lMovSpd != GetPoint(POINT_MOV_SPEED) || lAttSpd != GetPoint(POINT_ATT_SPEED))
	{

	NetworkSyncSystem::UpdatePacket(GetEntityHandle());
	}

	LOG_ERROR("LOAD_AFFECT_START_EVENT_BEGIN pid={} name={}", GetPlayerID(), GetName());
	StartAffectEvent();
	LOG_ERROR("LOAD_AFFECT_START_EVENT_END pid={} name={}", GetPlayerID(), GetName());

	if (!AffectState(entity))
		return;
	AffectState(entity)->isLoaded = true;

	// ��ȥ�� ���� �ε� �� �ʱ�ȭ
	LOG_ERROR("LOAD_AFFECT_DRAGONSOUL_BEGIN pid={} name={}", GetPlayerID(), GetName());
	DragonSoulSystem::Initialize(entity);
	LOG_ERROR("LOAD_AFFECT_DRAGONSOUL_END pid={} name={}", GetPlayerID(), GetName());

	// @fixme118 (regain affect hp/mp)
	if (!IsDead())
	{
		LOG_ERROR("LOAD_AFFECT_REFILL_POINTS_BEGIN pid={} name={}", GetPlayerID(), GetName());
		PointChange(POINT_HP, GetMaxHP() - GetHP());
		PointChange(POINT_SP, GetMaxSP() - ecs::PlayerRuntime::GetSP(GetEntityHandle()));
		LOG_ERROR("LOAD_AFFECT_REFILL_POINTS_END pid={} name={}", GetPlayerID(), GetName());
	}
#ifdef ENABLE_GUILD_ATTRIBUTE
	if (GetGuild())
		GetGuild()->GiveGuildBuff(GetEntityHandle());
	else
	{
		while (true)
		{
			CAffect* affect = FindAffect(AFFECT_GUILD_ATTRIBUTE);
			if (!affect)
				break;
			RemoveAffect(affect);
		}
	}
#endif

#ifdef ENABLE_BLOCK_MULTIFARM
	SetDropStatus();
#endif
#ifdef ENABLE_BIOLOGIST_UI
	LOG_ERROR("LOAD_AFFECT_BIOLOGIST_BEGIN pid={} name={}", GetPlayerID(), GetName());
	CheckBiologistReward();
	LOG_ERROR("LOAD_AFFECT_BIOLOGIST_END pid={} name={}", GetPlayerID(), GetName());
#endif
	LOG_ERROR("LOAD_AFFECT_END pid={} name={} count={} final_affects={}", GetPlayerID(), GetName(), dwCount, GetAffectContainer().size());
}

bool CHARACTER::AddAffect(uint32_t type, uint8_t applyOn, int32_t value, uint32_t flag,
    int32_t duration, int32_t spCost, bool overwrite, bool isCube)
{
    return AffectSystem::AddAffect(GetEntityHandle(), type, applyOn, value,
        flag, duration, spCost, overwrite, isCube);
}

void CHARACTER::RefreshAffect()
{
    AffectSystem::RefreshAffect(GetEntityHandle());
}

void CHARACTER::ComputeAffect(CAffect* affect, bool add)
{
    if (affect)
        AffectSystem::ComputeAffect(GetEntityHandle(), *affect, add);
}

bool CHARACTER::RemoveAffect(CAffect* affect)
{
    return AffectSystem::RemoveAffect(GetEntityHandle(), affect);
}

bool CHARACTER::RemoveAffect(uint32_t type)
{
    return AffectSystem::RemoveAffect(GetEntityHandle(), type);
}

bool CHARACTER::IsAffectFlag(uint32_t dwAff) const
{
	return AffectSystem::IsAffectFlag(GetEntityHandle(), dwAff);
}

void CHARACTER::RemoveGoodAffect()
{
    AffectSystem::RemoveGoodAffects(GetEntityHandle());
}

bool CHARACTER::IsGoodAffect(uint8_t bAffectType) const
{
	switch (bAffectType)
	{
		case (AFFECT_MOV_SPEED):
		case (AFFECT_ATT_SPEED):
		case (AFFECT_STR):
		case (AFFECT_DEX):
		case (AFFECT_INT):
		case (AFFECT_CON):
		case (AFFECT_CHINA_FIREWORK):

		case (SKILL_JEONGWI):
		case (SKILL_GEOMKYUNG):
		case (SKILL_CHUNKEON):
		case (SKILL_EUNHYUNG):
		case (SKILL_GYEONGGONG):
		case (SKILL_GWIGEOM):
		case (SKILL_TERROR):
		case (SKILL_JUMAGAP):
		case (SKILL_MANASHILED):
		case (SKILL_HOSIN):
		case (SKILL_REFLECT):
		case (SKILL_KWAESOK):
		case (SKILL_JEUNGRYEOK):
		case (SKILL_GICHEON):
			return true;
	}
	return false;
}

void CHARACTER::RemoveBadAffect()
{
    AffectSystem::RemoveBadAffects(GetEntityHandle());
}

void CHARACTER::SetPolymorph(uint32_t dwRaceNum, bool bMaintainStat)
{
	AffectSystem::SetPolymorph(GetEntityHandle(), dwRaceNum, bMaintainStat);
}
