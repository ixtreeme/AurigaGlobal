#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/VisibilitySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SkillSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/EntityInvariants.hpp"
#include "../../SRC/Server/GameServer/affect.h"
#include "../../SRC/Server/GameServer/arena.h"
#include "../../SRC/Server/GameServer/buffer_manager.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/constants.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/battle.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/guild.h"
#include "../../SRC/Server/GameServer/horsename_manager.h"
#include "../../SRC/Server/GameServer/item.h"
#include "../../SRC/Server/GameServer/locale_service.h"
#include "../../SRC/Server/GameServer/lua_incl.h"
#include "../../SRC/Server/GameServer/packet.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/utils.h"
#include "../../SRC/Server/GameServer/ecs/EntityFactory.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/dirty_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/events.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include <Core/Logging.hpp>

entt::registry g_registry;
entt::dispatcher g_dispatcher;
namespace A = AffectSystem;
namespace {
int checks = 0, computes = 0, packets = 0;
std::function<void(entt::entity, uint8_t, int64_t)> onChange;
std::function<void(entt::entity)> onCompute;
struct TestPoints { std::array<int64_t, POINT_MAX_NUM> values{}; };
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
[[noreturn]] void UnexpectedService(const char* name) { throw std::runtime_error(name); }
entt::entity Actor() {
    const auto e = g_registry.create();
    g_registry.emplace<ecs::TagPC>(e);
    g_registry.emplace<ecs::AffectList>(e);
    g_registry.emplace<TestPoints>(e);
    return e;
}
CAffect Value(uint32_t type = AFFECT_STR, int32_t value = 10, uint8_t point = POINT_ST) {
    return {type, point, value, AFF_SLOW, 60, 0};
}
void StorageChecks() {
    const auto e = Actor(), other = Actor();
    auto affect = A::Attach(e, Value());
    Check(affect && A::FindAffect(e, AFFECT_STR) == affect.get(), "authoritative lookup");
    Check(A::FindAffect(e, AFFECT_STR, POINT_ST, 10) == affect.get(), "value lookup");
    Check(!A::FindAffect(e, AFFECT_STR, POINT_DX), "apply filter");
    Check(!A::FindAffect(e, AFFECT_STR, POINT_ST, 11), "value filter");
    Check(!A::Detach(other, affect.get()), "foreign affect rejected");
    Check(!A::RemoveAffect(other, affect.get()), "foreign removal rejected");
    Check(A::Snapshot(e).size() == 1, "foreign removal preserved owner");
    std::weak_ptr<CAffect> lifetime = affect;
    auto batch = A::Snapshot(e);
    Check(A::Detach(e, affect.get()) == affect, "detach returns lease");
    Check(!A::Detach(e, affect.get()), "double detach rejected");
    affect.reset();
    Check(!lifetime.expired(), "snapshot keeps allocation alive");
    batch.clear();
    Check(lifetime.expired(), "last lease releases allocation");
    affect = A::Attach(e, Value());
    lifetime = affect; affect.reset();
    g_registry.destroy(e);
    Check(lifetime.expired(), "registry destruction releases affects");
    const auto recycled = Actor();
    Check(entt::to_entity(e) == entt::to_entity(recycled) && e != recycled, "versioned reuse fixture");
    Check(!A::Attach(e, Value()) && A::Snapshot(e).empty(), "stale owner rejected");
    Check(A::Snapshot(recycled).empty(), "replacement owner untouched");
    const auto bare = g_registry.create();
    g_registry.emplace<ecs::AffectList>(bare);
    Check(!A::Attach(bare, Value()), "non-character rejected");
    Check(!A::Attach(entt::null, Value()), "null owner rejected");
}
void FlagAndPointChecks() {
    const auto e = Actor();
    for (uint32_t flag = 1; flag < AFF_BITS_MAX; ++flag) {
        A::SetFlag(e, flag);
        Check(A::IsAffectFlag(e, flag), "set flag in both words");
        A::SetFlag(e, flag, false);
        Check(!A::IsAffectFlag(e, flag), "reset flag in both words");
    }
    A::SetFlag(e, AFF_YMIR);
    const auto flags = A::GetFlags(e);
    for (const uint32_t flag : {0u, static_cast<uint32_t>(AFF_BITS_MAX), 100000u, UINT32_MAX}) {
        A::SetFlag(e, flag);
        Check(!A::IsAffectFlag(e, flag), "invalid bit rejected");
        Check(A::GetFlags(e) == flags, "invalid bit does not change other bits");
    }
    Check(!A::IsLoaded(e), "initially unloaded");
    A::SetLoaded(e, true); Check(A::IsLoaded(e), "loaded flag authoritative");
    A::SetLoaded(e, false); Check(!A::IsLoaded(e), "logout clears loaded flag");
    auto value = Value(AFFECT_STR, INT32_MIN);
    A::ComputeAffect(e, value, true);
    Check(ecs::PointSystem::Get(e, POINT_ST) == INT32_MIN, "minimum signed bonus applied");
    A::ComputeAffect(e, value, false);
    Check(ecs::PointSystem::Get(e, POINT_ST) == 0, "minimum signed bonus widened before negation");
    value.dwFlag = UINT32_MAX;
    A::ComputeAffect(e, value, true);
    Check(A::GetFlags(e) == flags, "item ID flag not interpreted as a bit");
    A::ComputeAffect(e, value, false);
    value = Value(GUILD_SKILL_START);
    A::ComputeAffect(e, value, true);
    Check(ecs::PointSystem::Get(e, POINT_ST) == 0, "guild effect requires a guild at war");
    if (POINT_MAX_NUM <= UINT8_MAX) {
        value.bApplyOn = static_cast<uint8_t>(POINT_MAX_NUM);
        Check(!A::Attach(e, value), "invalid apply rejected by storage");
        A::ComputeAffect(e, value, true);
        Check(ecs::PointSystem::Get(e, POINT_ST) == 0, "invalid apply has no side effect");
    }
    g_registry.destroy(e);
    A::ComputeAffect(e, Value(), true); A::SetFlag(e, AFF_YMIR); A::SetLoaded(e, true);
    Check(!A::IsLoaded(e) && !A::IsAffectFlag(e, AFF_YMIR), "stale metadata writes ignored");
}
void RefreshChecks() {
    auto e = Actor();
    const auto first = A::Attach(e, Value());
    const auto second = A::Attach(e, Value(AFFECT_DEX, -3, POINT_DX));
    g_registry.get<ecs::AffectList>(e).affects.push_back(first);
    g_registry.get<ecs::AffectList>(e).affects.push_back({});
    onChange = [](entt::entity actor, uint8_t, int64_t) { A::RefreshAffect(actor); };
    A::RefreshAffect(e); onChange = {};
    Check(ecs::PointSystem::Get(e, POINT_ST) == 10 && ecs::PointSystem::Get(e, POINT_DX) == -3,
        "deduplicated refresh and nested refresh guard");
    Check(g_registry.get<ecs::AffectList>(e).refreshToken == 0, "guard released");

    e = Actor();
    A::Attach(e, Value());
    auto next = A::Attach(e, Value(AFFECT_DEX, 30));
    std::weak_ptr<CAffect> removed = next;
    auto* nextRaw = next.get(); next.reset();
    onChange = [&](entt::entity actor, uint8_t, int64_t) {
        A::Detach(actor, nextRaw);
        Check(!removed.expired(), "refresh lease survives nested removal");
    };
    A::RefreshAffect(e); onChange = {};
    Check(ecs::PointSystem::Get(e, POINT_ST) == 10, "removed next entry skipped");
    Check(removed.expired(), "refresh releases removed entry");

    e = Actor(); A::Attach(e, Value());
    next = A::Attach(e, Value(AFFECT_DEX, 30));
    onChange = [&](entt::entity, uint8_t, int64_t) { next->lApplyValue = 50; };
    A::RefreshAffect(e); onChange = {};
    Check(ecs::PointSystem::Get(e, POINT_ST) == 10, "overwritten snapshot skipped");

    e = Actor(); A::Attach(e, Value());
    onChange = [&](entt::entity actor, uint8_t, int64_t) {
        onChange = {};
        const auto added = A::Attach(actor, Value(AFFECT_DEX, 20));
        A::ComputeAffect(actor, *added, true);
    };
    A::RefreshAffect(e);
    Check(ecs::PointSystem::Get(e, POINT_ST) == 30, "newly applied entry not replayed by refresh");

    e = Actor(); A::Attach(e, Value()); A::Attach(e, Value(AFFECT_DEX, 30));
    entt::entity replacement = entt::null;
    onChange = [&](entt::entity actor, uint8_t, int64_t) {
        onChange = {}; g_registry.destroy(actor); replacement = Actor();
        A::Attach(replacement, Value(AFFECT_CON, 999));
    };
    A::RefreshAffect(e);
    Check(e != replacement && entt::to_entity(e) == entt::to_entity(replacement), "refresh destroys/recycles owner");
    Check(ecs::PointSystem::Get(replacement, POINT_ST) == 0, "old pass cannot affect recycled entity");

    e = Actor(); A::Attach(e, Value()); A::Attach(e, Value(AFFECT_DEX, 30));
    onChange = [&](entt::entity actor, uint8_t, int64_t) {
        onChange = {};
        g_registry.remove<ecs::AffectList>(actor);
        g_registry.emplace<ecs::AffectList>(actor);
        A::Attach(actor, Value(AFFECT_CON, 7)); A::RefreshAffect(actor);
    };
    A::RefreshAffect(e);
    Check(ecs::PointSystem::Get(e, POINT_ST) == 17, "new component pass independent of old pass");
    Check(g_registry.get<ecs::AffectList>(e).refreshToken == 0, "replacement component guard released");

    e = Actor(); A::Attach(e, Value());
    onChange = [](entt::entity, uint8_t, int64_t) { throw std::runtime_error("point service failure"); };
    bool threw = false;
    try { A::RefreshAffect(e); } catch (const std::runtime_error&) { threw = true; }
    onChange = {};
    Check(threw && g_registry.get<ecs::AffectList>(e).refreshToken == 0, "exception releases refresh guard");
    g_registry.get<TestPoints>(e).values.fill(0); A::RefreshAffect(e);
    Check(ecs::PointSystem::Get(e, POINT_ST) == 10, "refresh can run after exception");
    const auto duration = A::Snapshot(e).front()->lDuration;
    A::UpdateAffect(g_registry, 25); AffectSystem_Update(g_registry, 25);
    Check(A::Snapshot(e).front()->lDuration == duration, "no duplicate ECS expiry scheduler");
}
void RemovalChecks() {
    auto e = Actor(); auto affect = A::Attach(e, Value());
    A::ComputeAffect(e, *affect, true);
    const int beforeCompute = computes;
    onChange = [&](entt::entity actor, uint8_t, int64_t) {
        Check(!A::Lease(actor, affect.get()), "detached before point callbacks");
        Check(!A::RemoveAffect(actor, affect.get()), "nested double removal rejected");
    };
    Check(A::RemoveAffect(e, affect.get()), "native removal"); onChange = {};
    Check(ecs::PointSystem::Get(e, POINT_ST) == 0 && computes == beforeCompute + 1, "removal applies inverse then recomputes");
    Check(!A::IsAffectFlag(e, AFF_SLOW), "removal clears flag");
    Check(!A::RemoveAffect(e, affect.get()), "repeat removal has no side effects");
    Check(computes == beforeCompute + 1, "repeat removal does not recompute");

    for (const auto type : {AFFECT_REVIVE_INVISIBLE, AFFECT_MOUNT}) {
        affect = A::Attach(e, Value(type));
        const auto before = computes, beforePackets = packets;
        A::RemoveAffect(e, affect.get());
        Check(computes == before && packets == beforePackets + 1, "revive/mount skip full recomputation");
    }
    auto& points = g_registry.get<TestPoints>(e).values;
    points[POINT_HP] = 150; points[POINT_SP] = 120;
    affect = A::Attach(e, Value()); A::RemoveAffect(e, affect.get());
    Check(points[POINT_HP] == 100 && points[POINT_SP] == 80, "maximum HP/SP clamped after removal");

    e = Actor(); A::Attach(e, Value()); A::Attach(e, Value());
    onCompute = [&](entt::entity actor) { onCompute = {}; A::Attach(actor, Value(AFFECT_STR, 99)); };
    Check(A::RemoveAffect(e, static_cast<uint32_t>(AFFECT_STR)), "type removal batch");
    Check(A::Snapshot(e).size() == 1 && A::FindAffect(e, AFFECT_STR)->lApplyValue == 99,
        "type removal leaves callback additions for next operation");

    for (int callbackStage = 0; callbackStage < 3; ++callbackStage) {
        e = Actor(); affect = A::Attach(e, Value());
        g_registry.get<TestPoints>(e).values[POINT_HP] = 150;
        entt::entity replacement = entt::null;
        const auto destroy = [&](entt::entity actor) { g_registry.destroy(actor); replacement = Actor(); };
        if (callbackStage == 0)
            onChange = [&](entt::entity actor, uint8_t, int64_t) { onChange = {}; destroy(actor); };
        else if (callbackStage == 1)
            onCompute = [&](entt::entity actor) { onCompute = {}; destroy(actor); };
        else
            onChange = [&](entt::entity actor, uint8_t point, int64_t) {
                if (point == POINT_HP) { onChange = {}; destroy(actor); }
            };
        Check(A::RemoveAffect(e, affect.get()), "removal tolerates owner destruction");
        Check(replacement != entt::null && replacement != e, "removal callback executed");
        Check(A::Snapshot(replacement).empty(), "removal cannot alter replacement actor");
        onChange = {}; onCompute = {};
    }
    e = Actor(); A::Attach(e, Value(AFFECT_STUN)); A::Attach(e, Value(AFFECT_SLOW));
    A::Attach(e, Value(AFFECT_STR)); A::Attach(e, Value(AFFECT_AUTO_HP_RECOVERY));
    A::RemoveBadAffects(e);
    Check(!A::FindAffect(e, AFFECT_STUN) && !A::FindAffect(e, AFFECT_SLOW), "bad affects removed natively");
    Check(A::FindAffect(e, AFFECT_STR) != nullptr, "good affect survives bad removal");
    A::RemoveGoodAffects(e);
    Check(!A::FindAffect(e, AFFECT_STR) && A::FindAffect(e, AFFECT_AUTO_HP_RECOVERY), "good removal preserves unrelated types");
}
void LifetimeStressChecks() {
    for (int i = 0; i < 1000; ++i) {
        const auto e = Actor();
        auto lease = A::Attach(e, Value(AFFECT_STR, i));
        std::weak_ptr<CAffect> lifetime = lease;
        auto* raw = lease.get(); lease.reset();
        A::RefreshAffect(e); A::RemoveAffect(e, raw);
        Check(lifetime.expired(), "repeated native removal frees owned allocation");
        Check(!A::RemoveAffect(e, raw), "released pointer rejected without dereference");
        g_registry.destroy(e);
    }
}
}
int64_t ecs::PointSystem::Get(entt::entity e, uint8_t type) {
    const auto* points = g_registry.valid(e) ? g_registry.try_get<TestPoints>(e) : nullptr;
    return points && type < POINT_MAX_NUM ? points->values[type] : 0;
}
int32_t ecs::PointSystem::GetMaxHP(entt::entity) { return 100; }
int32_t ecs::PointSystem::GetMaxSP(entt::entity) { return 80; }
void ecs::PointSystem::Change(entt::entity e, uint8_t type, int64_t amount, bool, bool
#ifdef __ENABLE_BLOCK_EXP__
    , bool
#endif
) {
    Check(g_registry.valid(e) && g_registry.all_of<TestPoints>(e), "point change on live actor");
    Check(type < POINT_MAX_NUM, "point change type");
    g_registry.get<TestPoints>(e).values[type] += amount;
    if (onChange) { const auto callback = onChange; callback(e, type, amount); }
}
void ecs::PointSystem::Compute(entt::entity e) {
    ++computes;
    if (onCompute) { const auto callback = onCompute; callback(e); }
}
void NetworkSyncSystem::UpdatePacket(entt::entity) { ++packets; }
CGuild* ecs::SocialSystem::GetGuild(entt::entity) { return nullptr; }
bool ecs::PlayerRuntime::IsPC(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ecs::TagPC>(e); }
LPDESC ecs::PlayerRuntime::GetDesc(entt::entity) { return nullptr; }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { return 1; }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { return "affect-test"; }

