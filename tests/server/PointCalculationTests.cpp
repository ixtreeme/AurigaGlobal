#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/constants.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/skill.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/dragon_soul_table.h"
#include "../../SRC/Server/GameServer/PetSystem.h"
#include "../../SRC/Server/GameServer/utils.h"
#include "../../SRC/Server/GameServer/mob_manager.h"
#include "../../SRC/Server/GameServer/guild.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/horsename_manager.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/components/character_stats_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/vital_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/skill_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/combat_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SkillSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/DragonSoulSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
entt::dispatcher g_dispatcher;
TJobInitialPoints JobInitialPoints[JOB_MAX_NUM] {};
THorseStat c_aHorseStat[HORSE_MAX_LEVEL + 1] {};
const uint32_t* exp_table = nullptr;
constexpr TApplyInfo TestApplyInfo(unsigned type) {
    return {static_cast<uint8_t>(type == APPLY_MAX_HP ? POINT_MAX_HP :
        type == APPLY_MAX_SP ? POINT_MAX_SP : type == APPLY_MAX_HP_PCT ? POINT_MAX_HP_PCT :
        type == APPLY_MAX_SP_PCT ? POINT_MAX_SP_PCT : POINT_NONE)};
}
static_assert(APPLY_MAX_HP_PCT < 80 && APPLY_MAX_SP_PCT < 80);
#define POINT_TEST_APPLY_BLOCK(n) TestApplyInfo(n), TestApplyInfo(n+1), TestApplyInfo(n+2), TestApplyInfo(n+3), \
    TestApplyInfo(n+4), TestApplyInfo(n+5), TestApplyInfo(n+6), TestApplyInfo(n+7), \
    TestApplyInfo(n+8), TestApplyInfo(n+9), TestApplyInfo(n+10), TestApplyInfo(n+11), \
    TestApplyInfo(n+12), TestApplyInfo(n+13), TestApplyInfo(n+14), TestApplyInfo(n+15)
