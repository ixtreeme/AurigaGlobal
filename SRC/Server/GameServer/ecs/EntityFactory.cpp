#include "../stdafx.h"

#include "EntityFactory.hpp"

#include <algorithm>
#include <cstring>

#include "Registry.hpp"
#include "VIDRegistry.hpp"
#include "components/ai_components.hpp"
#include "components/combat_components.hpp"
#include "components/identity_components.hpp"
#include "components/inventory_components.hpp"
#include "components/movement_components.hpp"
#include "components/quest_components.hpp"
#include "components/session_components.hpp"
#include "components/skill_components.hpp"
#include "components/social_components.hpp"
#include "components/status_components.hpp"
#include "components/transform_components.hpp"
#include "components/vital_components.hpp"
#include "../char.h"
#include "../char_manager.h"
#include "../desc.h"
#include "../gm.h"
#include "../mob_manager.h"
#include "../utils.h"

namespace {

ecs::MovementState MakeDefaultMovementState(uint32_t now)
{
    return ecs::MovementState {
        0,
        0,
        now,
        now > 20000 ? now - 20000 : 0,
        0,
        now,
        false,
        false,
        false,
    };
}

ecs::AttackCooldown MakeDefaultAttackCooldown(uint32_t now)
{
    return ecs::AttackCooldown {
        now > 20000 ? now - 20000 : 0,
        0,
        0,
        0,
        0,
        0,
        0,
    };
}

ecs::SyncState MakeDefaultSyncState()
{
    timeval zeroTimeval {};
    return ecs::SyncState {
        get_float_time() - 3.0f,
        0,
        zeroTimeval,
    };
}

ecs::CombatStats MakeDefaultCombatStats(uint32_t alignment)
{
    return ecs::CombatStats {
        alignment,
        alignment,
        0,
        PK_MODE_PEACE,
        -100,
        0,
    };
}

ecs::AffectList MakeDefaultAffectList()
{
    return ecs::AffectList {
        {},
        {},
        TAffectFlag(0, 0),
        false,
    };
}

ecs::StatusFlags MakeDefaultStatusFlags()
{
    return ecs::StatusFlags {};
}

ecs::CharacterPoints MakeCharacterPoints(const TPlayerTable& data)
{
    ecs::CharacterPoints points {};

    points.base.job = data.job;
    points.base.voice = data.voice;
    points.base.level = data.level;
    points.base.exp = data.exp;
    points.base.gold = data.gold;
    points.base.hp = data.hp;
    points.base.sp = data.sp;
    points.base.iRandomHP = data.sRandomHP;
    points.base.iRandomSP = data.sRandomSP;
    points.base.stamina = data.stamina;
    points.base.skill_group = data.skill_group;

    points.instant.fRot = static_cast<float>(data.dir);
    points.instant.iMaxHP = data.hp;
    points.instant.iMaxSP = data.sp;
    points.instant.position = POS_STANDING;
    points.instant.gm_level = 0;
    points.instant.bBasePart = data.part_base;
    points.instant.iMaxStamina = data.stamina;
    std::copy_n(std::begin(data.parts), PART_MAX_NUM, std::begin(points.instant.parts));

    points.base.points[POINT_LEVEL] = data.level;
    points.base.points[POINT_EXP] = data.exp;
    points.base.points[POINT_GOLD] = data.gold;
    points.base.points[POINT_HP] = data.hp;
    points.base.points[POINT_MAX_HP] = data.hp;
    points.base.points[POINT_SP] = data.sp;
    points.base.points[POINT_MAX_SP] = data.sp;
    points.base.points[POINT_STAMINA] = data.stamina;
    points.base.points[POINT_MAX_STAMINA] = data.stamina;
    points.base.points[POINT_PLAYTIME] = data.playtime;
    points.base.points[POINT_STAT] = data.stat_point;
    points.base.points[POINT_SKILL] = data.skill_point;
    points.base.points[POINT_SUB_SKILL] = data.sub_skill_point;
    points.base.points[POINT_HORSE_SKILL] = data.horse_skill_point;
    points.base.points[POINT_LEVEL_STEP] = data.level_step;
    points.base.points[POINT_ST] = data.st;
    points.base.points[POINT_HT] = data.ht;
    points.base.points[POINT_DX] = data.dx;
    points.base.points[POINT_IQ] = data.iq;
    points.base.points[POINT_STAT_RESET_COUNT] = data.stat_reset_count;

    return points;
}

ecs::QuickSlots MakeQuickSlots(const TPlayerTable& data)
{
    ecs::QuickSlots quickSlots {};
    std::copy_n(std::begin(data.quickslot), QUICKSLOT_MAX_NUM, quickSlots.slots.begin());
    return quickSlots;
}

ecs::SkillLevels MakeSkillLevels(const TPlayerTable& data)
{
    ecs::SkillLevels levels {};
    levels.levels = new TPlayerSkill[SKILL_MAX_NUM] {};
    std::copy_n(std::begin(data.skills), SKILL_MAX_NUM, levels.levels);
    return levels;
}

ecs::RankPoints MakeRankPoints(const TPlayerTable& data)
{
    ecs::RankPoints rankPoints {};
#ifdef ENABLE_RANKING
    std::copy_n(std::begin(data.lRankPoints), RANKING_MAX_CATEGORIES, std::begin(rankPoints.points));
#else
    (void)data;
#endif
    return rankPoints;
}

ecs::LoginInfo MakeLoginInfo(const TPlayerTable& data, LPDESC desc, uint32_t now)
{
    ecs::LoginInfo info {};

    if (desc) {
        info.login = desc->GetAccountTable().login;
    }

    info.loginPlayTime = static_cast<uint32_t>(std::max(data.playtime, 0));
    info.playStartTime = now;
    info.mobile = data.szMobile;
    info.logOffInterval = data.logoff_interval;
    return info;
}

ecs::GMLevel MakeGMLevel(const TPlayerTable& data, LPDESC desc)
{
    if (!desc) {
        return ecs::GMLevel { 0 };
    }

    return ecs::GMLevel {
        gm_get_level(data.name, desc->GetHostName(), desc->GetAccountTable().login)
    };
}

void RegisterEntityVID(entt::registry& reg, entt::entity entity, uint32_t vid)
{
    reg.emplace<ecs::VIDComponent>(entity, vid);
    CVIDRegistry::Instance().Register(vid, entity);
}

ecs::AIFlags MakeAIFlags(const TMobTable& data)
{
    ecs::AIFlags flags {};
    flags.isAggressive = IS_SET(data.dwAIFlag, AIFLAG_AGGRESSIVE);
    flags.isCoward = IS_SET(data.dwAIFlag, AIFLAG_COWARD);
    flags.isAttackMob = IS_SET(data.dwAIFlag, AIFLAG_ATTACKMOB);
    flags.isNoAttackShinsu = IS_SET(data.dwAIFlag, AIFLAG_NOATTACKSHINSU);
    flags.isNoAttackChunjo = IS_SET(data.dwAIFlag, AIFLAG_NOATTACKCHUNJO);
    flags.isNoAttackJinno = IS_SET(data.dwAIFlag, AIFLAG_NOATTACKJINNO);
    flags.isBerserk = IS_SET(data.dwAIFlag, AIFLAG_BERSERK);
    flags.isDeadFly = false;
    flags.isStoneSkinner = IS_SET(data.dwAIFlag, AIFLAG_STONESKIN);
    flags.isGodSpeed = IS_SET(data.dwAIFlag, AIFLAG_GODSPEED);
    flags.isDeathBlower = IS_SET(data.dwAIFlag, AIFLAG_DEATHBLOW);
    flags.isReviver = IS_SET(data.dwAIFlag, AIFLAG_REVIVE);
    flags.isNoMove = IS_SET(data.dwAIFlag, AIFLAG_NOMOVE);
    flags.isGuard = (data.bType == CHAR_TYPE_NPC)
        && (data.dwVnum == 11000 || data.dwVnum == 11002 || data.dwVnum == 11004);
    return flags;
}

template <typename Tag>
entt::entity CreateMobEntity(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID)
{
    const uint32_t now = get_dword_time();
    const CMob* mobProto = CMobManager::instance().Get(data.dwVnum);
    const entt::entity existing = CVIDRegistry::Instance().Find(legacyVID);
    if (existing != entt::null && reg.valid(existing)) {
        return existing;
    }

    entt::entity entity = reg.create();

    RegisterEntityVID(reg, entity, legacyVID);
    reg.emplace<ecs::RaceComponent>(entity, ecs::RaceComponent { static_cast<uint16_t>(data.dwVnum) });
    reg.emplace<Tag>(entity);
    reg.emplace<ecs::Position>(entity, x, y, 0);
    reg.emplace<ecs::WarpPosition>(entity, 0, 0, 0);
    reg.emplace<ecs::ExitPosition>(entity, 0, 0, 0);
    reg.emplace<ecs::RegenPosition>(entity, x, y, 0.0f);
    reg.emplace<ecs::RotationComponent>(entity, 0.0f);
    reg.emplace<ecs::MapIndex>(entity, mapIndex);
    reg.emplace<ecs::MovementState>(entity, MakeDefaultMovementState(now));
    reg.emplace<ecs::MovementSpeed>(entity, std::max<int32_t>(1, data.sMovingSpeed), std::max<int32_t>(1, data.sMovingSpeed));
    reg.emplace<ecs::SyncState>(entity, MakeDefaultSyncState());
    reg.emplace<ecs::Health>(entity, static_cast<int32_t>(std::min<uint64_t>(data.dwMaxHP, INT32_MAX)), static_cast<int32_t>(std::min<uint64_t>(data.dwMaxHP, INT32_MAX)));
    reg.emplace<ecs::Mana>(entity, 0, 0);
    reg.emplace<ecs::Stamina>(entity, 0, 0);
    reg.emplace<ecs::LevelComponent>(entity, data.bLevel);
    reg.emplace<ecs::Experience>(entity, static_cast<int64_t>(data.dwExp), 0);
    reg.emplace<ecs::CharacterPoints>(entity, ecs::CharacterPoints {});
    reg.emplace<ecs::CombatStats>(entity, MakeDefaultCombatStats(0));
    reg.emplace<ecs::AttackCooldown>(entity, MakeDefaultAttackCooldown(now));
    reg.emplace<ecs::DamageMap>(entity, ecs::DamageMap {});
    reg.emplace<ecs::AffectList>(entity, MakeDefaultAffectList());
    reg.emplace<ecs::StatusFlags>(entity, MakeDefaultStatusFlags());
    reg.emplace<ecs::AIState>(entity, ecs::AIState { 0u, 0u, 1u });
    const auto aiFlags = MakeAIFlags(data);
    reg.emplace<ecs::AIFlags>(entity, aiFlags);
    reg.emplace<ecs::AggroTable>(entity, ecs::AggroTable {});
    reg.emplace<ecs::SpawnInfo>(entity, ecs::SpawnInfo { x, y, static_cast<uint32_t>(mapIndex), 0u, 0u });
    reg.emplace<ecs::MobDataRef>(entity, mobProto, nullptr);
    reg.emplace<ecs::FlyTargets>(entity, ecs::FlyTargets { 0u, {} });

    if (aiFlags.isGuard) {
        reg.emplace<ecs::GuardState>(entity, ecs::GuardState { entt::null, std::max<uint32_t>(data.wAggressiveSight, 500u), 0u });
    }

    if (data.bType == CHAR_TYPE_HORSE) {
        reg.emplace<ecs::HorseAITag>(entity);
    }

    if (data.bType == CHAR_TYPE_STONE) {
        reg.emplace<ecs::StoneAITag>(entity);
    }

    return entity;
}

void CleanupCombatReferences(entt::registry& reg, entt::entity victim)
{
    auto combatView = reg.view<ecs::CombatTarget>();
    for (auto other : combatView) {
        auto& target = combatView.get<ecs::CombatTarget>(other);
        if (target.target != victim) {
            continue;
        }

        target.target = entt::null;
        if (reg.all_of<ecs::CombatActiveTag>(other)) {
            reg.remove<ecs::CombatActiveTag>(other);
        }
    }

    auto aggroView = reg.view<ecs::AggroTable>();
    for (auto other : aggroView) {
        auto& aggro = aggroView.get<ecs::AggroTable>(other);
        auto& entries = aggro.entries;
        entries.erase(
            std::remove_if(entries.begin(), entries.end(), [victim](const auto& entry) {
                return entry.first == victim;
            }),
            entries.end());

        if (aggro.stoneOwner == victim) {
            aggro.stoneOwner = entt::null;
        }
    }
}

void RemoveFromLegacyMapSector(entt::registry& reg, entt::entity entity)
{
    (void)reg;
    (void)entity;
    // The legacy sectree still stores LPENTITY/LPCHARACTER pointers.
    // Actual entt::entity-backed map sector membership is introduced in Phase 4.
}

} // namespace

