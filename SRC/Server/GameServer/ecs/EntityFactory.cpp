#include "../stdafx.h"
#include "systems/PlayerRuntimeSystem.hpp"
#include "AIHelpers.hpp"

#include "EntityFactory.hpp"
#include "EntityInvariants.hpp"
#include "ItemInvariants.hpp"

#include <algorithm>
#include <cstring>

#include "ItemRegistry.hpp"
#include "PointSemantic.hpp"
#include "Registry.hpp"
#include "VIDRegistry.hpp"
#include "components/appearance_components.hpp"
#include "components/ai_components.hpp"
#include "components/character_runtime_components.hpp"
#include "components/character_stats_components.hpp"
#include "components/combat_components.hpp"
#include "components/identity_components.hpp"
#include "components/inventory_components.hpp"
#include "components/item_components.hpp"
#include "components/item_proto_components.hpp"
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

ecs::CharacterRuntimeFlagsComponent MakeDefaultRuntimeFlags()
{
    return ecs::CharacterRuntimeFlagsComponent {
        0u,
        0,
        POS_STANDING,
        0u,
        0u,
        GM_PLAYER,
        0u,
        0.0f,
    };
}

ecs::CharacterPoints MakeCharacterPoints(const TPlayerTable& data)
{
    ecs::CharacterPoints points {};

    points.base.job = data.job;
    points.base.voice = data.voice;
    points.base.iRandomHP = data.sRandomHP;
    points.base.iRandomSP = data.sRandomSP;
    points.base.skill_group = data.skill_group;


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
    levels.group = data.skill_group;
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

std::string MakeMobPlayerName(const TMobTable& data)
{
#ifdef ENABLE_MULTI_NAMES
    return data.szLocaleName[DEFAULT_LANGUAGE];
#else
    return data.szLocaleName;
#endif
}

void RegisterEntityVID(entt::registry& reg, entt::entity entity, uint32_t vid)
{
    reg.emplace_or_replace<ecs::VIDComponent>(entity, vid);
    CVIDRegistry::Instance().Register(vid, entity);
}

void AttachLegacyCharacter(entt::registry& reg, entt::entity entity, LPCHARACTER ch)
{
    reg.emplace_or_replace<ecs::LegacyCharPtr>(entity, ch);
    if (ch) {
        ch->SetEntityHandle(entity);
        if (LPDESC desc = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))) {
            desc->SetEntity(entity);
        }
    }
}