const TApplyInfo aApplyInfo[MAX_APPLY_NUM] {
    POINT_TEST_APPLY_BLOCK(0), POINT_TEST_APPLY_BLOCK(16), POINT_TEST_APPLY_BLOCK(32),
    POINT_TEST_APPLY_BLOCK(48), POINT_TEST_APPLY_BLOCK(64)
};
#undef POINT_TEST_APPLY_BLOCK
int test_server = 0, gPlayerMaxLevel = 150, g_iStatusPointGetLevelLimit = 115;
namespace P = ecs::PointSystem;
namespace {
int checks = 0, modifies = 0, packets = 0, supportLevel = 0, activeDeck = -1;
int64_t mountHP = 0;
std::function<void(entt::entity)> onItem, onAffect, onPacket;
struct Actor { bool player = true; uint8_t job = 0; uint32_t immune = 0; TMobTable mob {}; };
struct Gear { int flatHP = 300, pctHP = 20; bool rune = false, active = true; uint32_t immune = IMMUNE_STUN; TItemTable proto {}; };
std::map<std::pair<entt::entity, int>, entt::entity> equipment;
std::map<std::pair<entt::entity, int>, entt::entity> inventory;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected legacy/live service"); }
entt::entity ActorEntity(bool player = true) {
    const auto e = g_registry.create();
    auto& actor = g_registry.emplace<Actor>(e); actor.player = player;
    actor.mob.dwMaxHP = 3000; actor.mob.sAttackSpeed = 90; actor.mob.sMovingSpeed = 70; actor.mob.wDef = 7;
    g_registry.emplace<ecs::CharacterStatsComponent>(e);
    g_registry.emplace<ecs::CharacterPoints>(e, ecs::CharacterPoints {});
    g_registry.emplace<ecs::Health>(e, 500, 1000);
    g_registry.emplace<ecs::Mana>(e, 80, 200);
    g_registry.emplace<ecs::Stamina>(e, 100, 100);
    g_registry.emplace<ecs::LevelComponent>(e, 20);
    g_registry.emplace<ecs::SkillDamageBonus>(e);
    g_registry.emplace<ecs::CombatStats>(e, ecs::CombatStats {});
    for (const auto type : {POINT_ST, POINT_HT, POINT_DX, POINT_IQ}) P::SetReal(e, type, 10);
    P::Set(e, POINT_STAT, 17); P::Set(e, POINT_SKILL, 9); P::Set(e, POINT_PARTY_TANKER_BONUS, 50);
    P::SetRandomHP(e, 20); P::SetRandomSP(e, 30);
    return e;
}
entt::entity Wear(entt::entity owner, int slot = WEAR_BODY) {
    const auto item = g_registry.create();
    g_registry.emplace<ecs::ItemIdentity>(item).vnum = 100;
    g_registry.emplace<ecs::ItemOwner>(item).owner = owner;
    g_registry.emplace<Gear>(item); equipment[{owner, slot}] = item; return item;
}
void Reset() {
    g_registry.clear(); equipment.clear(); inventory.clear(); modifies = packets = supportLevel = 0; activeDeck = -1; mountHP = 0;
    onItem = onAffect = onPacket = {};
    for (auto& job : JobInitialPoints) {
        job = {}; job.max_hp = 1000; job.max_sp = 200; job.max_stamina = 100;
        job.hp_per_ht = 10; job.sp_per_iq = 5; job.stamina_per_con = 2;
    }
}
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() {
    static auto logger = std::make_shared<spdlog::logger>("points-error-test"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("points-test"); return logger;
}
CSkillManager::CSkillManager() = default;
CSkillManager::~CSkillManager() = default;
CSkillProto* CSkillManager::Get(uint32_t) { return nullptr; }
void CSkillProto::SetPointVar(std::string_view, double) { Unexpected(); }
double CPoly::Eval() { Unexpected(); }
CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
void CHARACTER_MANAGER::CheckBonusEvent(entt::entity) {}
void CPetSystem::RefreshBuff() { Unexpected(); }
DSManager::DSManager() = default;
DSManager::~DSManager() = default;
DragonSoulTable::~DragonSoulTable() = default;
bool DSManager::IsTimeLeftDragonSoul(entt::entity) const { return true; }

namespace ecs::PlayerRuntime {
bool IsValid(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<Actor>(e); }
bool IsPC(entt::entity e) { return IsValid(e) && g_registry.get<Actor>(e).player; }
uint8_t GetJob(entt::entity e) { return g_registry.get<Actor>(e).job; }
const TMobTable* GetMobTable(entt::entity e) { return IsPC(e) ? nullptr : &g_registry.get<Actor>(e).mob; }
LPDESC GetDesc(entt::entity e) { Check(IsValid(e), "stale descriptor lookup"); return nullptr; }
std::string_view GetName(entt::entity e) { Check(IsValid(e), "stale actor name"); return "actor"; }
uint32_t GetPlayerID(entt::entity) { return 1; }
uint32_t GetPacketVID(entt::entity) { return 2; }
void SetHP(entt::entity e, int64_t value) { g_registry.get<ecs::Health>(e).current = static_cast<int32_t>(value); }
void SetSP(entt::entity e, int64_t value) { g_registry.get<ecs::Mana>(e).current = static_cast<int32_t>(value); }
void SetMaxHP(entt::entity e, int64_t value) { g_registry.get<ecs::Health>(e).max = static_cast<int32_t>(value); }
void SetMaxSP(entt::entity e, int64_t value) { g_registry.get<ecs::Mana>(e).max = static_cast<int32_t>(value); }
void SetMaxStamina(entt::entity e, int64_t value) { g_registry.get<ecs::Stamina>(e).max = static_cast<int32_t>(value); }
int64_t GetMaxStamina(entt::entity e) { return g_registry.get<ecs::Stamina>(e).max; }
int GetStamina(entt::entity e) { return g_registry.get<ecs::Stamina>(e).current; }
void SetStamina(entt::entity e, int64_t value) { g_registry.get<ecs::Stamina>(e).current = static_cast<int32_t>(value); }
void SetPart(entt::entity e, uint8_t, uint16_t) { Check(IsValid(e), "stale appearance write"); }
uint16_t GetPart(entt::entity, uint8_t) { return 0; }
uint16_t GetOriginalPart(entt::entity, uint8_t) { return 0; }
void SetImmuneFlag(entt::entity e, uint32_t value) { g_registry.get<Actor>(e).immune = value; }
uint32_t GetImmuneFlag(entt::entity e) { return g_registry.get<Actor>(e).immune; }
void BuffOnAttr_ClearAll(entt::entity) {}
void BuffOnAttr_ValueChange(entt::entity, uint8_t, uint8_t, uint8_t) {}
CPetSystem* GetPetSystem(entt::entity) { return nullptr; }
}
namespace ItemSystem {
bool IsValidItem(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<Gear>(e); }
entt::entity GetItemOwner(entt::entity e) { return g_registry.get<ecs::ItemOwner>(e).owner; }
entt::entity GetWearItem(entt::entity e, uint8_t slot) {
    const auto it = equipment.find({e, slot}); return it == equipment.end() ? entt::null : it->second;
}
entt::entity GetInventoryItem(entt::entity e, uint16_t cell) {
    const auto it = inventory.find({e, cell}); return it == inventory.end() ? entt::null : it->second;
}
uint32_t GetItemVnum(entt::entity e) { return g_registry.get<ecs::ItemIdentity>(e).vnum; }
const TItemTable* GetItemProto(entt::entity e) { return &g_registry.get<Gear>(e).proto; }
bool IsRuneItem(entt::entity e) { return g_registry.get<Gear>(e).rune; }
uint32_t GetItemSocket(entt::entity e, int) { return g_registry.get<Gear>(e).active; }
uint32_t GetItemImmuneFlags(entt::entity e) { return g_registry.get<Gear>(e).immune; }
uint8_t GetItemType(entt::entity) { return ITEM_NONE; }
uint8_t GetItemSubType(entt::entity) { return 0; }
int32_t GetItemValue(entt::entity, uint32_t) { return 0; }
void ModifyPoints(entt::entity item, bool add) {
    Check(IsValidItem(item) && add, "invalid item calculation");
    const auto owner = GetItemOwner(item); const auto gear = g_registry.get<Gear>(item); ++modifies;
    P::ApplyPoint(owner, APPLY_MAX_HP, gear.flatHP); P::ApplyPoint(owner, APPLY_MAX_HP_PCT, gear.pctHP);
    P::ApplyPoint(owner, APPLY_SKILL, (42 << 24) | 0x00800000 | 3);
    if (onItem) onItem(owner);
}
}
namespace MountSystem {
void ComputeMountInventoryBonuses(entt::entity owner) { if (mountHP) P::Change(owner, POINT_MAX_HP, mountHP); }
uint32_t GetMountVnum(entt::entity) { return 0; }
int GetHorseLevel(entt::entity) { return 0; }
bool IsHorseRiding(entt::entity) { return false; }
int GetHorseArmor(entt::entity) { return 0; }
}
namespace SkillSystem {
int GetSkillPower(entt::entity, uint32_t, uint8_t) { return 0; }
int GetSkillLevel(entt::entity, uint32_t) { return supportLevel; }
uint8_t GetSkillGroup(entt::entity) { return 0; }
void ComputeSkillPoints(entt::entity) {}
}
namespace DragonSoulSystem {
int GetActiveDeck(entt::entity) { return activeDeck; }
bool IsDeckActivated(entt::entity) { return activeDeck >= 0; }
}
namespace AffectSystem {
bool IsPolymorphed(entt::entity) { return false; }
void RefreshAffect(entt::entity e) { if (onAffect) onAffect(e); }
}
namespace CombatSystem {
uint8_t GetAlignmentGrade(entt::entity e) {
    const auto alignment = g_registry.get<ecs::CombatStats>(e).realAlignment;
    if (alignment == 0) return 0;
    if (alignment == 50000) return 1;
    if (alignment == 50000000) return 20;
    Unexpected(); // Tier boundaries are tested against the real CombatSystem separately.
}
bool IsDead(entt::entity) { return false; }
bool IsStun(entt::entity) { return false; }
void BroadcastTargetPacket(entt::entity e) { Check(g_registry.valid(e), "stale target packet"); }
}
namespace ecs::SocialSystem {
LPPARTY GetParty(entt::entity) { return nullptr; }
CGuild* GetGuild(entt::entity) { return nullptr; }
}
void NetworkSyncSystem::UpdatePacket(entt::entity e) {
    Check(g_registry.valid(e), "stale final packet"); ++packets; if (onPacket) onPacket(e);
}

// The full point translation unit is linked, including unrelated quest,
// level-up and legacy wrappers. Fail immediately if a test enters those leaves.
int MAX(int a, int b) { return std::max(a, b); }
int MINMAX(int a, int b, int c) { return std::clamp(b, a, c); }
int number_ex(int, int, const char*, int) { Unexpected(); }
uint32_t get_dword_time() { return 1000; }
const CMob* CMobManager::Get(uint32_t) { Unexpected(); }
void ecs::ViewSystem::PacketView(entt::entity, const void*, int, entt::entity) { Unexpected(); }
void BroadcastNoticeNew(uint8_t, uint8_t, int, uint32_t, const char*, ...) { Unexpected(); }
void AffectSystem::SetPolymorph(entt::entity, uint32_t, bool) { Unexpected(); }
int AffectSystem::GetPolymorphPower(entt::entity) { Unexpected(); }
bool AffectSystem::IsPolyMaintainStat(entt::entity) { return false; }
uint32_t AffectSystem::GetPolymorphVnum(entt::entity) { return 0; }
void CHARACTER::Save() { Unexpected(); }
const char* CHARACTER::GetName(uint8_t) const { Unexpected(); }
int CHARACTER::GetLevel() const { Unexpected(); }
int64_t CHARACTER::GetHP() const { Unexpected(); }
int64_t CHARACTER::GetSP() const { Unexpected(); }
int64_t CHARACTER::GetMaxHP() const { Unexpected(); }
int64_t CHARACTER::GetMaxSP() const { Unexpected(); }
void CHARACTER::CalculateMoveDuration() { Unexpected(); }
void CHARACTER::MountVnum(uint32_t) { Unexpected(); }
int ecs::QuestSystem::GetFlag(entt::entity, std::string_view) { Unexpected(); }
void ecs::QuestSystem::SetFlag(entt::entity, std::string_view, int) { Unexpected(); }
void ecs::PlayerRuntime::SetExp(entt::entity, uint32_t) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetExp(entt::entity) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetNextExp(entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::SetGold(entt::entity, int64_t) { Unexpected(); }
void ecs::PlayerRuntime::SetLevel(entt::entity, uint8_t) { Unexpected(); }
bool ecs::PlayerRuntime::SetRankPoints(entt::entity, int, int64_t) { return true; }
void SkillSystem::SendSkillLevelPacket(entt::entity) { Unexpected(); }
void NetworkSyncSystem::PointsPacket(entt::entity) { Unexpected(); }
void CGuild::LevelChange(uint32_t, uint8_t) { Unexpected(); }
void CParty::SendPartyInfoOneToAll(entt::entity) { Unexpected(); }
void CParty::RequestSetMemberLevel(uint32_t, uint8_t) { Unexpected(); }
void ecs::MovementSystem::SetNowWalking(entt::entity, bool) { Unexpected(); }
void DESC::Packet(const void*, int) { Unexpected(); }
void quest::CQuestManager::LevelUp(uint32_t) { Unexpected(); }
void LogManager::CharLog(entt::entity, uint32_t, const char*, const char*) { Unexpected(); }
void LogManager::LevelLog(entt::entity, uint32_t, uint32_t) { Unexpected(); }
const char* CHorseNameManager::GetHorseName(uint32_t) { Unexpected(); }

namespace {
void FormulaChecks() {
    Reset(); const auto e = ActorEntity();
    P::SetReal(e, POINT_MAX_HP, 1000); P::Set(e, POINT_MAX_HP, 300);
    P::Set(e, POINT_MAX_HP_PCT, 20);
    P::Change(e, POINT_MAX_HP, 0);
    Check(P::GetMaxHP(e) == 1620 && P::GetReal(e, POINT_MAX_HP) == 1000, "HP base/modifier/total mixed");
    for (int i = 0; i < 100; ++i) {
        P::Change(e, POINT_MAX_HP, 0); Check(P::GetMaxHP(e) == 1620, "HP grew on zero change");
    }
    P::SetReal(e, POINT_MAX_SP, 1000); P::Set(e, POINT_MAX_SP, 300); P::Set(e, POINT_MAX_SP_PCT, 200);
    P::Change(e, POINT_MAX_SP, 0);
    Check(P::GetMaxSP(e) == 2100 && P::GetReal(e, POINT_MAX_SP) == 1000, "SP cap/base/flat mixed");
}
void RepeatedCalculation() {
    Reset(); const auto e = ActorEntity(); const auto item = Wear(e);
    P::SetInventoryExpansion(e, 12);
    P::Set(e, POINT_STEAL_HP, 99); // Removed equipment must not leave this behind.
    for (int i = 0; i < 100; ++i) {
        P::Compute(e);
        Check(P::GetMaxHP(e) == 2364, "recompute changed HP or stacked item/alignment");
        Check(P::Get(e, POINT_HP) == 500 && P::Get(e, POINT_SP) == 80, "recompute healed/damaged character");
        Check(P::Get(e, POINT_STAT) == 17 && P::Get(e, POINT_SKILL) == 9 && P::GetInventoryExpansion(e) == 12, "persistent points reset");
        Check(P::Get(e, POINT_STEAL_HP) == 0 && g_registry.get<ecs::SkillDamageBonus>(e).bySkill.at(42) == 3, "stale/stacked bonus");
        Check(ecs::PlayerRuntime::GetImmuneFlag(e) == IMMUNE_STUN, "item immunity missing");
        Check(P::Get(e, POINT_ATT_GRADE) == 60, "entity-only player took NPC battle formula");
    }
    equipment.clear(); g_registry.destroy(item); P::Compute(e);
    Check(P::GetMaxHP(e) == 1670 && g_registry.get<ecs::SkillDamageBonus>(e).bySkill.empty() &&
        ecs::PlayerRuntime::GetImmuneFlag(e) == 0, "removed item retained bonuses");
}
void SourceChecks() {
    Reset(); auto e = ActorEntity(false);
    g_registry.get<Actor>(e).mob.dwImmuneFlag = IMMUNE_POISON;
    P::Set(e, POINT_PARTY_TANKER_BONUS, 0);
    for (int i = 0; i < 10; ++i) {
        P::Compute(e);
        Check(P::GetMaxHP(e) == 3000 && P::GetReal(e, POINT_MAX_HP) == 3000, "NPC HP grew");
        Check(P::GetMaxSP(e) == 0 && P::Get(e, POINT_SP) == 0 && P::Get(e, POINT_HP) == 500, "NPC vitals wrong");
        Check(P::Get(e, POINT_DEF_GRADE) == 37 && P::Get(e, POINT_ATT_SPEED) == 90 &&
            P::Get(e, POINT_MOV_SPEED) == 70, "NPC prototype/formula lost");
        Check(ecs::PlayerRuntime::GetImmuneFlag(e) == IMMUNE_POISON, "NPC immunity lost");
    }
    Reset(); e = ActorEntity(); const auto rune = Wear(e);
    g_registry.get<Gear>(rune).rune = true; g_registry.get<Gear>(rune).active = false;
    P::Compute(e); Check(modifies == 0 && P::GetMaxHP(e) == 1670, "inactive rune applied");
    g_registry.get<Gear>(rune).active = true;
    equipment[{e, WEAR_HEAD}] = rune; // Corrupt duplicate slot: never count twice.
    P::Compute(e); Check(modifies == 1 && P::GetMaxHP(e) == 2364, "rune duplicated");
    g_registry.get<ecs::ItemOwner>(rune).owner = ActorEntity();
    P::Compute(e); Check(modifies == 1 && P::GetMaxHP(e) == 1670, "foreign equipment applied");

    Reset(); e = ActorEntity(); activeDeck = 0; Wear(e, WEAR_MAX_NUM);
    P::Compute(e); Check(modifies == 1 && P::GetMaxHP(e) == 2364, "active dragon soul missing");
    activeDeck = DRAGON_SOUL_DECK_MAX_NUM;
    P::Compute(e); Check(modifies == 1 && P::GetMaxHP(e) == 1670, "invalid dragon soul deck applied");

    Reset(); e = ActorEntity(); const auto belt = Wear(e); equipment.clear(); mountHP = 100;
    g_registry.get<ecs::ItemIdentity>(belt).vnum = 18000;
    auto& proto = g_registry.get<Gear>(belt).proto;
    proto.aApplies[0] = {APPLY_MAX_HP, 200}; proto.aApplies[1] = {APPLY_MAX_HP_PCT, 10};
    proto.aApplies[2] = {255, 9000}; // Malformed prototype type must not index the apply table.
    inventory[{e, BELT_INVENTORY_SLOT_START}] = belt;
    inventory[{e, BELT_INVENTORY_SLOT_START + 1}] = belt;
    for (int i = 0; i < 10; ++i) {
        P::Compute(e); Check(P::GetMaxHP(e) == 2167 && modifies == 0, "belt/mount totals grew or duplicated");
    }
    g_registry.get<ecs::ItemIdentity>(belt).vnum = 18160;
    P::Compute(e); Check(P::GetMaxHP(e) == 1770, "out-of-whitelist belt applied");

    Reset(); e = ActorEntity(); supportLevel = 255;
    P::Compute(e); Check(P::GetMaxHP(e) == 4670 && P::Get(e, POINT_MALL_ATTBONUS) == 10, "support skill upper bound");
    supportLevel = -1; P::Compute(e);
    Check(P::GetMaxHP(e) == 1670 && P::Get(e, POINT_MALL_ATTBONUS) == 0, "support skill lower bound");
    g_registry.get<ecs::CombatStats>(e).realAlignment = 50000000;
    P::Compute(e); Check(P::GetMaxHP(e) == 61170 && P::Get(e, POINT_ATTBONUS_MONSTER) == 85, "top alignment tier");
    g_registry.get<ecs::CombatStats>(e).realAlignment = 50000;
    P::Compute(e); Check(P::GetMaxHP(e) == 2170 && P::Get(e, POINT_ATTBONUS_MONSTER) == 3, "alignment downgrade stacked");

    Reset(); e = ActorEntity(); g_registry.get<Actor>(e).job = JOB_MAX_NUM;
    P::Compute(e); Check(packets == 0 && P::GetMaxHP(e) == 1000, "invalid job mutated points");
    g_registry.remove<ecs::Health>(e); P::Compute(e);
    Check(packets == 0, "incomplete archetype published");
    g_registry.destroy(e); P::Compute(e); P::Compute(entt::null);
    Check(packets == 0, "stale/null entity published");
}
void CallbackChecks() {
    for (int stage = 0; stage < 3; ++stage) {
        Reset(); const auto e = ActorEntity(); Wear(e);
        const auto nested = [&](entt::entity current) {
            Check(g_registry.get<ecs::CharacterStatsComponent>(current).recomputing, "callback outside guard");
            P::Compute(current);
        };
        if (stage == 0) onItem = nested; if (stage == 1) onAffect = nested; if (stage == 2) onPacket = nested;
        P::Compute(e);
        Check(modifies == 1 && packets == 1 && P::GetMaxHP(e) == 2364 &&
            !g_registry.get<ecs::CharacterStatsComponent>(e).recomputing, "reentrant calculation stacked/kept guard");
    }
    Reset(); const auto e = ActorEntity(); Wear(e);
    onItem = [](entt::entity) { throw std::runtime_error("injected calculation failure"); };
    bool caught = false; try { P::Compute(e); } catch (const std::runtime_error&) { caught = true; }
    Check(caught && !g_registry.get<ecs::CharacterStatsComponent>(e).recomputing, "exception stranded guard");
    onItem = {}; P::Compute(e); Check(P::GetMaxHP(e) == 2364, "retry stacked partial calculation");
    Reset(); const auto old = ActorEntity(); Wear(old); entt::entity replacement = entt::null;
    onItem = [&](entt::entity current) { g_registry.destroy(current); replacement = ActorEntity(); };
    P::Compute(old);
    Check(replacement != old && g_registry.valid(replacement) && P::GetMaxHP(replacement) == 1000 && packets == 0, "recycled owner was mutated");
}
}
int main() {
    try {
        CSkillManager skills; CHARACTER_MANAGER characters; DSManager dragonSouls;
        FormulaChecks(); RepeatedCalculation(); SourceChecks(); CallbackChecks();
        std::cout << "Point calculation checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
