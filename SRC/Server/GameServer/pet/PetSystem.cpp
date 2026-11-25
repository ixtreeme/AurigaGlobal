#include "stdafx.h"
#include "PetSystem.h"
#include "char_manager.h"
#include "config.h"
#include "utils.h"
#include "vector.h"
#include "packet.h"
#include "../ecs/Registry.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/components/pet_mount_components.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include <algorithm>
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
    if (!actor.typedSeal || !PetSystem::IsSummoned(actor)
        || CombatSystem::IsDead(actor.character))
    {
        PetSystem::Unsummon(owner, actor.vnum);
        return true;
    }
    return !PetSystem::HasOption(actor, PetSystem::EPetOption_Followable)
        || UpdateFollowAI(owner, actor);
}

void TeardownActor(PetRecord& actor, bool keepRecord)
{
    const auto character = std::exchange(actor.character, entt::null);
    actor.typedSeal = 0;
    if (!keepRecord)
        actor.vnum = 0;
    if (ecs::PlayerRuntime::IsValid(character))
        ecs::PlayerRuntime::DestroyCharacter(character);
}

void EnsureFollowEvent(entt::entity owner, PetRuntime& runtime)
{
    if (runtime.updateEvent)
        return;
    auto* info = AllocEventInfo<petsystem_event_info>();
    info->owner = owner;
    runtime.updateEvent = event_create(petsystem_update_event, info, PASSES_PER_SEC(1) / 4);
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
    return actor && !actor->typedSeal && IsSummoned(*actor);
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

bool SummonTyped(entt::entity owner, uint32_t vnum, uint64_t seal, bool mount, std::string_view name)
{
    if (!seal || !vnum || !ecs::PlayerRuntime::IsValid(owner) || CountSummoned(owner))
        return false;
    auto* runtime = Runtime(owner);
    if (!runtime || runtime->destroying)
        return false;
    auto* actor = FindLiveRecord(*runtime, vnum);
    if (!actor) actor = FreeRecordSlot(*runtime);
    if (!actor)
    {
        runtime->actors.emplace_back();
        actor = &runtime->actors.back();
    }
    TeardownActor(*actor, false);
    const auto map = ecs::PlayerRuntime::GetMapIndex(owner);
    const auto x = ecs::PlayerRuntime::GetX(owner) + number(-100, 100);
    const auto y = ecs::PlayerRuntime::GetY(owner) + number(-100, 100);
    const auto z = ecs::PlayerRuntime::GetZ(owner);
    const auto rotation = static_cast<int>(ecs::PlayerRuntime::GetRotation(owner) + 180);
    const auto follower = CHARACTER_MANAGER::instance().SpawnMobEntity(vnum, map, x, y, z, false, rotation, false);
    if (!g_registry.valid(follower))
        return false;
    *actor = {};
    actor->character = follower;
    actor->typedSeal = seal;
    actor->vnum = vnum;
    actor->options = DefaultPetOptions;
    auto& flags = g_registry.get_or_emplace<ecs::StatusFlags>(follower);
    flags.isPet = !mount;
    flags.isMount = mount;
    if (mount)
        g_registry.emplace_or_replace<ecs::MountOwner>(follower, owner);
    if (!name.empty())
        ecs::PlayerRuntime::SetName(follower, name);
    ecs::PlayerRuntime::SetEmpire(follower, ecs::PlayerRuntime::GetEmpire(owner));
    if (!ecs::MovementSystem::Show(follower, map, x, y, z))
    {
        TeardownActor(*actor, false);
        return false;
    }
    EnsureFollowEvent(owner, *runtime);
    return true;
}

uint64_t LiveTypedSeal(entt::entity owner)
{
    const auto* runtime = g_registry.valid(owner) ? g_registry.try_get<PetRuntime>(owner) : nullptr;
    if (runtime)
        for (const auto& actor : runtime->actors)
            if (actor.typedSeal && IsSummoned(actor))
                return actor.typedSeal;
    return 0;
}

void RecallTyped(entt::entity owner, uint64_t seal)
{
    const auto* runtime = g_registry.valid(owner) ? g_registry.try_get<PetRuntime>(owner) : nullptr;
    if (runtime && seal)
        for (const auto& actor : runtime->actors)
            if (actor.typedSeal == seal)
            {
                Unsummon(owner, actor.vnum, true);
                return;
            }
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
    TeardownActor(*actor, !deleteFromList);
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
            TeardownActor(actor, true);
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
            TeardownActor(actor, true);
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
        if (actor.character != entt::null)
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

} // namespace PetSystem
