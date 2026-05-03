#include "../../stdafx.h"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"

#include "AffectSystem.hpp"
#include "QuestSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "MovementSystem.hpp"

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
#include "../AIHelpers.hpp"
#include "../EntityFactory.hpp"
#include "ItemSystem.hpp"
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/status_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include <Core/Logging.hpp>

namespace {

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);

const int poison_damage_rate[MOB_RANK_MAX_NUM] = {
    80, 50, 40, 30, 25, 1
};

int GetPoisonDamageRate(LegacyCharHandle ch)
{
    int iRate = ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)) ? 50 : poison_damage_rate[ch->GetMobRank()];
    iRate = MAX(0, iRate - ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_POISON_REDUCE));
    return iRate;
}

EVENTINFO(TPoisonEventInfo)
{
    DynamicCharacterPtr ch;
    int count;
    uint32_t attacker_pid;

    TPoisonEventInfo()
        : ch()
        , count(0)
        , attacker_pid(0)
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

    auto* ch = info->ch.Get();
    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkPoisonEvent
    auto* pkAttacker = CHARACTER_MANAGER::instance().FindByPID(info->attacker_pid);
    int dam = ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) * GetPoisonDamageRate(ch) / 1000;
    if (test_server) {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_NOTICE, "Poison Damage %d", dam);
    }

    const entt::entity poisonEntity = AIHelpers::EcsOf(ch);
    if (poisonEntity != entt::null) {
        g_dispatcher.trigger(ecs::EvPoisonApplied { poisonEntity, dam });
    }

    if (ch->Damage(pkAttacker, dam, DAMAGE_TYPE_POISON)) {
        ch->m_pkPoisonEvent = nullptr;
        return 0;
    }

    --info->count;
    if (info->count) {
        return PASSES_PER_SEC(3);
    }

    ch->m_pkPoisonEvent = nullptr;
    return 0;
}

#ifdef ENABLE_WOLFMAN_CHARACTER
const int bleeding_damage_rate[MOB_RANK_MAX_NUM] = {
    80, 50, 40, 30, 25, 1
};

int GetBleedingDamageRate(LegacyCharHandle ch)
{
    int iRate = ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)) ? 50 : bleeding_damage_rate[ch->GetMobRank()];
    iRate = MAX(0, iRate - ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_BLEEDING_REDUCE));
#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_ITEM_BLEEDING_AS_POISON)
    iRate = MAX(0, iRate - ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_POISON_REDUCE));
#endif
    return iRate;
}

EVENTINFO(TBleedingEventInfo)
{
    DynamicCharacterPtr ch;
    int count;
    uint32_t attacker_pid;

    TBleedingEventInfo()
        : ch()
        , count(0)
        , attacker_pid(0)
    {
    }
};

EVENTFUNC(bleeding_event)
{
    TBleedingEventInfo* info = dynamic_cast<TBleedingEventInfo*>(event->info);
    if (info == nullptr) {
        LOG_ERROR("bleeding_event> <Factor> Null pointer");
        return 0;
    }

    auto* ch = info->ch.Get();
    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkBleedingEvent
    auto* pkAttacker = CHARACTER_MANAGER::instance().FindByPID(info->attacker_pid);
    int dam = ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) * GetBleedingDamageRate(ch) / 1000;
    if (test_server) {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_NOTICE, "Bleeding Damage %d", dam);
    }

    const entt::entity bleedingEntity = AIHelpers::EcsOf(ch);
    if (bleedingEntity != entt::null) {
        g_dispatcher.trigger(ecs::EvBleedingApplied { bleedingEntity, dam });
    }

    if (ch->Damage(pkAttacker, dam, DAMAGE_TYPE_BLEEDING)) {
        ch->m_pkBleedingEvent = nullptr;
        return 0;
    }

    --info->count;
    if (info->count) {
        return PASSES_PER_SEC(3);
    }

    ch->m_pkBleedingEvent = nullptr;
    return 0;
}
#endif

EVENTINFO(TFireEventInfo)
{
    DynamicCharacterPtr ch;
    int count;
    int amount;
    uint32_t attacker_pid;

    TFireEventInfo()
        : ch()
        , count(0)
        , amount(0)
        , attacker_pid(0)
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

    auto* ch = info->ch.Get();
    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkFireEvent
    auto* pkAttacker = CHARACTER_MANAGER::instance().FindByPID(info->attacker_pid);
    int dam = info->amount;
    if (test_server) {
        ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_NOTICE, "Fire Damage %d", dam);
    }

    const entt::entity fireEntity = AIHelpers::EcsOf(ch);
    if (fireEntity != entt::null) {
        g_dispatcher.trigger(ecs::EvFireApplied { fireEntity, dam });
    }

    if (ch->Damage(pkAttacker, dam, DAMAGE_TYPE_FIRE)) {
        ch->m_pkFireEvent = nullptr;
        return 0;
    }

    --info->count;
    if (info->count) {
        return PASSES_PER_SEC(3);
    }

    ch->m_pkFireEvent = nullptr;
    return 0;
}

int poison_level_adjust[9] = {
    100, 90, 80, 70, 50, 30, 10, 5, 0
};

#ifdef ENABLE_WOLFMAN_CHARACTER
int bleeding_level_adjust[9] = {
    100, 90, 80, 70, 50, 30, 10, 5, 0
};
#endif

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

void SyncAffectList(entt::entity e, LegacyCharHandle ch)
{
    if (!ch || e == entt::null || !g_registry.valid(e)) {
        return;
    }

    auto& affectList = g_registry.get_or_emplace<ecs::AffectList>(e);
    affectList.affects.assign(ch->GetAffectContainer().begin(), ch->GetAffectContainer().end());
    affectList.flags = ch->GetAffectFlags();
    affectList.isLoaded = true;
}

} // namespace

namespace AffectSystem {

void ApplyFire(entt::entity target, entt::entity attacker, int amount, int count)
{
    MarkFire(target, true);

    auto* ch = LegacyCharOf(target);
    if (!ch || ch->m_pkFireEvent) {
        return;
    }

    ch->AddAffect(AFFECT_FIRE, POINT_NONE, 0, AFF_FIRE, count * 3 + 1, 0, true);

    TFireEventInfo* info = AllocEventInfo<TFireEventInfo>();
    info->ch = ch;
    info->count = count;
    info->amount = amount;

    if (auto* pkAttacker = LegacyCharOf(attacker)) {
        info->attacker_pid = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkAttacker));
    } else {
        info->attacker_pid = 0;
    }

    ch->m_pkFireEvent = event_create(fire_event, info, 1);
}

