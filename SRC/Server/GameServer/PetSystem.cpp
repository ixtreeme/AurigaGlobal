#include "stdafx.h"
#include "PetSystem.h"
#include "char_manager.h"
#include "config.h"
#include "utils.h"
#include "vector.h"
#include "packet.h"
#include "ecs/Registry.hpp"
#include "ecs/components/identity_components.hpp"
#include "ecs/components/pet_mount_components.hpp"
#include "ecs/components/status_components.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

EVENTINFO(petsystem_event_info)
{
    entt::entity owner { entt::null };
};

EVENTFUNC(petsystem_update_event);

namespace
{
using PetRecord = ecs::PetActorState;
using PetRuntime = ecs::PetRuntime;

bool IsOwnedSummonItem(entt::entity owner, entt::entity item)
{
    return ecs::PlayerRuntime::IsValid(owner) && ItemSystem::IsValidItem(item)
        && ItemSystem::GetItemOwner(item) == owner;
}

uint32_t PetSkin(entt::entity owner)
{
#ifdef ENABLE_COSTUME_PET
    const auto skin = ItemSystem::GetWearItem(owner, WEAR_COSTUME_PET_SKIN);
    if (ItemSystem::IsValidItem(skin))
        return ItemSystem::GetItemValue(skin, 0);
#endif
    return 0;
}

bool SnapFollowerToOwner(entt::entity follower, entt::entity owner, int32_t x, int32_t y, int32_t z)
{
    if (!ecs::PlayerRuntime::IsValid(follower) || !ecs::PlayerRuntime::IsValid(owner))
        return false;
    if (!ecs::MovementSystem::Show(follower, ecs::PlayerRuntime::GetMapIndex(owner), x, y, z))
        return false;
    ecs::MovementSystem::Stop(follower);
    ecs::MovementSystem::SendMovePacket(follower, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

PetRuntime* Runtime(entt::entity owner)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return nullptr;
    return &g_registry.get_or_emplace<PetRuntime>(owner);
}

PetRecord* FindLiveRecord(PetRuntime& runtime, uint32_t vnum)
{
    if (vnum == 0)
        return nullptr;
    for (auto& record : runtime.actors)
        if (record.vnum == vnum)
            return &record;
    return nullptr;
}

PetRecord* FreeRecordSlot(PetRuntime& runtime)
{
    for (auto& record : runtime.actors)
        if (record.vnum == 0)
            return &record;
    return nullptr;
}

void UpdatePetComponent(entt::entity owner, entt::entity item, bool summoned)
{
    if (!ecs::PlayerRuntime::IsValid(owner))
        return;
    auto& state = g_registry.get_or_emplace<ecs::PetComponent>(owner);
    if (!ItemSystem::IsValidItem(item))
    {
        state = {};
        return;
    }
    state.owner = owner;
    state.item = item;
    state.itemID = ItemSystem::GetItemID(item);
    state.itemVID = ItemSystem::GetItemVID(item);
    state.itemVnum = ItemSystem::GetItemVnum(item);
    state.level = 0;
    state.state = summoned ? 1u : 0u;
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        state.sockets[i] = static_cast<int32_t>(ItemSystem::GetItemSocket(item, i));
}

void ClearBuff(entt::entity owner, PetRecord& actor)
{
    const auto applies = std::exchange(actor.buffApplies, {});
    if (!ecs::PlayerRuntime::IsValid(owner))
        return;
    for (const auto& apply : applies)
        if (apply.bType != APPLY_NONE)
            ecs::PointSystem::ApplyPoint(owner, apply.bType,
                apply.bType == APPLY_SKILL ? apply.lValue ^ 0x00800000 : -apply.lValue);
}

bool CanGiveBuff(entt::entity owner, const PetRecord& actor)
{
    return PetSystem::IsSummoned(actor) && IsOwnedSummonItem(owner, actor.summonItem)
        && ((actor.vnum != 34004 && actor.vnum != 34009) || ecs::SocialSystem::GetDungeon(owner));
}

void GiveBuff(entt::entity owner, PetRecord& actor)
{
    actor.buffApplies = {};
    if (!CanGiveBuff(owner, actor))
        return;
    const auto* proto = ItemSystem::GetItemProto(actor.summonItem);
    if (!proto)
        return;
    for (const auto& apply : proto->aApplies)
        if (apply.bType >= MAX_APPLY_NUM || apply.lValue == std::numeric_limits<int>::min())
            return;
    std::copy(std::begin(proto->aApplies), std::end(proto->aApplies), actor.buffApplies.begin());
    ItemSystem::ModifyPoints(actor.summonItem, true);
}

bool Follow(entt::entity owner, PetRecord& actor, float minDistance)
{
    if (!ecs::PlayerRuntime::IsValid(owner) || !PetSystem::IsSummoned(actor))
        return false;
    const auto ownerX = ecs::PlayerRuntime::GetX(owner);
    const auto ownerY = ecs::PlayerRuntime::GetY(owner);
    const auto charX = ecs::PlayerRuntime::GetX(actor.character);
    const auto charY = ecs::PlayerRuntime::GetY(actor.character);
    const float distance = DISTANCE_SQRT(ownerX - charX, ownerY - charY);
    if (distance <= minDistance)
        return false;
    ecs::MovementSystem::SetRotation(actor.character, GetDegreeFromPositionXY(charX, charY, ownerX, ownerY));
    float dx, dy;
    GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(actor.character), distance - minDistance, &dx, &dy);
    if (!ecs::MovementSystem::Goto(actor.character, static_cast<int>(charX + dx + 0.5f), static_cast<int>(charY + dy + 0.5f)))
        return false;
    ecs::MovementSystem::SendMovePacket(actor.character, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

bool UpdateFollowAI(entt::entity owner, PetRecord& actor)
{
    if (!PetSystem::IsSummoned(actor) || !ecs::PlayerRuntime::IsValid(owner)
        || !ecs::PlayerRuntime::GetMobTable(actor.character))
        return false;
    const auto ownerX = ecs::PlayerRuntime::GetX(owner);
    const auto ownerY = ecs::PlayerRuntime::GetY(owner);
    const auto charX = ecs::PlayerRuntime::GetX(actor.character);
    const auto charY = ecs::PlayerRuntime::GetY(actor.character);
    const float distance = DISTANCE_APPROX(charX - ownerX, charY - ownerY);
    constexpr int approach = 200;
    if (distance >= 4500.f || ecs::PlayerRuntime::GetMapIndex(actor.character) != ecs::PlayerRuntime::GetMapIndex(owner))
    {
        const float rotation = ecs::PlayerRuntime::GetRotation(owner) * 3.141592f / 180.f;
        return SnapFollowerToOwner(actor.character, owner, ownerX - approach * cos(rotation),
            ownerY - approach * sin(rotation), ecs::PlayerRuntime::GetZ(owner));
    }
    if (distance >= 300.f)
    {
        ecs::MovementSystem::SetNowWalking(actor.character, distance < 900.f);
        Follow(owner, actor, approach);
        CombatSystem::SetLastAttacked(actor.character, get_dword_time());
    }
    else
        ecs::MovementSystem::SendMovePacket(actor.character, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

bool UpdateActor(entt::entity owner, PetRecord& actor)
{
    // Preserve pets across owner death; only follower death ends the summon.
    if (!IsOwnedSummonItem(owner, actor.summonItem) || !PetSystem::IsSummoned(actor)
        || CombatSystem::IsDead(actor.character))
    {
        PetSystem::Unsummon(owner, actor.vnum);
        return true;
    }
    return !PetSystem::HasOption(actor, PetSystem::EPetOption_Followable)
        || UpdateFollowAI(owner, actor);
}

void UnmountActor(entt::entity owner, PetRecord& actor)
{
    const auto ridingVnum = std::exchange(actor.ridingVnum, 0u);
    if (!ecs::PlayerRuntime::IsValid(owner))
        return;
    if (ridingVnum && MountSystem::GetMountVnum(owner) == ridingVnum)
        MountSystem::SetMountVnum(owner, 0);
    if (MountSystem::IsHorseRiding(owner))
        MountSystem::StopRiding(owner);
}

void TeardownActor(entt::entity owner, PetRecord& actor, bool keepRecord)
{
    if (actor.ridingVnum)
        UnmountActor(owner, actor);
    const auto character = actor.character;
    const auto item = actor.summonItem;
    const bool hadSummon = character != entt::null || item != entt::null;
    actor.character = entt::null;
    actor.summonItem = entt::null;
    // Clear the owner-side snapshot only when it still names this summon item;
    // a callback may already have installed a replacement.
    if (const auto* state = g_registry.try_get<ecs::PetComponent>(owner);
        state && item != entt::null && state->item == item)
        UpdatePetComponent(owner, entt::null, false);
    if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemOwner(item) == owner)
    {
        ItemSystem::SetItemSocket(item, 2, 0);
        ItemSystem::UnlockItem(item);
    }
    ClearBuff(owner, actor);
    if (!keepRecord)
        actor.vnum = 0;
    if (hadSummon && ecs::PlayerRuntime::IsValid(owner))
        ecs::PointSystem::Compute(owner);
    if (ecs::PlayerRuntime::IsValid(character))
        ecs::PlayerRuntime::DestroyCharacter(character);
}
} // namespace

EVENTFUNC(petsystem_update_event)
{
    const auto* info = dynamic_cast<petsystem_event_info*>(event->info);
    if (!info || !ecs::PlayerRuntime::IsValid(info->owner))
        return 0;
    const auto owner = info->owner;
    const auto* runtime = g_registry.try_get<ecs::PetRuntime>(owner);
    // A cancelled callback cannot enter a replacement runtime on the same owner.
    if (!runtime || runtime->updateEvent != event)
        return 0;
    PetSystem::Update(owner, 0);
    return PASSES_PER_SEC(1) / 4;
}

namespace PetSystem {

bool HasOption(const ecs::PetActorState& actor, uint32_t option)
{
    return (actor.options & option) != 0;
}

bool IsSummoned(const ecs::PetActorState& actor)
{
    return ecs::PlayerRuntime::IsValid(actor.character);
}

ecs::PetActorState* FindActor(entt::entity owner, uint32_t vnum)
{
    if (owner == entt::null || !g_registry.valid(owner) || vnum == 0)
        return nullptr;
    auto* runtime = g_registry.try_get<ecs::PetRuntime>(owner);
    if (!runtime)
        return nullptr;
    for (auto& record : runtime->actors)
        if (record.vnum == vnum)
            return &record;
    return nullptr;
}

ecs::PetActorState* FindActorByVID(entt::entity owner, uint32_t vid)
{
    if (owner == entt::null || !g_registry.valid(owner) || vid == 0)
        return nullptr;
    auto* runtime = g_registry.try_get<ecs::PetRuntime>(owner);
    if (!runtime)
        return nullptr;
    for (auto& record : runtime->actors)
        if (IsSummoned(record) && ecs::PlayerRuntime::GetPacketVID(record.character) == vid)
            return &record;
    return nullptr;
}

bool IsPetSummoned(entt::entity owner, uint32_t vnum)
{
    const auto* actor = FindActor(owner, vnum);
    return actor && IsSummoned(*actor);
}

size_t CountSummoned(entt::entity owner)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return 0;
    const auto* runtime = g_registry.try_get<ecs::PetRuntime>(owner);
    if (!runtime)
        return 0;
    return std::count_if(runtime->actors.begin(), runtime->actors.end(),
        [](const auto& record) { return g_registry.valid(record.character); });
}

ecs::PetActorState* Summon(entt::entity owner, uint32_t vnum, entt::entity item,
    const char* petName, bool spawnFar, uint32_t options)
{
    if (!IsOwnedSummonItem(owner, item) || !ItemSystem::GetItemProto(item))
        return nullptr;
    auto* runtime = Runtime(owner);
    if (!runtime || runtime->destroying)
        return nullptr;
    for (const auto& record : runtime->actors)
        if (record.vnum != vnum && record.vnum != 0 && record.summonItem == item)
            return nullptr;

    auto* actor = FindLiveRecord(*runtime, vnum);
    if (!actor)
    {
        actor = FreeRecordSlot(*runtime);
        if (!actor)
        {
            runtime->actors.push_back({});
            actor = &runtime->actors.back();
        }
        *actor = {};
        actor->vnum = vnum;
        actor->options = options;
    }

    int32_t x = ecs::PlayerRuntime::GetX(owner);
    int32_t y = ecs::PlayerRuntime::GetY(owner);
    const auto z = ecs::PlayerRuntime::GetZ(owner);
    x += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);
    y += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);

    if (IsSummoned(*actor))
    {
        // Do not silently adopt a different item and leave the old one locked.
        if (item != actor->summonItem || !SnapFollowerToOwner(actor->character, owner, x, y, z))
            return nullptr;
        return actor;
    }

    TeardownActor(owner, *actor, true);
    runtime = Runtime(owner);
    actor = runtime ? FindLiveRecord(*runtime, vnum) : nullptr;
    if (!actor)
        return nullptr;
    actor->character = CHARACTER_MANAGER::instance().SpawnMobEntity(
        PetSkin(owner) ? PetSkin(owner) : vnum,
        ecs::PlayerRuntime::GetMapIndex(owner), x, y, z, false,
        static_cast<int>(ecs::PlayerRuntime::GetRotation(owner) + 180), false);
    if (!IsSummoned(*actor))
        return nullptr;
    g_registry.get_or_emplace<ecs::StatusFlags>(actor->character).isPet = true;
    ecs::PlayerRuntime::SetEmpire(actor->character, ecs::PlayerRuntime::GetEmpire(owner));
    g_registry.emplace_or_replace<ecs::PlayerName>(actor->character,
        std::string(ecs::PlayerRuntime::GetName(owner)) + "'s Pet");
    if (!ecs::MovementSystem::Show(actor->character, ecs::PlayerRuntime::GetMapIndex(owner), x, y, z))
    {
        TeardownActor(owner, *actor, true);
        return nullptr;
    }
    ItemSystem::SetItemSocket(item, 2, 1);
    ItemSystem::LockItem(item);
    actor->summonItem = item;
    UpdatePetComponent(owner, item, true);
    ecs::PointSystem::Compute(owner);
    runtime = Runtime(owner);
    actor = runtime ? FindLiveRecord(*runtime, vnum) : nullptr;
    if (!actor)
        return nullptr;
#ifdef ENABLE_RECALL
    AffectSystem::RemoveAffect(owner, AFFECT_RECALL1);
    AffectSystem::AddAffect(owner, AFFECT_RECALL1, APPLY_NONE, 0,
        ItemSystem::GetItemID(item), INFINITE_AFFECT_DURATION, 0, true, false);
#endif
    if (!runtime->updateEvent && runtime->updateEvent == nullptr)
    {
        auto* info = AllocEventInfo<petsystem_event_info>();
        info->owner = owner;
        runtime->updateEvent = event_create(petsystem_update_event, info, PASSES_PER_SEC(1) / 4);
    }
    (void)petName;
    return actor;
}

void Unsummon(entt::entity owner, uint32_t vnum, bool deleteFromList)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::PetRuntime>(owner);
    if (!runtime)
        return;
    auto* actor = FindLiveRecord(*runtime, vnum);
    if (!actor)
        return;
    TeardownActor(owner, *actor, !deleteFromList);
    runtime = g_registry.try_get<ecs::PetRuntime>(owner);
    if (runtime && CountSummoned(owner) == 0)
        event_cancel(&runtime->updateEvent);
}

