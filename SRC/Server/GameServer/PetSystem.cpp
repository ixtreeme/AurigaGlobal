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
#include <utility>
#include <limits>

namespace
{
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
}

EVENTINFO(petsystem_event_info)
{
    entt::entity owner { entt::null };
};

EVENTFUNC(petsystem_update_event)
{
    const auto* info = dynamic_cast<petsystem_event_info*>(event->info);
    if (!info || !ecs::PlayerRuntime::IsValid(info->owner))
        return 0;
    auto* system = ecs::PlayerRuntime::GetPetSystem(info->owner);
    // A cancelled callback cannot enter a replacement system on the same owner.
    if (!system || !system->IsUpdateEvent(event))
        return 0;
    system->Update(0);
    return PASSES_PER_SEC(1) / 4;
}

CPetActor::CPetActor(entt::entity owner, uint32_t vnum, uint32_t options)
    : m_owner(owner), m_vnum(vnum), m_options(options)
{
}

CPetActor::~CPetActor()
{
    Unsummon();
}

bool CPetActor::IsSummoned() const
{
    return ecs::PlayerRuntime::IsValid(m_character);
}

void CPetActor::SetName(const char*)
{
    // Preserve the existing owner-based naming rule.
    if (IsSummoned() && ecs::PlayerRuntime::IsValid(m_owner))
        g_registry.emplace_or_replace<ecs::PlayerName>(m_character,
            std::string(ecs::PlayerRuntime::GetName(m_owner)) + "'s Pet");
}

bool CPetActor::Mount()
{
    if (!ecs::PlayerRuntime::IsValid(m_owner) || !HasOption(EPetOption_Mountable))
        return false;
    const auto skin = PetSkin(m_owner);
    m_ridingVnum = skin ? skin : m_vnum;
    MountSystem::SetMountVnum(m_owner, m_ridingVnum);
    return MountSystem::GetMountVnum(m_owner) == m_ridingVnum;
}

void CPetActor::Unmount()
{
    const auto ridingVnum = std::exchange(m_ridingVnum, 0u);
    if (!ecs::PlayerRuntime::IsValid(m_owner))
        return;
    if (ridingVnum && MountSystem::GetMountVnum(m_owner) == ridingVnum)
        MountSystem::SetMountVnum(m_owner, 0);
    if (MountSystem::IsHorseRiding(m_owner))
        MountSystem::StopRiding(m_owner);
}

void CPetActor::Unsummon()
{
    if (m_ridingVnum)
        Unmount();
    // Publish the detached state before any point/character callbacks.
    const auto character = std::exchange(m_character, entt::null);
    const auto item = m_summonItem;
    const bool hadSummon = character != entt::null || item != entt::null;
    m_vid = 0;
    SetSummonItem(entt::null);
    if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemOwner(item) == m_owner)
    {
        ItemSystem::SetItemSocket(item, 2, 0);
        ItemSystem::UnlockItem(item);
    }
    ClearBuff();
    if (hadSummon && ecs::PlayerRuntime::IsValid(m_owner))
        ecs::PointSystem::Compute(m_owner);
    if (ecs::PlayerRuntime::IsValid(character))
        ecs::PlayerRuntime::DestroyCharacter(character);
}