void RemoveFire(entt::entity e)
{
    MarkFire(e, false);

    auto* ch = LegacyCharOf(e);
    if (!ch) {
        return;
    }

    ch->RemoveAffect(AFFECT_FIRE);
    event_cancel(&ch->m_pkFireEvent);
}

void ApplyPoison(entt::entity target, entt::entity attacker)
{
    MarkPoison(target, true);

    auto* ch = LegacyCharOf(target);
    if (!ch || ch->m_pkPoisonEvent) {
        return;
    }

    if (ch->m_bHasPoisoned && !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))) {
        return;
    }

#ifdef ENABLE_WOLFMAN_CHARACTER
    if (ch->m_pkBleedingEvent) {
        return;
    }

    if (ch->m_bHasBled && !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))) {
        return;
    }
#endif

    auto* pkAttacker = LegacyCharOf(attacker);
    if (pkAttacker && ecs::PointSystem::GetLevel(AIHelpers::EcsOf(pkAttacker)) < ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch))) {
        int delta = ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) - ecs::PointSystem::GetLevel(AIHelpers::EcsOf(pkAttacker));
        if (delta > 8) {
            delta = 8;
        }

        if (number(1, 100) > poison_level_adjust[delta]) {
            return;
        }
    }

    ch->m_bHasPoisoned = true;
    ch->AddAffect(AFFECT_POISON, POINT_NONE, 0, AFF_POISON, POISON_LENGTH + 1, 0, true);

    TPoisonEventInfo* info = AllocEventInfo<TPoisonEventInfo>();
    info->ch = ch;
    info->count = 10;
    info->attacker_pid = pkAttacker ? ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkAttacker)) : 0;
    ch->m_pkPoisonEvent = event_create(poison_event, info, 1);

    if (test_server && pkAttacker) {
        char buf[256];
        snprintf(buf, sizeof(buf), "POISON %s -> %s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkAttacker)).data(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
        ecs::ChatSystem::Send(AIHelpers::EcsOf(pkAttacker), CHAT_TYPE_INFO, "%s", buf);
    }
}

void RemovePoison(entt::entity e)
{
    MarkPoison(e, false);

    auto* ch = LegacyCharOf(e);
    if (!ch) {
        return;
    }

    ch->RemoveAffect(AFFECT_POISON);
    event_cancel(&ch->m_pkPoisonEvent);
}

#ifdef ENABLE_WOLFMAN_CHARACTER
void ApplyBleeding(entt::entity target, entt::entity attacker)
{
    MarkBleeding(target, true);

    auto* ch = LegacyCharOf(target);
    if (!ch || ch->m_pkBleedingEvent) {
        return;
    }

    if (ch->m_bHasBled && !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))) {
        return;
    }

    if (ch->m_pkPoisonEvent) {
        return;
    }

    if (ch->m_bHasPoisoned && !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))) {
        return;
    }

    auto* pkAttacker = LegacyCharOf(attacker);
    if (pkAttacker && ecs::PointSystem::GetLevel(AIHelpers::EcsOf(pkAttacker)) < ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch))) {
        int delta = ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)) - ecs::PointSystem::GetLevel(AIHelpers::EcsOf(pkAttacker));
        if (delta > 8) {
            delta = 8;
        }

        if (number(1, 100) > bleeding_level_adjust[delta]) {
            return;
        }
    }

    ch->m_bHasBled = true;
    ch->AddAffect(AFFECT_BLEEDING, POINT_NONE, 0, AFF_BLEEDING, BLEEDING_LENGTH + 1, 0, true);

    TBleedingEventInfo* info = AllocEventInfo<TBleedingEventInfo>();
    info->ch = ch;
    info->count = 10;
    info->attacker_pid = pkAttacker ? ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkAttacker)) : 0;
    ch->m_pkBleedingEvent = event_create(bleeding_event, info, 1);

    if (test_server && pkAttacker) {
        char buf[256];
        snprintf(buf, sizeof(buf), "BLEEDING %s -> %s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkAttacker)).data(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
        ecs::ChatSystem::Send(AIHelpers::EcsOf(pkAttacker), CHAT_TYPE_INFO, "%s", buf);
    }
}

void RemoveBleeding(entt::entity e)
{
    MarkBleeding(e, false);

    auto* ch = LegacyCharOf(e);
    if (!ch) {
        return;
    }

    ch->RemoveAffect(AFFECT_BLEEDING);
    event_cancel(&ch->m_pkBleedingEvent);
}
#else
void ApplyBleeding(entt::entity, entt::entity)
{
}

void RemoveBleeding(entt::entity)
{
}
#endif

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
            ch->ApplyPoint(aiMobEnchantApplyIdx[i], table->cEnchants[i]);
        }
    }

#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_MOB_BLEEDING_AS_POISON)
    if (table->cEnchants[MOB_ENCHANT_POISON] != 0) {
        ch->ApplyPoint(APPLY_BLEEDING_PCT, table->cEnchants[MOB_ENCHANT_POISON] / 50);
    }
#endif

    for (int i = 0; i < MOB_RESISTS_MAX_NUM; ++i) {
        if (table->cResists[i] != 0) {
            ch->ApplyPoint(aiMobResistsApplyIdx[i], table->cResists[i]);
        }
    }

#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_MOB_CLAW_AS_DAGGER)
    if (table->cResists[MOB_RESIST_DAGGER] != 0) {
        ch->ApplyPoint(APPLY_RESIST_CLAW, table->cResists[MOB_RESIST_DAGGER]);
    }
#endif

#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_MOB_BLEEDING_AS_POISON)
    if (table->cResists[MOB_RESIST_POISON] != 0) {
        ch->ApplyPoint(APPLY_BLEEDING_REDUCE, table->cResists[MOB_RESIST_POISON]);
    }
#endif

    if (target != entt::null && g_registry.valid(target)) {
        g_registry.emplace_or_replace<ecs::DirtyTag>(target);
    }
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

    for (auto* affect : affectList->affects) {
        if (!affect) {
            continue;
        }

        if (affect->dwType == type && (apply == APPLY_NONE || affect->bApplyOn == apply)) {
            return affect;
        }
    }

    return nullptr;
}

bool IsAffectFlag(entt::entity e, uint32_t flag)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return false;
    }

    const auto* affectList = g_registry.try_get<ecs::AffectList>(e);
    return affectList ? affectList->flags.IsSet(flag) : false;
}

