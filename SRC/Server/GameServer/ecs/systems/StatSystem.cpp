#include "../../stdafx.h"
#include "StatSystem.hpp"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "ItemSystem.hpp"
#include "MountSystem.hpp"
#include "SkillSystem.hpp"
#include "DragonSoulSystem.hpp"
#include "AffectSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "../Registry.hpp"
#include "../components/character_stats_components.hpp"
#include "../components/vital_components.hpp"
#include "../components/skill_components.hpp"
#include "../components/combat_components.hpp"
#include "../../char.h"
#include "../../constants.h"
#include "../../char_manager.h"
#include "../../skill.h"
#include "../../DragonSoul.h"
#include "../../PetSystem.h"
#include "../../horse_rider.h"
#include <Core/Logging.hpp>
#include <algorithm>
#include <array>
#include <unordered_set>

namespace ecs::PointSystem {
namespace {
bool HasPointState(entt::entity e)
{
    return g_registry.valid(e) && g_registry.all_of<CharacterStatsComponent, CharacterPoints, Health, Mana, Stamina>(e);
}

struct RecomputeGuard {
    entt::entity entity;
    ~RecomputeGuard() {
        if (g_registry.valid(entity))
            if (auto* state = g_registry.try_get<CharacterStatsComponent>(entity))
                state->recomputing = false;
    }
};

void ApplyAlignmentPoints(entt::entity e)
{
    if (!ecs::PlayerRuntime::IsPC(e)) return;
    const auto* combat = g_registry.try_get<CombatStats>(e);
    const uint32_t alignment = combat ? combat->realAlignment / 10 : 0;
    static constexpr std::array<uint32_t, 20> ceilings {
        4999,14999,19999,29999,49999,74999,99999,124999,174999,249999,
        499999,749999,999999,1499999,2499999,2999999,3499999,3999999,4499999,4999999
    };
    const auto grade = std::lower_bound(ceilings.begin(), ceilings.end(), alignment) - ceilings.begin();
    static constexpr uint8_t types[] {POINT_MAX_HP, POINT_ATTBONUS_MONSTER, POINT_ATTBONUS_HUMAN,
        POINT_ATTBONUS_METIN, POINT_ATTBONUS_BOSS, POINT_ATTBONUS_MEDI_PVM,
        POINT_NORMAL_HIT_DAMAGE_BONUS, POINT_SKILL_DAMAGE_BONUS};
    static constexpr int bonuses[][21] {
        {500,1000,1500,2000,2500,4000,6000,8000,10000,12000,14000,16000,18000,20000,25000,30000,35000,40000,45000,50000,60000},
        {1,3,5,7,9,12,15,18,21,25,25,30,35,40,50,55,60,65,70,75,85},
        {1,3,5,7,9,12,15,18,21,25,25,30,35,40,50,55,60,65,70,75,85},
        {0,0,0,0,0,0,0,0,5,10,15,20,25,30,35,40,45,50,55,60,70},
        {0,0,0,0,0,0,0,0,0,5,10,15,20,25,30,35,40,45,50,55,65},
        {0,0,0,0,0,5,5,5,5,5,10,10,15,20,25,30,35,40,45,50,60},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,5,10,15,20,25,30,35,45},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,5,10,15,20,25,30,35,45}
    };
    for (size_t i = 0; i < std::size(types) && HasPointState(e); ++i)
        if (bonuses[i][grade]) Change(e, types[i], bonuses[i][grade]);
}

