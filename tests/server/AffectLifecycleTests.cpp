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
#include "../../SRC/Server/GameServer/ecs/components/movement_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/character_runtime_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/transform_components.hpp"
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
std::function<void(entt::entity)> onUpdate, onSync;
std::function<void()> onSchedule;
std::function<void(uint8_t)> onClientPacket, onDBPacket;
std::vector<LPEVENT> scheduled;
int schedules = 0, cancels = 0, syncs = 0, chats = 0;
bool failSchedule = false;
int64_t nowSeconds = 1000;
std::array<int, PREMIUM_MAX_NUM> premiumRemaining{};
int hairDeadline = 0, horseDeadline = 0, hairResets = 0;
std::function<void(entt::entity)> onPart, onQuest;
std::function<void(entt::entity, bool)> onHorse;
std::vector<bool> horseCalls;
std::vector<TPacketUpdateHorseName> horsePackets;
entt::entity connected = entt::null;
DESC* client = nullptr;
std::vector<TPacketGCAffectAdd> clientAdds;
std::vector<TPacketGCAffectRemove> clientRemoves;
std::vector<uint8_t> dbHeaders;
std::map<std::pair<uint32_t, uint8_t>, TPacketGDAddAffect> storedAffects;
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
bool Add(entt::entity e, int value = 10, uint32_t type = AFFECT_STR,
    uint8_t apply = POINT_ST, uint32_t flag = 0, bool overwrite = true, bool cube = false, int duration = 60) {
    return A::AddAffect(e, type, apply, value, flag, duration, 0, overwrite, cube);
}
void ResetPublication() {
    clientAdds.clear(); clientRemoves.clear(); dbHeaders.clear(); storedAffects.clear();
    onClientPacket = {}; onDBPacket = {}; onUpdate = {}; onChange = {}; onCompute = {}; onSync = {}; onSchedule = {};
    onPart = {}; onQuest = {}; onHorse = {}; horseCalls.clear(); horsePackets.clear();
}
void AddChecks() {
    auto e = Actor();
    const int before = schedules;
    Check(Add(e), "entity-only add without descriptor");
    Check(ecs::PointSystem::Get(e, POINT_ST) == 10, "native add points");
    Check(Add(e, 25), "overwrite existing affect");
    Check(A::Snapshot(e).size() == 1 && ecs::PointSystem::Get(e, POINT_ST) == 25, "overwrite reverses old bonus once");
    Check(schedules == before + 1, "adds reuse a single timer");
    Check(Add(e, 5, AFFECT_STR, POINT_DX, 0, true, true), "cube second apply slot");
    Check(A::Snapshot(e).size() == 2 && ecs::PointSystem::Get(e, POINT_DX) == 5, "cube preserves different apply slot");
    Check(Add(e, 9, AFFECT_STR, POINT_DX, 0, true, true), "cube overwrite matching apply");
    Check(ecs::PointSystem::Get(e, POINT_DX) == 9, "cube inverse and new points");
    Check(Add(e, 2, AFFECT_STR, POINT_ST, 0, false), "non-overwrite stack");
    Check(A::Snapshot(e).size() == 3 && ecs::PointSystem::Get(e, POINT_ST) == 27, "non-overwrite preserves existing records");
    Check(Add(e, 1, AFFECT_DEX, POINT_DX, 0, true, false, 0), "zero duration normalized");
    Check(A::FindAffect(e, AFFECT_DEX)->lDuration == 1, "one-second minimum for zero duration");
    if (POINT_MAX_NUM <= UINT8_MAX)
        Check(!Add(e, 1, AFFECT_STR, static_cast<uint8_t>(POINT_MAX_NUM)), "invalid apply rejected before overwrite");
    g_registry.destroy(e);
    Check(!Add(e) && !Add(entt::null), "stale/null add rejected");
    e = Actor(); failSchedule = true;
    Check(!Add(e) && A::Snapshot(e).empty() && ecs::PointSystem::Get(e, POINT_ST) == 0,
        "failed scheduling grants no affect or points");
    failSchedule = false; Check(Add(e), "scheduling failure can be retried");

    e = Actor(); Add(e, 5);
    onChange = [&](entt::entity actor, uint8_t, int64_t amount) {
        if (amount < 0) { onChange = {}; Check(Add(actor, 77), "nested overwrite during old bonus removal"); }
    };
    Check(!Add(e, 20), "obsolete outer overwrite stopped before attachment");
    Check(A::Snapshot(e).size() == 1 && A::FindAffect(e, AFFECT_STR)->lApplyValue == 77 &&
        ecs::PointSystem::Get(e, POINT_ST) == 77, "nested replacement wins");
    e = Actor(); Add(e, 5);
    onChange = [&](entt::entity actor, uint8_t, int64_t amount) {
        if (amount < 0) { onChange = {}; A::RemoveAffect(actor, static_cast<uint32_t>(AFFECT_STR)); }
    };
    Check(!Add(e, 20) && A::Snapshot(e).empty(), "remove request in replacement gap cancels outer add");
    e = Actor();
    onUpdate = [&](entt::entity actor) { onUpdate = {}; A::RemoveAffect(actor, static_cast<uint32_t>(AFFECT_STR)); };
    Check(!Add(e) && A::Snapshot(e).empty(), "callback removal cancels add publication");

    for (int stage = 0; stage < 3; ++stage) {
        e = Actor(); entt::entity replacement = entt::null;
        const auto destroy = [&] { g_registry.destroy(e); replacement = Actor(); };
        if (stage == 0) onSchedule = [&] { onSchedule = {}; destroy(); };
        if (stage == 1) onChange = [&](entt::entity, uint8_t, int64_t) { onChange = {}; destroy(); };
        if (stage == 2) onUpdate = [&](entt::entity) { onUpdate = {}; destroy(); };
        Check(!Add(e), "destroyed owner aborts add");
        Check(replacement != e && A::Snapshot(replacement).empty(), "recycled owner is untouched");
    }
    e = Actor();
    g_registry.emplace<ecs::Position>(e, 100, 200, 0);
    g_registry.emplace<ecs::MovementDestination>(e, 300, 400);
    auto& movement = g_registry.emplace<ecs::MovementState>(e);
    movement.moveDuration = 500; movement.moveStartTime = 99;
    g_registry.emplace<ecs::CharacterRuntimeFlagsComponent>(e).position = POS_FIGHTING;
    const int beforeSync = syncs;
    Check(Add(e, 0, AFFECT_STUN, POINT_NONE, AFF_STUN), "native stun add");
    Check(!g_registry.all_of<ecs::MovementDestination>(e) && movement.moveDuration == 0 && movement.moveStartTime == 0,
        "stun aborts destination and timing");
    Check(g_registry.get<ecs::CharacterRuntimeFlagsComponent>(e).position == POS_STANDING && syncs == beforeSync + 1,
        "stun leaves fighting posture and publishes sync");
    Check(Add(e, 0, AFFECT_STUN, POINT_NONE, AFF_STUN) && syncs == beforeSync + 1, "stationary stun sends no movement sync");
    e = Actor();
    Add(e, 5, SKILL_GEOMKYUNG, POINT_ST, AFF_GEOMGYEONG);
    Add(e, 7, SKILL_GWIGEOM, POINT_ST, AFF_GWIGUM);
    Check(Add(e, 0, AFFECT_POLYMORPH, POINT_NONE), "polymorph add");
    Check(!A::FindAffect(e, SKILL_GEOMKYUNG) && !A::FindAffect(e, SKILL_GWIGEOM), "polymorph removes conflicting buffs");
}
void TimerChecks() {
    auto e = Actor();
    Check(A::StartAffectEvent(e), "native timer start");
    auto timer = g_registry.get<ecs::AffectTickState>(e).timer;
    const auto count = schedules;
    Check(A::StartAffectEvent(e) && schedules == count, "idempotent timer start");
    Check(static_cast<char_event_info*>(timer->info)->ch == e, "timer carries versioned entity only");
    A::StopAffectEvent(e);
    Check(timer->is_force_to_end && !g_registry.get<ecs::AffectTickState>(e).timer, "timer stopped and slot cleared");
    A::StartAffectEvent(e);
    auto replacement = g_registry.get<ecs::AffectTickState>(e).timer;
    Check(timer->func(timer, 0) == 0 && g_registry.get<ecs::AffectTickState>(e).timer == replacement,
        "old callback cannot stop replacement timer");
    g_registry.destroy(e);
    Check(replacement->is_force_to_end && replacement->func(replacement, 0) == 0, "entity destruction cancels timer and rejects stale callback");
    e = Actor();
    onSchedule = [&] { Check(!A::StartAffectEvent(e), "recursive start cannot report an uninstalled timer"); };
    const auto before = schedules;
    Check(A::StartAffectEvent(e) && schedules == before + 1, "recursive start does not allocate duplicate timer");
    onSchedule = {};
    timer = g_registry.get<ecs::AffectTickState>(e).timer;
    g_registry.remove<ecs::AffectTickState>(e);
    Check(timer->is_force_to_end, "component removal cancels timer");
    onSchedule = [&] {
        onSchedule = {};
        Check(!Add(e), "nested add cannot use a pending timer");
        A::StopAffectEvent(e);
    };
    Check(!A::StartAffectEvent(e) && A::Snapshot(e).empty(), "cancelled pending creation grants no nested affect");
    onSchedule = [&] { onSchedule = {}; A::StopAffectEvent(e); A::StartAffectEvent(e); };
    Check(!A::StartAffectEvent(e), "superseded pending start rejected");
    Check(g_registry.get<ecs::AffectTickState>(e).timer && !g_registry.get<ecs::AffectTickState>(e).timer->is_force_to_end,
        "new timer survives outer start completion");
    timer = g_registry.get<ecs::AffectTickState>(e).timer;
    Check(timer->func(timer, 0) == 0 && timer->is_force_to_end, "unmigrated tick leaf absent: stop safely");
    A::StartAffectEvent(e);
    timer = g_registry.get<ecs::AffectTickState>(e).timer;
    g_registry.remove<ecs::AffectList>(e);
    Check(timer->func(timer, 0) == 0 && !g_registry.get<ecs::AffectTickState>(e).timer,
        "missing affect storage clears the stopped timer slot");
    g_registry.emplace<ecs::AffectList>(e);
    Check(A::StartAffectEvent(e) && g_registry.get<ecs::AffectTickState>(e).timer != timer,
        "recreated affect storage can start a fresh timer");
    // EnTT moves the last component into an erased slot. Moving is not cancellation.
    const auto a = Actor(), b = Actor(); A::StartAffectEvent(a); A::StartAffectEvent(b);
    timer = g_registry.get<ecs::AffectTickState>(b).timer;
    g_registry.remove<ecs::AffectTickState>(a);
    Check(!timer->is_force_to_end && g_registry.get<ecs::AffectTickState>(b).timer == timer, "component relocation transfers timer ownership");
    g_registry.clear<ecs::AffectTickState>();
    Check(timer->is_force_to_end, "shutdown cancels ECS timers before queue teardown");
}
void PublicationChecks() {
    DESC descriptor;
    CLIENT_DESC database;
    client = &descriptor; db_clientdesc = &database;
    auto reset = [&] { ResetPublication(); connected = Actor(); return connected; };
    auto e = reset();
    Check(Add(e, 41), "connected add");
    Check(clientAdds.size() == 1 && dbHeaders.size() == 1 && clientRemoves.empty(), "single client/DB add");
    const auto& packet = clientAdds.back().elem;
    Check(packet.dwType == AFFECT_STR && packet.bApplyOn == POINT_ST && packet.lApplyValue == 41 &&
        packet.lDuration == 60 && packet.lSPCost == 0 && packet.dwFlag == 0, "client fields preserved");
    Check(storedAffects.at({AFFECT_STR, POINT_ST}).dwPID == 1, "entity player ID persisted");
    Check(Add(e, 51) && storedAffects.at({AFFECT_STR, POINT_ST}).elem.lApplyValue == 51,
        "overwrite persists replacement");
    Check(clientRemoves.size() == 1 && clientAdds.size() == 2, "overwrite publishes remove/add");
    for (const uint32_t type : {AFFECT_WAR_FLAG, AFFECT_REVIVE_INVISIBLE, AFFECT_PREMIUM_START, AFFECT_PREMIUM_END}) {
        e = reset(); Check(Add(e, 0, type, POINT_NONE), "no-save affect accepted");
        Check(clientAdds.size() == 1 && dbHeaders.empty(), "no-save affect not persisted");
    }
    for (int stage = 0; stage < 3; ++stage) {
        e = reset();
        const auto replace = [&] { Check(Add(e, 99), "nested packet replacement"); };
        if (stage == 0) onUpdate = [&](entt::entity) { onUpdate = {}; replace(); };
        if (stage == 1) onDBPacket = [&](uint8_t header) {
            if (header == HEADER_GD_ADD_AFFECT) { onDBPacket = {}; replace(); }
        };
        if (stage == 2) onClientPacket = [&](uint8_t header) {
            if (header == HEADER_GC_AFFECT_ADD) { onClientPacket = {}; replace(); }
        };
        Check(!Add(e, 10), "superseded outer add not reported current");
        Check(A::FindAffect(e, AFFECT_STR)->lApplyValue == 99 && clientAdds.back().elem.lApplyValue == 99 &&
            storedAffects.at({AFFECT_STR, POINT_ST}).elem.lApplyValue == 99, "latest value wins ECS, client and DB");
    }
    e = reset(); Add(e, 5);
    onDBPacket = [&](uint8_t header) {
        if (header == HEADER_GD_REMOVE_AFFECT) { onDBPacket = {}; Check(Add(e, 77), "replacement during remove packet"); }
    };
    Check(!Add(e, 20), "outer overwrite aborted after nested remove publication");
    Check(clientAdds.back().elem.lApplyValue == 77 && storedAffects.at({AFFECT_STR, POINT_ST}).elem.lApplyValue == 77,
        "obsolete remove cannot erase replacement");
    e = reset();
    onDBPacket = [&](uint8_t header) {
        if (header == HEADER_GD_ADD_AFFECT) { onDBPacket = {}; Check(Add(e, 33, AFFECT_STR, POINT_DX, 0, true, true), "other cube slot during publication"); }
    };
    Check(Add(e, 11), "different apply slot does not cancel publication");
    Check(storedAffects.size() == 2 && clientAdds.size() == 2, "both wire keys published");
    e = reset();
    onDBPacket = [&](uint8_t) { onDBPacket = {}; g_registry.destroy(e); connected = Actor(); };
    Check(!Add(e) && clientAdds.empty(), "owner destroyed by DB callback: no stale client packet");
    ResetPublication(); connected = entt::null; client = nullptr; db_clientdesc = nullptr;
}
A::AffectLease InstallTimed(entt::entity e, uint32_t type, int32_t duration,
    uint8_t point = POINT_NONE, int32_t value = 0, int32_t cost = 0, uint32_t flag = 0) {
    auto affect = A::Attach(e, {type, point, value, flag, duration, cost});
    Check(static_cast<bool>(affect), "timed affect fixture attached");
    A::ComputeAffect(e, *affect, true);
    return affect;
}
void ExpiryChecks() {
    ResetPublication();
    auto e = Actor();
    auto affect = InstallTimed(e, AFFECT_STR, 2, POINT_ST, 10, 4, AFF_SLOW);
    g_registry.get<TestPoints>(e).values[POINT_SP] = 10;
    const int beforeCompute = computes;
    Check(!A::ProcessAffect(e) && affect->lDuration == 1 && ecs::PointSystem::Get(e, POINT_SP) == 6,
        "live effect pays SP and loses one second");
    Check(A::ProcessAffect(e) && ecs::PointSystem::Get(e, POINT_SP) == 2 &&
        ecs::PointSystem::Get(e, POINT_ST) == 0 && !A::IsAffectFlag(e, AFF_SLOW),
        "final tick removes points and flag once");
    Check(computes == beforeCompute && A::ProcessAffect(e), "ordinary expiry does not reapply all points");
    for (int duration : {0, -1, INT32_MIN}) {
        e = Actor(); InstallTimed(e, AFFECT_STR, duration, POINT_ST, 7);
        Check(A::ProcessAffect(e) && ecs::PointSystem::Get(e, POINT_ST) == 0,
            "invalid loaded duration expires without signed underflow");
    }
    e = Actor(); InstallTimed(e, AFFECT_STR, 60, POINT_ST, 5, 7);
    g_registry.get<TestPoints>(e).values[POINT_SP] = 6;
    Check(A::ProcessAffect(e) && ecs::PointSystem::Get(e, POINT_SP) == 6,
        "insufficient SP expires effect without an overdraft");
    e = Actor(); affect = InstallTimed(e, AFFECT_STR, INT32_MAX, POINT_NONE, 0, INT32_MAX);
    g_registry.get<TestPoints>(e).values[POINT_SP] = static_cast<int64_t>(INT32_MAX) + 3;
    Check(!A::ProcessAffect(e) && affect->lDuration == INT32_MAX - 1 && ecs::PointSystem::Get(e, POINT_SP) == 3,
        "large duration and SP cost remain representable");
#ifdef ENABLE_SOUL_SYSTEM
    e = Actor();
    InstallTimed(e, AFFECT_SOUL_RED, 2, POINT_NONE, 0, 123456);
    InstallTimed(e, AFFECT_SOUL_BLUE, 2, POINT_NONE, 0, 234567);
    Check(!A::ProcessAffect(e) && A::Snapshot(e).size() == 2 && ecs::PointSystem::Get(e, POINT_SP) == 0,
        "soul seal IDs are not interpreted as SP costs");
#endif
    e = Actor(); InstallTimed(e, GUILD_SKILL_START, 20);
    Check(A::ProcessAffect(e), "guild skill expires without a guild war");
    e = Actor(); affect = InstallTimed(e, AFFECT_STR, 3, POINT_NONE, 0, 2);
    g_registry.get<ecs::AffectList>(e).affects.push_back(affect);
    g_registry.get<TestPoints>(e).values[POINT_SP] = 10;
    Check(!A::ProcessAffect(e) && affect->lDuration == 2 && ecs::PointSystem::Get(e, POINT_SP) == 8,
        "duplicate lease is processed once");
    g_registry.destroy(e);
    Check(A::ProcessAffect(e) && A::ProcessAffect(entt::null), "stale/null expiry is harmless");

    e = Actor(); affect = InstallTimed(e, AFFECT_STR, 3, POINT_NONE, 0, 2);
    g_registry.get<TestPoints>(e).values[POINT_SP] = 10;
    onChange = [&](entt::entity owner, uint8_t type, int64_t) {
        if (type == POINT_SP) { onChange = {}; Check(!A::ProcessAffect(owner), "nested expiry sees live records but does no work"); }
    };
    Check(!A::ProcessAffect(e) && affect->lDuration == 2 && ecs::PointSystem::Get(e, POINT_SP) == 8,
        "recursive pass cannot double-charge SP or duration");
    e = Actor(); InstallTimed(e, AFFECT_STR, 1, POINT_NONE, 0, 2);
    g_registry.get<TestPoints>(e).values[POINT_SP] = 10;
    onChange = [&](entt::entity owner, uint8_t type, int64_t) {
        if (type == POINT_SP) { onChange = {}; Check(Add(owner, 42, AFFECT_STR, POINT_ST, 0, true, false, 30), "replacement during SP debit"); }
    };
    Check(!A::ProcessAffect(e) && A::FindAffect(e, AFFECT_STR)->lDuration == 30 &&
        ecs::PointSystem::Get(e, POINT_ST) == 42, "SP callback replacement is not expired by old pass");

    e = Actor(); InstallTimed(e, AFFECT_STR, 1, POINT_ST, 5);
    auto second = InstallTimed(e, AFFECT_DEX, 2, POINT_DX, 8);
    onChange = [&](entt::entity owner, uint8_t type, int64_t amount) {
        if (type == POINT_ST && amount < 0) {
            onChange = {};
            A::RemoveAffect(owner, second.get());
            InstallTimed(owner, AFFECT_DEX, 50, POINT_DX, 19);
        }
    };
    Check(!A::ProcessAffect(e) && A::FindAffect(e, AFFECT_DEX)->lDuration == 50,
        "replaced future batch entry waits until the next pass");
    for (int stage = 0; stage < 3; ++stage) {
        e = Actor(); InstallTimed(e, AFFECT_STR, stage == 1 ? 1 : 2, POINT_MOV_SPEED, 5, stage == 1 ? 0 : 2);
        g_registry.get<TestPoints>(e).values[POINT_SP] = 10;
        entt::entity replacement = entt::null;
        const auto destroy = [&] { g_registry.destroy(e); replacement = Actor(); };
        if (stage == 1) onUpdate = [&](entt::entity) { onUpdate = {}; destroy(); };
        else onChange = [&](entt::entity owner, uint8_t type, int64_t) {
            if (type != POINT_SP) return;
            onChange = {};
            if (stage == 0) destroy();
            else { g_registry.remove<ecs::AffectList>(owner); g_registry.emplace<ecs::AffectList>(owner); InstallTimed(owner, AFFECT_DEX, 88); }
        };
        Check(A::ProcessAffect(e) == (stage != 2), "invalidated pass stops but reports whether the current storage is empty");
        if (stage == 2) Check(A::FindAffect(e, AFFECT_DEX)->lDuration == 88, "replacement storage not ticked");
        else Check(replacement != e && A::Snapshot(replacement).empty(), "recycled entity untouched by expiry");
    }
    e = Actor(); affect = InstallTimed(e, AFFECT_STR, 3, POINT_NONE, 0, 1);
    g_registry.get<TestPoints>(e).values[POINT_SP] = 10;
    onChange = [&](entt::entity, uint8_t, int64_t) { throw std::runtime_error("expiry callback"); };
    bool threw = false;
    try { A::ProcessAffect(e); } catch (const std::runtime_error&) { threw = true; }
    onChange = {};
    Check(threw && g_registry.get<ecs::AffectList>(e).expiryToken == 0 && !A::ProcessAffect(e),
        "exception releases pass guard so a later tick can run");

    e = Actor(); InstallTimed(e, AFFECT_STR, 1);
    auto& points = g_registry.get<TestPoints>(e).values;
    points[POINT_HP] = 150; points[POINT_SP] = 110;
    Check(A::ProcessAffect(e) && points[POINT_HP] == 100 && points[POINT_SP] == 80, "expiry clamps HP/SP to maxima");
    e = Actor(); InstallTimed(e, AFFECT_STR, 1);
    g_registry.get<TestPoints>(e).values[POINT_HP] = 150;
    g_registry.get<TestPoints>(e).values[POINT_SP] = 110;
    onChange = [&](entt::entity owner, uint8_t point, int64_t) {
        if (point == POINT_HP) { onChange = {}; g_registry.destroy(owner); }
    };
    Check(A::ProcessAffect(e) && !g_registry.valid(e), "HP clamp destruction prevents a stale SP clamp");
    ResetPublication();
}
void DeadlineChecks() {
    ResetPublication(); nowSeconds = 1000;
    auto e = Actor();
    Check(A::GetBattlePassDeadline(e) == 0 && A::SetBattlePassDeadline(e, 1020) &&
        A::GetBattlePassRemainingSeconds(e) == 20, "battle pass deadline is entity-owned");
    auto affect = InstallTimed(e, AFFECT_BATTLE_PASS, 100, POINT_BATTLE_PASS_ID, 1);
    Check(!A::ProcessAffect(e) && affect->lDuration == 20, "battle pass uses absolute remaining time");
    nowSeconds = 1020;
    Check(A::ProcessAffect(e), "battle pass expires at the boundary tick");
    A::SetBattlePassDeadline(e, UINT32_MAX); nowSeconds = 0;
    Check(A::GetBattlePassRemainingSeconds(e) == INT32_MAX, "far-future timestamp clamps instead of wrapping negative");
    InstallTimed(e, AFFECT_BATTLE_PASS, 3);
    Check(!A::ProcessAffect(e) && A::FindAffect(e, AFFECT_BATTLE_PASS)->lDuration == INT32_MAX - 1,
        "remaining plus one cannot overflow duration");
    g_registry.remove<ecs::AffectList>(e); g_registry.emplace<ecs::AffectList>(e);
    Check(A::GetBattlePassDeadline(e) == UINT32_MAX, "clearing affects does not erase persisted deadline");
    g_registry.destroy(e);
    Check(!A::SetBattlePassDeadline(e, 12) && A::GetBattlePassDeadline(e) == 0 && !A::SetBattlePassDeadline(entt::null, 12),
        "stale deadline writes rejected");
    const auto nonCharacter = g_registry.create();
    Check(!A::SetBattlePassDeadline(nonCharacter, 12), "deadline cannot be attached to a non-character entity");
    nowSeconds = 1000; e = Actor(); A::SetBattlePassDeadline(e, 999);
    InstallTimed(e, AFFECT_BATTLE_PASS, 100);
    onCompute = [&](entt::entity owner) {
        onCompute = {};
        A::SetBattlePassDeadline(owner, 2000);
        InstallTimed(owner, AFFECT_BATTLE_PASS, 1000);
    };
    Check(!A::ProcessAffect(e) && A::GetBattlePassDeadline(e) == 2000,
        "expiry cannot clear a new deadline installed by removal callback");

    for (uint8_t premium : {uint8_t(0), uint8_t(PREMIUM_MAX_NUM - 1)}) {
        e = Actor(); premiumRemaining[premium] = 42;
        affect = InstallTimed(e, AFFECT_PREMIUM_START + premium, 2);
        Check(!A::ProcessAffect(e) && affect->lDuration == 42, "premium duration follows account remaining time");
        premiumRemaining[premium] = -1;
        Check(A::ProcessAffect(e), "expired account premium removed");
    }
    e = Actor(); premiumRemaining[0] = INT32_MAX;
    affect = InstallTimed(e, AFFECT_PREMIUM_START, 2);
    Check(!A::ProcessAffect(e) && affect->lDuration == INT32_MAX - 1, "premium duration refresh saturates");
    e = Actor(); InstallTimed(e, AFFECT_PREMIUM_START + PREMIUM_MAX_NUM, 1);
    Check(A::ProcessAffect(e), "reserved premium sentinel does not access account array or become immortal");
    ResetPublication();
}
void HairAndHorseChecks() {
    ResetPublication(); nowSeconds = 1000; hairDeadline = horseDeadline = 2000;
    auto e = Actor();
    auto hair = InstallTimed(e, AFFECT_HAIR, 5);
    auto horse = InstallTimed(e, AFFECT_HORSE_NAME, 5);
    Check(!A::ProcessAffect(e) && hair->lDuration == 5 && horse->lDuration == 5,
        "valid hair/horse quest deadlines offset ordinary duration ticking");
    hairDeadline = horseDeadline = 999;
    Check(A::ProcessAffect(e) && horseCalls == std::vector<bool>({false, true}) && hairResets > 0,
        "expired hair resets and horse name refreshes via entities");
    e = Actor(); InstallTimed(e, AFFECT_HAIR, 50); InstallTimed(e, AFFECT_HAIR, 60);
    Check(A::ProcessAffect(e), "expired hair removes every existing record of its type");
    for (uint32_t type : {AFFECT_HAIR, AFFECT_HORSE_NAME}) {
        e = Actor(); hairDeadline = horseDeadline = 2000;
        InstallTimed(e, type, INT32_MAX);
        Check(!A::ProcessAffect(e) && A::FindAffect(e, type)->lDuration == INT32_MAX - 1,
            "quest-based duration extension cannot overflow");
    }
    e = Actor(); hairDeadline = 999; InstallTimed(e, AFFECT_HAIR, 1);
    onPart = [&](entt::entity owner) {
        onPart = {}; Check(Add(owner, 0, AFFECT_HAIR, POINT_NONE, 0, true, false, 50), "replacement during hair appearance reset");
    };
    Check(!A::ProcessAffect(e) && A::FindAffect(e, AFFECT_HAIR)->lDuration == 49,
        "old hair expiry does not remove callback replacement");

    e = Actor(); horseDeadline = 999; InstallTimed(e, AFFECT_HORSE_NAME, 1);
    CHorseNameManager::instance().UpdateHorseName(1, "Original"); horseCalls.clear();
    onHorse = [&](entt::entity owner, bool summon) {
        if (summon) return;
        onHorse = {};
        A::RemoveAffect(owner, AFFECT_HORSE_NAME);
        InstallTimed(owner, AFFECT_HORSE_NAME, 50);
        CHorseNameManager::instance().UpdateHorseName(1, "Replacement");
    };
    CHorseNameManager::instance().Validate(e);
    Check(horseCalls == std::vector<bool>({false}) && std::string(CHorseNameManager::instance().GetHorseName(1)) == "Replacement",
        "old horse validation preserves callback replacement name");
    e = Actor(); InstallTimed(e, AFFECT_HORSE_NAME, 1); horseCalls.clear();
    onHorse = [&](entt::entity owner, bool) { CHorseNameManager::instance().Validate(owner); };
    CHorseNameManager::instance().Validate(e); onHorse = {};
    Check(horseCalls == std::vector<bool>({false, true}), "recursive horse validation runs once");
    for (int stage = 0; stage < 3; ++stage) {
        e = Actor(); InstallTimed(e, AFFECT_HORSE_NAME, 1); horseCalls.clear();
        if (stage == 0) onQuest = [&](entt::entity owner) { onQuest = {}; g_registry.destroy(owner); };
        if (stage == 1) onHorse = [&](entt::entity owner, bool) { onHorse = {}; g_registry.destroy(owner); };
        if (stage == 2) onCompute = [&](entt::entity owner) { onCompute = {}; g_registry.destroy(owner); };
        CHorseNameManager::instance().Validate(e);
        Check(!g_registry.valid(e) && horseCalls.size() <= 1, "horse validation stops after owner destruction");
    }
    ResetPublication();
}
void ExpiryPublicationChecks() {
    DESC descriptor; CLIENT_DESC database;
    client = &descriptor; db_clientdesc = &database;
    ResetPublication(); connected = Actor();
    Add(connected, 8, AFFECT_STR, POINT_ST, 0, true, false, 1);
    Check(A::ProcessAffect(connected) && storedAffects.empty() && clientRemoves.size() == 1,
        "natural expiry removes the client and DB record");
    for (int stage = 0; stage < 2; ++stage) {
        ResetPublication(); connected = Actor();
        Add(connected, 8, AFFECT_STR, POINT_ST, 0, true, false, 1);
        const auto replace = [&] { Check(Add(connected, 99, AFFECT_STR, POINT_ST, 0, true, false, 50), "replacement during expiry publication"); };
        if (stage == 0) onDBPacket = [&](uint8_t header) { if (header == HEADER_GD_REMOVE_AFFECT) { onDBPacket = {}; replace(); } };
        else onClientPacket = [&](uint8_t header) { if (header == HEADER_GC_AFFECT_REMOVE) { onClientPacket = {}; replace(); } };
        Check(!A::ProcessAffect(connected) && A::FindAffect(connected, AFFECT_STR)->lDuration == 50 &&
            storedAffects.at({AFFECT_STR, POINT_ST}).elem.lApplyValue == 99 && clientAdds.back().elem.lApplyValue == 99,
            "nested expiry replacement wins ECS, DB and client publication");
    }
    ResetPublication(); connected = Actor(); horseDeadline = 999;
    Add(connected, 0, AFFECT_HORSE_NAME, POINT_NONE);
    CHorseNameManager::instance().Validate(connected);
    Check(horsePackets.size() == 1 && horsePackets[0].dwPlayerID == 1 &&
        std::all_of(std::begin(horsePackets[0].szHorseName), std::end(horsePackets[0].szHorseName), [](char c) { return c == '\0'; }),
        "horse-name expiry persists empty name for the entity's player ID");
    ResetPublication(); connected = entt::null; client = nullptr; db_clientdesc = nullptr;
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
void NetworkSyncSystem::UpdatePacket(entt::entity e) {
    ++packets;
    if (onUpdate) { const auto callback = onUpdate; callback(e); }
}
CGuild* ecs::SocialSystem::GetGuild(entt::entity) { return nullptr; }
bool ecs::PlayerRuntime::IsPC(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ecs::TagPC>(e); }
LPDESC ecs::PlayerRuntime::GetDesc(entt::entity e) { return e == connected ? client : nullptr; }
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
void intrusive_ptr_add_ref(event* value) { ++value->ref_count; }
void intrusive_ptr_release(event* value) { if (--value->ref_count == 0) delete value; }
LPEVENT event_create_ex(TEVENTFUNC func, event_info_data* info, int delay) {
    Check(delay == passes_per_sec, "one-second affect timer");
    ++schedules;
    if (failSchedule) { delete info; return {}; }
    LPEVENT value(new event);
    value->func = func; value->info = info;
    scheduled.push_back(value);
    if (onSchedule) { const auto callback = onSchedule; callback(); }
    return value;
}
void event_cancel(LPEVENT* timer) {
    if (!timer || !*timer) return;
    if (!(*timer)->is_force_to_end) ++cancels;
    (*timer)->is_force_to_end = true;
    *timer = nullptr;
}
void ecs::ChatSystem::Send(entt::entity,unsigned char,char const *,...) { UnexpectedService(__func__); }
void ecs::ChatSystem::SendNew(entt::entity,unsigned char,unsigned int,char const *,...) { ++chats; }
int ecs::PointSystem::GetLevel(entt::entity) { UnexpectedService(__func__); }
void ecs::PointSystem::ApplyPoint(entt::entity,unsigned char,int) { UnexpectedService(__func__); }
void ecs::PlayerRuntime::SetPart(entt::entity e, unsigned char part, unsigned short value) {
    Check(part == PART_HAIR && value == 0, "expiry only resets hair appearance");
    ++hairResets;
    if (onPart) { const auto callback = onPart; callback(e); }
}
unsigned char ecs::PlayerRuntime::GetMobRank(entt::entity) { UnexpectedService(__func__); }
int ecs::QuestSystem::GetFlag(entt::entity e, std::string_view flag) {
    if (onQuest) { const auto callback = onQuest; callback(e); }
    if (flag == "hair.limit_time") return hairDeadline;
    if (flag == "horse_name.valid_till") return horseDeadline;
    UnexpectedService("unexpected quest flag");
}
int ecs::PlayerRuntime::GetPremiumRemainSeconds(entt::entity, uint8_t type) {
    Check(type < PREMIUM_MAX_NUM, "premium index must be within the account array");
    return premiumRemaining[type];
}
int64_t ecs::PlayerRuntime::GetMaxStamina(entt::entity) { UnexpectedService(__func__); }
void NetworkSyncSystem::BroadcastSyncPacket(entt::registry&, entt::entity e) {
    ++syncs;
    if (onSync) { const auto callback = onSync; callback(e); }
}
void ecs::MovementSystem::SyncDestinationClear(entt::entity e) {
    g_registry.remove<ecs::MovementDestination>(e);
    if (auto* movement = g_registry.try_get<ecs::MovementState>(e)) {
        movement->moveStartTime = 0; movement->moveDuration = 0;
    }
}
void ecs::PlayerRuntime::SetPosition(entt::entity e, int position) {
    g_registry.get<ecs::CharacterRuntimeFlagsComponent>(e).position = position;
}
bool CombatSystem::Damage(entt::entity,entt::entity,int,unsigned char) { UnexpectedService(__func__); }
void CombatSystem::SetComboSequence(entt::entity,unsigned char) { UnexpectedService(__func__); }
void CombatSystem::SetValidComboInterval(entt::entity,int) { UnexpectedService(__func__); }
unsigned int CGuild::UnderAnyWar(unsigned char) { UnexpectedService(__func__); }
void CGuild::GiveGuildBuff(entt::entity) { UnexpectedService(__func__); }
int CEntity::GetX(void)const { UnexpectedService(__func__); }
int CEntity::GetY(void)const { UnexpectedService(__func__); }
int64_t get_global_time(void) { return nowSeconds; }
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
void DESC::Packet(const void* data, int size) {
    const auto header = *static_cast<const uint8_t*>(data);
    if (header == HEADER_GC_AFFECT_ADD) {
        Check(size == sizeof(TPacketGCAffectAdd), "client add packet size");
        clientAdds.push_back(*static_cast<const TPacketGCAffectAdd*>(data));
    } else if (header == HEADER_GC_AFFECT_REMOVE) {
        Check(size == sizeof(TPacketGCAffectRemove), "client remove packet size");
        clientRemoves.push_back(*static_cast<const TPacketGCAffectRemove*>(data));
    } else UnexpectedService("unexpected client packet");
    if (onClientPacket) { const auto callback = onClientPacket; callback(header); }
}
void CLIENT_DESC::DBPacket(uint8_t header, uint32_t, const void* data, uint32_t size) {
    dbHeaders.push_back(header);
    if (header == HEADER_GD_ADD_AFFECT) {
        Check(size == sizeof(TPacketGDAddAffect), "DB add packet size");
        const auto value = *static_cast<const TPacketGDAddAffect*>(data);
        storedAffects[{value.elem.dwType, value.elem.bApplyOn}] = value;
    } else if (header == HEADER_GD_REMOVE_AFFECT) {
        Check(size == sizeof(TPacketGDRemoveAffect), "DB remove packet size");
        const auto value = *static_cast<const TPacketGDRemoveAffect*>(data);
        storedAffects.erase({value.dwType, value.bApplyOn});
    } else if (header == HEADER_GD_UPDATE_HORSE_NAME) {
        Check(size == sizeof(TPacketUpdateHorseName), "horse-name DB packet size");
        horsePackets.push_back(*static_cast<const TPacketUpdateHorseName*>(data));
    } else UnexpectedService("unexpected DB packet");
    if (onDBPacket) { const auto callback = onDBPacket; callback(header); }
}
// Real descriptor objects with inert transport/processor construction, not
// fabricated pointers. Only the packet sinks above are exercised.
DESC::DESC() {}
DESC::~DESC() {}
void DESC::Destroy() { UnexpectedService(__func__); }
void DESC::SetPhase(int) { UnexpectedService(__func__); }
CLIENT_DESC::CLIENT_DESC() {}
CLIENT_DESC::~CLIENT_DESC() {}
void CLIENT_DESC::Destroy() { UnexpectedService(__func__); }
void CLIENT_DESC::SetPhase(int) { UnexpectedService(__func__); }
CInputProcessor::CInputProcessor() {}
bool CInputProcessor::Process(DESC*, const void*, int, int&) { UnexpectedService(__func__); }
void CInputProcessor::Handshake(DESC*, const char*) { UnexpectedService(__func__); }
CInputHandshake::CInputHandshake() {}
CInputHandshake::~CInputHandshake() {}
int CInputHandshake::Analyze(DESC*, uint8_t, const char*) { UnexpectedService(__func__); }
int CInputLogin::Analyze(DESC*, uint8_t, const char*) { UnexpectedService(__func__); }
int CInputMain::Analyze(DESC*, uint8_t, const char*) { UnexpectedService(__func__); }
int CInputDead::Analyze(DESC*, uint8_t, const char*) { UnexpectedService(__func__); }
int CInputDB::Analyze(DESC*, uint8_t, const char*) { UnexpectedService(__func__); }
bool CInputDB::Process(DESC*, const void*, int, int&) { UnexpectedService(__func__); }
CInputP2P::CInputP2P() {}
CInputAuth::CInputAuth() {}
int CInputP2P::Analyze(DESC*, uint8_t, const char*) { UnexpectedService(__func__); }
int CInputAuth::Analyze(DESC*, uint8_t, const char*) { UnexpectedService(__func__); }
CPacketInfo::CPacketInfo() : m_pCurrentPacket(nullptr), m_dwStartTime(0) {}
CPacketInfo::~CPacketInfo() {}
CPacketInfoCG::CPacketInfoCG() {}
CPacketInfoGG::CPacketInfoGG() {}
CPacketInfoCG::~CPacketInfoCG() {}
CPacketInfoGG::~CPacketInfoGG() {}
Cipher::Cipher() : activated_(false), encoder_(nullptr), decoder_(nullptr), key_agreement_(nullptr) {}
Cipher::~Cipher() {}
void battle_end(entt::entity) { UnexpectedService(__func__); }
void MountSystem::SummonHorse(entt::entity e, bool summon, bool fromFar, uint32_t vnum, const char* name) {
    Check(g_registry.valid(e) && fromFar && vnum == 0 && name == nullptr, "horse-name refresh uses the live owner entity");
    horseCalls.push_back(summon);
    if (onHorse) { const auto callback = onHorse; callback(e, summon); }
}
int quest::CQuestManager::GetEventFlag(std::string const &) { UnexpectedService(__func__); }
entt::entity ItemSystem::FindItemByID(entt::entity,unsigned int) { UnexpectedService(__func__); }
entt::entity ItemSystem::GetWearItem(entt::entity,unsigned char) { UnexpectedService(__func__); }
bool ItemSystem::IsValidItem(entt::entity) { UnexpectedService(__func__); }
bool ItemSystem::LockItem(entt::entity,bool) { UnexpectedService(__func__); }

int main() {
    try {
        CHorseNameManager horseNames;
        StorageChecks(); FlagAndPointChecks(); RefreshChecks(); RemovalChecks();
        AddChecks(); TimerChecks(); PublicationChecks(); LifetimeStressChecks();
        ExpiryChecks(); DeadlineChecks(); HairAndHorseChecks(); ExpiryPublicationChecks();
        std::cout << "Affect checks passed: " << checks << '\n'; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