bool AddAffect(entt::entity e, uint32_t type, uint8_t applyOn, int32_t applyValue,
               uint32_t flag, int32_t duration, int32_t spCost, bool overwrite,
               bool isCube)
{
    auto* ch = LegacyCharOf(e);
    if (!ch) {
        return false;
    }

    const bool result = ch->AddAffect(type, applyOn, applyValue, flag, duration, spCost, overwrite, isCube);
    SyncAffectList(e, ch);
    return result;
}

bool RemoveAffect(entt::entity e, uint32_t type)
{
    auto* ch = LegacyCharOf(e);
    if (!ch) {
        return false;
    }

    const bool result = ch->RemoveAffect(type);
    SyncAffectList(e, ch);
    return result;
}

bool RemoveAffect(entt::entity e, CAffect* affect)
{
    auto* ch = LegacyCharOf(e);
    if (!ch || !affect) {
        return false;
    }

    const bool result = ch->RemoveAffect(affect);
    SyncAffectList(e, ch);
    return result;
}

void ClearAffect(entt::entity e, bool save)
{
    auto* ch = LegacyCharOf(e);
    if (!ch) {
        return;
    }

    ch->ClearAffect(save);
    SyncAffectList(e, ch);
}

void RefreshAffect(entt::entity e)
{
    auto* ch = LegacyCharOf(e);
    if (!ch) {
        return;
    }

    ch->RefreshAffect();
    SyncAffectList(e, ch);
}

void UpdateAffect(entt::registry& reg, uint32_t tick)
{
    if ((tick % PASSES_PER_SEC(1)) != 0) {
        return;
    }

    auto view = reg.view<ecs::AffectList, ecs::LegacyCharPtr>();
    view.each([&](entt::entity e, ecs::AffectList& affectList, const ecs::LegacyCharPtr& legacy) {
        if (affectList.affects.empty() && affectList.skillAffects.empty()) {
            return;
        }

        auto* ch = legacy.ptr;
        if (!ch) {
            return;
        }

        SyncAffectList(e, ch);
    });
}

} // namespace AffectSystem

void CHARACTER::AttackedByFire(LPCHARACTER pkAttacker, int amount, int count)
{
    AffectSystem::ApplyFire
        (AIHelpers::EcsOf(this),
        pkAttacker ? AIHelpers::EcsOf(pkAttacker) : entt::null,
        amount,
        count);
}

void CHARACTER::AttackedByPoison(LPCHARACTER pkAttacker)
{
    AffectSystem::ApplyPoison(
        AIHelpers::EcsOf(this),
        pkAttacker ? AIHelpers::EcsOf(pkAttacker) : entt::null);
}

#ifdef ENABLE_WOLFMAN_CHARACTER
void CHARACTER::AttackedByBleeding(LPCHARACTER pkAttacker)
{
    AffectSystem::ApplyBleeding(
        AIHelpers::EcsOf(this),
        pkAttacker ? AIHelpers::EcsOf(pkAttacker) : entt::null);
}
#endif

void CHARACTER::RemoveFire()
{
    AffectSystem::RemoveFire(AIHelpers::EcsOf(this));
}

void CHARACTER::RemovePoison()
{
    AffectSystem::RemovePoison(AIHelpers::EcsOf(this));
}

#ifdef ENABLE_WOLFMAN_CHARACTER
void CHARACTER::RemoveBleeding()
{
    AffectSystem::RemoveBleeding(AIHelpers::EcsOf(this));
}
#endif

bool CHARACTER::IsImmune(uint32_t dwImmuneFlag)
{
    return AffectSystem::IsImmune(AIHelpers::EcsOf(this), dwImmuneFlag);
}

void CHARACTER::ApplyMobAttribute(const TMobTable* table)
{
    AffectSystem::ApplyMobAttribute(AIHelpers::EcsOf(this), table);
}

void AffectSystem_Update(entt::registry& reg, uint32_t tick)
{
    if ((tick % PASSES_PER_SEC(1)) != 0) {
        return;
    }

    AffectSystem::UpdateAffect(reg, tick);

    // Legacy CHARACTER::ProcessAffect owns CAffect* lifetime. The ECS
    // component is a mirror only; releasing those pointers here races the
    // legacy affect event and corrupts the heap on login/tick boundaries.
    return;

    auto view = reg.view<ecs::AffectList, ecs::LegacyCharPtr>();
    view.each([&](const entt::entity entity, ecs::AffectList& affectList, const ecs::LegacyCharPtr& legacy) {
        (void)legacy;
        if (affectList.affects.empty() && affectList.skillAffects.empty()) {
            return;
        }
        bool dirty = false;

        for (auto it = affectList.affects.begin(); it != affectList.affects.end();) {
            CAffect* affect = *it;
            if (!affect) {
                it = affectList.affects.erase(it);
                dirty = true;
                continue;
            }

            if (affect->lDuration != INFINITE_AFFECT_DURATION) {
                --affect->lDuration;
            }

            if (affect->lDuration > 0 || affect->lDuration == INFINITE_AFFECT_DURATION) {
                ++it;
                continue;
            }

            g_dispatcher.trigger(ecs::EvAffectExpired { entity, affect->dwType });
            CAffect::Release(affect);
            it = affectList.affects.erase(it);
            dirty = true;
        }

        affectList.skillAffects.erase(
            std::remove_if(affectList.skillAffects.begin(), affectList.skillAffects.end(), [&](TAffectSkills& affect) {
                if (affect.lDuration != INFINITE_AFFECT_DURATION) {
                    --affect.lDuration;
                }

                if (affect.lDuration > 0 || affect.lDuration == INFINITE_AFFECT_DURATION) {
                    return false;
                }

                g_dispatcher.trigger(ecs::EvAffectExpired { entity, affect.dwType });
                dirty = true;
                return true;
            }),
            affectList.skillAffects.end());

        if (dirty) {
            reg.emplace_or_replace<ecs::DirtyTag>(entity);
        }
    });
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
	auto it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		CAffect * pkAffect = *it++;

		if (pkAffect->dwType == dwType && (bApply == APPLY_NONE || bApply == pkAffect->bApplyOn))
			return pkAffect;
	}

	return nullptr;
}

EVENTFUNC(affect_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("affect_event> <Factor> Null pointer");
		return 0;
	}

	auto* ch = info->ch.Get();

	if (ch == nullptr) { // <Factor>
		return 0;
	}

	if (!ch->UpdateAffect())
		return 0;
	else
		return passes_per_sec; // 1��
}