uint32_t CPetActor::Summon(const char* petName, entt::entity item, bool spawnFar)
{
    if (!IsOwnedSummonItem(m_owner, item) || !ItemSystem::GetItemProto(item))
        return 0;
    int32_t x = ecs::PlayerRuntime::GetX(m_owner);
    int32_t y = ecs::PlayerRuntime::GetY(m_owner);
    const auto z = ecs::PlayerRuntime::GetZ(m_owner);
    x += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);
    y += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);
    if (IsSummoned())
    {
        // Do not silently adopt a different item and leave the old one locked.
        if (item != m_summonItem || !SnapFollowerToOwner(m_character, m_owner, x, y, z))
            return 0;
        return m_vid;
    }
    Unsummon();
    const auto skin = PetSkin(m_owner);
    m_character = CHARACTER_MANAGER::instance().SpawnMobEntity(skin ? skin : m_vnum,
        ecs::PlayerRuntime::GetMapIndex(m_owner), x, y, z, false,
        static_cast<int>(ecs::PlayerRuntime::GetRotation(m_owner) + 180), false);
    if (!IsSummoned())
        return 0;
    g_registry.get_or_emplace<ecs::StatusFlags>(m_character).isPet = true;
    ecs::PlayerRuntime::SetEmpire(m_character, ecs::PlayerRuntime::GetEmpire(m_owner));
    m_vid = ecs::PlayerRuntime::GetPacketVID(m_character);
    SetName(petName);
    if (!ecs::MovementSystem::Show(m_character, ecs::PlayerRuntime::GetMapIndex(m_owner), x, y, z))
    {
        Unsummon();
        return 0;
    }
    ItemSystem::SetItemSocket(item, 2, 1);
    ItemSystem::LockItem(item);
    SetSummonItem(item);
    ecs::PointSystem::Compute(m_owner);
#ifdef ENABLE_RECALL
    AffectSystem::RemoveAffect(m_owner, AFFECT_RECALL1);
    AffectSystem::AddAffect(m_owner, AFFECT_RECALL1, APPLY_NONE, 0,
        ItemSystem::GetItemID(item), INFINITE_AFFECT_DURATION, 0, true, false);
#endif
    return m_vid;
}