void ApplyBeltPoints(entt::entity e)
{
    std::array<int64_t, POINT_MAX_NUM> bonuses {};
    std::unordered_set<entt::entity> seen;
    for (int cell = BELT_INVENTORY_SLOT_START; cell < BELT_INVENTORY_SLOT_END; ++cell) {
        const auto item = ItemSystem::GetInventoryItem(e, cell);
        if (!ItemSystem::IsValidItem(item) || ItemSystem::GetItemOwner(item) != e ||
            !seen.insert(item).second) continue;
        const auto vnum = ItemSystem::GetItemVnum(item);
        // Same enabled whitelist as before; the commented-out mount IDs
        // never contributed here. Item attributes belong to mount storage.
        if (vnum < 18000 || vnum > 18159) continue;
        const auto* proto = ItemSystem::GetItemProto(item);
        if (!proto) continue;
        for (const auto apply : proto->aApplies) {
            if (apply.bType == APPLY_NONE || apply.bType >= MAX_APPLY_NUM || !apply.lValue) continue;
            const auto type = aApplyInfo[apply.bType].bPointType;
            if (type != POINT_NONE && type < POINT_MAX_NUM) bonuses[type] += apply.lValue;
        }
    }
    for (size_t type = 1; type < bonuses.size() && HasPointState(e); ++type)
        if (bonuses[type]) Change(e, static_cast<uint8_t>(type), bonuses[type]);
}
} // namespace

