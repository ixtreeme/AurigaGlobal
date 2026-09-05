#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/MountSystem.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/constants.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/components/pet_mount_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include <iostream>
#include <stdexcept>

// Compile the complete production MountSystem.cpp. Only external services are
// doubled; no CHARACTER/CItem is created or attached to the entity fixtures.
entt::registry g_registry;
entt::dispatcher g_dispatcher;
int passes_per_sec = 25;
const TApplyInfo aApplyInfo[MAX_APPLY_NUM] = {};

namespace {
struct CharacterState {
    int32_t x = 1000, y = 1000, z = 0, map = 1;
    float rotation = 0;
    uint32_t vid = 0, mount = 0;
    uint8_t empire = 1;
    bool dead = false, war = false;
    entt::entity wear = entt::null, skin = entt::null;
    std::string name = "owner";
};
struct ItemState {
    entt::entity owner = entt::null;
    uint32_t id = 0, vid = 0, vnum = 71100;
    bool unlimited = true, hasProto = true;
    TItemTable proto {};
    int64_t sockets[ITEM_SOCKET_MAX_NUM] {};
};
int checks = 0, destroyed = 0, spawned = 0, shown = 0, moves = 0, packets = 0;
int affects = 0, cancels = 0, lastDuration = 0, liveEvents = 0;
uint32_t tick = 1000, nextVID = 1, spawnedVnum = 0;
bool failSpawn = false, failShow = false;
LPEVENT scheduled;
CAffect bonus {};
std::set<entt::entity> bonusOwners;

void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
CharacterState& State(entt::entity e)
{
    Check(g_registry.valid(e) && g_registry.all_of<CharacterState>(e), "service received stale/non-character entity");
    return g_registry.get<CharacterState>(e);
}
entt::entity Character()
{
    const auto e = g_registry.create();
    g_registry.emplace<CharacterState>(e).vid = nextVID++;
    return e;
}
entt::entity Item(entt::entity owner)
{
    const auto e = g_registry.create();
    auto& item = g_registry.emplace<ItemState>(e);
    item.owner = owner;
    item.id = item.vid = nextVID++;
    State(owner).wear = e;
    return e;
}
void Reset()
{
    scheduled.reset();
    Check(liveEvents == 0, "event leaked");
    g_registry.clear();
    bonusOwners.clear();
    destroyed = spawned = shown = moves = packets = affects = cancels = 0;
    failSpawn = failShow = false;
}
}

CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
entt::entity CHARACTER_MANAGER::SpawnMobEntity(uint32_t vnum, int32_t map, int32_t x, int32_t y,
    int32_t z, bool, int rotation, bool)
{
    ++spawned;
    spawnedVnum = vnum;
    if (failSpawn)
        return entt::null;
    const auto e = Character();
    auto& state = State(e);
    state.map = map; state.x = x; state.y = y; state.z = z; state.rotation = rotation;
    return e;
}
void intrusive_ptr_add_ref(EVENT* e) { ++e->ref_count; }
void intrusive_ptr_release(EVENT* e) { if (--e->ref_count == 0) { --liveEvents; delete e; } }
LPEVENT event_create_ex(TEVENTFUNC func, event_info_data* info, int32_t)
{
    LPEVENT result(new EVENT);
    ++liveEvents;
    result->func = func; result->info = info;
    scheduled = result;
    return result;
}
void event_cancel(LPEVENT* event)
{
    if (*event) { ++cancels; (*event)->is_force_to_end = true; }
    event->reset();
}
uint32_t get_dword_time() { return tick; }
int number_ex(int from, int, const char*, int) { return from; }
float GetDegreeFromPositionXY(int32_t, int32_t, int32_t, int32_t) { return 0.f; }
void GetDeltaByDegree(float, float distance, float* x, float* y) { *x = distance; *y = 0.f; }