void SeedCharacterStatsComponentFromLegacy(entt::registry& reg, entt::entity entity)
{
    const auto* legacy = reg.try_get<ecs::LegacyCharPtr>(entity);
    if (!legacy || !legacy->ptr) {
        return;
    }

    auto& stats = reg.get_or_emplace<ecs::CharacterStatsComponent>(entity);

    for (uint32_t i = 0; i < POINT_MAX_NUM; ++i) {
        if (!ecs::IsStatArrayPoint(static_cast<uint8_t>(i))) {
            continue;
        }

        stats.points[i] = legacy->ptr->GetPoint(static_cast<uint8_t>(i));
    }
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
    entt::entity entity = CVIDRegistry::Instance().Find(legacyVID);
    if (entity != entt::null && reg.valid(entity) && reg.any_of<Tag>(entity)) {
        return entity;
    }

    if (entity == entt::null || !reg.valid(entity)) {
        entity = reg.create();
        RegisterEntityVID(reg, entity, legacyVID);
    }

    reg.emplace_or_replace<ecs::RaceComponent>(entity, ecs::RaceComponent { static_cast<uint16_t>(data.dwVnum) });
    reg.emplace_or_replace<ecs::RaceState>(entity, ecs::RaceState { data.dwVnum, 0u });
    reg.emplace_or_replace<ecs::PlayerName>(entity, MakeMobPlayerName(data));
    reg.emplace_or_replace<Tag>(entity);
    reg.emplace_or_replace<ecs::SocialRefs>(entity, ecs::SocialRefs {});
    reg.emplace_or_replace<ecs::Position>(entity, x, y, 0);
    reg.emplace_or_replace<ecs::WarpPosition>(entity, 0, 0, 0);
    reg.emplace_or_replace<ecs::ExitPosition>(entity, 0, 0, 0);
    reg.emplace_or_replace<ecs::RegenPosition>(entity, x, y, 0.0f);
    reg.emplace_or_replace<ecs::RotationComponent>(entity, 0.0f);
    reg.emplace_or_replace<ecs::MapIndex>(entity, mapIndex);
    reg.emplace_or_replace<ecs::MovementState>(entity, MakeDefaultMovementState(now));
    reg.emplace_or_replace<ecs::MovementSpeed>(entity, std::max<int32_t>(1, data.sMovingSpeed), std::max<int32_t>(1, data.sMovingSpeed));
    reg.emplace_or_replace<ecs::SyncState>(entity, MakeDefaultSyncState());
    reg.emplace_or_replace<ecs::Health>(entity, static_cast<int32_t>(std::min<uint64_t>(data.dwMaxHP, INT32_MAX)), static_cast<int32_t>(std::min<uint64_t>(data.dwMaxHP, INT32_MAX)));
    reg.emplace_or_replace<ecs::Mana>(entity, 0, 0);
    reg.emplace_or_replace<ecs::Stamina>(entity, 0, 0);
    reg.emplace_or_replace<ecs::LevelComponent>(entity, data.bLevel);
    reg.emplace_or_replace<ecs::Experience>(entity, static_cast<int64_t>(data.dwExp), 0);
    reg.emplace_or_replace<ecs::CharacterPoints>(entity, ecs::CharacterPoints {});
    reg.get_or_emplace<ecs::CharacterStatsComponent>(entity);
    SeedCharacterStatsComponentFromLegacy(reg, entity);
    reg.emplace_or_replace<ecs::AppearancePartsComponent>(entity, ecs::AppearancePartsComponent {});
    auto runtimeFlags = MakeDefaultRuntimeFlags();
    runtimeFlags.aiFlag = data.dwAIFlag;
    reg.emplace_or_replace<ecs::CharacterRuntimeFlagsComponent>(entity, runtimeFlags);
    reg.emplace_or_replace<ecs::CombatStats>(entity, MakeDefaultCombatStats(0));
    reg.emplace_or_replace<ecs::AttackCooldown>(entity, MakeDefaultAttackCooldown(now));
    reg.emplace_or_replace<ecs::DamageMap>(entity, ecs::DamageMap {});
    reg.emplace_or_replace<ecs::AffectList>(entity, MakeDefaultAffectList());
    reg.emplace_or_replace<ecs::StatusFlags>(entity, MakeDefaultStatusFlags());
    reg.emplace_or_replace<ecs::ImmunityFlags>(entity, ecs::ImmunityFlags {});
    reg.emplace_or_replace<ecs::AIState>(entity, ecs::AIState { 0u, 0u, 1u });
    const auto aiFlags = MakeAIFlags(data);
    reg.emplace_or_replace<ecs::AIFlags>(entity, aiFlags);
    reg.emplace_or_replace<ecs::AggroTable>(entity, ecs::AggroTable {});
    reg.emplace_or_replace<ecs::SpawnInfo>(entity, ecs::SpawnInfo { x, y, static_cast<uint32_t>(mapIndex), 0u, 0u });
    reg.emplace_or_replace<ecs::MobDataRef>(entity, mobProto, nullptr);
    reg.emplace_or_replace<ecs::FlyTargets>(entity, ecs::FlyTargets { 0u, {} });

    if (aiFlags.isGuard) {
        reg.emplace_or_replace<ecs::GuardState>(entity, ecs::GuardState { entt::null, std::max<uint32_t>(data.wAggressiveSight, 500u), 0u });
    }

    if (data.bType == CHAR_TYPE_HORSE) {
        reg.emplace_or_replace<ecs::HorseAITag>(entity);
    }

    if (data.bType == CHAR_TYPE_STONE) {
        reg.emplace_or_replace<ecs::StoneAITag>(entity);
    }

    ecs::Invariants::ValidateCharacterTags(reg, entity, "factory.mob_entity");
    ecs::Invariants::ValidateCommonIdentity(reg, entity, "factory.mob_entity");
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

ecs::ItemIdentity MakeItemIdentity(LPITEM item)
{
    return ecs::ItemIdentity {
        item->GetID(),
        item->GetOriginalVnum(),
        item->GetVID(),
        item->GetMaskVnum(),
    };
}

ecs::ItemLocation MakeItemLocation(LPITEM item)
{
    return ecs::ItemLocation {
        item->GetWindow(),
        item->GetCell(),
    };
}

ecs::ItemCount MakeItemCount(LPITEM item)
{
    return ecs::ItemCount { item->GetCount() };
}

ecs::ItemPrototypeMeta MakeItemPrototypeMeta(LPITEM item)
{
    return ecs::ItemPrototypeMeta {
        item->GetType(),
        item->GetSubType(),
    };
}

ecs::ItemOwner MakeItemOwner(LPITEM item)
{
    uint32_t ownerPID = 0;

    if (const auto* owner = item->GetOwner()) {
        ownerPID = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(owner));
    }

    return ecs::ItemOwner {
        ownerPID,
        item->GetLastOwnerPID(),
        ownerPID,
    };
}