void Compute(entt::entity e)
{
    if (!HasPointState(e)) return;
    const bool player = ecs::PlayerRuntime::IsPC(e);
    const auto job = ecs::PlayerRuntime::GetJob(e);
    const auto* mob = ecs::PlayerRuntime::GetMobTable(e);
    if ((player && job >= JOB_MAX_NUM) || (!player && !mob)) return;
    if (g_registry.get<CharacterStatsComponent>(e).recomputing) return;
    g_registry.get<CharacterStatsComponent>(e).recomputing = true;
    RecomputeGuard guard {e};

    // Copy mob inputs before any callback. No component/prototype pointer is
    // retained across item, affect, pet, event or network services.
    const int64_t mobHP = mob ? mob->dwMaxHP : 0;
    const int mobAttackSpeed = mob ? mob->sAttackSpeed : 0;
    const int mobMoveSpeed = mob ? mob->sMovingSpeed : 0;
    const uint32_t mobImmune = mob ? mob->dwImmuneFlag : 0;
    const int64_t hpBefore = Get(e, POINT_HP), spBefore = Get(e, POINT_SP);
    static constexpr uint8_t preservedTypes[] {
        POINT_STAT, POINT_STAT_RESET_COUNT, POINT_SKILL, POINT_SUB_SKILL,
        POINT_HORSE_SKILL, POINT_LEVEL_STEP, POINT_PARTY_ATTACKER_BONUS,
        POINT_PARTY_TANKER_BONUS, POINT_PARTY_BUFFER_BONUS,
        POINT_PARTY_SKILL_MASTER_BONUS, POINT_PARTY_HASTE_BONUS,
        POINT_PARTY_DEFENDER_BONUS, POINT_HP_RECOVERY, POINT_SP_RECOVERY
    };
    std::array<int64_t, std::size(preservedTypes)> preserved {};
    for (size_t i = 0; i < preserved.size(); ++i) preserved[i] = Get(e, preservedTypes[i]);
    // Clear the authority, not the unused CHARACTER copy. This also resets
    // flat HP/SP modifiers and the actual skill-damage component.
    std::fill(std::begin(g_registry.get<CharacterStatsComponent>(e).points),
        std::end(g_registry.get<CharacterStatsComponent>(e).points), 0);
    std::fill(std::begin(g_registry.get<CharacterPoints>(e).instant.points),
        std::end(g_registry.get<CharacterPoints>(e).instant.points), 0);
    g_registry.get_or_emplace<SkillDamageBonus>(e).bySkill.clear();
    ecs::PlayerRuntime::BuffOnAttr_ClearAll(e);
    if (!HasPointState(e)) return;
    ecs::PlayerRuntime::SetImmuneFlag(e, player ? 0 : mobImmune);

    for (size_t i = 0; i < preserved.size(); ++i) Set(e, preservedTypes[i], preserved[i]);
    for (const auto type : {POINT_ST, POINT_HT, POINT_DX, POINT_IQ}) Set(e, type, GetReal(e, type));
#ifdef ENABLE_FIX_LEVELUP_EFFECT
    ecs::PlayerRuntime::SetPart(e, PART_MAIN, ecs::PlayerRuntime::GetPart(e, PART_MAIN));
#else
    ecs::PlayerRuntime::SetPart(e, PART_MAIN, ecs::PlayerRuntime::GetOriginalPart(e, PART_MAIN));
#endif
    for (const auto part : {PART_WEAPON, PART_HEAD, PART_HAIR
#ifdef ENABLE_RUNE_SYSTEM
        , PART_RUNE
#endif
#ifdef ENABLE_ACCE_SYSTEM
        , PART_ACCE
#endif
#ifdef ENABLE_COSTUME_EFFECT
        , PART_EFFECT_BODY, PART_EFFECT_WEAPON
#endif
    }) ecs::PlayerRuntime::SetPart(e, part, ecs::PlayerRuntime::GetOriginalPart(e, part));

    MountSystem::ComputeMountInventoryBonuses(e);
    if (!HasPointState(e)) return;
    int64_t maxHP = mobHP, maxSP = 0, maxStamina = 0;
    if (player) {
        const auto initial = JobInitialPoints[job];
        maxHP = initial.max_hp + GetRandomHP(e) + Get(e, POINT_HT) * initial.hp_per_ht;
        maxSP = initial.max_sp + GetRandomSP(e) + Get(e, POINT_IQ) * initial.sp_per_iq;
        maxStamina = initial.max_stamina + Get(e, POINT_HT) * initial.stamina_per_con;
        if (auto* skill = CSkillManager::instance().Get(SKILL_ADD_HP)) {
            skill->SetPointVar("k", 1.0f * SkillSystem::GetSkillPower(e, SKILL_ADD_HP) / 100.0f);
            maxHP += static_cast<int>(skill->kPointPoly.Eval());
        }
#ifdef ENABLE_NEW_SECONDARY_SKILLS
        static constexpr int values[4][11] {
            {0,1,2,3,4,5,6,7,8,9,10}, {0,2,4,6,8,10,12,14,16,18,20},
            {0,1,2,3,4,5,6,7,8,9,10}, {0,200,400,800,1200,1600,2000,2200,2500,2700,3000}
        };
        const auto support = [&](uint32_t skill) { return std::clamp(SkillSystem::GetSkillLevel(e, skill), 0, 10); };
        Change(e, POINT_MALL_ATTBONUS, values[0][support(NEW_SUPPORT_SKILL_ATTACK)]);
        Change(e, POINT_MALL_GOLDBONUS, values[1][support(NEW_SUPPORT_SKILL_YANG)]);
        Change(e, POINT_ATTBONUS_MONSTER, values[2][support(NEW_SUPPORT_SKILL_MONSTERS)]);
        maxHP += values[3][support(NEW_SUPPORT_SKILL_HP)];
#endif
        Set(e, POINT_MOV_SPEED, 200);
        if (!HasPointState(e)) return;
        Set(e, POINT_ATT_SPEED, 100);
        Change(e, POINT_ATT_SPEED, Get(e, POINT_PARTY_HASTE_BONUS));
        Set(e, POINT_CASTING_SPEED, 100);
    } else {
        Set(e, POINT_ATT_SPEED, mobAttackSpeed);
        Set(e, POINT_MOV_SPEED, mobMoveSpeed);
        if (!HasPointState(e)) return;
        Set(e, POINT_CASTING_SPEED, mobAttackSpeed);
    }
    if (!HasPointState(e)) return;
    const auto mount = MountSystem::GetMountVnum(e);
    if (player && mount) {
        const auto horseLevel = std::clamp(MountSystem::GetHorseLevel(e), 0, HORSE_MAX_LEVEL);
        const auto horse = c_aHorseStat[horseLevel];
        const bool ridingHorse = mount >= 20101 && mount <= 20107;
        for (const auto [type, value] : {
            std::pair<uint8_t, int>{POINT_ST, ridingHorse ? horse.iST : 36},
            {POINT_DX, ridingHorse ? horse.iDX : 18}, {POINT_HT, ridingHorse ? horse.iHT : 53},
            {POINT_IQ, ridingHorse ? horse.iIQ : 71}}) {
            if (!HasPointState(e)) return;
            if (value > Get(e, type)) Change(e, type, value - Get(e, type));
        }
    }
    ApplyBeltPoints(e);
    if (!HasPointState(e)) return;
    ComputeBattlePoints(e);
    if (!HasPointState(e)) return;
    SetReal(e, POINT_MAX_HP, maxHP);
    SetReal(e, POINT_MAX_SP, maxSP);
    Change(e, POINT_MAX_HP, 0);
    Change(e, POINT_MAX_SP, 0);
    ecs::PlayerRuntime::SetMaxStamina(e, maxStamina);
    if (!HasPointState(e)) return;

    std::unordered_set<entt::entity> applied;
    for (int slot = 0; slot < WEAR_MAX_NUM; ++slot) {
        const auto item = ItemSystem::GetWearItem(e, slot);
        if (!ItemSystem::IsValidItem(item) || ItemSystem::GetItemOwner(item) != e || !applied.insert(item).second) continue;
#ifdef ENABLE_RUNE_SYSTEM
        if (ItemSystem::IsRuneItem(item) && ItemSystem::GetItemSocket(item, 1) != 1) continue;
#endif
        const auto immune = ItemSystem::GetItemImmuneFlags(item);
        ItemSystem::ModifyPoints(item, true);
        if (!HasPointState(e)) return;
        if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemOwner(item) == e && ItemSystem::GetWearItem(e, slot) == item)
            ecs::PlayerRuntime::SetImmuneFlag(e, ecs::PlayerRuntime::GetImmuneFlag(e) | immune);
    }