namespace ecs::PlayerRuntime {
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
uint32_t GetPacketVID(entt::entity e) { return State(e).vid; }
std::string_view GetName(entt::entity e) { return State(e).name; }
int32_t GetX(entt::entity e) { return State(e).x; }
int32_t GetY(entt::entity e) { return State(e).y; }
int32_t GetZ(entt::entity e) { return State(e).z; }
int32_t GetMapIndex(entt::entity e) { return State(e).map; }
float GetRotation(entt::entity e) { return State(e).rotation; }
uint8_t GetEmpire(entt::entity e) { return State(e).empire; }
void SetEmpire(entt::entity e, uint8_t empire) { State(e).empire = empire; }
const TMobTable* GetMobTable(entt::entity e) { State(e); static TMobTable table{}; return &table; }
void DestroyCharacter(entt::entity e) { State(e); ++destroyed; g_registry.destroy(e); }
}
namespace ItemSystem {
bool IsValidItem(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ItemState>(e); }
entt::entity GetItemOwner(entt::entity e) { return IsValidItem(e) ? g_registry.get<ItemState>(e).owner : entt::null; }
uint32_t GetItemID(entt::entity e) { return IsValidItem(e) ? g_registry.get<ItemState>(e).id : 0; }
uint32_t GetItemVID(entt::entity e) { return IsValidItem(e) ? g_registry.get<ItemState>(e).vid : 0; }
uint32_t GetItemVnum(entt::entity e) { return IsValidItem(e) ? g_registry.get<ItemState>(e).vnum : 0; }
const TItemTable* GetItemProto(entt::entity e) { const auto& item = g_registry.get<ItemState>(e); return item.hasProto ? &item.proto : nullptr; }
bool IsUnlimitedTimeUnique(entt::entity e) { return g_registry.get<ItemState>(e).unlimited; }
uint32_t GetItemSocket(entt::entity e, int i) { return static_cast<uint32_t>(g_registry.get<ItemState>(e).sockets[i]); }
bool SetItemSocket(entt::entity e, int i, uint32_t value, bool) { g_registry.get<ItemState>(e).sockets[i] = value; return true; }
int32_t GetItemValue(entt::entity e, uint32_t i) { return g_registry.get<ItemState>(e).proto.alValues[i]; }
entt::entity GetWearItem(entt::entity owner, uint8_t slot) {
    return slot == WEAR_COSTUME_MOUNT ? State(owner).wear : State(owner).skin;
}
}
namespace MountSystem {
bool IsHorseRiding(entt::entity e) { State(e); return false; }
bool StopRiding(entt::entity) { throw std::runtime_error("unexpected horse riding"); }
entt::entity GetSummonedHorse(entt::entity e) { State(e); return entt::null; }
void SummonHorse(entt::entity, bool, bool, uint32_t, const char*) { throw std::runtime_error("unexpected horse summon"); }
uint32_t GetMountVnum(entt::entity e) { return State(e).mount; }
void SetMountVnum(entt::entity e, uint32_t vnum) { State(e).mount = vnum; }
}
namespace CombatSystem {
bool IsDead(entt::entity e) { return State(e).dead; }
void SetLastAttacked(entt::entity e, uint32_t) { State(e); }
}
namespace ecs::MovementSystem {
bool Show(entt::entity e, int32_t map, int32_t x, int32_t y, int32_t z, bool) {
    ++shown;
    auto& s = State(e);
    if (failShow) return false;
    s.map = map; s.x = x; s.y = y; s.z = z;
    return true;
}
void Stop(entt::entity e) { State(e); }
void SendMovePacket(entt::entity e, uint8_t, uint8_t, uint32_t, uint32_t, uint32_t, uint32_t, float) { State(e); ++packets; }
bool Goto(entt::entity e, int32_t, int32_t) { State(e); ++moves; return true; }
void SetRotation(entt::entity e, float r
#ifdef ENABLE_ANCIENT_PYRAMID
    , bool
#endif
) { State(e).rotation = r; }
void SyncWalkingWrite(entt::entity e, bool) { State(e); }
}
CAffect* AffectSystem::FindAffect(entt::entity e, uint32_t, uint8_t) { return bonusOwners.contains(e) ? &bonus : nullptr; }
bool AffectSystem::AddAffect(entt::entity e, uint32_t type, uint8_t, int32_t value, uint32_t,
    int32_t duration, int32_t, bool, bool)
{
    State(e); ++affects; lastDuration = duration;
    if (type == AFFECT_MOUNT) State(e).mount = value;
    if (type == AFFECT_MOUNT_BONUS) bonusOwners.insert(e);
    return true;
}
bool AffectSystem::RemoveAffect(entt::entity e, uint32_t type) {
    State(e);
    if (type == AFFECT_MOUNT_BONUS) bonusOwners.erase(e);
    if (type == AFFECT_MOUNT) State(e).mount = 0;
    return true;
}
void ecs::PointSystem::Change(entt::entity e, uint8_t, int64_t, bool, bool
#ifdef __ENABLE_BLOCK_EXP__
    , bool
#endif
) { State(e); }
CWarMap* ecs::SocialSystem::GetWarMap(entt::entity e) {
    static int marker;
    return State(e).war ? reinterpret_cast<CWarMap*>(&marker) : nullptr;
}
#ifdef TEXTS_IMPROVEMENT
void ecs::ChatSystem::SendNew(entt::entity e, uint8_t, uint32_t, const char*, ...) { State(e); }
#endif