void UnsummonAll(entt::entity owner)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::PetRuntime>(owner);
    if (!runtime)
        return;
    event_cancel(&runtime->updateEvent);
    for (auto& actor : runtime->actors)
        if (actor.vnum != 0)
            TeardownActor(owner, actor, true);
}

void DeleteActor(entt::entity owner, uint32_t vnum)
{
    Unsummon(owner, vnum, true);
}

void DestroyRuntime(entt::entity owner)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::PetRuntime>(owner);
    if (!runtime || runtime->destroying)
        return;
    runtime->destroying = true;
    event_cancel(&runtime->updateEvent);
    // Teardown runs with destroying set: ComputePoints -> RefreshBuff must see
    // only living actors, never one whose teardown is in progress.
    for (auto& actor : runtime->actors)
        if (actor.vnum != 0)
            TeardownActor(owner, actor, true);
    runtime->actors.clear();
    if (g_registry.valid(owner))
        g_registry.remove<ecs::PetRuntime>(owner);
}

bool Update(entt::entity owner, uint32_t deltaTime)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::PetRuntime>(owner);
    if (!runtime)
        return true;
    const uint32_t now = get_dword_time();
    if (runtime->updatePeriod > now - runtime->lastUpdateTime)
        return true;
    bool result = true;
    std::vector<uint32_t> vnums;
    vnums.reserve(runtime->actors.size());
    for (const auto& actor : runtime->actors)
        if (actor.character != entt::null || actor.summonItem != entt::null)
            vnums.push_back(actor.vnum);
    for (const uint32_t vnum : vnums)
    {
        auto* current = g_registry.try_get<ecs::PetRuntime>(owner);
        auto* actor = current ? FindLiveRecord(*current, vnum) : nullptr;
        if (!actor)
            continue;
        result = UpdateActor(owner, *actor) && result;
    }
    auto* current = g_registry.try_get<ecs::PetRuntime>(owner);
    if (current)
    {
        current->lastUpdateTime = now;
        if (CountSummoned(owner) == 0)
            event_cancel(&current->updateEvent);
    }
    return result;
}