ecs::ItemEquipped MakeItemEquipped(LPITEM item)
{
    uint8_t slot = 0;
    if (item->IsEquipped() && item->GetCell() >= INVENTORY_MAX_NUM) {
        slot = static_cast<uint8_t>(item->GetCell() - INVENTORY_MAX_NUM);
    }

    return ecs::ItemEquipped { item->IsEquipped(), slot };
}

ecs::ItemFlags MakeItemFlags(LPITEM item)
{
    return ecs::ItemFlags {
        item->GetFlag(),
        item->IsExchanging(),
        item->GetSkipSave(),
        item->isLocked(),
    };
}

ecs::ItemSockets MakeItemSockets(LPITEM item)
{
    ecs::ItemSockets sockets {};
    std::copy_n(item->GetSockets(), ITEM_SOCKET_MAX_NUM, sockets.sockets.begin());
    return sockets;
}

ecs::ItemAttributes MakeItemAttributes(LPITEM item)
{
    ecs::ItemAttributes attributes {};
    std::copy_n(item->GetAttributes(), ITEM_ATTRIBUTE_MAX_NUM, attributes.attrs.begin());
    return attributes;
}

ecs::ItemProtoRef MakeItemProtoRef(LPITEM item)
{
    ecs::ItemProtoRef protoRef {};
    const TItemTable* proto = item->GetProto();
    if (!proto)
        return protoRef;

    protoRef.base_vnum = proto->dwVnum;
    protoRef.type = proto->bType;
    protoRef.subtype = proto->bSubType;
    protoRef.weapon_min = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[3]));
    protoRef.weapon_max = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[4]));
    protoRef.defense = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[1]));
    protoRef.magic_min = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[5]));
    protoRef.magic_max = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[6]));
#ifdef ENABLE_MULTI_NAMES
    std::strncpy(protoRef.name, proto->szLocaleName[0], ITEM_NAME_MAX_LEN);
#else
    std::strncpy(protoRef.name, proto->szLocaleName, ITEM_NAME_MAX_LEN);
#endif
    protoRef.name[ITEM_NAME_MAX_LEN] = '\0';
    protoRef.size = proto->bSize;
    protoRef.level_limit = static_cast<uint8_t>(std::clamp(item->GetLevelLimit(), 0, 255));
    protoRef.wear_flags = proto->dwWearFlags;
    protoRef.anti_flags = proto->dwAntiFlags;
    protoRef.immune_flags = proto->dwImmuneFlag;
    protoRef.refined_vnum = item->GetRefinedVnum();
    protoRef.refine_level = static_cast<uint8_t>(std::clamp(item->GetRefineLevel(), 0, 255));
    protoRef.limit_timer_wear_index = static_cast<int8_t>(proto->cLimitTimerBasedOnWearIndex);
    protoRef.proto = proto;
    return protoRef;
}