bool CHARACTER::UpdateAffect()
{
#ifdef ENABLE_BUG_FIXES
	if (!GetWear(WEAR_WEAPON)) {
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
		if (GetMaxSP() <= GetSP())
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

	AutoRecoveryItemProcess(AFFECT_AUTO_HP_RECOVERY);
	AutoRecoveryItemProcess(AFFECT_AUTO_SP_RECOVERY);
#ifdef ENABLE_NEW_USE_POTION
	AutoRecoveryItemProcess(AFFECT_AUTO_HP_RECOVERY2);
	AutoRecoveryItemProcess(AFFECT_AUTO_SP_RECOVERY2);
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
	if (ProcessAffect())
		if (GetPoint(POINT_HP_RECOVERY) == 0 && GetPoint(POINT_SP_RECOVERY) == 0 && GetStamina() == GetMaxStamina())
		{
			m_pkAffectEvent = nullptr;
			return false;
		}

	return true;
}

void CHARACTER::StartAffectEvent()
{
	if (m_pkAffectEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();
	info->ch = this;
	m_pkAffectEvent = event_create(affect_event, info, passes_per_sec);
	LOG_TRACE("StartAffectEvent {} {} {}", GetName(), static_cast<const void*>(this), static_cast<const void*>(get_pointer(m_pkAffectEvent)));
}

#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
void CHARACTER::ClearAffectSkills() {
	size_t j = m_list_pkAffectSkills.size();
	if (j < 1)
		return;

	m_list_pkAffectSkills.erase(m_list_pkAffectSkills.begin(), m_list_pkAffectSkills.end());
	m_list_pkAffectSkills.shrink_to_fit();
}

void CHARACTER::SaveAffectSkills(uint32_t dwType, uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag, int32_t lDuration, int32_t lSPCost) {
	TAffectSkills t;
	t.dwType = dwType;
	t.bApplyOn = bApplyOn;
	t.lApplyValue = lApplyValue;
	t.dwFlag = dwFlag;
	t.lDuration = lDuration;
	t.lSPCost = lSPCost;
	t.dwTime = get_global_time();

	m_list_pkAffectSkills.push_back(t);
}

void CHARACTER::LoadAffectSkills() {
	size_t j = m_list_pkAffectSkills.size();
	if (j < 1)
		return;

	int32_t lDuration = 0;
	for (size_t i = 0; i < j; ++i) {
		lDuration = m_list_pkAffectSkills[i].lDuration - (get_global_time() - m_list_pkAffectSkills[i].dwTime);
		if (lDuration > 0)
			AddAffect(m_list_pkAffectSkills[i].dwType, m_list_pkAffectSkills[i].bApplyOn, m_list_pkAffectSkills[i].lApplyValue, m_list_pkAffectSkills[i].dwFlag, lDuration, m_list_pkAffectSkills[i].lSPCost, false);
	}

	ClearAffectSkills();
}
#endif

void CHARACTER::ClearAffect(bool bSave)
{
	TAffectFlag afOld = m_afAffectFlag;
	uint16_t	wMovSpd = GetPoint(POINT_MOV_SPEED);
	uint16_t	wAttSpd = GetPoint(POINT_ATT_SPEED);

	auto it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		CAffect * pkAff = *it;

		if (bSave)
		{
#ifdef ENABLE_SOUL_SYSTEM
			if ( pkAff->dwType == AFFECT_SOUL_RED || pkAff->dwType == AFFECT_SOUL_BLUE )
			{
				++it;
				continue;
			}
#endif

			if ( IS_NO_CLEAR_ON_DEATH_AFFECT(pkAff->dwType) || IS_NO_SAVE_AFFECT(pkAff->dwType) )
			{
				++it;
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
				//++it;
				//continue;
			}
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
			else if ((pkAff->dwType == AFFECT_DROP_BLOCK) || (pkAff->dwType == AFFECT_DROP_UNBLOCK)) {
				++it;
				continue;
			}
#endif

#ifdef __AUTO_QUQUE_ATTACK__
			if (pkAff->dwType == AFFECT_AUTO_METIN_FARM)
			{
				++it;
				continue;
			}
#endif
#ifdef ENABLE_GUILD_ATTRIBUTE
			if (AFFECT_GUILD_ATTRIBUTE == pkAff->dwType)
			{
				++it;
				continue;
			}
#endif
			if (IsPC())
			{
				SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
			}
		}

		ComputeAffect(pkAff, false);

		it = m_list_pkAffect.erase(it);
		CAffect::Release(pkAff);
	}

	if (afOld != m_afAffectFlag ||
			wMovSpd != GetPoint(POINT_MOV_SPEED) ||
			wAttSpd != GetPoint(POINT_ATT_SPEED))
		SyncAffectList(AIHelpers::EcsOf(this), this);
	NetworkSyncSystem::UpdatePacket(AIHelpers::EcsOf(this));

	CheckMaximumPoints();

	if (m_list_pkAffect.empty())
		event_cancel(&m_pkAffectEvent);
}

int CHARACTER::ProcessAffect()
{
	bool	bDiff	= false;
	CAffect	*pkAff;

	//
	// �����̾� ó��
	//
	for (int i = 0; i <= PREMIUM_MAX_NUM; ++i)
	{
		int aff_idx = i + AFFECT_PREMIUM_START;

		pkAff = FindAffect(aff_idx);

		if (!pkAff)
			continue;

		int remain = GetPremiumRemainSeconds(i);

		if (remain < 0)
		{
			RemoveAffect(aff_idx);
			bDiff = true;
		}
		else
			pkAff->lDuration = remain + 1;
	}

#ifdef ENABLE_VOTE_FOR_BONUS
	pkAff = FindAffect(AFFECT_VOTEFORBONUS);
	if (pkAff)
	{
		int32_t remain = pkAff->lDuration - get_global_time();
		if (remain <= 0)
		{
			RemoveAffect(AFFECT_VOTEFORBONUS);
			bDiff = true;
		}
	}
#endif

#ifdef ENABLE_BATTLE_PASS
	pkAff = FindAffect(AFFECT_BATTLE_PASS);
	if (pkAff)
	{
		int remain = GetBattlePassEndTime();

		if (remain < 0)
		{
			RemoveAffect(AFFECT_BATTLE_PASS);
			m_dwBattlePassEndTime = 0;
			bDiff = true;
		}
		else
			pkAff->lDuration = remain + 1;
	}
#endif

	////////// HAIR_AFFECT
	pkAff = FindAffect(AFFECT_HAIR);
	if (pkAff)
	{
		// IF HAIR_LIMIT_TIME() < CURRENT_TIME()
		if ( ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(this), "hair.limit_time") < get_global_time())
		{
			// SET HAIR NORMAL
			this->SetPart(PART_HAIR, 0);
			// REMOVE HAIR AFFECT
			RemoveAffect(AFFECT_HAIR);
		}
		else
		{
			// INCREASE AFFECT DURATION
			++(pkAff->lDuration);
		}
	}
	////////// HAIR_AFFECT
	//

	CHorseNameManager::instance().Validate(this);

	TAffectFlag afOld = m_afAffectFlag;
	int64_t lMovSpd = GetPoint(POINT_MOV_SPEED);
	int64_t lAttSpd = GetPoint(POINT_ATT_SPEED);


	auto it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		pkAff = *it;

		bool bEnd = false;

		if (pkAff->dwType >= GUILD_SKILL_START && pkAff->dwType <= GUILD_SKILL_END)
		{
			if (!GetGuild() || !GetGuild()->UnderAnyWar())
				bEnd = true;
		}

#ifdef ENABLE_SOUL_SYSTEM
		if (pkAff->lSPCost > 0 && pkAff->dwType != AFFECT_SOUL_RED && pkAff->dwType != AFFECT_SOUL_BLUE)
#else
		if (pkAff->lSPCost > 0)
#endif
		{
			if (GetSP() < pkAff->lSPCost)
				bEnd = true;
			else
				PointChange(POINT_SP, -pkAff->lSPCost);
		}

		// AFFECT_DURATION_BUG_FIX
		// ���� ȿ�� �����۵� �ð��� ���δ�.
		// �ð��� �ſ� ũ�� ��� ������ ��� ���� ���̶� ������.
		if ( --pkAff->lDuration <= 0 )
		{
			bEnd = true;
		}
		// END_AFFECT_DURATION_BUG_FIX

		if (bEnd)
		{
			it = m_list_pkAffect.erase(it);
			ComputeAffect(pkAff, false);
			bDiff = true;
			if (IsPC())
			{
				SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
			}

			CAffect::Release(pkAff);

			continue;
		}

		++it;
	}

	if (bDiff)
	{
		if (afOld != m_afAffectFlag ||
				lMovSpd != GetPoint(POINT_MOV_SPEED) ||
				lAttSpd != GetPoint(POINT_ATT_SPEED))
		{
			SyncAffectList(AIHelpers::EcsOf(this), this);
	NetworkSyncSystem::UpdatePacket(AIHelpers::EcsOf(this));
		}

		CheckMaximumPoints();
	}

	if (m_list_pkAffect.empty())
		return true;

	return false;
}