void SetUpdatePeriod(entt::entity owner, uint32_t ms)
{
    if (auto* runtime = Runtime(owner))
        runtime->updatePeriod = ms;
}

void RefreshBuff(entt::entity owner)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::PetRuntime>(owner);
    if (!runtime || runtime->destroying)
        return;
    std::vector<std::pair<uint32_t, entt::entity>> actors;
    for (const auto& actor : runtime->actors)
        actors.push_back({actor.vnum, actor.summonItem});
    for (const auto& [vnum, item] : actors)
    {
        auto* current = g_registry.try_get<ecs::PetRuntime>(owner);
        if (!current || current->destroying)
            return;
        auto* actor = FindLiveRecord(*current, vnum);
        if (!actor || !IsSummoned(*actor) || actor->summonItem != item)
            continue;
        GiveBuff(owner, *actor);
    }
}

bool Mount(entt::entity owner, uint32_t vnum)
{
    auto* actor = FindActor(owner, vnum);
    if (!actor || !ecs::PlayerRuntime::IsValid(owner) ||
        !HasOption(*actor, EPetOption_Mountable))
        return false;
    const auto skin = PetSkin(owner);
    actor->ridingVnum = skin ? skin : actor->vnum;
    MountSystem::SetMountVnum(owner, actor->ridingVnum);
    return MountSystem::GetMountVnum(owner) == actor->ridingVnum;
}

void Unmount(entt::entity owner, uint32_t vnum)
{
    auto* actor = FindActor(owner, vnum);
    if (!actor)
        return;
    UnmountActor(owner, *actor);
}

void UpdatePetSkin(entt::entity owner)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::PetRuntime>(owner);
    if (!runtime)
        return;
    std::vector<std::pair<uint32_t, entt::entity>> pending;
    for (const auto& actor : runtime->actors)
        if (actor.vnum != 0 && IsSummoned(actor) && IsOwnedSummonItem(owner, actor.summonItem))
            pending.push_back({actor.vnum, actor.summonItem});
    for (const auto& [vnum, item] : pending)
    {
        Unsummon(owner, vnum);
        Summon(owner, vnum, item, "", false);
    }
    if (auto* current = g_registry.try_get<ecs::PetRuntime>(owner);
        current && CountSummoned(owner) == 0)
        event_cancel(&current->updateEvent);
}

} // namespace PetSystem
