#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/MountSystem.h"
#include "../../SRC/Server/GameServer/PetSystem.h"
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
#include <functional>

// Compile the complete production MountSystem.cpp/PetSystem.cpp. External services are
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
    bool dead = false, war = false, dungeon = false, hasMob = true, walking = false;
    int64_t petBonus = 0;
    entt::entity wear = entt::null, skin = entt::null;
    std::string name = "owner";
};
struct ItemState {
    entt::entity owner = entt::null;
    uint32_t id = 0, vid = 0, vnum = 71100;
    bool unlimited = true, hasProto = true, locked = false;
    int attributeBonus = 0;
    TItemTable proto {};
    int64_t sockets[ITEM_SOCKET_MAX_NUM] {};
};
int checks = 0, destroyed = 0, spawned = 0, shown = 0, moves = 0, packets = 0;
int affects = 0, cancels = 0, lastDuration = 0, liveEvents = 0;
uint32_t tick = 1000, nextVID = 1, spawnedVnum = 0;
bool failSpawn = false, failShow = false;
int computes = 0, modifications = 0, mobQueries = 0, clearCalls = 0, lastClearValue = 0;
std::function<void()> onCompute;
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
    computes = modifications = mobQueries = clearCalls = lastClearValue = 0;
    onCompute = {};
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
const TMobTable* GetMobTable(entt::entity e) { ++mobQueries; static TMobTable table{}; return State(e).hasMob ? &table : nullptr; }
void DestroyCharacter(entt::entity e) { State(e); ++destroyed; g_registry.destroy(e); }
CPetSystem* GetPetSystem(entt::entity e) {
    if (!IsValid(e)) return nullptr;
    const auto* refs = g_registry.try_get<ecs::PetRuntimeRefs>(e);
    return refs ? refs->petSystem : nullptr;
}
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
bool LockItem(entt::entity e, bool locked) { Check(IsValidItem(e), "locking stale item"); g_registry.get<ItemState>(e).locked = locked; return true; }
bool UnlockItem(entt::entity e) { return LockItem(e, false); }
void ModifyPoints(entt::entity e, bool add) {
    Check(IsValidItem(e) && add, "pet buff used invalid item/removal path");
    const auto& item = g_registry.get<ItemState>(e);
    ++modifications;
    for (const auto& apply : item.proto.aApplies)
        if (apply.bType != APPLY_NONE) State(item.owner).petBonus += apply.lValue;
    State(item.owner).petBonus += item.attributeBonus;
}
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
void SetNowWalking(entt::entity e, bool walking) { State(e).walking = walking; }
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
void ecs::PointSystem::Compute(entt::entity e) {
    State(e).petBonus = 0;
    ++computes;
    if (onCompute) onCompute();
    if (auto* pets = ecs::PlayerRuntime::GetPetSystem(e)) pets->RefreshBuff();
}
void ecs::PointSystem::ApplyPoint(entt::entity e, uint8_t, int value) {
    State(e).petBonus += value;
    ++clearCalls;
    lastClearValue = value;
}
CWarMap* ecs::SocialSystem::GetWarMap(entt::entity e) {
    static int marker;
    return State(e).war ? reinterpret_cast<CWarMap*>(&marker) : nullptr;
}
LPDUNGEON ecs::SocialSystem::GetDungeon(entt::entity e) {
    static int marker;
    return State(e).dungeon ? reinterpret_cast<LPDUNGEON>(&marker) : nullptr;
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

entt::entity PetItem(entt::entity owner, int value = 10)
{
    const auto item = Item(owner);
    auto& material = g_registry.get<ItemState>(item);
    material.proto.aApplies[0] = { APPLY_MOV_SPEED, value };
    material.attributeBonus = 7;
    return item;
}

void PetLifecycle()
{
    Reset();
    const auto owner = Character(), item = PetItem(owner);
    CPetSystem system(owner);
    system.SetUpdatePeriod(0);
    auto* actor = system.Summon(34001, item, "", false);
    Check(actor && actor->GetOwner() == owner && actor->IsSummoned(), "pet native summon failed");
    const auto pet = actor->GetCharacter();
    Check(!g_registry.any_of<ecs::LegacyCharPtr>(pet), "pet fixture acquired legacy character");
    Check(g_registry.get<ecs::StatusFlags>(pet).isPet, "pet marker missing");
    Check(g_registry.get<ecs::PlayerName>(pet).value == "owner's Pet", "pet name missing");
    Check(State(owner).petBonus == 17 && modifications == 1, "pet bonuses not rebuilt on summon");
    Check(g_registry.get<ItemState>(item).locked && ItemSystem::GetItemSocket(item, 2) == 1, "pet item not locked");
    Check(g_registry.get<ecs::PetComponent>(owner).item == item
        && g_registry.get<ecs::PetComponent>(owner).sockets[2] == 1, "pet component not synchronized");
    Check(system.GetByVID(actor->GetVID()) == actor && !system.GetByVID(0), "pet VID lookup failed");
    const auto timer = scheduled;
    State(pet).x = State(owner).x + 500;
    Check(system.Update(0) && State(pet).walking && moves == 1, "near pet did not walk");
    State(pet).x += 1000;
    Check(system.Update(0) && !State(pet).walking && moves == 2, "far pet did not run");
    State(pet).map = 2;
    Check(system.Update(0) && State(pet).map == State(owner).map, "pet cross-map return failed");
    State(owner).dead = true;
    Check(system.Update(0) && actor->IsSummoned(), "owner death changed pet survival rule");
    Check(timer->func(timer, 0) != 0, "pet owner-entity timer rejected");
    onCompute = [&] { Check(!actor->IsSummoned() && actor->GetSummonItem() == entt::null,
        "unsummon reentered with still-published handles"); };
    system.Unsummon(34001);
    onCompute = {};
    Check(!g_registry.valid(pet) && State(owner).petBonus == 0 && clearCalls == 1, "pet cleanup lost bonuses/character");
    Check(!g_registry.get<ItemState>(item).locked && ItemSystem::GetItemSocket(item, 2) == 0, "pet item remained locked");
    Check(g_registry.get<ecs::PetComponent>(owner).item == entt::null, "pet component not cleared");
    const auto computed = computes;
    system.Unsummon(34001);
    system.Destroy();
    Check(computes == computed && destroyed == 1, "pet teardown repeated side effects");
    Check(timer->func(timer, 0) == 0 && !ecs::PlayerRuntime::GetPetSystem(owner), "pet timer/runtime outlived system");
}

void PetMultipleAndReentrantDeletion()
{
    Reset();
    const auto owner = Character(), first = PetItem(owner), second = PetItem(owner, 20);
    CPetSystem system(owner);
    system.SetUpdatePeriod(0);
    auto* a = system.Summon(34001, first, "", false);
    auto* b = system.Summon(34002, second, "", false);
    Check(a && b && State(owner).petBonus == 44, "multiple-pet bonuses incorrect");
    State(a->GetCharacter()).hasMob = State(b->GetCharacter()).hasMob = false;
    mobQueries = 0;
    Check(!system.Update(0) && mobQueries == 2, "failed pet AI skipped another pet");
    onCompute = [&] { Check(!system.GetByVnum(34001), "deleting actor still visible during RefreshBuff"); };
    system.DeletePet(a);
    onCompute = {};
    Check(system.CountSummoned() == 1 && State(owner).petBonus == 27, "single deletion removed remaining bonus");
    Check(!g_registry.get<ItemState>(first).locked && g_registry.get<ItemState>(second).locked, "single deletion unlocked wrong item");
    a = system.Summon(34001, first, "", false);
    Check(a && system.CountSummoned() == 2, "pet resummon failed");
    system.UnsummonAll();
    Check(system.CountSummoned() == 0 && State(owner).petBonus == 0, "UnsummonAll left a pet/bonus behind");
    Check(!g_registry.get<ItemState>(first).locked && !g_registry.get<ItemState>(second).locked, "UnsummonAll left item locks");
    Check(system.GetByVnum(34001) && system.GetByVnum(34002), "UnsummonAll unexpectedly deleted actors");
    Check(scheduled->func(scheduled, 0) == 0, "UnsummonAll left active timer");
    a = system.Summon(34001, first, "", false);
    b = system.Summon(34002, second, "", false);
    int callbacks = 0;
    onCompute = [&] {
        ++callbacks;
        Check(system.CountSummoned() == static_cast<size_t>(2 - callbacks), "destructor published a dying actor");
        Check(!system.Summon(34003, first, "", false), "summon reentered Destroy");
        system.Destroy(); // Recursive teardown is harmless.
    };
    system.Destroy();
    onCompute = {};
    Check(callbacks == 2 && State(owner).petBonus == 0 && system.CountSummoned() == 0,
        "multi-actor Destroy failed during reentrant point calculation");
    Check(!system.GetByVnum(34001) && !system.GetByVnum(34002), "Destroy retained actors");
}

void PetStaleHandles()
{
    for (int which = 0; which < 4; ++which)
    {
        Reset();
        const auto owner = Character(), item = PetItem(owner);
        CPetSystem system(owner);
        system.SetUpdatePeriod(0);
        auto* actor = system.Summon(34001, item, "", false);
        const auto timer = scheduled;
        const auto pet = actor->GetCharacter();
        const auto old = which == 0 ? owner : which == 1 ? item : pet;
        if (which == 3)
            g_registry.get<ItemState>(item).owner = Character();
        else
            g_registry.destroy(old);
        const auto replacement = Character();
        Check(which == 3 || (entt::to_entity(old) == entt::to_entity(replacement) && old != replacement),
            "pet fixture did not recycle an entity generation");
        if (which == 0)
            Check(timer->func(timer, 0) == 0, "stale-owner timer reached recycled character");
        Check(system.Update(0) && !actor->IsSummoned() && actor->GetSummonItem() == entt::null,
            "stale pet owner/item/follower not cleaned");
        Check(g_registry.valid(replacement), "pet cleanup destroyed recycled entity");
        if (which != 0)
            Check(State(owner).petBonus == 0, "stale pet retained owner bonus");
        if (which == 3)
        {
            const auto newOwner = ItemSystem::GetItemOwner(item);
            Check(State(newOwner).petBonus == 0 && g_registry.get<ItemState>(item).locked,
                "pet cleanup modified transferred item's new owner or lock");
        }
        system.Destroy();
        Check(g_registry.valid(replacement), "pet destructor destroyed recycled entity");
    }
}

void PetFailuresSkinsAndBonuses()
{
    Reset();
    const auto owner = Character(), item = PetItem(owner), other = Character();
    CPetSystem system(owner);
    system.SetUpdatePeriod(0);
    Check(!system.Summon(34004, entt::null, "", false), "null pet item accepted");
    g_registry.get<ItemState>(item).owner = other;
    Check(!system.Summon(34004, item, "", false) && spawned == 0, "foreign pet item spawned");
    g_registry.get<ItemState>(item).owner = owner;
    g_registry.get<ItemState>(item).hasProto = false;
    Check(!system.Summon(34004, item, "", false) && spawned == 0, "pet missing proto accepted");
    g_registry.get<ItemState>(item).hasProto = true;
    failSpawn = true;
    Check(!system.Summon(34004, item, "", false) && !scheduled, "failed pet spawn started timer");
    failSpawn = false; failShow = true;
    Check(!system.Summon(34004, item, "", false) && destroyed == 1 && !scheduled,
        "failed pet show leaked follower or timer");
    Check(!g_registry.get<ItemState>(item).locked && State(owner).petBonus == 0, "failed pet show applied side effects");
    failShow = false;
    const auto skin = Item(owner);
    State(owner).skin = skin;
    g_registry.get<ItemState>(skin).proto.alValues[0] = 34500;
    auto* actor = system.Summon(34004, item, "", false);
    Check(actor && actor->GetVnum() == 34004 && spawnedVnum == 34500, "skin replaced pet's stable identity");
    Check(State(owner).petBonus == 0, "skin bypassed dungeon-only pet bonus restriction");
    State(owner).dungeon = true;
    ecs::PointSystem::Compute(owner);
    Check(State(owner).petBonus == 17, "dungeon pet bonus missing");
    const auto second = PetItem(owner);
    const auto pet = actor->GetCharacter();
    Check(!system.Summon(34004, second, "", false) && actor->GetSummonItem() == item,
        "active pet rebound to a different item");
    Check(!system.Summon(34001, item, "", false), "one item became owned by two pet actors");
    Check(system.Summon(34004, item, "", true) == actor && actor->GetCharacter() == pet,
        "repeat summon duplicated active follower");
    State(owner).skin = entt::null;
    system.UpdatePetSkin();
    Check(actor->GetVnum() == 34004 && spawnedVnum == 34004 && actor->GetSummonItem() == item,
        "removing pet skin lost base identity/item");
    State(owner).dungeon = false;
    const auto beforeClear = clearCalls;
    system.Unsummon(actor, true);
    Check(!system.GetByVnum(34004) && clearCalls == beforeClear + 1 && lastClearValue == -10,
        "dungeon exit prevented removal of previously applied bonus");
    Check(State(owner).petBonus == 0, "pet attributes survived removal");
    auto& material = g_registry.get<ItemState>(item);
    material.proto.aApplies[0].bType = MAX_APPLY_NUM;
    actor = system.Summon(34001, item, "", false);
    Check(actor && State(owner).petBonus == 0, "malformed pet apply entered point system");
    system.Unsummon(34001);
    material.proto.aApplies[0] = { APPLY_MOV_SPEED, std::numeric_limits<int>::min() };
    actor = system.Summon(34001, item, "", false);
    Check(actor && State(owner).petBonus == 0, "unnegatable pet bonus accepted");
    system.Unsummon(34001);
    material.proto.aApplies[0] = { APPLY_SKILL, 0x00800020 };
    system.Summon(34001, item, "", false);
    system.Unsummon(34001);
    Check(lastClearValue == 0x20, "skill bonus removal did not toggle the add bit");
}

void PetReplacementTimerAndItem()
{
    Reset();
    const auto owner = Character(), item = PetItem(owner);
    CPetSystem oldSystem(owner);
    oldSystem.Summon(34001, item, "", false);
    const auto oldTimer = scheduled;
    oldSystem.Destroy();
    CPetSystem newSystem(owner);
    auto* actor = newSystem.Summon(34001, item, "", false);
    Check(oldTimer->func(oldTimer, 0) == 0, "old pet timer entered replacement subsystem");
    Check(scheduled->func(scheduled, 0) != 0, "new pet timer rejected");
    oldSystem.Destroy();
    Check(ecs::PlayerRuntime::GetPetSystem(owner) == &newSystem, "old pet system cleared new runtime");
    const auto oldVID = ItemSystem::GetItemVID(item);
    g_registry.destroy(item);
    const auto replacement = PetItem(owner);
    Check(item != replacement && entt::to_entity(item) == entt::to_entity(replacement), "item generation not recycled");
    g_registry.get<ItemState>(replacement).vid = oldVID;
    g_registry.get<ItemState>(replacement).locked = true;
    g_registry.get<ItemState>(replacement).sockets[2] = 1;
    auto& state = g_registry.get<ecs::PetComponent>(owner);
    state.item = replacement;
    actor->Unsummon();
    Check(g_registry.get<ItemState>(replacement).locked && ItemSystem::GetItemSocket(replacement, 2) == 1,
        "stale pet unlocked another item with same VID");
    Check(state.item == replacement && state.itemVID == oldVID, "pet cleared replacement ECS state");
    newSystem.Destroy();
}

void PetNullOwnerAndMounting()
{
    Reset();
    CPetActor orphan(entt::null, 34001);
    orphan.SetName("");
    orphan.Unmount();
    orphan.ClearBuff();
    orphan.GiveBuff();
    orphan.UpdatePetSkin();
    Check(!orphan.Mount() && !orphan.Summon("", entt::null), "null-owner pet entered services");
    orphan.Unsummon();
    const auto owner = Character();
    CPetActor notMountable(owner, 34001);
    Check(!notMountable.Mount(), "non-mountable pet mounted");
    CPetActor first(owner, 34001, CPetActor::EPetOption_Mountable);
    CPetActor second(owner, 34002, CPetActor::EPetOption_Mountable);
    Check(first.Mount() && second.Mount(), "entity pet mounting failed");
    first.Unsummon();
    Check(State(owner).mount == 34002, "old pet actor unmounted replacement");
    second.Unsummon();
    Check(State(owner).mount == 0, "pet mount survived actor teardown");
}

void PetDeathOptionsAndSkinFailure()
{
    Reset();
    const auto owner = Character(), item = PetItem(owner);
    CPetSystem system(owner);
    system.SetUpdatePeriod(1000);
    auto* actor = system.Summon(34001, item, "", false, CPetActor::EPetOption_Summonable);
    Check(actor && system.Update(0) && mobQueries == 0, "non-followable pet ran follow AI");
    State(actor->GetCharacter()).dead = true;
    Check(system.Update(0) && actor->IsSummoned(), "update period was ignored");
    tick += 1000;
    Check(system.Update(0) && !actor->IsSummoned() && State(owner).petBonus == 0,
        "dead pet was not detached or retained its bonuses");
    Check(!g_registry.get<ItemState>(item).locked, "dead pet left summon item locked");
    Check(system.Summon(34001, item, "", false) == actor, "dead actor could not resummon");
    failShow = true;
    system.UpdatePetSkin();
    Check(!actor->IsSummoned() && !g_registry.get<ItemState>(item).locked && State(owner).petBonus == 0,
        "failed skin respawn leaked follower/item lock/bonus");
    Check(scheduled->func(scheduled, 0) == 0, "failed skin respawn retained timer");
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
        const auto mountChecks = checks;
        PetLifecycle();
        PetMultipleAndReentrantDeletion();
        PetStaleHandles();
        PetFailuresSkinsAndBonuses();
        PetReplacementTimerAndItem();
        PetNullOwnerAndMounting();
        PetDeathOptionsAndSkinFailure();
        Reset();
        std::cout << "Mount/pet lifecycle checks passed: " << checks
            << " (mount: " << mountChecks << ", pet: " << checks - mountChecks << ")\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