void CHARACTER::SaveAffect()
{
	TPacketGDAddAffect p;

	auto it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		CAffect * pkAff = *it++;
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

	LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));

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
			ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), info->count, static_cast<const void*>(info->data), static_cast<const void*>(ch));
		ch->LoadAffect(info->count, (TPacketAffectElement*)info->data);
		LOG_ERROR("AFFECT_EVENT_LOAD_END pid={} name={} count={} data={}",
			ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), info->count, static_cast<const void*>(info->data));
		LOG_ERROR("AFFECT_EVENT_DATA_DELETE_BEGIN pid={} data={}", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), static_cast<const void*>(info->data));
		M2_DELETE_ARRAY(info->data);
		info->data = nullptr;
		LOG_ERROR("AFFECT_EVENT_DATA_DELETE_END pid={} data={}", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)), static_cast<const void*>(info->data));
		return 0;
	}
	else
	{
		LOG_ERROR("input_db.cpp:quest_login_event INVALID PHASE pid {}", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
		M2_DELETE_ARRAY(info->data);
		info->data = nullptr;
		return 0;
	}
}

#ifdef ENABLE_BIOLOGIST_UI
void CHARACTER::CheckBiologistReward() {
	int stat = GetQuestFlag("biologist.stat");
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
	m_bIsLoadedAffect = false;
	LPDESC desc = GetDesc();
	LOG_ERROR("LOAD_AFFECT_BEGIN pid={} name={} count={} elements={} desc={}",
		GetPlayerID(), GetName(), dwCount, static_cast<const void*>(pElements), static_cast<const void*>(desc));

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

	LOG_ERROR("LOAD_AFFECT_CLEAR_BEGIN pid={} name={} existing_affects={}", GetPlayerID(), GetName(), m_list_pkAffect.size());
	ClearAffect(true);
	LOG_ERROR("LOAD_AFFECT_CLEAR_END pid={} name={} remaining_affects={}", GetPlayerID(), GetName(), m_list_pkAffect.size());

	if (test_server)
		LOG_INFO("LOAD_AFFECT: {} count {}", GetName(), dwCount);

	TAffectFlag afOld = m_afAffectFlag;

	int64_t lMovSpd = GetPoint(POINT_MOV_SPEED);
	int64_t lAttSpd = GetPoint(POINT_ATT_SPEED);

	for (uint32_t i = 0; i < dwCount; ++i, ++pElements)
	{
		////// �������� �ε������ʴ´�.
		////if (pElements->dwType == SKILL_MUYEONG)
		////	continue;
		if (AFFECT_AUTO_HP_RECOVERY == pElements->dwType || AFFECT_AUTO_SP_RECOVERY == pElements->dwType)
		{
			LPITEM item = FindItemByID( pElements->dwFlag );
			if (nullptr == item)
				continue;

			ItemSystem::LockItem(EntityFactory::CreateItemEntity(g_registry, item));
		}
#ifdef ENABLE_NEW_USE_POTION
		else if (AFFECT_AUTO_HP_RECOVERY2 == pElements->dwType || AFFECT_AUTO_SP_RECOVERY2 == pElements->dwType)
		{
			LPITEM item = FindItemByID( pElements->dwFlag );
			if (nullptr == item)
				continue;

			ItemSystem::LockItem(EntityFactory::CreateItemEntity(g_registry, item));
		}
		else if ((pElements->dwType >= AFFECT_NEW_POTION1) && (pElements->dwType <= AFFECT_NEW_POTION31))
		{
			LPITEM item = FindItemByID(pElements->dwFlag);
			if (item)
				ItemSystem::LockItem(EntityFactory::CreateItemEntity(g_registry, item));
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
			LPITEM item = FindItemByID(pElements->dwFlag);
			if (item)
				ItemSystem::LockItem(EntityFactory::CreateItemEntity(g_registry, item));
			else
				continue;
		}
#endif
#ifdef __NEWPET_SYSTEM__
		else if (pElements->dwType == AFFECT_RECALL2)
		{
			LPITEM item = FindItemByID(pElements->dwFlag);
			if (item)
				ItemSystem::LockItem(EntityFactory::CreateItemEntity(g_registry, item));
			else
				continue;
		}
#endif
#endif

#ifdef ENABLE_SOUL_SYSTEM
		if(pElements->dwType == AFFECT_SOUL_RED || pElements->dwType == AFFECT_SOUL_BLUE)
		{
			LPITEM item = FindItemByID( pElements->lSPCost );

			if (!item)
				continue;

			ItemSystem::LockItem(EntityFactory::CreateItemEntity(g_registry, item));
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

		CAffect* pkAff = CAffect::Acquire();
		m_list_pkAffect.push_back(pkAff);

		pkAff->dwType		= pElements->dwType;
		pkAff->bApplyOn		= pElements->bApplyOn;
		pkAff->lApplyValue	= pElements->lApplyValue;
		pkAff->dwFlag		= pElements->dwFlag;
		pkAff->lDuration	= pElements->lDuration;
		pkAff->lSPCost		= pElements->lSPCost;

		SendAffectAddPacket(GetDesc(), pkAff);

		ComputeAffect(pkAff, true);


	}
	LOG_ERROR("LOAD_AFFECT_LOOP_END pid={} name={} loaded_affects={}", GetPlayerID(), GetName(), m_list_pkAffect.size());

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

	if (afOld != m_afAffectFlag || lMovSpd != GetPoint(POINT_MOV_SPEED) || lAttSpd != GetPoint(POINT_ATT_SPEED))
	{
		SyncAffectList(AIHelpers::EcsOf(this), this);
	NetworkSyncSystem::UpdatePacket(AIHelpers::EcsOf(this));
	}

	LOG_ERROR("LOAD_AFFECT_START_EVENT_BEGIN pid={} name={}", GetPlayerID(), GetName());
	StartAffectEvent();
	LOG_ERROR("LOAD_AFFECT_START_EVENT_END pid={} name={}", GetPlayerID(), GetName());

	m_bIsLoadedAffect = true;

	// ��ȥ�� ���� �ε� �� �ʱ�ȭ
	LOG_ERROR("LOAD_AFFECT_DRAGONSOUL_BEGIN pid={} name={}", GetPlayerID(), GetName());
	DragonSoul_Initialize();
	LOG_ERROR("LOAD_AFFECT_DRAGONSOUL_END pid={} name={}", GetPlayerID(), GetName());

	// @fixme118 (regain affect hp/mp)
	if (!IsDead())
	{
		LOG_ERROR("LOAD_AFFECT_REFILL_POINTS_BEGIN pid={} name={}", GetPlayerID(), GetName());
		PointChange(POINT_HP, GetMaxHP() - GetHP());
		PointChange(POINT_SP, GetMaxSP() - GetSP());
		LOG_ERROR("LOAD_AFFECT_REFILL_POINTS_END pid={} name={}", GetPlayerID(), GetName());
	}
#ifdef ENABLE_GUILD_ATTRIBUTE
	if (GetGuild())
		GetGuild()->GiveGuildBuff(this);
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
	LOG_ERROR("LOAD_AFFECT_END pid={} name={} count={} final_affects={}", GetPlayerID(), GetName(), dwCount, m_list_pkAffect.size());
}

bool CHARACTER::AddAffect(uint32_t dwType, uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag, int32_t lDuration, int32_t lSPCost, bool bOverride, bool IsCube )
{
#ifdef ENABLE_BUG_FIXES
	if (bApplyOn >= POINT_MAX_NUM)
	{
		LOG_ERROR("Character::AddAffect invalid ApplyOn {} for affect {} on {}", static_cast<int>(bApplyOn), dwType, GetName());
		return false;
	}
#endif

#ifdef ENABLE_BUG_FIXES
	if (dwType == AFFECT_POLYMORPH) {
		if (IsAffectFlag(AFF_GEOMGYEONG)) {
			RemoveAffect(SKILL_GEOMKYUNG);
		}

		if (IsAffectFlag(AFF_GWIGUM)) {
			RemoveAffect(SKILL_GWIGEOM);
		}
	}
#endif

	// CHAT_BLOCK
	if (dwType == AFFECT_BLOCK_CHAT && lDuration > 1)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 414, "%d", (lDuration / 60));
#endif
	}
	// END_OF_CHAT_BLOCK

	if (lDuration == 0)
	{
		LOG_ERROR("Character::AddAffect lDuration == 0 duration {} type {}", lDuration, dwType);
		lDuration = 1;
	}

	CAffect * pkAff = nullptr;

	if (IsCube)
		pkAff = FindAffect(dwType,bApplyOn);
	else
		pkAff = FindAffect(dwType);

	if (dwFlag == AFF_STUN)
	{
		if (m_posDest.x != GetX() || m_posDest.y != GetY())
		{
			m_posDest.x = m_posStart.x = GetX();
			m_posDest.y = m_posStart.y = GetY();
			battle_end(this);

			// LPENTITY.4-fixup.2.c: stun forces movement abort, mirror into
			// ECS so native dispatch does not encode the pre-stun destination
			// as an active move on subsequent viewer inserts.
			ecs::MovementSystem::SyncDestinationClear(AIHelpers::EcsOf(this));

			NetworkSyncSystem::BroadcastSyncPacket(g_registry, AIHelpers::EcsOf(this));
		}
	}

	// �̹� �ִ� ȿ���� ���� ���� ó��
	if (pkAff && bOverride)
	{
		ComputeAffect(pkAff, false); // �ϴ� ȿ���� �����ϰ�

		if (GetDesc()) {
			SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
		}
	}
	else
	{
		//
		// �� ���带 �߰�
		//
		// NOTE: ���� ���� type ���ε� ���� ����Ʈ�� ���� �� �ִ�.
		//
		pkAff = CAffect::Acquire();
		m_list_pkAffect.push_back(pkAff);

	}

	//LOG_TRACE("AddAffect {} type {} apply {} {} flag {} duration {}", GetName(), dwType, static_cast<int>(bApplyOn), lApplyValue, dwFlag, lDuration);
	//LOG_TRACE("AddAffect {} type {} apply {} {} flag {} duration {}", GetName(), dwType, static_cast<int>(bApplyOn), lApplyValue, dwFlag, lDuration);

	pkAff->dwType	= dwType;
	pkAff->bApplyOn	= bApplyOn;
	pkAff->lApplyValue	= lApplyValue;
	pkAff->dwFlag	= dwFlag;
	pkAff->lDuration	= lDuration;
	pkAff->lSPCost	= lSPCost;

	uint16_t wMovSpd = GetPoint(POINT_MOV_SPEED);
	uint16_t wAttSpd = GetPoint(POINT_ATT_SPEED);

	ComputeAffect(pkAff, true);

	if (pkAff->dwFlag || wMovSpd != GetPoint(POINT_MOV_SPEED) || wAttSpd != GetPoint(POINT_ATT_SPEED))
		SyncAffectList(AIHelpers::EcsOf(this), this);
	NetworkSyncSystem::UpdatePacket(AIHelpers::EcsOf(this));

	StartAffectEvent();

	if (IsPC())
	{
		SendAffectAddPacket(GetDesc(), pkAff);

		if (IS_NO_SAVE_AFFECT(pkAff->dwType))
			return true;

		TPacketGDAddAffect p;
		p.dwPID			= GetPlayerID();
		p.elem.dwType		= pkAff->dwType;
		p.elem.bApplyOn		= pkAff->bApplyOn;
		p.elem.lApplyValue	= pkAff->lApplyValue;
		p.elem.dwFlag		= pkAff->dwFlag;
		p.elem.lDuration	= pkAff->lDuration;
		p.elem.lSPCost		= pkAff->lSPCost;
		db_clientdesc->DBPacket(HEADER_GD_ADD_AFFECT, 0, &p, sizeof(p));
	}

	return true;
}