entt::entity EntityFactory::CreatePC(entt::registry& reg, const TPlayerTable& data, LPDESC desc, uint32_t legacyVID)
{
    const uint32_t now = get_dword_time();
    const ecs::GMLevel gmLevel = MakeGMLevel(data, desc);

    const entt::entity existing = CVIDRegistry::Instance().Find(legacyVID);
    if (existing != entt::null && reg.valid(existing)) {
        if (desc) {
            desc->SetEntity(existing);
        }
        return existing;
    }

    entt::entity entity = reg.create();

    RegisterEntityVID(reg, entity, legacyVID);
    reg.emplace<ecs::PlayerID>(entity, data.id);
    reg.emplace<ecs::AccountID>(entity, ecs::AccountID { desc ? desc->GetAccountTable().id : 0u });
    reg.emplace<ecs::EmpireComponent>(entity, ecs::EmpireComponent { static_cast<uint8_t>(desc ? desc->GetEmpire() : 0u) });
    reg.emplace<ecs::RaceComponent>(entity, ecs::RaceComponent { data.job });
    reg.emplace<ecs::PlayerName>(entity, std::string(data.name));
    reg.emplace<ecs::GMLevel>(entity, gmLevel);
    reg.emplace<ecs::TagPC>(entity);

    reg.emplace<ecs::Position>(entity, data.x, data.y, data.z);
    reg.emplace<ecs::WarpPosition>(entity, 0, 0, 0);
    reg.emplace<ecs::ExitPosition>(entity, data.lExitX, data.lExitY, data.lExitMapIndex);
    reg.emplace<ecs::RegenPosition>(entity, 0, 0, 0.0f);
    reg.emplace<ecs::RotationComponent>(entity, static_cast<float>(data.dir));
    reg.emplace<ecs::MapIndex>(entity, data.lMapIndex);

    reg.emplace<ecs::MovementState>(entity, MakeDefaultMovementState(now));
    reg.emplace<ecs::MovementSpeed>(entity, 100, 100);
    reg.emplace<ecs::SyncState>(entity, MakeDefaultSyncState());

    reg.emplace<ecs::Health>(entity, data.hp, data.hp);
    reg.emplace<ecs::Mana>(entity, data.sp, data.sp);
    reg.emplace<ecs::Stamina>(entity, data.stamina, data.stamina);
    reg.emplace<ecs::LevelComponent>(entity, data.level);
    reg.emplace<ecs::Experience>(entity, static_cast<int64_t>(data.exp), 0);
    reg.emplace<ecs::CharacterPoints>(entity, MakeCharacterPoints(data));

    reg.emplace<ecs::CombatStats>(entity, MakeDefaultCombatStats(data.lAlignment));
    reg.emplace<ecs::AttackCooldown>(entity, MakeDefaultAttackCooldown(now));
    reg.emplace<ecs::DamageMap>(entity, ecs::DamageMap {});

    reg.emplace<ecs::AffectList>(entity, MakeDefaultAffectList());
    auto& statusFlags = reg.emplace<ecs::StatusFlags>(entity, MakeDefaultStatusFlags());
    statusFlags.isGM = (gmLevel.level > 0);

    reg.emplace<ecs::NetworkSession>(entity, desc);
    reg.emplace<ecs::LoginInfo>(entity, MakeLoginInfo(data, desc, now));
    reg.emplace<ecs::AntiFlood>(entity, ecs::AntiFlood { 0, 0u, 0, 0u });

    reg.emplace<ecs::EquipmentSlots>(entity, ecs::EquipmentSlots {});
    reg.emplace<ecs::InventoryGrid>(entity, ecs::InventoryGrid {});
    reg.emplace<ecs::GoldAmount>(entity, data.gold);
    reg.emplace<ecs::QuickSlots>(entity, MakeQuickSlots(data));
    reg.emplace<ecs::SafeboxRef>(entity, nullptr, nullptr, -1, 0, 0, false);

    reg.emplace<ecs::SkillLevels>(entity, MakeSkillLevels(data));
    reg.emplace<ecs::SkillCooldowns>(entity, ecs::SkillCooldowns { {}, now, false });
    reg.emplace<ecs::SkillDamageBonus>(entity, ecs::SkillDamageBonus {});
    reg.emplace<ecs::SkillColor>(entity, ecs::SkillColor {});

    reg.emplace<ecs::PartyMembership>(entity, nullptr, now > 180000 ? now - 180000 : 0);
    reg.emplace<ecs::GuildMembership>(entity, nullptr, now > 60000 ? now - 60000 : 0);
    reg.emplace<ecs::DungeonMembership>(entity, nullptr, 0, nullptr);
    reg.emplace<ecs::MarriageState>(entity, nullptr, nullptr);
    reg.emplace<ecs::ShopState>(entity, nullptr, nullptr, std::string {}, true, false, 0, 0u);
    reg.emplace<ecs::MountState>(entity, ecs::MountState { 0u, 0u, data.horse.bLevel, 0u, 0u, 0 });

    reg.emplace<ecs::QuestContext>(entity, 0u, 0u, nullptr);
    reg.emplace<ecs::RankPoints>(entity, MakeRankPoints(data));
    reg.emplace<ecs::AlignBonuses>(entity, ecs::AlignBonuses { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, static_cast<uint8_t>(255) });

    if (desc) {
        desc->SetEntity(entity);
    }

    return entity;
}

