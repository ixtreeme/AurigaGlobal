#include "../../stdafx.h"

#include "AffectSystem.hpp"

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
#include "../Registry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/status_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"

namespace {

const int poison_damage_rate[MOB_RANK_MAX_NUM] = {
    80, 50, 40, 30, 25, 1
};

int GetPoisonDamageRate(LPCHARACTER ch)
{
    int iRate = ch->IsPC() ? 50 : poison_damage_rate[ch->GetMobRank()];
    iRate = MAX(0, iRate - ch->GetPoint(POINT_POISON_REDUCE));
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
        sys_err("poison_event> <Factor> Null pointer");
        return 0;
    }

    LPCHARACTER ch = info->ch;
    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkPoisonEvent
    LPCHARACTER pkAttacker = CHARACTER_MANAGER::instance().FindByPID(info->attacker_pid);
    int dam = ch->GetMaxHP() * GetPoisonDamageRate(ch) / 1000;
    if (test_server) {
        ch->ChatPacket(CHAT_TYPE_NOTICE, "Poison Damage %d", dam);
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

int GetBleedingDamageRate(LPCHARACTER ch)
{
    int iRate = ch->IsPC() ? 50 : bleeding_damage_rate[ch->GetMobRank()];
    iRate = MAX(0, iRate - ch->GetPoint(POINT_BLEEDING_REDUCE));
#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_ITEM_BLEEDING_AS_POISON)
    iRate = MAX(0, iRate - ch->GetPoint(POINT_POISON_REDUCE));
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
        sys_err("bleeding_event> <Factor> Null pointer");
        return 0;
    }

    LPCHARACTER ch = info->ch;
    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkBleedingEvent
    LPCHARACTER pkAttacker = CHARACTER_MANAGER::instance().FindByPID(info->attacker_pid);
    int dam = ch->GetMaxHP() * GetBleedingDamageRate(ch) / 1000;
    if (test_server) {
        ch->ChatPacket(CHAT_TYPE_NOTICE, "Bleeding Damage %d", dam);
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
        sys_err("fire_event> <Factor> Null pointer");
        return 0;
    }

    LPCHARACTER ch = info->ch;
    if (ch == nullptr) {
        return 0;
    }

    // Phase 10: WRITES_STATE - deferred until ECS component covers m_pkFireEvent
    LPCHARACTER pkAttacker = CHARACTER_MANAGER::instance().FindByPID(info->attacker_pid);
    int dam = info->amount;
    if (test_server) {
        ch->ChatPacket(CHAT_TYPE_NOTICE, "Fire Damage %d", dam);
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

LPCHARACTER LegacyCharacter(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return nullptr;
    }

    auto* vid = g_registry.try_get<ecs::VIDComponent>(e);
    return vid ? CHARACTER_MANAGER::instance().Find(vid->value) : nullptr;
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

void SyncAffectList(entt::entity e, LPCHARACTER ch)
{
    if (!ch || e == entt::null || !g_registry.valid(e)) {
        return;
    }

    auto& affectList = g_registry.get_or_emplace<ecs::AffectList>(e);
    affectList.affects.assign(ch->GetAffectContainer().begin(), ch->GetAffectContainer().end());
    affectList.isLoaded = true;
}

} // namespace

namespace AffectSystem {

void ApplyFire(entt::entity target, entt::entity attacker, int amount, int count)
{
    MarkFire(target, true);

    LPCHARACTER ch = LegacyCharacter(target);
    if (!ch || ch->m_pkFireEvent) {
        return;
    }

    ch->AddAffect(AFFECT_FIRE, POINT_NONE, 0, AFF_FIRE, count * 3 + 1, 0, true);

    TFireEventInfo* info = AllocEventInfo<TFireEventInfo>();
    info->ch = ch;
    info->count = count;
    info->amount = amount;

    if (LPCHARACTER pkAttacker = LegacyCharacter(attacker)) {
        info->attacker_pid = pkAttacker->GetPlayerID();
    } else {
        info->attacker_pid = 0;
    }

    ch->m_pkFireEvent = event_create(fire_event, info, 1);
}

void RemoveFire(entt::entity e)
{
    MarkFire(e, false);

    LPCHARACTER ch = LegacyCharacter(e);
    if (!ch) {
        return;
    }

    ch->RemoveAffect(AFFECT_FIRE);
    event_cancel(&ch->m_pkFireEvent);
}

void ApplyPoison(entt::entity target, entt::entity attacker)
{
    MarkPoison(target, true);

    LPCHARACTER ch = LegacyCharacter(target);
    if (!ch || ch->m_pkPoisonEvent) {
        return;
    }

    if (ch->m_bHasPoisoned && !ch->IsPC()) {
        return;
    }

#ifdef ENABLE_WOLFMAN_CHARACTER
    if (ch->m_pkBleedingEvent) {
        return;
    }

    if (ch->m_bHasBled && !ch->IsPC()) {
        return;
    }
#endif

    LPCHARACTER pkAttacker = LegacyCharacter(attacker);
    if (pkAttacker && pkAttacker->GetLevel() < ch->GetLevel()) {
        int delta = ch->GetLevel() - pkAttacker->GetLevel();
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
    info->attacker_pid = pkAttacker ? pkAttacker->GetPlayerID() : 0;
    ch->m_pkPoisonEvent = event_create(poison_event, info, 1);

    if (test_server && pkAttacker) {
        char buf[256];
        snprintf(buf, sizeof(buf), "POISON %s -> %s", pkAttacker->GetName(), ch->GetName());
        pkAttacker->ChatPacket(CHAT_TYPE_INFO, "%s", buf);
    }
}

void RemovePoison(entt::entity e)
{
    MarkPoison(e, false);

    LPCHARACTER ch = LegacyCharacter(e);
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

    LPCHARACTER ch = LegacyCharacter(target);
    if (!ch || ch->m_pkBleedingEvent) {
        return;
    }

    if (ch->m_bHasBled && !ch->IsPC()) {
        return;
    }

    if (ch->m_pkPoisonEvent) {
        return;
    }

    if (ch->m_bHasPoisoned && !ch->IsPC()) {
        return;
    }

    LPCHARACTER pkAttacker = LegacyCharacter(attacker);
    if (pkAttacker && pkAttacker->GetLevel() < ch->GetLevel()) {
        int delta = ch->GetLevel() - pkAttacker->GetLevel();
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
    info->attacker_pid = pkAttacker ? pkAttacker->GetPlayerID() : 0;
    ch->m_pkBleedingEvent = event_create(bleeding_event, info, 1);

    if (test_server && pkAttacker) {
        char buf[256];
        snprintf(buf, sizeof(buf), "BLEEDING %s -> %s", pkAttacker->GetName(), ch->GetName());
        pkAttacker->ChatPacket(CHAT_TYPE_INFO, "%s", buf);
    }
}

void RemoveBleeding(entt::entity e)
{
    MarkBleeding(e, false);

    LPCHARACTER ch = LegacyCharacter(e);
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
    LPCHARACTER ch = LegacyCharacter(e);
    if (!ch) {
        return false;
    }

    if (IS_SET(ch->GetImmuneFlag(), immuneFlag))
    {
#ifdef ENABLE_IMMUNE_PERC
        int immune_pct = 90;
        int percent = number(1, 100);

        if (percent <= immune_pct)
#else
        if (true)
#endif
        {
            if (test_server && ch->IsPC()) {
                ch->ChatPacket(CHAT_TYPE_PARTY, "<IMMUNE_SUCCESS> (%s)", ch->GetName());
            }

            return true;
        }

        if (test_server && ch->IsPC()) {
            ch->ChatPacket(CHAT_TYPE_PARTY, "<IMMUNE_FAIL> (%s)", ch->GetName());
        }

        return false;
    }

    if (test_server && ch->IsPC()) {
        ch->ChatPacket(CHAT_TYPE_PARTY, "<IMMUNE_FAIL> (%s) NO_IMMUNE_FLAG", ch->GetName());
    }

    return false;
}

void ApplyMobAttribute(entt::entity target, const TMobTable* table)
{
    LPCHARACTER ch = LegacyCharacter(target);
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
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->FindAffect(type, apply) : nullptr;
}

bool AddAffect(entt::entity e, uint32_t type, uint8_t applyOn, int32_t applyValue,
               uint32_t flag, int32_t duration, int32_t spCost, bool overwrite,
               bool isCube)
{
    LPCHARACTER ch = LegacyCharacter(e);
    if (!ch) {
        return false;
    }

    const bool result = ch->AddAffect(type, applyOn, applyValue, flag, duration, spCost, overwrite, isCube);
    SyncAffectList(e, ch);
    return result;
}

bool RemoveAffect(entt::entity e, uint32_t type)
{
    LPCHARACTER ch = LegacyCharacter(e);
    if (!ch) {
        return false;
    }

    const bool result = ch->RemoveAffect(type);
    SyncAffectList(e, ch);
    return result;
}

void ClearAffect(entt::entity e, bool save)
{
    LPCHARACTER ch = LegacyCharacter(e);
    if (!ch) {
        return;
    }

    ch->ClearAffect(save);
    SyncAffectList(e, ch);
}

void RefreshAffect(entt::entity e)
{
    LPCHARACTER ch = LegacyCharacter(e);
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

    auto view = reg.view<ecs::AffectList, ecs::VIDComponent>();
    view.each([&](entt::entity e, ecs::AffectList& affectList, const ecs::VIDComponent& vid) {
        if (affectList.affects.empty() && affectList.skillAffects.empty()) {
            return;
        }

        LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid.value);
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

    auto view = reg.view<ecs::AffectList, ecs::VIDComponent>();
    view.each([&](const entt::entity entity, ecs::AffectList& affectList, const ecs::VIDComponent& vid) {
        (void)vid;
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
		sys_err( "affect_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}

	if (!ch->UpdateAffect())
		return 0;
	else
		return passes_per_sec; // 1초
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

	// affect_event 에서 처리할 일은 아니지만, 1초짜리 이벤트에서 처리하는 것이
	// 이것 뿐이라 여기서 물약 처리를 한다.
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
	
	// 스테미나 회복
	if (GetMaxStamina() > GetStamina())
	{
		int iSec = (get_dword_time() - GetStopTime()) / 3000;
		if (iSec)
			PointChange(POINT_STAMINA, GetMaxStamina()/1);
	}


	// ProcessAffect는 affect가 없으면 true를 리턴한다.
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
	sys_log(1, "StartAffectEvent %s %p %p", GetName(), this, get_pointer(m_pkAffectEvent));
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
		UpdatePacket();

	CheckMaximumPoints();

	if (m_list_pkAffect.empty())
		event_cancel(&m_pkAffectEvent);
}

int CHARACTER::ProcessAffect()
{
	bool	bDiff	= false;
	CAffect	*pkAff;

	//
	// 프리미엄 처리
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
		if ( this->GetQuestFlag("hair.limit_time") < get_global_time())
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
		// 무한 효과 아이템도 시간을 줄인다.
		// 시간을 매우 크게 잡기 때문에 상관 없을 것이라 생각됨.
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
			UpdatePacket();
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

		sys_log(1, "AFFECT_SAVE: %u %u %d %d", pkAff->dwType, pkAff->bApplyOn, pkAff->lApplyValue, pkAff->lDuration);

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
		sys_err( "load_affect_login_event_info> <Factor> Null pointer" );
		return 0;
	}

	uint32_t dwPID = info->pid;
	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);

	if (!ch)
	{
		M2_DELETE_ARRAY(info->data);
		return 0;
	}

	LPDESC d = ch->GetDesc();

	if (!d)
	{
		M2_DELETE_ARRAY(info->data);
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
		return 0;
	}
	else if (d->IsPhase(PHASE_GAME))
	{
		sys_log(1, "Affect Load by Event");
		ch->LoadAffect(info->count, (TPacketAffectElement*)info->data);
		M2_DELETE_ARRAY(info->data);
		return 0;
	}
	else
	{
		sys_err("input_db.cpp:quest_login_event INVALID PHASE pid %d", ch->GetPlayerID());
		M2_DELETE_ARRAY(info->data);
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

	if (!GetDesc()->IsPhase(PHASE_GAME))
	{
		if (test_server)
			sys_log(0, "LOAD_AFFECT: Creating Event", GetName(), dwCount);

		load_affect_login_event_info* info = AllocEventInfo<load_affect_login_event_info>();

		info->pid = GetPlayerID();
		info->count = dwCount;
		info->data = M2_NEW char[sizeof(TPacketAffectElement) * dwCount];
		memcpy(info->data, pElements, sizeof(TPacketAffectElement) * dwCount);

		event_create(load_affect_login_event, info, PASSES_PER_SEC(1));

		return;
	}

	ClearAffect(true);

	if (test_server)
		sys_log(0, "LOAD_AFFECT: %s count %d", GetName(), dwCount);

	TAffectFlag afOld = m_afAffectFlag;

	int64_t lMovSpd = GetPoint(POINT_MOV_SPEED);
	int64_t lAttSpd = GetPoint(POINT_ATT_SPEED);

	for (uint32_t i = 0; i < dwCount; ++i, ++pElements)
	{
		////// 무영진은 로드하지않는다.
		////if (pElements->dwType == SKILL_MUYEONG)
		////	continue;
		if (AFFECT_AUTO_HP_RECOVERY == pElements->dwType || AFFECT_AUTO_SP_RECOVERY == pElements->dwType)
		{
			LPITEM item = FindItemByID( pElements->dwFlag );
			if (nullptr == item)
				continue;
			
			item->Lock(true);
		}
#ifdef ENABLE_NEW_USE_POTION
		else if (AFFECT_AUTO_HP_RECOVERY2 == pElements->dwType || AFFECT_AUTO_SP_RECOVERY2 == pElements->dwType)
		{
			LPITEM item = FindItemByID( pElements->dwFlag );
			if (nullptr == item)
				continue;
			
			item->Lock(true);
		}
		else if ((pElements->dwType >= AFFECT_NEW_POTION1) && (pElements->dwType <= AFFECT_NEW_POTION31))
		{
			LPITEM item = FindItemByID(pElements->dwFlag);
			if (item)
				item->Lock(true);
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
				item->Lock(true);
			else
				continue;
		}
#endif
#ifdef __NEWPET_SYSTEM__
		else if (pElements->dwType == AFFECT_RECALL2)
		{
			LPITEM item = FindItemByID(pElements->dwFlag);
			if (item)
				item->Lock(true);
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

			item->Lock(true);
		}
#endif

		if (pElements->bApplyOn >= POINT_MAX_NUM)
		{
			sys_err("invalid affect data %s ApplyOn %u ApplyValue %d",
					GetName(), pElements->bApplyOn, pElements->lApplyValue);
			continue;
		}

		if (test_server)
		{
			sys_log(0, "Load Affect : Affect %s %d %d", GetName(), pElements->dwType, pElements->bApplyOn );
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
		UpdatePacket();
	}

	StartAffectEvent();

	m_bIsLoadedAffect = true;

	// 용혼석 셋팅 로드 및 초기화
	DragonSoul_Initialize();

	// @fixme118 (regain affect hp/mp)
	if (!IsDead())
	{
		PointChange(POINT_HP, GetMaxHP() - GetHP());
		PointChange(POINT_SP, GetMaxSP() - GetSP());
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
	CheckBiologistReward();
#endif
}

bool CHARACTER::AddAffect(uint32_t dwType, uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag, int32_t lDuration, int32_t lSPCost, bool bOverride, bool IsCube )
{
#ifdef ENABLE_BUG_FIXES
	if (bApplyOn >= POINT_MAX_NUM)
	{
		sys_err("Character::AddAffect invalid ApplyOn %u for affect %u on %s", bApplyOn, dwType, GetName());
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
		ChatPacketNew(CHAT_TYPE_INFO, 414, "%d", (lDuration / 60));
#endif
	}
	// END_OF_CHAT_BLOCK

	if (lDuration == 0)
	{
		sys_err("Character::AddAffect lDuration == 0 type %d", lDuration, dwType);
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

			SyncPacket();
		}
	}

	// 이미 있는 효과를 덮어 쓰는 처리
	if (pkAff && bOverride)
	{
		ComputeAffect(pkAff, false); // 일단 효과를 삭제하고

		if (GetDesc()) {
			SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
		}
	}
	else
	{
		//
		// 새 에펙를 추가
		//
		// NOTE: 따라서 같은 type 으로도 여러 에펙트를 붙을 수 있다.
		//
		pkAff = CAffect::Acquire();
		m_list_pkAffect.push_back(pkAff);

	}

	//sys_log(1, "AddAffect %s type %d apply %d %d flag %u duration %d", GetName(), dwType, bApplyOn, lApplyValue, dwFlag, lDuration);
	//sys_log(0, "AddAffect %s type %d apply %d %d flag %u duration %d", GetName(), dwType, bApplyOn, lApplyValue, dwFlag, lDuration);

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
		UpdatePacket();

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

	// 백기 버그 수정.
	// 백기 버그는 버프 스킬 시전->둔갑->백기 사용(AFFECT_REVIVE_INVISIBLE) 후 바로 공격 할 경우에 발생한다.
	// 원인은 둔갑을 시전하는 시점에, 버프 스킬 효과를 무시하고 둔갑 효과만 적용되게 되어있는데,
	// 백기 사용 후 바로 공격하면 RemoveAffect가 불리게 되고, ComputePoints하면서 둔갑 효과 + 버프 스킬 효과가 된다.
	// ComputePoints에서 둔갑 상태면 버프 스킬 효과 안 먹히도록 하면 되긴 하는데,
	// ComputePoints는 광범위하게 사용되고 있어서 큰 변화를 주는 것이 꺼려진다.(어떤 side effect가 발생할지 알기 힘들다.)
	// 따라서 AFFECT_REVIVE_INVISIBLE가 RemoveAffect로 삭제되는 경우만 수정한다.
	// 시간이 다 되어 백기 효과가 풀리는 경우는 버그가 발생하지 않으므로 그와 똑같이 함.
	//		(ProcessAffect를 보면 시간이 다 되어서 Affect가 삭제되는 경우, ComputePoints를 부르지 않는다.)
	if (AFFECT_REVIVE_INVISIBLE != pkAff->dwType
#ifdef ENABLE_BUG_FIXES
	&& AFFECT_MOUNT != pkAff->dwType
#endif
	) {
		ComputePoints();
	} else {
		UpdatePacket();
	}

	CheckMaximumPoints();

	if (test_server)
		sys_log(0, "AFFECT_REMOVE: %s (flag %u apply: %u)", GetName(), pkAff->dwFlag, pkAff->bApplyOn);

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
		ChatPacketNew(CHAT_TYPE_INFO, 474, "");
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
	// 수인족(WOLFMEN) 버프 추가
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
		// 수인족(WOLFMEN) 버프 추가
		case (SKILL_JEOKRANG):
		case (SKILL_CHEONGRANG):
#endif
			return true;
	}
	return false;
}

void CHARACTER::RemoveBadAffect()
{
	sys_log(0, "RemoveBadAffect %s", GetName());
	// 독
	RemovePoison();
#ifdef ENABLE_WOLFMAN_CHARACTER
	RemoveBleeding();
#endif
	RemoveFire();

	// 스턴           : Value%로 상대방을 5초간 머리 위에 별이 돌아간다. (때리면 1/2 확률로 풀림)               AFF_STUN
	RemoveAffect(AFFECT_STUN);

	// 슬로우         : Value%로 상대방의 공속/이속 모두 느려진다. 수련도에 따라 달라짐 기술로 사용 한 경우에   AFF_SLOW
	RemoveAffect(AFFECT_SLOW);

	// 투속마령
	RemoveAffect(SKILL_TUSOK);

	// 저주
	//RemoveAffect(SKILL_CURSE);

	// 파법술
	//RemoveAffect(SKILL_PABUP);

	// 기절           : Value%로 상대방을 기절시킨다. 2초                                                       AFF_FAINT
	//RemoveAffect(AFFECT_FAINT);

	// 다리묶임       : Value%로 상대방의 이동속도를 떨어트린다. 5초간 -40                                      AFF_WEB
	//RemoveAffect(AFFECT_WEB);

	// 잠들기         : Value%로 상대방을 10초간 잠재운다. (때리면 풀림)                                        AFF_SLEEP
	//RemoveAffect(AFFECT_SLEEP);

	// 저주           : Value%로 상대방의 공등/방등 모두 떨어트린다. 수련도에 따라 달라짐 기술로 사용 한 경우에 AFF_CURSE
	//RemoveAffect(AFFECT_CURSE);

	// 마비           : Value%로 상대방을 4초간 마비시킨다.                                                     AFF_PARA
	//RemoveAffect(AFFECT_PARALYZE);

	// 부동박부       : 무당 기술
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
	sys_log(0, "POLYMORPH: %s race %u ", GetName(), dwRaceNum);
	if (dwRaceNum != 0)
		StopRiding();
	SET_BIT(m_bAddChrState, ADD_CHARACTER_STATE_SPAWN);
	m_afAffectFlag.Set(AFF_SPAWN);
	ViewReencode();
	REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_SPAWN);
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
