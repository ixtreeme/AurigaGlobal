#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/combat_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/dirty_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/movement_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/vital_components.hpp"
#include "../../SRC/Server/GameServer/ecs/CharacterAccessors.hpp"
#include "../../SRC/Server/GameServer/ecs/AIHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/SpatialHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/events.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/EntityFactory.hpp"
#include "../../SRC/Server/GameServer/ecs/EntityInvariants.hpp"
#include "../../SRC/Server/GameServer/ecs/NetworkService.hpp"
#include "../../SRC/Server/GameServer/ecs/VIDRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/utils.h"
#include "../../SRC/Server/GameServer/constants.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/desc_manager.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/item.h"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/mob_manager.h"
#include "../../SRC/Server/GameServer/battle.h"
#include "../../SRC/Server/GameServer/pvp.h"
#include "../../SRC/Server/GameServer/skill.h"
#include "../../SRC/Server/GameServer/start_position.h"
#include "../../SRC/Server/GameServer/profiler.h"
#include "../../SRC/Server/GameServer/cmd.h"
#include "../../SRC/Server/GameServer/dungeon.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/unique_item.h"
#include "../../SRC/Server/GameServer/priv_manager.h"
#include "../../SRC/Server/GameServer/db.h"
#include "../../SRC/Server/GameServer/vector.h"
#include "../../SRC/Server/GameServer/marriage.h"
#include "../../SRC/Server/GameServer/arena.h"
#include "../../SRC/Server/GameServer/regen.h"
#include "../../SRC/Server/GameServer/exchange.h"
#include "../../SRC/Server/GameServer/shop_manager.h"
#include "../../SRC/Server/GameServer/dev_log.h"
#include "../../SRC/Server/GameServer/ani.h"
#include "../../SRC/Server/GameServer/BattleArena.h"
#include "../../SRC/Server/GameServer/packet.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/affect.h"
#include "../../SRC/Server/GameServer/guild.h"
#include "../../SRC/Server/GameServer/guild_manager.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/questlua.h"
#include "../../SRC/Server/GameServer/New_PetSystem.h"
#include "../../SRC/Server/GameServer/battle_pass.h"
#include "../../SRC/Server/GameServer/OrcsDungeon.h"
#include "../../SRC/Server/GameServer/TritonTempleDungeon.h"
#include "../../SRC/Server/GameServer/ValentineDungeon.h"
#include "../../SRC/Server/GameServer/RuneDungeon.h"
#include "../../SRC/Server/GameServer/PyramidDungeonRazor93.h"
#include "../../SRC/Server/GameServer/NightmareDungeonRazor93.h"
#include "../../SRC/Server/GameServer/Halloween2022Dungeon.h"
#include "../../SRC/Server/GameServer/VikingDungeon.h"
#include "../../SRC/Server/GameServer/EasterDungeon.h"

entt::registry g_registry;
entt::dispatcher g_dispatcher;
const TMobRankStat MobRankStats[MOB_RANK_MAX_NUM] {};
const uint32_t party_exp_distribute_table[PLAYER_EXP_TABLE_MAX + 1] {};
const int* aiPercentByDeltaLev = nullptr;
const int* aiPercentByDeltaLevForBoss = nullptr;
const int aiExpLossPercents[PLAYER_EXP_TABLE_MAX + 1] {};
const SStoneDropInfo aStoneDrop[STONE_INFO_MAX_NUM] {};
int test_server = 0, g_bItemCountLimit = 0;
uint8_t g_bChannel = 1;
bool g_bSkillDisable = false, g_NoDropMetinStone = false;
int passes_per_sec = 25;
namespace C = CombatSystem;
namespace {
int checks = 0, computes = 0, packets = 0, alignmentPackets = 0;
uint32_t tick = 1000;
bool guild = false;
std::function<void(entt::entity)> onCompute, onPacket, onAlignment;
void Check(bool condition, const char* why) { ++checks; if (!condition) throw std::runtime_error(why); }
entt::entity Actor() {
    auto e = g_registry.create();
    g_registry.emplace<ecs::TagPC>(e);
    g_registry.emplace<ecs::CombatStats>(e);
    return e;
}
void Reset() {
    g_registry.clear(); computes = packets = alignmentPackets = 0; tick = 1000; guild = false;
    onCompute = onPacket = onAlignment = {}; passes_per_sec = 25;
}
void AssertActor(entt::entity e) {
    Check(g_registry.valid(e) && g_registry.any_of<ecs::TagPC, ecs::TagNPC, ecs::TagMonster, ecs::TagStone>(e), "stale/non-character service call");
}
}
int thecore_pulse() { return static_cast<int>(tick); }
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("combat-state"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }
namespace ecs::PlayerRuntime {
std::string_view GetName(entt::entity e) { AssertActor(e); return "entity-only"; }
uint32_t GetPlayerID(entt::entity e) { AssertActor(e); return 7; }
}
namespace ecs::SocialSystem {
CGuild* GetGuild(entt::entity e) {
    AssertActor(e);
    // Opaque presence token only; the tested path must never dereference it.
    return guild ? reinterpret_cast<CGuild*>(&guild) : nullptr;
}
}
void ecs::PointSystem::Compute(entt::entity e) {
    AssertActor(e); ++computes;
    const auto callback = onCompute; if (callback) callback(e);
}
void NetworkSyncSystem::UpdatePacket(entt::entity e) {
    AssertActor(e); ++packets;
    const auto callback = onPacket; if (callback) callback(e);
}
void NetworkSyncSystem::BroadcastCharAdditionalInfo(entt::registry& reg, entt::entity e) {
    Check(&reg == &g_registry, "wrong registry"); AssertActor(e); ++alignmentPackets;
    const auto callback = onAlignment; if (callback) callback(e);
}