void CHARACTER::RefreshAffect()
{
	auto it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		CAffect * pkAff = *it++;
		ComputeAffect(pkAff, true);
	}
}

void CHARACTER::ComputeAffect(CAffect * pkAff, bool bAdd)
{
	if (bAdd && pkAff->dwType >= GUILD_SKILL_START && pkAff->dwType <= GUILD_SKILL_END)
	{
		if (!GetGuild())
			return;

		if (!GetGuild()->UnderAnyWar())
			return;
	}

	if (pkAff->dwFlag)
	{
		if (!bAdd)
			m_afAffectFlag.Reset(pkAff->dwFlag);
		else
			m_afAffectFlag.Set(pkAff->dwFlag);
	}

	if (bAdd)
		PointChange(pkAff->bApplyOn, pkAff->lApplyValue);
	else
		PointChange(pkAff->bApplyOn, -pkAff->lApplyValue);

	if (pkAff->dwType == SKILL_MUYEONG)
	{
		if (bAdd)
			StartMuyeongEvent();
		else
			StopMuyeongEvent();
	}

#ifdef ENABLE_NEW_GYEONGGONG_SKILL
	if (pkAff->dwType == SKILL_GYEONGGONG)
	{
		if (bAdd)
			StartGyeongGongEvent();
		else
			StopGyeongGongEvent();
	}
#endif
}