entt::entity EntityFactory::CreateMonster(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID)
{
    entt::entity entity = CreateMobEntity<ecs::TagMonster>(reg, data, x, y, mapIndex, legacyVID);
    return entity;
}

entt::entity EntityFactory::CreateNPC(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID)
{
    entt::entity entity = CreateMobEntity<ecs::TagNPC>(reg, data, x, y, mapIndex, legacyVID);
    return entity;
}

entt::entity EntityFactory::CreateStone(entt::registry& reg, const TMobTable& data, int x, int y, int mapIndex, uint32_t legacyVID)
{
    entt::entity entity = CreateMobEntity<ecs::TagStone>(reg, data, x, y, mapIndex, legacyVID);
    return entity;
}

void EntityFactory::Destroy(entt::registry& reg, entt::entity e)
{
    if (!reg.valid(e)) {
        return;
    }

    CleanupCombatReferences(reg, e);

    if (const auto* vid = reg.try_get<ecs::VIDComponent>(e)) {
        CVIDRegistry::Instance().Unregister(vid->value);
    }

    if (const auto* session = reg.try_get<ecs::NetworkSession>(e)) {
        if (session->desc) {
            session->desc->BindCharacter(nullptr);
        }
    }

    RemoveFromLegacyMapSector(reg, e);

    if (auto* skillLevels = reg.try_get<ecs::SkillLevels>(e)) {
        delete[] skillLevels->levels;
        skillLevels->levels = nullptr;
    }

    reg.destroy(e);
}