namespace {
void AlignmentChecks() {
    Reset(); const auto e = Actor();
    static constexpr uint32_t ceilings[] {4999,14999,19999,29999,49999,74999,99999,124999,174999,249999,
        499999,749999,999999,1499999,2499999,2999999,3499999,3999999,4499999,4999999};
    for (uint8_t grade = 0; grade < std::size(ceilings); ++grade) {
        const uint32_t last = ceilings[grade] * 10 + 9;
        C::UpdateAlignment(e, static_cast<int64_t>(last) - C::GetRealAlignment(e));
        Check(C::GetAlignmentGrade(e) == grade, "tier inclusive upper boundary");
        C::UpdateAlignment(e, 1);
        Check(C::GetAlignmentGrade(e) == grade + 1, "tier next boundary");
    }
    Check(C::GetRealAlignment(e) == C::MAX_ALIGNMENT, "top tier maximum");
    C::UpdateAlignment(e, std::numeric_limits<int64_t>::max());
    Check(C::GetRealAlignment(e) == C::MAX_ALIGNMENT, "positive delta overflow");
    C::UpdateAlignment(e, std::numeric_limits<int64_t>::min());
    Check(C::GetRealAlignment(e) == 0 && C::GetAlignment(e) == 0, "negative delta wrapped to maximum");
    for (const int64_t start : {0, 1, 49999, 50000, 49999999, 50000000}) {
        for (const int64_t delta : {-50000001, -50000, -1, 0, 1, 50000, 50000001}) {
            C::UpdateAlignment(e, start - C::GetRealAlignment(e));
            C::UpdateAlignment(e, delta);
            Check(C::GetRealAlignment(e) == std::clamp<int64_t>(start + delta, 0, C::MAX_ALIGNMENT), "signed delta/clamp mismatch");
            Check(C::GetRealAlignment(e) == C::GetAlignment(e), "alignment mirror drift");
        }
    }
    Reset(); const auto fine = Actor();
    C::UpdateAlignment(fine, 1);
    Check(computes == 0 && alignmentPackets == 0, "sub-display change published");
    C::UpdateAlignment(fine, 9);
    Check(computes == 0 && alignmentPackets == 1, "same-grade change recomputed");
    C::UpdateAlignment(fine, 0);
    Check(alignmentPackets == 1, "no-op update published");
    C::UpdateAlignment(fine, 49990);
    Check(computes == 1 && alignmentPackets == 2, "grade change did not compute/publish");
    g_registry.get<ecs::CombatStats>(fine).realAlignment = UINT32_MAX;
    C::UpdateAlignment(fine, 0);
    Check(C::GetRealAlignment(fine) == C::MAX_ALIGNMENT, "corrupt stored alignment not normalized");
}
void CallbackChecks() {
    Reset(); const auto e = Actor();
    onCompute = [&](entt::entity current) {
        Check(C::GetRealAlignment(current) == 50000, "callback saw uncommitted alignment");
        onCompute = {}; C::UpdateAlignment(current, -50000);
    };
    C::UpdateAlignment(e, 50000);
    Check(C::GetRealAlignment(e) == 0 && computes == 2 && alignmentPackets == 1, "obsolete outer publication");
    Reset(); const auto aba = Actor();
    onCompute = [&](entt::entity current) {
        onCompute = {}; C::UpdateAlignment(current, -50000); C::UpdateAlignment(current, 50000);
    };
    C::UpdateAlignment(aba, 50000);
    Check(C::GetRealAlignment(aba) == 50000 && computes == 3 && alignmentPackets == 2, "ABA update republished");
    Reset(); const auto old = Actor(); entt::entity replacement = entt::null;
    onCompute = [&](entt::entity current) { g_registry.destroy(current); replacement = Actor(); };
    C::UpdateAlignment(old, 50000);
    Check(replacement != old && C::GetRealAlignment(replacement) == 0 && alignmentPackets == 0, "recycled alignment owner touched");
    for (bool killer : {false, true}) {
        Reset(); const auto actor = Actor(); replacement = entt::null;
        onPacket = [&](entt::entity current) { g_registry.destroy(current); replacement = Actor(); };
        if (killer) C::SetKillerMode(actor, true); else C::SetPKMode(actor, PK_MODE_FREE);
        Check(replacement != entt::null && replacement != actor && !C::IsKillerMode(replacement) &&
            C::GetPKMode(replacement) == PK_MODE_PEACE, "packet callback mutated replacement");
    }
}
void ModeChecks() {
    for (const uint32_t start : {1000u, static_cast<uint32_t>(INT_MAX) - 100u, UINT32_MAX - 100u}) {
        Reset(); const auto e = Actor(); tick = start;
        C::SetKillerMode(e, true);
        Check(C::IsKillerMode(e) && packets == 1, "killer mode not enabled");
        tick += 100; C::SetKillerMode(e, true);
        Check(packets == 1 && g_registry.get<ecs::CombatStats>(e).killerModePulse == start, "same state extended timer");
        tick = start + 749; C::UpdateKillerMode(e); Check(C::IsKillerMode(e), "early killer expiry");
        tick = start + 750; C::UpdateKillerMode(e);
        Check(!C::IsKillerMode(e) && packets == 2, "killer timeout/wrap failed");
        C::UpdateKillerMode(e); Check(packets == 2, "inactive killer republished");
    }
    Reset(); const auto e = Actor();
    C::SetPKMode(e, PK_MODE_MAX_NUM); Check(packets == 0, "invalid PK mode published");
    C::SetPKMode(e, PK_MODE_GUILD);
    Check(C::GetPKMode(e) == PK_MODE_FREE && packets == 1, "guild-less mode not normalized");
    C::SetPKMode(e, PK_MODE_GUILD); Check(packets == 1, "normalized no-op published");
    guild = true; C::SetPKMode(e, PK_MODE_GUILD);
    Check(C::GetPKMode(e) == PK_MODE_GUILD && packets == 2, "guild mode rejected");
    guild = false; C::SetPKMode(e, PK_MODE_GUILD);
    Check(C::GetPKMode(e) == PK_MODE_FREE && packets == 3, "guild departure skipped normalization");
}
void MultiplierAndValidityChecks() {
    for (int kind = 0; kind < 3; ++kind) {
        Reset(); const auto creature = Actor(); g_registry.remove<ecs::TagPC>(creature);
        if (kind == 0) g_registry.emplace<ecs::TagNPC>(creature);
        if (kind == 1) g_registry.emplace<ecs::TagMonster>(creature);
        if (kind == 2) g_registry.emplace<ecs::TagStone>(creature);
        g_registry.remove<ecs::CombatStats>(creature);
        Check(C::GetAttackMultiplier(creature) == 1 && C::GetDamageMultiplier(creature) == 1, "missing component defaults");
        C::SetAttackMultiplier(creature, 3); C::SetDamageMultiplier(creature, 0.25f);
        C::UpdateAlignment(creature, 50000); C::SetKillerMode(creature, true);
        Check(C::GetAttackMultiplier(creature) == 3 && C::GetDamageMultiplier(creature) == 0.25f &&
            C::GetRealAlignment(creature) == 50000 && C::IsKillerMode(creature), "non-PC state migration failed");
    }
    Reset(); const auto e = Actor();
    Check(C::GetAttackMultiplier(e) == 1 && C::GetDamageMultiplier(e) == 1, "default multipliers");
    C::SetAttackMultiplier(e, 2.5f); C::SetDamageMultiplier(e, 0.5f);
    Check(C::GetAttackMultiplier(e) == 2.5f && C::GetDamageMultiplier(e) == 0.5f, "entity-only multiplier write");
    for (const float invalid : {-1.f, std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        C::SetAttackMultiplier(e, invalid); C::SetDamageMultiplier(e, invalid);
        Check(C::GetAttackMultiplier(e) == 2.5f && C::GetDamageMultiplier(e) == 0.5f, "invalid multiplier accepted");
    }
    C::SetAttackMultiplier(e, 0); C::SetDamageMultiplier(e, 0);
    Check(C::GetAttackMultiplier(e) == 0 && C::GetDamageMultiplier(e) == 0, "zero multiplier rejected");
    g_registry.destroy(e); const auto replacement = Actor(); const auto nonCharacter = g_registry.create();
    for (const auto invalid : {e, entt::entity(entt::null), nonCharacter}) {
        C::UpdateAlignment(invalid, 123); C::SetPKMode(invalid, PK_MODE_FREE);
        C::SetKillerMode(invalid, true); C::UpdateKillerMode(invalid);
        C::SetAttackMultiplier(invalid, 9); C::SetDamageMultiplier(invalid, 9);
        Check(C::GetRealAlignment(invalid) == 0 && C::GetAlignment(invalid) == 0 &&
            C::GetPKMode(invalid) == PK_MODE_PROTECT && !C::IsKillerMode(invalid) &&
            C::GetAttackMultiplier(invalid) == 1 && C::GetDamageMultiplier(invalid) == 1, "invalid owner state");
    }
    Check(!g_registry.any_of<ecs::CombatStats, ecs::StatusFlags>(nonCharacter) &&
        C::GetAttackMultiplier(replacement) == 1 && C::GetRealAlignment(replacement) == 0 && packets == 0, "invalid write attached state");
}
}
// Unrelated leaves from the complete combat translation unit are fail-fast
// doubles. No combat, loot, DB, quest, CHARACTER or item allocation is allowed.
[[noreturn]] void UnexpectedService(const char* service) { throw std::runtime_error(service); }
int MAX(int,int) { UnexpectedService(__func__); }
int MIN(int,int) { UnexpectedService(__func__); }
int MINMAX(int,int,int) { UnexpectedService(__func__); }
int number_ex(int,int,char const *,int) { UnexpectedService(__func__); }
unsigned int get_dword_time(void) { UnexpectedService(__func__); }
void intrusive_ptr_add_ref(event *) { UnexpectedService(__func__); }
void intrusive_ptr_release(event *) { UnexpectedService(__func__); }
boost::intrusive_ptr<event> event_create_ex(int (*)(boost::intrusive_ptr<event>,int),event_info_data *,int) { UnexpectedService(__func__); }
void ecs::ChatSystem::Send(entt::entity,unsigned char,char const *,...) { UnexpectedService(__func__); }
void ecs::ChatSystem::SendNew(entt::entity,unsigned char,unsigned int,char const *,...) { UnexpectedService(__func__); }
void ecs::ViewSystem::ViewReencode(entt::entity) { UnexpectedService(__func__); }
void ecs::ViewSystem::PacketView(entt::entity,void const *,int,entt::entity) { UnexpectedService(__func__); }
DESC * ecs::PlayerRuntime::GetDesc(entt::entity) { UnexpectedService(__func__); }
unsigned char ecs::PlayerRuntime::GetEmpire(entt::entity) { UnexpectedService(__func__); }
unsigned int ecs::PlayerRuntime::GetPacketVID(entt::entity) { UnexpectedService(__func__); }
unsigned int ecs::PlayerRuntime::GetRaceNum(entt::entity) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::IsRaceFlag(entt::entity,unsigned int) { UnexpectedService(__func__); }
int64_t ecs::PlayerRuntime::GetHP(entt::entity) { UnexpectedService(__func__); }
int ecs::PlayerRuntime::GetHPPct(entt::entity) { UnexpectedService(__func__); }
unsigned int ecs::PlayerRuntime::GetAIFlag(entt::entity) { UnexpectedService(__func__); }
unsigned char ecs::PlayerRuntime::GetJob(entt::entity) { UnexpectedService(__func__); }
int ecs::PlayerRuntime::GetMapIndex(entt::entity) { UnexpectedService(__func__); }
int ecs::PlayerRuntime::GetX(entt::entity) { UnexpectedService(__func__); }
int ecs::PlayerRuntime::GetY(entt::entity) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::IsValid(entt::entity) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::IsPC(entt::entity) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::IsNPC(entt::entity) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::IsGuardNPC(entt::entity) { UnexpectedService(__func__); }
boost::intrusive_ptr<event> ecs::PlayerRuntime::GetCharEvent(entt::entity,ecs::PlayerRuntime::CharEvent) { UnexpectedService(__func__); }
void ecs::PlayerRuntime::SetCharEvent(entt::entity,ecs::PlayerRuntime::CharEvent,boost::intrusive_ptr<event>) { UnexpectedService(__func__); }
void ecs::PlayerRuntime::CancelCharEvent(entt::entity,ecs::PlayerRuntime::CharEvent) { UnexpectedService(__func__); }
void ecs::PlayerRuntime::SetPosition(entt::entity,int) { UnexpectedService(__func__); }
void ecs::PlayerRuntime::StartRecoveryEvent(entt::entity) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::IsStone(entt::entity) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::IsMonster(entt::entity) { UnexpectedService(__func__); }
unsigned char ecs::PlayerRuntime::GetMobRank(entt::entity) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::SetQuestNPCID(entt::entity,unsigned int) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::UpdateMissionProgress(entt::entity,unsigned int,unsigned int,unsigned int,unsigned int,bool) { UnexpectedService(__func__); }
int64_t ecs::PlayerRuntime::GetRankPoints(entt::entity,int) { UnexpectedService(__func__); }
bool ecs::PlayerRuntime::SetRankPoints(entt::entity,int,int64_t) { UnexpectedService(__func__); }
void SendAffectAddPacket(DESC *,CAffect *) { UnexpectedService(__func__); }
bool AffectSystem::IsImmune(entt::entity,unsigned int) { UnexpectedService(__func__); }
CAffect * AffectSystem::FindAffect(entt::entity,unsigned int,unsigned char) { UnexpectedService(__func__); }
bool AffectSystem::IsAffectFlag(entt::entity,unsigned int) { UnexpectedService(__func__); }
bool AffectSystem::AddAffect(entt::entity,unsigned int,unsigned char,int,unsigned int,int,int,bool,bool) { UnexpectedService(__func__); }
bool AffectSystem::RemoveAffect(entt::entity,unsigned int) { UnexpectedService(__func__); }
bool AffectSystem::IsPolymorphed(entt::entity) { UnexpectedService(__func__); }
int64_t ecs::PointSystem::Get(entt::entity,unsigned char) { UnexpectedService(__func__); }
int ecs::PointSystem::GetMaxHP(entt::entity) { UnexpectedService(__func__); }
int ecs::PointSystem::GetMaxSP(entt::entity) { UnexpectedService(__func__); }
int ecs::PointSystem::GetLevel(entt::entity) { UnexpectedService(__func__); }
void ecs::PointSystem::Change(entt::entity,unsigned char,int64_t,bool,bool,bool) { UnexpectedService(__func__); }
CParty * ecs::SocialSystem::GetParty(entt::entity) { UnexpectedService(__func__); }
int ecs::QuestSystem::GetFlag(entt::entity,std::string_view) { UnexpectedService(__func__); }
void ecs::QuestSystem::SetFlag(entt::entity,std::string_view,int) { UnexpectedService(__func__); }
bool NetworkSyncSystem::SetSyncOwner(entt::entity,entt::entity,bool) { UnexpectedService(__func__); }
void NetworkSyncSystem::ClearSync(entt::entity) { UnexpectedService(__func__); }
void NetworkSyncSystem::BroadcastSyncPacket(entt::registry &,entt::entity) { UnexpectedService(__func__); }
void NetworkSyncSystem::BroadcastEffect(entt::registry &,entt::entity,unsigned char) { UnexpectedService(__func__); }
double CPoly::Eval(void) { UnexpectedService(__func__); }
void CSkillProto::SetPointVar(std::string_view,double) { UnexpectedService(__func__); }
CSkillProto * CSkillManager::Get(unsigned int) { UnexpectedService(__func__); }
bool CGuild::UnderWar(unsigned int) { UnexpectedService(__func__); }
unsigned int CGuild::UnderAnyWar(unsigned char) { UnexpectedService(__func__); }
bool CEntity::IsType(int)const { UnexpectedService(__func__); }
int CEntity::GetX(void)const { UnexpectedService(__func__); }
int CEntity::GetY(void)const { UnexpectedService(__func__); }
int CEntity::GetZ(void)const { UnexpectedService(__func__); }
pixel_position_s CEntity::GetXYZ(void)const { UnexpectedService(__func__); }
SECTREE * CEntity::GetSectree(void)const { UnexpectedService(__func__); }
void BroadcastNotice(char const *,bool) { UnexpectedService(__func__); }
void Cube_close(CHARACTER *) { UnexpectedService(__func__); }
bool AttrTransfer_is_open(entt::entity) { UnexpectedService(__func__); }
void AttrTransfer_close(entt::entity) { UnexpectedService(__func__); }
unsigned short CHARACTER::GetRaceNum(void)const { UnexpectedService(__func__); }
char const * CHARACTER::GetName(unsigned char)const { UnexpectedService(__func__); }
unsigned int CHARACTER::GetPacketVID(void)const { UnexpectedService(__func__); }
unsigned char CHARACTER::GetJob(void)const { UnexpectedService(__func__); }
int CHARACTER::GetLevel(void)const { UnexpectedService(__func__); }
unsigned char CHARACTER::GetGMLevel(void)const { UnexpectedService(__func__); }
unsigned int CHARACTER::GetExp(void)const { UnexpectedService(__func__); }
void CHARACTER::SetExp(unsigned int) { UnexpectedService(__func__); }
unsigned int CHARACTER::GetNextExp(void)const { UnexpectedService(__func__); }
void CHARACTER::SetPosition(int) { UnexpectedService(__func__); }
int CHARACTER::GetPosition(void)const { UnexpectedService(__func__); }
void CHARACTER::SetHP(int64_t) { UnexpectedService(__func__); }
int64_t CHARACTER::GetHP(void)const { UnexpectedService(__func__); }
int64_t CHARACTER::GetSP(void)const { UnexpectedService(__func__); }
int64_t CHARACTER::GetMaxHP(void)const { UnexpectedService(__func__); }
void CHARACTER::SetMaxSP(int64_t) { UnexpectedService(__func__); }
int64_t CHARACTER::GetMaxSP(void)const { UnexpectedService(__func__); }
void CHARACTER::SetPoint(unsigned char,int64_t) { UnexpectedService(__func__); }
int64_t CHARACTER::GetPoint(unsigned char)const { UnexpectedService(__func__); }
int CHARACTER::GetLimitPoint(unsigned char)const { UnexpectedService(__func__); }
SMobTable const & CHARACTER::GetMobTable(void)const { UnexpectedService(__func__); }
unsigned char CHARACTER::GetMobRank(void)const { UnexpectedService(__func__); }
unsigned char CHARACTER::GetMobBattleType(void)const { UnexpectedService(__func__); }
unsigned short CHARACTER::GetMobAttackRange(void)const { UnexpectedService(__func__); }
bool CHARACTER::HasReviverInParty(void)const { UnexpectedService(__func__); }
unsigned int CHARACTER::GetMonsterDrainSPPoint(void)const { UnexpectedService(__func__); }
void CHARACTER::PointChange(unsigned char,int64_t,bool,bool,bool) { UnexpectedService(__func__); }
void CHARACTER::SetRotation(float,bool) { UnexpectedService(__func__); }
void CHARACTER::SetRotationToXY(int,int) { UnexpectedService(__func__); }
float CHARACTER::GetRotation(void)const { UnexpectedService(__func__); }
int CHARACTER::SetInvincible(bool) { UnexpectedService(__func__); }
bool CHARACTER::GetInvincible(void) { UnexpectedService(__func__); }
int CHARACTER::IncreaseMobRigHP(int) { UnexpectedService(__func__); }
bool CHARACTER::Goto(int,int) { UnexpectedService(__func__); }
bool CHARACTER::CanMove(void)const { UnexpectedService(__func__); }
bool CHARACTER::Sync(int,int) { UnexpectedService(__func__); }
void CHARACTER::OnMove(bool) { UnexpectedService(__func__); }
float CHARACTER::GetMoveSpeed(void)const { UnexpectedService(__func__); }
void CHARACTER::CalculateMoveDuration(void) { UnexpectedService(__func__); }
void CHARACTER::SendMovePacket(unsigned char,unsigned char,unsigned int,unsigned int,unsigned int,unsigned int,float) { UnexpectedService(__func__); }
void CHARACTER::SyncQuickslot(unsigned char,unsigned char,unsigned char) { UnexpectedService(__func__); }
void CHARACTER::ClearAffect(bool) { UnexpectedService(__func__); }
bool CHARACTER::AddAffect(unsigned int,unsigned char,int,unsigned int,int,int,bool,bool) { UnexpectedService(__func__); }
bool CHARACTER::RemoveAffect(unsigned int) { UnexpectedService(__func__); }
bool CHARACTER::IsAffectFlag(unsigned int)const { UnexpectedService(__func__); }
CAffect * CHARACTER::FindAffect(unsigned int,unsigned char)const { UnexpectedService(__func__); }
void CHARACTER::ClearAffectSkills(void) { UnexpectedService(__func__); }
void CHARACTER::SetDungeon(CDungeon *) { UnexpectedService(__func__); }
bool CHARACTER::IsEquipUniqueItem(unsigned int)const { UnexpectedService(__func__); }
bool CHARACTER::IsEquipUniqueGroup(unsigned int)const { UnexpectedService(__func__); }
void CHARACTER::GiveGold(int64_t) { UnexpectedService(__func__); }
void CHARACTER::CloseMyShop(void) { UnexpectedService(__func__); }
void CHARACTER::SetQuestDamage(int,int) { UnexpectedService(__func__); }
bool CHARACTER::IsImmortal(void)const { UnexpectedService(__func__); }
int64_t CHARACTER::GetRankPoints(int) { UnexpectedService(__func__); }
void CHARACTER::SetRankPoints(int,int64_t) { UnexpectedService(__func__); }
int CHARACTER::GetSkillLevel(unsigned int)const { UnexpectedService(__func__); }
int CHARACTER::GetSkillPower(unsigned int,unsigned char)const { UnexpectedService(__func__); }
int CHARACTER::ComputeSkill(unsigned int,entt::entity,unsigned char) { UnexpectedService(__func__); }
unsigned char CHARACTER::GetSkillGroup(void)const { UnexpectedService(__func__); }
void CHARACTER::CloseSafebox(void) { UnexpectedService(__func__); }
void CHARACTER::MonsterLog(char const *,...) { UnexpectedService(__func__); }
unsigned char CHARACTER::GetEmpire(void)const { UnexpectedService(__func__); }
void CHARACTER::SetQuestNPCID(unsigned int) { UnexpectedService(__func__); }
int CHARACTER::GetQuestFlag(std::string const &)const { UnexpectedService(__func__); }
void CHARACTER::SetQuestFlag(std::string const &,int) { UnexpectedService(__func__); }
void CHARACTER::SetNextStatePulse(int) { UnexpectedService(__func__); }
CHARACTER * CHARACTER::GetMarryPartner(void)const { UnexpectedService(__func__); }
int CHARACTER::GetMarriageBonus(unsigned int,bool) { UnexpectedService(__func__); }
int CHARACTER::GetPremiumRemainSeconds(unsigned char)const { UnexpectedService(__func__); }
bool CHARACTER::IsCubeOpen(void)const { UnexpectedService(__func__); }
bool CHARACTER::UnEquipSpecialRideUniqueItem(void) { UnexpectedService(__func__); }
bool CHARACTER::IsPet(void)const { UnexpectedService(__func__); }
bool CHARACTER::IsNewPet(void)const { UnexpectedService(__func__); }
void CHARACTER::CloseAcce(void) { UnexpectedService(__func__); }
int CHARACTER::GetSoulItemDamage(entt::entity,int,unsigned char) { UnexpectedService(__func__); }
unsigned int CHARACTER::GetMissionProgress(unsigned int,unsigned int) { UnexpectedService(__func__); }
void CHARACTER::UpdateMissionProgress(unsigned int,unsigned int,unsigned int,unsigned int,bool) { UnexpectedService(__func__); }
bool CHARACTER::IsCompletedMission(unsigned char) { UnexpectedService(__func__); }
unsigned char CHARACTER::GetBattlePassId(void) { UnexpectedService(__func__); }
unsigned int CParty::GetLeaderPID(void) { UnexpectedService(__func__); }
bool CParty::IsPositionNearLeader(entt::entity) { UnexpectedService(__func__); }
void CParty::SendMessageA(entt::entity,unsigned char,unsigned int,unsigned int) { UnexpectedService(__func__); }
int CParty::GetExpBonusPercent(void) { UnexpectedService(__func__); }
CHARACTER * CParty::GetNextOwnership(CHARACTER *,int,int) { UnexpectedService(__func__); }
int CParty::GetExpDistributionMode(void) { UnexpectedService(__func__); }
CHARACTER * CParty::GetExpCentralizeCharacter(void) { UnexpectedService(__func__); }
unsigned int SECTREE::GetAttribute(int,int) { UnexpectedService(__func__); }
bool SECTREE::IsAttr(int,int,unsigned int) { UnexpectedService(__func__); }
SECTREE * SECTREE_MANAGER::Get(int,int,int) { UnexpectedService(__func__); }
bool SECTREE_MANAGER::GetMovablePosition(int,int,int,pixel_position_s &) { UnexpectedService(__func__); }
bool SECTREE_MANAGER::IsMovablePosition(int,int,int) { UnexpectedService(__func__); }
unsigned char SECTREE_MANAGER::GetEmpireFromMapIndex(int) { UnexpectedService(__func__); }
void CDungeon::Notice(unsigned int,char const *,bool) { UnexpectedService(__func__); }
void CDungeon::KillAll(void) { UnexpectedService(__func__); }
void CDungeon::KillAllMonsters(void) { UnexpectedService(__func__); }
void CDungeon::SpawnRegen(char const *,bool) { UnexpectedService(__func__); }
void CDungeon::ClearRegen(void) { UnexpectedService(__func__); }
void CDungeon::DeadCharacter(CHARACTER *) { UnexpectedService(__func__); }
void CDungeon::ExitAllLobby(unsigned char) { UnexpectedService(__func__); }
int CDungeon::GetFlag(std::string) { UnexpectedService(__func__); }
void CDungeon::SetFlag(std::string,int) { UnexpectedService(__func__); }
void CDungeon::UpdateMastHP(void) { UnexpectedService(__func__); }
int MountSystem::GetHorseHealth(entt::entity) { UnexpectedService(__func__); }
int MountSystem::GetHorseMaxHealth(entt::entity) { UnexpectedService(__func__); }
void DESC::Packet(void const *,int) { UnexpectedService(__func__); }
bool DESC::DelayedDisconnect(int) { UnexpectedService(__func__); }
char const * get_table_postfix(void) { UnexpectedService(__func__); }
entt::entity ItemSystem::GetInventoryItem(entt::entity,unsigned short) { UnexpectedService(__func__); }
entt::entity ItemSystem::GetExtraInventoryItem(entt::entity,unsigned short) { UnexpectedService(__func__); }
entt::entity ItemSystem::FindSpecifyItem(entt::entity,unsigned int,bool) { UnexpectedService(__func__); }
entt::entity ItemSystem::GetWearItem(entt::entity,unsigned char) { UnexpectedService(__func__); }
bool ItemSystem::EquipItemEcs(entt::entity,entt::entity,int) { UnexpectedService(__func__); }
bool ItemSystem::IsValidItem(entt::entity) { UnexpectedService(__func__); }
bool ItemSystem::IsDragonSoulItem(entt::entity) { UnexpectedService(__func__); }
bool ItemSystem::IsExtraItem(entt::entity) { UnexpectedService(__func__); }
unsigned int ItemSystem::GetItemVnum(entt::entity) { UnexpectedService(__func__); }
unsigned int ItemSystem::GetItemOriginalVnum(entt::entity) { UnexpectedService(__func__); }
unsigned char ItemSystem::GetItemType(entt::entity) { UnexpectedService(__func__); }
unsigned char ItemSystem::GetItemSubType(entt::entity) { UnexpectedService(__func__); }
unsigned int ItemSystem::GetItemCount(entt::entity) { UnexpectedService(__func__); }
char const * ItemSystem::GetItemName(entt::entity) { UnexpectedService(__func__); }
unsigned int ItemSystem::GetItemAntiFlag(entt::entity) { UnexpectedService(__func__); }
SItemTable const * ItemSystem::GetItemProto(entt::entity) { UnexpectedService(__func__); }
bool ItemSystem::SetItemCountEcs(entt::entity,unsigned int) { UnexpectedService(__func__); }
bool ItemSystem::AddItemCountEcs(entt::entity,int) { UnexpectedService(__func__); }
bool ItemSystem::DestroyItemEntityEcs(entt::entity,char const *) { UnexpectedService(__func__); }
unsigned int ItemSystem::GetItemSocket(entt::entity,int) { UnexpectedService(__func__); }
TPlayerItemAttribute ItemSystem::GetItemAttribute(entt::entity,int) { UnexpectedService(__func__); }
int ItemSystem::GetItemAttributeType(entt::entity,int) { UnexpectedService(__func__); }
int ItemSystem::GetItemAttributeValue(entt::entity,int) { UnexpectedService(__func__); }
bool ItemSystem::SetItemSocket(entt::entity,int,unsigned int,bool) { UnexpectedService(__func__); }
bool ItemSystem::SetItemAttribute(entt::entity,int,int,int) { UnexpectedService(__func__); }
bool ItemSystem::PlaceItemEcs(entt::entity,entt::entity,unsigned char,unsigned short) { UnexpectedService(__func__); }
bool ItemSystem::RemoveItemEcs(entt::entity) { UnexpectedService(__func__); }
int ItemSystem::GetEmptyInventoryPositionEcs(entt::entity,entt::entity) { UnexpectedService(__func__); }
bool ItemSystem::PlaceItemOnGroundLegacyBoundary(entt::entity,int,pixel_position_s const &,int) { UnexpectedService(__func__); }
bool ItemSystem::IsItemVnumStackable(unsigned int) { UnexpectedService(__func__); }
bool ItemSystem::SetGroundOwnership(entt::entity,entt::entity,int) { UnexpectedService(__func__); }
CPIDRegistry & CPIDRegistry::Instance(void) { UnexpectedService(__func__); }
std::vector<entt::entity,std::allocator<entt::entity> > CPIDRegistry::Snapshot(void)const { UnexpectedService(__func__); }
void CHARACTER_MANAGER::DestroyCharacter(CHARACTER *) { UnexpectedService(__func__); }
CHARACTER * CHARACTER_MANAGER::SpawnMob(unsigned int,int,int,int,int,bool,int,bool) { UnexpectedService(__func__); }
CHARACTER * CHARACTER_MANAGER::SpawnGroup(unsigned int,int,int,int,int,int,regen *,bool,CDungeon *) { UnexpectedService(__func__); }
void CHARACTER_MANAGER::SelectStone(entt::entity) { UnexpectedService(__func__); }
CHARACTER * CHARACTER_MANAGER::Find(unsigned int) { UnexpectedService(__func__); }
CHARACTER * CHARACTER_MANAGER::FindByPID(unsigned int) { UnexpectedService(__func__); }
entt::entity CHARACTER_MANAGER::FindEntity(unsigned int) { UnexpectedService(__func__); }
void CHARACTER_MANAGER::KillLog(unsigned int) { UnexpectedService(__func__); }
int CHARACTER_MANAGER::GetMobGoldAmountRate(entt::entity) { UnexpectedService(__func__); }
int CHARACTER_MANAGER::GetMobGoldDropRate(entt::entity) { UnexpectedService(__func__); }
int CHARACTER_MANAGER::GetMobExpRate(entt::entity) { UnexpectedService(__func__); }
int CHARACTER_MANAGER::GetUserDamageRate(entt::entity) { UnexpectedService(__func__); }
event_struct_ const * CHARACTER_MANAGER::CheckEventIsActive(unsigned char,unsigned char) { UnexpectedService(__func__); }
entt::entity ITEM_MANAGER::CreateItem(unsigned int,unsigned int,unsigned int,bool,int,bool) { UnexpectedService(__func__); }
bool ITEM_MANAGER::CreateDropItem(CHARACTER *,CHARACTER *,std::vector<entt::entity,std::allocator<entt::entity> > &) { UnexpectedService(__func__); }
int CalcMeleeDamage(entt::entity,entt::entity,bool,bool) { UnexpectedService(__func__); }
int CalcMagicDamage(entt::entity,entt::entity) { UnexpectedService(__func__); }
int CalcArrowDamage(entt::entity,entt::entity,entt::entity,entt::entity,bool) { UnexpectedService(__func__); }
bool battle_is_attackable(entt::entity,entt::entity) { UnexpectedService(__func__); }
int battle_melee_attack(entt::entity,entt::entity) { UnexpectedService(__func__); }
void battle_end(entt::entity) { UnexpectedService(__func__); }
void NormalAttackAffect(entt::entity,entt::entity) { UnexpectedService(__func__); }
bool CPVPManager::Dead(entt::entity,unsigned int) { UnexpectedService(__func__); }
void LogManager::ItemLogEntity(entt::entity,entt::entity,char const *,char const *) { UnexpectedService(__func__); }
void LogManager::CharLog(entt::entity,unsigned int,char const *,char const *) { UnexpectedService(__func__); }
void LogManager::HackLog(char const *,entt::entity) { UnexpectedService(__func__); }
int CPrivManager::GetPriv(entt::entity,unsigned char) { UnexpectedService(__func__); }
_SQLMsg * DBManager::DirectQuery(char const *,...) { UnexpectedService(__func__); }
void DBManager::SendMoneyLog(unsigned char,unsigned int,int64_t) { UnexpectedService(__func__); }
unsigned int DBManager::EscapeString(char *,uint64_t,char const *,unsigned int) { UnexpectedService(__func__); }
float GetDegreeFromPositionXY(int,int,int,int) { UnexpectedService(__func__); }
void GetDeltaByDegree(float,float,float *,float *) { UnexpectedService(__func__); }
float GetDegreeDelta(float,float) { UnexpectedService(__func__); }
bool marriage::TMarriage::IsNear(void) { UnexpectedService(__func__); }
void marriage::TMarriage::Update(unsigned int) { UnexpectedService(__func__); }
marriage::TMarriage * marriage::CManager::Get(unsigned int) { UnexpectedService(__func__); }
bool CArenaManager::OnDead(entt::entity,entt::entity) { UnexpectedService(__func__); }
void CExchange::Cancel(void) { UnexpectedService(__func__); }
void CShopManager::StopShopping(CHARACTER *) { UnexpectedService(__func__); }
bool CBattleArena::IsBattleArenaMap(int) { UnexpectedService(__func__); }
void CGuildManager::Kill(CHARACTER *,CHARACTER *) { UnexpectedService(__func__); }
void quest::CQuestManager::Kill(unsigned int,unsigned int) { UnexpectedService(__func__); }
void quest::CQuestManager::Die(unsigned int,unsigned int) { UnexpectedService(__func__); }
void quest::CQuestManager::QuestDamage(unsigned int,unsigned int) { UnexpectedService(__func__); }
bool CNewPetSystem::IsActivePet(void) { UnexpectedService(__func__); }
void CNewPetSystem::SetExp(int,int) { UnexpectedService(__func__); }
int CNewPetSystem::GetLevel(void) { UnexpectedService(__func__); }
int CNewPetSystem::GetLevelStep(void) { UnexpectedService(__func__); }
bool CBattlePass::BattlePassMissionGetInfo(unsigned char,unsigned char,unsigned int *,unsigned int *) { UnexpectedService(__func__); }
bool CBattlePass::IsEligibleForPlayerKill(unsigned int,unsigned int) { UnexpectedService(__func__); }
void CBattlePass::RegisterPlayerKill(unsigned int,unsigned int) { UnexpectedService(__func__); }
COrcsDungeon & COrcsDungeon::instance(void) { UnexpectedService(__func__); }
void COrcsDungeon::OnMobKilled(entt::entity,entt::entity) { UnexpectedService(__func__); }
CTritonTempleDungeon & CTritonTempleDungeon::instance(void) { UnexpectedService(__func__); }
void CTritonTempleDungeon::OnMobKilled(entt::entity,entt::entity) { UnexpectedService(__func__); }
CValentineDungeon & CValentineDungeon::instance(void) { UnexpectedService(__func__); }
void CValentineDungeon::OnMobKilled(entt::entity,entt::entity) { UnexpectedService(__func__); }
CRuneDungeon & CRuneDungeon::instance(void) { UnexpectedService(__func__); }
void CRuneDungeon::OnMobKilled(entt::entity,entt::entity) { UnexpectedService(__func__); }
CPyramidDungeonRazor93 & CPyramidDungeonRazor93::instance(void) { UnexpectedService(__func__); }
void CPyramidDungeonRazor93::OnMobKilled(entt::entity,entt::entity) { UnexpectedService(__func__); }
CNightmareDungeonRazor93 & CNightmareDungeonRazor93::instance(void) { UnexpectedService(__func__); }
void CNightmareDungeonRazor93::OnMobKilled(entt::entity,entt::entity) { UnexpectedService(__func__); }
CHalloween2022Dungeon & CHalloween2022Dungeon::instance(void) { UnexpectedService(__func__); }
void CHalloween2022Dungeon::OnMobKilled(entt::entity,entt::entity) { UnexpectedService(__func__); }
CVikingDungeon & CVikingDungeon::instance(void) { UnexpectedService(__func__); }
void CVikingDungeon::OnMobKilled(entt::entity,entt::entity) { UnexpectedService(__func__); }
CEasterDungeon & CEasterDungeon::instance(void) { UnexpectedService(__func__); }
void CEasterDungeon::OnMobKilled(entt::entity,entt::entity) { UnexpectedService(__func__); }
void Map1MassSpawnEvent_OnMobDead(unsigned int) { UnexpectedService(__func__); }

int main() {
    try {
        AlignmentChecks(); CallbackChecks(); ModeChecks(); MultiplierAndValidityChecks();
        std::cout << "Combat state checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