bool CPetActor::UpdateFollowAI()
{
    if (!IsSummoned() || !ecs::PlayerRuntime::IsValid(m_owner)
        || !ecs::PlayerRuntime::GetMobTable(m_character))
        return false;
    const auto ownerX = ecs::PlayerRuntime::GetX(m_owner);
    const auto ownerY = ecs::PlayerRuntime::GetY(m_owner);
    const auto charX = ecs::PlayerRuntime::GetX(m_character);
    const auto charY = ecs::PlayerRuntime::GetY(m_character);
    const float distance = DISTANCE_APPROX(charX - ownerX, charY - ownerY);
    constexpr int approach = 200;
    if (distance >= 4500.f || ecs::PlayerRuntime::GetMapIndex(m_character) != ecs::PlayerRuntime::GetMapIndex(m_owner))
    {
        const float rotation = ecs::PlayerRuntime::GetRotation(m_owner) * 3.141592f / 180.f;
        return SnapFollowerToOwner(m_character, m_owner, ownerX - approach * cos(rotation),
            ownerY - approach * sin(rotation), ecs::PlayerRuntime::GetZ(m_owner));
    }
    if (distance >= 300.f)
    {
        ecs::MovementSystem::SetNowWalking(m_character, distance < 900.f);
        Follow(approach);
        CombatSystem::SetLastAttacked(m_character, get_dword_time());
    }
    else
        ecs::MovementSystem::SendMovePacket(m_character, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

bool CPetActor::Update(uint32_t)
{
    // Preserve pets across owner death; only follower death ends the summon.
    if (!IsOwnedSummonItem(m_owner, m_summonItem) || !IsSummoned() || CombatSystem::IsDead(m_character))
    {
        Unsummon();
        return true;
    }
    return !HasOption(EPetOption_Followable) || UpdateFollowAI();
}

bool CPetActor::Follow(float minDistance)
{
    if (!ecs::PlayerRuntime::IsValid(m_owner) || !IsSummoned())
        return false;
    const auto ownerX = ecs::PlayerRuntime::GetX(m_owner);
    const auto ownerY = ecs::PlayerRuntime::GetY(m_owner);
    const auto charX = ecs::PlayerRuntime::GetX(m_character);
    const auto charY = ecs::PlayerRuntime::GetY(m_character);
    const float distance = DISTANCE_SQRT(ownerX - charX, ownerY - charY);
    if (distance <= minDistance)
        return false;
    ecs::MovementSystem::SetRotation(m_character, GetDegreeFromPositionXY(charX, charY, ownerX, ownerY));
    float dx, dy;
    GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(m_character), distance - minDistance, &dx, &dy);
    if (!ecs::MovementSystem::Goto(m_character, static_cast<int>(charX + dx + 0.5f), static_cast<int>(charY + dy + 0.5f)))
        return false;
    ecs::MovementSystem::SendMovePacket(m_character, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

void CPetActor::SetSummonItem(entt::entity item)
{
    if (!IsOwnedSummonItem(m_owner, item))
    {
        if (ecs::PlayerRuntime::IsValid(m_owner))
            if (auto* state = g_registry.try_get<ecs::PetComponent>(m_owner);
                state && m_summonItem != entt::null && state->item == m_summonItem)
                *state = {};
        m_summonItem = entt::null;
        return;
    }
    m_summonItem = item;
    auto& state = g_registry.get_or_emplace<ecs::PetComponent>(m_owner);
    state.owner = m_owner;
    state.item = item;
    state.itemID = ItemSystem::GetItemID(item);
    state.itemVID = ItemSystem::GetItemVID(item);
    state.itemVnum = ItemSystem::GetItemVnum(item);
    state.level = 0;
    state.state = IsSummoned() ? 1u : 0u;
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        state.sockets[i] = static_cast<int32_t>(ItemSystem::GetItemSocket(item, i));
}

bool CPetActor::CanGiveBuff() const
{
    return IsSummoned() && IsOwnedSummonItem(m_owner, m_summonItem)
        && ((m_vnum != 34004 && m_vnum != 34009) || ecs::SocialSystem::GetDungeon(m_owner));
}

void CPetActor::GiveBuff()
{
    // Called by ComputePoints AFTER the owner's point array has been reset.
    m_buffApplies = {};
    if (!CanGiveBuff())
        return;
    const auto* proto = ItemSystem::GetItemProto(m_summonItem);
    if (!proto)
        return;
    for (const auto& apply : proto->aApplies)
        if (apply.bType >= MAX_APPLY_NUM || apply.lValue == std::numeric_limits<int>::min())
            return;
    std::copy(std::begin(proto->aApplies), std::end(proto->aApplies), m_buffApplies.begin());
    ItemSystem::ModifyPoints(m_summonItem, true);
}

void CPetActor::ClearBuff()
{
    const auto applies = std::exchange(m_buffApplies, {});
    if (!ecs::PlayerRuntime::IsValid(m_owner))
        return;
    // Remove prototype contributions before ComputePoints for speed-point
    // notifications. The recomputation rebuilds item attributes as well.
    for (const auto& apply : applies)
        if (apply.bType != APPLY_NONE)
            ecs::PointSystem::ApplyPoint(m_owner, apply.bType,
                apply.bType == APPLY_SKILL ? apply.lValue ^ 0x00800000 : -apply.lValue);
}

CPetSystem::CPetSystem(entt::entity owner) : m_owner(owner)
{
    if (ecs::PlayerRuntime::IsValid(owner))
        g_registry.get_or_emplace<ecs::PetRuntimeRefs>(owner).petSystem = this;
}

CPetSystem::~CPetSystem()
{
    Destroy();
}

void CPetSystem::Destroy()
{
    if (m_destroying)
        return;
    m_destroying = true;
    event_cancel(&m_updateEvent);
    // Extract BEFORE destruction: ComputePoints -> RefreshBuff must see only
    // living actors, never the one whose destructor is currently running.
    while (!m_petActorMap.empty())
    {
        auto detached = m_petActorMap.extract(m_petActorMap.begin());
    }
    if (ecs::PlayerRuntime::IsValid(m_owner))
        if (auto* refs = g_registry.try_get<ecs::PetRuntimeRefs>(m_owner); refs && refs->petSystem == this)
            refs->petSystem = nullptr;
    m_destroying = false;
}

bool CPetSystem::Update(uint32_t deltaTime)
{
    const uint32_t now = get_dword_time();
    if (m_updatePeriod > now - m_lastUpdateTime)
        return true;
    bool result = true;
    for (auto& [vnum, actor] : m_petActorMap)
        if (actor->GetCharacter() != entt::null || actor->GetSummonItem() != entt::null)
            result = actor->Update(deltaTime) && result;
    m_lastUpdateTime = now;
    if (CountSummoned() == 0)
        event_cancel(&m_updateEvent);
    return result;
}

void CPetSystem::DeletePet(uint32_t vnum)
{
    auto detached = m_petActorMap.extract(vnum);
    if (!detached.empty())
        detached.mapped().reset();
    if (CountSummoned() == 0)
        event_cancel(&m_updateEvent);
}

void CPetSystem::DeletePet(CPetActor* actor)
{
    for (const auto& [vnum, owned] : m_petActorMap)
        if (owned.get() == actor)
        {
            DeletePet(vnum);
            return;
        }
}

void CPetSystem::Unsummon(uint32_t vnum, bool deleteFromList)
{
    if (deleteFromList)
        DeletePet(vnum);
    else if (auto* actor = GetByVnum(vnum))
        actor->Unsummon();
    if (CountSummoned() == 0)
        event_cancel(&m_updateEvent);
}

void CPetSystem::Unsummon(CPetActor* actor, bool deleteFromList)
{
    for (const auto& [vnum, owned] : m_petActorMap)
        if (owned.get() == actor)
        {
            Unsummon(vnum, deleteFromList);
            return;
        }
}

void CPetSystem::UnsummonAll()
{
    event_cancel(&m_updateEvent);
    for (auto& [vnum, actor] : m_petActorMap)
        actor->Unsummon();
}

CPetActor* CPetSystem::Summon(uint32_t vnum, entt::entity item, const char* petName, bool spawnFar, uint32_t options)
{
    if (m_destroying || !IsOwnedSummonItem(m_owner, item))
        return nullptr;
    for (const auto& [key, owned] : m_petActorMap)
        if (key != vnum && owned->GetSummonItem() == item)
            return nullptr;
    g_registry.get_or_emplace<ecs::PetRuntimeRefs>(m_owner).petSystem = this;
    auto* actor = GetByVnum(vnum);
    if (!actor)
    {
        auto fresh = std::make_unique<CPetActor>(m_owner, vnum, options);
        actor = fresh.get();
        m_petActorMap.emplace(vnum, std::move(fresh));
    }
    if (!actor->Summon(petName, item, spawnFar))
        return nullptr;
    if (!m_updateEvent)
    {
        auto* info = AllocEventInfo<petsystem_event_info>();
        info->owner = m_owner;
        m_updateEvent = event_create(petsystem_update_event, info, PASSES_PER_SEC(1) / 4);
    }
    return actor;
}

CPetActor* CPetSystem::GetByVID(uint32_t vid) const
{
    if (vid != 0)
        for (const auto& [vnum, actor] : m_petActorMap)
            if (actor->IsSummoned() && actor->GetVID() == vid)
                return actor.get();
    return nullptr;
}

CPetActor* CPetSystem::GetByVnum(uint32_t vnum) const
{
    const auto it = m_petActorMap.find(vnum);
    return it != m_petActorMap.end() ? it->second.get() : nullptr;
}

size_t CPetSystem::CountSummoned() const
{
    return std::count_if(m_petActorMap.begin(), m_petActorMap.end(),
        [](const auto& entry) { return entry.second->IsSummoned(); });
}

void CPetSystem::SetUpdatePeriod(uint32_t ms)
{
    m_updatePeriod = ms;
}

void CPetSystem::RefreshBuff()
{
    for (auto& [vnum, actor] : m_petActorMap)
        if (actor->IsSummoned())
            actor->GiveBuff();
}

#ifdef ENABLE_COSTUME_PET
void CPetActor::UpdatePetSkin()
{
    const auto item = m_summonItem;
    if (IsOwnedSummonItem(m_owner, item))
    {
        Unsummon();
        Summon("", item, false);
    }
}

void CPetSystem::UpdatePetSkin()
{
    for (auto& [vnum, actor] : m_petActorMap)
        if (actor->IsSummoned())
            actor->UpdatePetSkin();
    if (CountSummoned() == 0)
        event_cancel(&m_updateEvent);
}
#endif