// Unrelated service doubles are fail-fast; the production TU is compiled whole.
const TApplyInfo aApplyInfo[MAX_APPLY_NUM] {};
const int aiMobEnchantApplyIdx[MOB_ENCHANTS_MAX_NUM] {};
const int aiMobResistsApplyIdx[MOB_RESISTS_MAX_NUM] {};
const int aiPolymorphPowerByLevel[SKILL_MAX_LEVEL + 1] {};
int passes_per_sec = 25, test_server = 0;
CLIENT_DESC* db_clientdesc = nullptr;
std::shared_ptr<spdlog::logger> logging::GetLogger(void) {
    static auto logger = std::make_shared<spdlog::logger>("affect-lifecycle"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger(void) { return logging::GetLogger(); }
int MAX(int,int) { UnexpectedService(__func__); }
int MIN(int,int) { UnexpectedService(__func__); }
int MINMAX(int,int,int) { UnexpectedService(__func__); }
int number_ex(int,int,char const *,int) { UnexpectedService(__func__); }
unsigned int get_dword_time(void) { UnexpectedService(__func__); }
void intrusive_ptr_add_ref(event *) { UnexpectedService(__func__); }
void intrusive_ptr_release(event *) { UnexpectedService(__func__); }
boost::intrusive_ptr<event> event_create_ex(int (*)(boost::intrusive_ptr<event>,int),event_info_data *,int) { UnexpectedService(__func__); }
void event_cancel(boost::intrusive_ptr<event> *) { UnexpectedService(__func__); }
void ecs::ChatSystem::Send(entt::entity,unsigned char,char const *,...) { UnexpectedService(__func__); }
void ecs::ChatSystem::SendNew(entt::entity,unsigned char,unsigned int,char const *,...) { UnexpectedService(__func__); }
int ecs::PointSystem::GetLevel(entt::entity) { UnexpectedService(__func__); }
void ecs::PointSystem::ApplyPoint(entt::entity,unsigned char,int) { UnexpectedService(__func__); }
void ecs::PlayerRuntime::SetPart(entt::entity,unsigned char,unsigned short) { UnexpectedService(__func__); }
unsigned char ecs::PlayerRuntime::GetMobRank(entt::entity) { UnexpectedService(__func__); }
int ecs::QuestSystem::GetFlag(entt::entity,std::string_view) { UnexpectedService(__func__); }
void NetworkSyncSystem::BroadcastSyncPacket(entt::registry &,entt::entity) { UnexpectedService(__func__); }
void ecs::MovementSystem::SyncDestinationClear(entt::entity) { UnexpectedService(__func__); }
bool CombatSystem::Damage(entt::entity,entt::entity,int,unsigned char) { UnexpectedService(__func__); }
void CombatSystem::SetComboSequence(entt::entity,unsigned char) { UnexpectedService(__func__); }
void CombatSystem::SetValidComboInterval(entt::entity,int) { UnexpectedService(__func__); }
unsigned int CGuild::UnderAnyWar(unsigned char) { UnexpectedService(__func__); }
void CGuild::GiveGuildBuff(entt::entity) { UnexpectedService(__func__); }
int CEntity::GetX(void)const { UnexpectedService(__func__); }
int CEntity::GetY(void)const { UnexpectedService(__func__); }
int64_t get_global_time(void) { UnexpectedService(__func__); }
char const * CHARACTER::GetName(unsigned char)const { UnexpectedService(__func__); }
void CHARACTER::SetHP(int64_t) { UnexpectedService(__func__); }
int64_t CHARACTER::GetHP(void)const { UnexpectedService(__func__); }
int64_t CHARACTER::GetSP(void)const { UnexpectedService(__func__); }
int CHARACTER::GetStamina(void)const { UnexpectedService(__func__); }
void CHARACTER::SetMaxHP(int64_t) { UnexpectedService(__func__); }
int64_t CHARACTER::GetMaxHP(void)const { UnexpectedService(__func__); }
int64_t CHARACTER::GetMaxSP(void)const { UnexpectedService(__func__); }
int64_t CHARACTER::GetMaxStamina(void)const { UnexpectedService(__func__); }
int64_t CHARACTER::GetPoint(unsigned char)const { UnexpectedService(__func__); }
void CHARACTER::PointChange(unsigned char,int64_t,bool,bool,bool) { UnexpectedService(__func__); }
void CHARACTER::CheckMaximumPoints(void) { UnexpectedService(__func__); }
int CHARACTER::GetCurrentDestX(void)const { UnexpectedService(__func__); }
int CHARACTER::GetCurrentDestY(void)const { UnexpectedService(__func__); }
unsigned int CHARACTER::GetStopTime(void)const { UnexpectedService(__func__); }
bool CHARACTER::IsDead(void)const { UnexpectedService(__func__); }
void CHARACTER::StartMuyeongEvent(void) { UnexpectedService(__func__); }
void CHARACTER::StopMuyeongEvent(void) { UnexpectedService(__func__); }
void CHARACTER::StartGyeongGongEvent(void) { UnexpectedService(__func__); }
void CHARACTER::StopGyeongGongEvent(void) { UnexpectedService(__func__); }
int CHARACTER::GetQuestFlag(std::string const &)const { UnexpectedService(__func__); }
int CHARACTER::GetPremiumRemainSeconds(unsigned char)const { UnexpectedService(__func__); }
void CHARACTER::AutoRecoveryItemProcess(EAffectTypes) { UnexpectedService(__func__); }
void CHARACTER::AutoRecallProcess(void) { UnexpectedService(__func__); }
void CHARACTER::DragonSoul_Initialize(void) { UnexpectedService(__func__); }
void CHARACTER::SetDropStatus(void) { UnexpectedService(__func__); }
bool MountSystem::StopRiding(entt::entity) { UnexpectedService(__func__); }
void ecs::VisibilitySystem::Reencode(entt::registry &,entt::entity) { UnexpectedService(__func__); }
int SkillSystem::GetSkillLevel(entt::entity,unsigned int) { UnexpectedService(__func__); }
CHARACTER * CHARACTER_MANAGER::FindByPID(unsigned int) { UnexpectedService(__func__); }
bool CArenaManager::IsArenaMap(unsigned int) { UnexpectedService(__func__); }
void DESC::Packet(void const *,int) { UnexpectedService(__func__); }
void CLIENT_DESC::DBPacket(unsigned char,unsigned int,void const *,unsigned int) { UnexpectedService(__func__); }
void battle_end(entt::entity) { UnexpectedService(__func__); }
void CHorseNameManager::Validate(CHARACTER *) { UnexpectedService(__func__); }
int quest::CQuestManager::GetEventFlag(std::string const &) { UnexpectedService(__func__); }
entt::entity ItemSystem::FindItemByID(entt::entity,unsigned int) { UnexpectedService(__func__); }
entt::entity ItemSystem::GetWearItem(entt::entity,unsigned char) { UnexpectedService(__func__); }
bool ItemSystem::IsValidItem(entt::entity) { UnexpectedService(__func__); }
bool ItemSystem::LockItem(entt::entity,bool) { UnexpectedService(__func__); }

int main() {
    try {
        StorageChecks(); FlagAndPointChecks(); RefreshChecks(); RemovalChecks(); LifetimeStressChecks();
        std::cout << "Affect checks passed: " << checks << '\n'; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