bool CHARACTER::RemoveAffect(CAffect * pkAff)
{
	if (!pkAff)
		return false;

	// AFFECT_BUF_FIX
	m_list_pkAffect.remove(pkAff);
	// END_OF_AFFECT_BUF_FIX

	ComputeAffect(pkAff, false);

	// ��� ���� ����.
	// ��� ���״� ���� ��ų ����->�а�->��� ���(AFFECT_REVIVE_INVISIBLE) �� �ٷ� ���� �� ��쿡 �߻��Ѵ�.
	// ������ �а��� �����ϴ� ������, ���� ��ų ȿ���� �����ϰ� �а� ȿ���� ����ǰ� �Ǿ��ִµ�,
	// ��� ��� �� �ٷ� �����ϸ� RemoveAffect�� �Ҹ��� �ǰ�, ComputePoints�ϸ鼭 �а� ȿ�� + ���� ��ų ȿ���� �ȴ�.
	// ComputePoints���� �а� ���¸� ���� ��ų ȿ�� �� �������� �ϸ� �Ǳ� �ϴµ�,
	// ComputePoints�� �������ϰ� ���ǰ� �־ ū ��ȭ�� �ִ� ���� ��������.(� side effect�� �߻����� �˱� �����.)
	// ���� AFFECT_REVIVE_INVISIBLE�� RemoveAffect�� �����Ǵ� ��츸 �����Ѵ�.
	// �ð��� �� �Ǿ� ��� ȿ���� Ǯ���� ���� ���װ� �߻����� �����Ƿ� �׿� �Ȱ��� ��.
	//		(ProcessAffect�� ���� �ð��� �� �Ǿ Affect�� �����Ǵ� ���, ComputePoints�� �θ��� �ʴ´�.)
	if (AFFECT_REVIVE_INVISIBLE != pkAff->dwType
#ifdef ENABLE_BUG_FIXES
	&& AFFECT_MOUNT != pkAff->dwType
#endif
	) {
		ComputePoints();
	} else {
		SyncAffectList(AIHelpers::EcsOf(this), this);
	NetworkSyncSystem::UpdatePacket(AIHelpers::EcsOf(this));
	}

	CheckMaximumPoints();

	if (test_server)
		LOG_TRACE("AFFECT_REMOVE: {} (flag {} apply: {})", GetName(), pkAff->dwFlag, static_cast<int>(pkAff->bApplyOn));

	if (IsPC())
	{
		SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
	}

	CAffect::Release(pkAff);
	return true;
}

bool CHARACTER::RemoveAffect(uint32_t dwType)
{
#ifdef TEXTS_IMPROVEMENT
	if (dwType == AFFECT_BLOCK_CHAT) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(this), CHAT_TYPE_INFO, 474, "");
	}
#endif

	bool flag = false;

	CAffect * pkAff;

	while ((pkAff = FindAffect(dwType)))
	{
		RemoveAffect(pkAff);
		flag = true;
	}

	return flag;
}

bool CHARACTER::IsAffectFlag(uint32_t dwAff) const
{
	return m_afAffectFlag.IsSet(dwAff);
}