void SyncItemEntity(entt::registry& reg, entt::entity entity, LPITEM item)
{
    reg.emplace_or_replace<ecs::ItemIdentity>(entity, MakeItemIdentity(item));
    reg.emplace_or_replace<ecs::ItemLocation>(entity, MakeItemLocation(item));
    reg.emplace_or_replace<ecs::ItemCount>(entity, MakeItemCount(item));
    reg.emplace_or_replace<ecs::ItemPrototypeMeta>(entity, MakeItemPrototypeMeta(item));
    reg.emplace_or_replace<ecs::ItemOwner>(entity, MakeItemOwner(item));
    reg.emplace_or_replace<ecs::ItemEquipped>(entity, MakeItemEquipped(item));
    reg.emplace_or_replace<ecs::ItemFlags>(entity, MakeItemFlags(item));
    reg.emplace_or_replace<ecs::ItemSockets>(entity, MakeItemSockets(item));
    reg.emplace_or_replace<ecs::ItemAttributes>(entity, MakeItemAttributes(item));
    reg.emplace_or_replace<ecs::ItemProtoRef>(entity, MakeItemProtoRef(item));
}

} // namespace

entt::entity EntityFactory::EnsureLegacyCharacterEntity(entt::registry& reg, LPCHARACTER ch, uint32_t legacyVID)
{
    if (!ch) {
        return entt::null;
    }

    const entt::entity existing = CVIDRegistry::Instance().Find(legacyVID);
    if (existing != entt::null && reg.valid(existing)) {
        RegisterEntityVID(reg, existing, legacyVID);
        AttachLegacyCharacter(reg, existing, ch);
        return existing;
    }

    const entt::entity entity = reg.create();
    RegisterEntityVID(reg, entity, legacyVID);
    AttachLegacyCharacter(reg, entity, ch);
    return entity;
}