namespace {
void Lifecycle()
{
    Reset();
    const auto owner = Character(), item = Item(owner);
    CMountSystem system(owner);
    system.SetUpdatePeriod(0);
    system.Summon(20110, item, false);
    auto* actor = system.GetByVnum(20110);
    Check(actor && actor->IsSummoned() && actor->GetOwner() == owner, "native summon failed");
    const auto mount = actor->GetCharacter();
    Check(!g_registry.any_of<ecs::LegacyCharPtr>(mount), "mount fixture gained a legacy pointer");
    Check(g_registry.get<ecs::StatusFlags>(mount).isMount, "mount marker missing");
    Check(g_registry.get<ecs::PlayerName>(mount).value == "owner's Mount", "mount name missing");
    Check(actor->GetSummonItem() == item && system.CountSummoned() == 1, "summon item not retained");
    Check(system.GetByVID(actor->GetVID()) == actor && !system.GetByVID(0), "VID lookup mismatch");
    State(mount).x += 1000;
    Check(system.Update(0) && moves == 1, "follow did not run");
    State(mount).map = 2;
    Check(system.Update(0) && State(mount).map == State(owner).map, "cross-map follower not returned");
    const auto timer = scheduled;
    Check(timer->func(timer, 0) != 0, "native owner timer did not run");
    system.Unsummon(20110);
    Check(!g_registry.valid(mount) && destroyed == 1 && !actor->IsSummoned(), "unsummon did not destroy once");
    Check(g_registry.get<ecs::MountComponent>(owner).itemVID == 0, "summon state was not cleared");
    system.Unsummon(20110);
    system.Destroy();
    Check(destroyed == 1 && !g_registry.get<ecs::MountRuntimeRefs>(owner).mountSystem, "repeated destruction not idempotent");
    Check(timer->func(timer, 0) == 0, "cancelled timer still called subsystem");
}

void StaleHandles()
{
    for (int which = 0; which < 4; ++which)
    {
        Reset();
        const auto owner = Character(), item = Item(owner);
        CMountSystem system(owner);
        system.SetUpdatePeriod(0);
        system.Summon(20110, item, false);
        auto* actor = system.GetByVnum(20110);
        const auto mount = actor->GetCharacter();
        const auto old = which == 0 ? owner : which == 1 ? item : mount;
        if (which == 3)
            g_registry.get<ItemState>(item).owner = Character();
        else
            g_registry.destroy(old);
        const auto replacement = Character();
        Check(which == 3 || (entt::to_entity(old) == entt::to_entity(replacement) && old != replacement),
            "fixture did not recycle entity with a new generation");
        Check(system.Update(0) && !actor->IsSummoned(), "stale handle was not cleaned");
        Check(g_registry.valid(replacement), "cleanup destroyed replacement entity");
        Check(actor->GetSummonItem() == entt::null, "stale summon item retained");
        system.Destroy();
        Check(g_registry.valid(replacement), "destructor destroyed replacement entity");
    }
}

void FailureAndMounting()
{
    Reset();
    const auto owner = Character(), item = Item(owner), other = Character();
    CMountSystem system(owner);
    system.Summon(20110, entt::null, false);
    g_registry.get<ItemState>(item).owner = other;
    system.Summon(20110, item, false);
    Check(spawned == 0 && !scheduled, "invalid summon entered factory");
    g_registry.get<ItemState>(item).owner = owner;
    failSpawn = true;
    system.Summon(20110, item, false);
    Check(!scheduled && system.CountSummoned() == 0, "failed spawn started timer");
    failSpawn = false; failShow = true;
    system.Summon(20110, item, false);
    Check(destroyed == 1 && !scheduled && system.CountSummoned() == 0, "failed show leaked character/timer");
    failShow = false;
    system.Summon(20110, item, false);
    auto* actor = system.GetByVnum(20110);
    const auto mount = actor->GetCharacter();
    auto& material = g_registry.get<ItemState>(item);
    material.unlimited = false; material.sockets[0] = time(nullptr) - 1;
    system.Mount(20110, item);
    Check(actor->GetCharacter() == mount && affects == 0 && material.sockets[2] == 0, "expired item changed state");
    material.unlimited = true; material.hasProto = false;
    system.Mount(20110, item);
    Check(actor->GetCharacter() == mount && affects == 0, "missing proto removed follower");
    material.hasProto = true; material.proto.aApplies[0].bType = MAX_APPLY_NUM;
    system.Mount(20110, item);
    Check(affects == 0, "invalid apply type accepted");
    material.proto.aApplies[0].bType = 0;
    State(owner).war = true;
    system.Mount(20110, item);
    Check(actor->GetCharacter() == mount && affects == 0, "war restriction failed");
    State(owner).war = false;
    system.Mount(20110, item);
    Check(!actor->IsSummoned() && State(owner).mount == 20110 && material.sockets[2] == 1, "mount did not complete");
    Check(lastDuration == 86400, "unlimited duration changed");
    system.Unmount(20110);
    Check(actor->IsSummoned() && State(owner).mount == 0 && material.sockets[2] == 0, "unmount did not restore follower");
    const auto skin = Item(owner);
    State(owner).wear = item; State(owner).skin = skin;
    g_registry.get<ItemState>(skin).proto.alValues[0] = 20200;
    system.UpdateMountSkin();
    Check(spawnedVnum == 20200 && actor->GetSummonItem() == item, "skin update lost item entity");
    system.Mount(20110, item);
    Check(State(owner).mount == 20200 && material.sockets[2] == 1, "skin riding return state wrong");
    system.Destroy();
    Check(State(owner).mount == 0 && !bonusOwners.contains(owner), "riding teardown left mount affects");
}

void ReplacedRuntimeAndActor()
{
    Reset();
    const auto owner = Character(), item = Item(owner);
    CMountSystem oldSystem(owner);
    oldSystem.Summon(20110, item, false);
    const auto oldEvent = scheduled;
    oldSystem.Destroy();
    CMountSystem newSystem(owner);
    newSystem.Summon(20110, item, false);
    Check(oldEvent->func(oldEvent, 0) == 0, "old timer entered replacement system");
    Check(scheduled->func(scheduled, 0) != 0, "replacement timer rejected");
    oldSystem.Destroy();
    Check(g_registry.get<ecs::MountRuntimeRefs>(owner).mountSystem == &newSystem,
        "old system cleared replacement runtime reference");
    newSystem.Destroy();

    CMountActor oldActor(owner, 20110), newActor(owner, 20111);
    Check(oldActor.Mount(item), "first actor failed to mount");
    Check(newActor.Mount(item), "second actor failed to mount");
    oldActor.Unsummon();
    Check(State(owner).mount == 20111 && bonusOwners.contains(owner),
        "old actor teardown removed another actor's mount");
    newActor.Unsummon();
    Check(State(owner).mount == 0, "current riding actor did not clean up");
}

void NullOwnerAndReusedItemState()
{
    Reset();
    CMountActor orphan(entt::null, 20110);
    orphan.SetName();
    orphan.Unmount();
    Check(!orphan.Mount(entt::null) && orphan.Summon(entt::null, false) == 0,
        "null-owner actor entered services");
    orphan.Unsummon();
    const auto owner = Character(), item = Item(owner);
    CMountActor actor(owner, 20110);
    Check(actor.Summon(item, false) != 0, "fixture summon failed");
    const auto oldVID = ItemSystem::GetItemVID(item);
    g_registry.destroy(item);
    const auto replacement = Item(owner);
    g_registry.get<ItemState>(replacement).vid = oldVID;
    auto& state = g_registry.get<ecs::MountComponent>(owner);
    state.item = replacement;
    actor.Unsummon();
    Check(state.item == replacement && state.itemVID == oldVID,
        "stale actor cleared a replacement item with the same VID");
}
}

int main()
{
    try {
        CHARACTER_MANAGER factory;
        Lifecycle();
        StaleHandles();
        FailureAndMounting();
        ReplacedRuntimeAndActor();
        NullOwnerAndReusedItemState();
        Reset();
        std::cout << "Mount lifecycle checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