void CHARACTER::RemoveGoodAffect()
{
	RemoveAffect(AFFECT_MOV_SPEED);
	RemoveAffect(AFFECT_ATT_SPEED);
	RemoveAffect(AFFECT_STR);
	RemoveAffect(AFFECT_DEX);
	RemoveAffect(AFFECT_INT);
	RemoveAffect(AFFECT_CON);
	RemoveAffect(AFFECT_CHINA_FIREWORK);
	RemoveAffect(SKILL_JEONGWI);
	RemoveAffect(SKILL_GEOMKYUNG);
	RemoveAffect(SKILL_GYEONGGONG);
	RemoveAffect(SKILL_GWIGEOM);
	RemoveAffect(SKILL_TERROR);
	RemoveAffect(SKILL_JUMAGAP);
	RemoveAffect(SKILL_MANASHILED);
	RemoveAffect(SKILL_HOSIN);
	RemoveAffect(SKILL_REFLECT);
	RemoveAffect(SKILL_GICHEON);
	RemoveAffect(SKILL_KWAESOK);
	RemoveAffect(SKILL_JEUNGRYEOK);
	RemoveAffect(SKILL_CHUNKEON);
	RemoveAffect(SKILL_EUNHYUNG);
#ifdef ENABLE_WOLFMAN_CHARACTER
	// ������(WOLFMEN) ���� �߰�
	RemoveAffect(SKILL_JEOKRANG);
	RemoveAffect(SKILL_CHEONGRANG);
#endif
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
#ifdef ENABLE_WOLFMAN_CHARACTER
		// ������(WOLFMEN) ���� �߰�
		case (SKILL_JEOKRANG):
		case (SKILL_CHEONGRANG):
#endif
			return true;
	}
	return false;
}

void CHARACTER::RemoveBadAffect()
{
	LOG_INFO("RemoveBadAffect {}", GetName());
	// ��
	RemovePoison();
#ifdef ENABLE_WOLFMAN_CHARACTER
	RemoveBleeding();
#endif
	RemoveFire();

	// ����           : Value%�� ������ 5�ʰ� �Ӹ� ���� ���� ���ư���. (������ 1/2 Ȯ���� Ǯ��)               AFF_STUN
	RemoveAffect(AFFECT_STUN);

	// ���ο�         : Value%�� ������ ����/�̼� ��� ��������. ���õ��� ���� �޶��� ����� ��� �� ��쿡   AFF_SLOW
	RemoveAffect(AFFECT_SLOW);

	// ���Ӹ���
	RemoveAffect(SKILL_TUSOK);

	// ����
	//RemoveAffect(SKILL_CURSE);

	// �Ĺ���
	//RemoveAffect(SKILL_PABUP);

	// ����           : Value%�� ������ ������Ų��. 2��                                                       AFF_FAINT
	//RemoveAffect(AFFECT_FAINT);

	// �ٸ�����       : Value%�� ������ �̵��ӵ��� ����Ʈ����. 5�ʰ� -40                                      AFF_WEB
	//RemoveAffect(AFFECT_WEB);

	// ����         : Value%�� ������ 10�ʰ� ������. (������ Ǯ��)                                        AFF_SLEEP
	//RemoveAffect(AFFECT_SLEEP);

	// ����           : Value%�� ������ ����/��� ��� ����Ʈ����. ���õ��� ���� �޶��� ����� ��� �� ��쿡 AFF_CURSE
	//RemoveAffect(AFFECT_CURSE);

	// ����           : Value%�� ������ 4�ʰ� �����Ų��.                                                     AFF_PARA
	//RemoveAffect(AFFECT_PARALYZE);

	// �ε��ں�       : ���� ���
	//RemoveAffect(SKILL_BUDONG);
}

int CHARACTER::GetPolymorphPower() const
{
	if (test_server)
	{
		int value = quest::CQuestManager::instance().GetEventFlag("poly");
		if (value)
			return value;
	}
	return aiPolymorphPowerByLevel[MINMAX(0, GetSkillLevel(SKILL_POLYMORPH), 40)];
}
void CHARACTER::SetPolymorph(uint32_t dwRaceNum, bool bMaintainStat)
{
#ifdef ENABLE_WOLFMAN_CHARACTER
	if (dwRaceNum < MAIN_RACE_MAX_NUM)
#else
	if (dwRaceNum < JOB_MAX_NUM)
#endif
	{
		dwRaceNum = 0;
		bMaintainStat = false;
	}
	if (m_dwPolymorphRace == dwRaceNum)
		return;
	m_bPolyMaintainStat = bMaintainStat;
	m_dwPolymorphRace = dwRaceNum;
	if (auto* race = g_registry.try_get<ecs::RaceState>(AIHelpers::EcsOf(this))) {
		race->polymorphRace = dwRaceNum;
	}
	if (auto* polymorph = g_registry.try_get<ecs::PolymorphState>(AIHelpers::EcsOf(this))) {
		polymorph->raceVnum = dwRaceNum;
		polymorph->maintainStat = bMaintainStat;
	}
	LOG_INFO("POLYMORPH: {} race {} ", GetName(), dwRaceNum);
	if (dwRaceNum != 0)
		StopRiding();
	SET_BIT(m_bAddChrState, ADD_CHARACTER_STATE_SPAWN);
	// LPENTITY.4-fixup.2.f: mirror SPAWN bit into ECS StatusFlags so the
	// native BuildCharacterInsert during ViewReencode emits the same
	// bStateFlag byte as legacy.
	if (auto* status = g_registry.try_get<ecs::StatusFlags>(AIHelpers::EcsOf(this)))
		status->isSpawnState = true;
	m_afAffectFlag.Set(AFF_SPAWN);
	ViewReencode();
	REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_SPAWN);
	if (auto* status = g_registry.try_get<ecs::StatusFlags>(AIHelpers::EcsOf(this)))
		status->isSpawnState = false;
	if (!bMaintainStat)
	{
		PointChange(POINT_ST, 0);
		PointChange(POINT_DX, 0);
		PointChange(POINT_IQ, 0);
		PointChange(POINT_HT, 0);
	}
	SetValidComboInterval(0);
	SetComboSequence(0);
	ComputeBattlePoints();
}
int32_t CHARACTER::SetInvincible(bool arg)
{
	isInvincible = arg;
	return 1;
}
bool CHARACTER::GetInvincible()
{
	return isInvincible;
}
int32_t CHARACTER::IncreaseMobHP(int32_t lArg)
{
	int32_t t = GetMaxHP() + lArg;
	SetMaxHP(t);
	SetHP(t);
	PointChange(POINT_HP, t, true);
	return 1;
}
int32_t CHARACTER::IncreaseMobRigHP(int32_t lArg)
{
	PointChange(POINT_HP_REGEN, GetPoint(POINT_HP_REGEN) + lArg, true);
	return 1;
}