#ifdef ENABLE_EVENT_MANAGER
    CHARACTER_MANAGER::instance().CheckBonusEvent(e);
    if (!HasPointState(e)) return;
#endif
    const int deck = DragonSoulSystem::GetActiveDeck(e);
    if (DragonSoulSystem::IsDeckActivated(e) && deck >= 0 && deck < DRAGON_SOUL_DECK_MAX_NUM) {
        for (int slot = WEAR_MAX_NUM + DS_SLOT_MAX * deck; slot < WEAR_MAX_NUM + DS_SLOT_MAX * (deck + 1); ++slot) {
            const auto item = ItemSystem::GetWearItem(e, slot);
            if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemOwner(item) == e &&
                applied.insert(item).second && DSManager::instance().IsTimeLeftDragonSoul(item))
                ItemSystem::ModifyPoints(item, true);
            if (!HasPointState(e)) return;
        }
    }
    SkillSystem::ComputeSkillPoints(e);
    if (!HasPointState(e)) return;
    // Existing leaf services; their full lifecycle migration is separate.
    AffectSystem::RefreshAffect(e);
    if (!HasPointState(e)) return;
#ifdef __PET_SYSTEM__
    if (auto* pets = ecs::PlayerRuntime::GetPetSystem(e)) pets->RefreshBuff();
    if (!HasPointState(e)) return;
#endif
    ApplyAlignmentPoints(e);
    if (!HasPointState(e)) return;
    if (player || Get(e, POINT_HP) > GetMaxHP(e))
        Change(e, POINT_HP, std::min<int64_t>(hpBefore, GetMaxHP(e)) - Get(e, POINT_HP));
    if (!HasPointState(e)) return;
    if (player || Get(e, POINT_SP) > GetMaxSP(e))
        Change(e, POINT_SP, std::min<int64_t>(spBefore, GetMaxSP(e)) - Get(e, POINT_SP));
    if (!HasPointState(e)) return;
    NetworkSyncSystem::UpdatePacket(e);
    if (HasPointState(e)) ComputeBattlePoints(e);
}
} // namespace ecs::PointSystem

void CHARACTER::ComputePoints()
{
    ecs::PointSystem::Compute(GetEntityHandle());
}