entt::entity EntityFactory::CreatePC(entt::registry& reg, const TPlayerTable& data, LPDESC desc, uint32_t legacyVID)
{
    const uint32_t now = get_dword_time();
    const ecs::GMLevel gmLevel = MakeGMLevel(data, desc);

    entt::entity entity = CVIDRegistry::Instance().Find(legacyVID);
    if (entity != entt::null && reg.valid(entity) && reg.all_of<ecs::TagPC>(entity)) {
        if (desc) {
            desc->SetEntity(entity);
        }
        return entity;
    }

    if (entity == entt::null || !reg.valid(entity)) {
        entity = reg.create();
        RegisterEntityVID(reg, entity, legacyVID);
    }

    reg.emplace_or_replace<ecs::PlayerID>(entity, data.id);
    reg.emplace_or_replace<ecs::AccountID>(entity, ecs::AccountID { desc ? desc->GetAccountTable().id : 0u });
    reg.emplace_or_replace<ecs::EmpireComponent>(entity, ecs::EmpireComponent { static_cast<uint8_t>(desc ? desc->GetEmpire() : 0u) });
    reg.emplace_or_replace<ecs::RaceComponent>(entity, ecs::RaceComponent { data.job });
    reg.emplace_or_replace<ecs::RaceState>(entity, ecs::RaceState { static_cast<uint32_t>(data.job), 0u });
    reg.emplace_or_replace<ecs::PlayerName>(entity, std::string(data.name));
    reg.emplace_or_replace<ecs::GMLevel>(entity, gmLevel);
    reg.emplace_or_replace<ecs::TagPC>(entity);
    reg.emplace_or_replace<ecs::SocialRefs>(entity, ecs::SocialRefs {});

    reg.emplace_or_replace<ecs::Position>(entity, data.x, data.y, data.z);
    reg.emplace_or_replace<ecs::WarpPosition>(entity, 0, 0, 0);
    reg.emplace_or_replace<ecs::ExitPosition>(entity, data.lExitX, data.lExitY, data.lExitMapIndex);
    reg.emplace_or_replace<ecs::RegenPosition>(entity, 0, 0, 0.0f);
    reg.emplace_or_replace<ecs::RotationComponent>(entity, static_cast<float>(data.dir));
    reg.emplace_or_replace<ecs::MapIndex>(entity, data.lMapIndex);

    reg.emplace_or_replace<ecs::MovementState>(entity, MakeDefaultMovementState(now));
    reg.emplace_or_replace<ecs::MovementSpeed>(entity, 100, 100);
    reg.emplace_or_replace<ecs::SyncState>(entity, MakeDefaultSyncState());

    reg.emplace_or_replace<ecs::Health>(entity, data.hp, data.hp);
    reg.emplace_or_replace<ecs::Mana>(entity, data.sp, data.sp);
    reg.emplace_or_replace<ecs::Stamina>(entity, data.stamina, data.stamina);
    auto runtimeFlags = MakeDefaultRuntimeFlags();
    runtimeFlags.gmLevel = gmLevel.level;
    runtimeFlags.rotation = static_cast<float>(data.dir);
    reg.emplace_or_replace<ecs::CharacterRuntimeFlagsComponent>(entity, runtimeFlags);
    ecs::AppearancePartsComponent appearance {};
    appearance.basePart = data.part_base;
    std::copy_n(std::begin(data.parts), PART_MAX_NUM, std::begin(appearance.parts));
    reg.emplace_or_replace<ecs::AppearancePartsComponent>(entity, appearance);
    reg.emplace_or_replace<ecs::LevelComponent>(entity, data.level);
    reg.emplace_or_replace<ecs::Experience>(entity, static_cast<int64_t>(data.exp), 0);
    reg.emplace_or_replace<ecs::CharacterPoints>(entity, MakeCharacterPoints(data));
    reg.get_or_emplace<ecs::CharacterStatsComponent>(entity);
    SeedCharacterStatsComponentFromLegacy(reg, entity);

    reg.emplace_or_replace<ecs::CombatStats>(entity, MakeDefaultCombatStats(data.lAlignment));
    reg.emplace_or_replace<ecs::AttackCooldown>(entity, MakeDefaultAttackCooldown(now));
    reg.emplace_or_replace<ecs::DamageMap>(entity, ecs::DamageMap {});

    reg.emplace_or_replace<ecs::AffectList>(entity, MakeDefaultAffectList());
    auto& statusFlags = reg.emplace_or_replace<ecs::StatusFlags>(entity, MakeDefaultStatusFlags());
    statusFlags.isGM = (gmLevel.level > 0);
    reg.emplace_or_replace<ecs::ImmunityFlags>(entity, ecs::ImmunityFlags {});

    reg.emplace_or_replace<ecs::NetworkSession>(entity, desc);
    reg.emplace_or_replace<ecs::LoginInfo>(entity, MakeLoginInfo(data, desc, now));
    reg.emplace_or_replace<ecs::AntiFlood>(entity, ecs::AntiFlood { 0, 0u, 0, 0u });

    reg.emplace_or_replace<ecs::EquipmentSlots>(entity, ecs::EquipmentSlots {});
    reg.emplace_or_replace<ecs::InventoryGrid>(entity, ecs::InventoryGrid {});
    reg.emplace_or_replace<ecs::MainInventoryRuntimeComponent>(entity, ecs::MainInventoryRuntimeComponent {});
#ifdef ENABLE_EXTRA_INVENTORY
    reg.emplace_or_replace<ecs::ExtraInventoryRuntimeComponent>(entity, ecs::ExtraInventoryRuntimeComponent {});
#endif
    reg.emplace_or_replace<ecs::CubeWindowComponent>(entity, ecs::CubeWindowComponent {});
    reg.emplace_or_replace<ecs::DragonSoulInventoryComponent>(entity, ecs::DragonSoulInventoryComponent {});
    reg.emplace_or_replace<ecs::DragonSoulRuntimeStateComponent>(entity, ecs::DragonSoulRuntimeStateComponent {});
#ifdef __ATTR_TRANSFER_SYSTEM__
    reg.emplace_or_replace<ecs::AttrTransferWindowComponent>(entity, ecs::AttrTransferWindowComponent {});
#endif
#ifdef ENABLE_ACCE_SYSTEM
    reg.emplace_or_replace<ecs::AcceWindowComponent>(entity, ecs::AcceWindowComponent {});
#endif
#ifdef ENABLE_SWITCHBOT
    reg.emplace_or_replace<ecs::SwitchbotRuntimeComponent>(entity, ecs::SwitchbotRuntimeComponent {});
#endif
    reg.emplace_or_replace<ecs::GoldAmount>(entity, data.gold);
    reg.emplace_or_replace<ecs::QuickSlots>(entity, MakeQuickSlots(data));
    reg.emplace_or_replace<ecs::SafeboxRef>(entity, nullptr, nullptr, -1, 0, 0, false);

    reg.emplace_or_replace<ecs::SkillLevels>(entity, MakeSkillLevels(data));
    reg.emplace_or_replace<ecs::SkillCooldowns>(entity, ecs::SkillCooldowns { {}, now, false });
    reg.emplace_or_replace<ecs::SkillDamageBonus>(entity, ecs::SkillDamageBonus {});
    reg.emplace_or_replace<ecs::SkillColor>(entity, ecs::SkillColor {});

    reg.emplace_or_replace<ecs::PartyMembership>(entity, nullptr, now > 180000 ? now - 180000 : 0);
    reg.emplace_or_replace<ecs::GuildMembership>(entity, nullptr, now > 60000 ? now - 60000 : 0);
    reg.emplace_or_replace<ecs::DungeonMembership>(entity, nullptr, 0, nullptr);
    reg.emplace_or_replace<ecs::MarriageState>(entity, nullptr, nullptr);
    reg.emplace_or_replace<ecs::ShopState>(entity, ecs::ShopState {});
    reg.emplace_or_replace<ecs::WarpBlockState>(entity, ecs::WarpBlockState {});
    reg.emplace_or_replace<ecs::MountState>(entity, ecs::MountState {
        0u,
        0u,
        data.horse.bLevel,
        0u,
        0u,
        0,
        data.horse.bRiding != 0,
    });

    reg.emplace_or_replace<ecs::QuestContext>(entity, 0u, 0u, nullptr);
    reg.emplace_or_replace<ecs::RankPoints>(entity, MakeRankPoints(data));
    reg.emplace_or_replace<ecs::AlignBonuses>(entity, ecs::AlignBonuses { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, static_cast<uint8_t>(255) });

    if (desc) {
        desc->SetEntity(entity);
    }

    ecs::Invariants::ValidateCharacterTags(reg, entity, "factory.pc");
    ecs::Invariants::ValidatePCIdentity(reg, entity, "factory.pc");
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

entt::entity EntityFactory::CreateItemEntity(entt::registry& reg, LPITEM item)
{
    if (!item) {
        return entt::null;
    }

    const uint32_t itemID = item->GetID();
    if (itemID == 0) {
        return entt::null;
    }

    const entt::entity existing = CItemRegistry::Instance().Find(itemID);
    if (existing != entt::null && reg.valid(existing)) {
        SyncItemEntity(reg, existing, item);
        CItemRegistry::Instance().Register(itemID, item->GetVID(), existing);
        ecs::ItemInvariants::ValidateItemEntity(reg, existing, "item.factory.existing");
        return existing;
    }

    const entt::entity entity = reg.create();
    SyncItemEntity(reg, entity, item);
    CItemRegistry::Instance().Register(itemID, item->GetVID(), entity);
    ecs::ItemInvariants::ValidateItemEntity(reg, entity, "item.factory.create");
    return entity;
}

void EntityFactory::DestroyItemEntity(entt::registry& reg, LPITEM item)
{
    if (!item) {
        return;
    }

    const uint32_t itemID = item->GetID();
    if (itemID == 0) {
        return;
    }

    const entt::entity entity = CItemRegistry::Instance().Find(itemID);
    CItemRegistry::Instance().Unregister(itemID);

    if (entity != entt::null && reg.valid(entity)) {
        reg.destroy(entity);
    }
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

    if (const auto* legacy = reg.try_get<ecs::LegacyCharPtr>(e); legacy && legacy->ptr) {
        legacy->ptr->SetEntityHandle(entt::null);
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
