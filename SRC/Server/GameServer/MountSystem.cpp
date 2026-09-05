#include "stdafx.h"
#include "MountSystem.h"
#include "char_manager.h"
#include "constants.h"
#include "config.h"
#include "utils.h"
#include "vector.h"
#include "packet.h"
#include "ecs/Registry.hpp"
#include "ecs/EventDispatcher.hpp"
#include "ecs/events.hpp"
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
#include <limits>
#include <utility>

namespace
{
bool IsOwnedSummonItem(entt::entity owner, entt::entity item)
{
    return ecs::PlayerRuntime::IsValid(owner) && ItemSystem::IsValidItem(item)
        && ItemSystem::GetItemOwner(item) == owner;
}

uint32_t MountSkin(entt::entity owner)
{
#ifdef ENABLE_COSTUME_MOUNT
    const auto item = ItemSystem::GetWearItem(owner, WEAR_COSTUME_MOUNT_SKIN);
    if (ItemSystem::IsValidItem(item))
        return ItemSystem::GetItemValue(item, 0);
#endif
    return 0;
}

bool MountDuration(entt::entity owner, entt::entity item, int32_t& duration)
{
    if (!IsOwnedSummonItem(owner, item))
        return false;
    const auto* proto = ItemSystem::GetItemProto(item);
    if (!proto)
        return false;
    for (const auto& apply : proto->aApplies)
        if (apply.bType >= MAX_APPLY_NUM)
            return false;
    const int64_t remaining = ItemSystem::IsUnlimitedTimeUnique(item) ? 86400
        : static_cast<int64_t>(ItemSystem::GetItemSocket(item, 0)) - time(nullptr);
    if (remaining <= 0)
        return false;
    duration = static_cast<int32_t>(std::min<int64_t>(remaining, std::numeric_limits<int32_t>::max()));
    return true;
}

void ClearHorse(entt::entity owner)
{
    if (MountSystem::IsHorseRiding(owner))
        MountSystem::StopRiding(owner);
    if (ecs::PlayerRuntime::IsValid(MountSystem::GetSummonedHorse(owner)))
        MountSystem::SummonHorse(owner, false);
}

bool SnapFollowerToOwner(entt::entity follower, entt::entity owner, int32_t x, int32_t y, int32_t z)
{
    if (!ecs::PlayerRuntime::IsValid(follower) || !ecs::PlayerRuntime::IsValid(owner))
        return false;
    // Show also handles a follower detached from its old sectree or on another map.
    if (!ecs::MovementSystem::Show(follower, ecs::PlayerRuntime::GetMapIndex(owner), x, y, z))
        return false;
    ecs::MovementSystem::Stop(follower);
    ecs::MovementSystem::SendMovePacket(follower, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}
}

EVENTINFO(mountsystem_event_info)
{
    entt::entity owner { entt::null };
};

EVENTFUNC(mountsystem_update_event)
{
    const auto* info = dynamic_cast<mountsystem_event_info*>(event->info);
    if (!info || !ecs::PlayerRuntime::IsValid(info->owner))
        return 0;
    const auto owner = info->owner;
    const auto* refs = g_registry.try_get<ecs::MountRuntimeRefs>(owner);
    if (!refs || !refs->mountSystem || !refs->mountSystem->IsUpdateEvent(event))
        return 0;
    refs->mountSystem->Update(0);
    if (ecs::PlayerRuntime::IsValid(owner))
        g_dispatcher.trigger(ecs::EvMountSystemUpdate { owner });
    return PASSES_PER_SEC(1) / 4;
}

CMountActor::CMountActor(entt::entity owner, uint32_t vnum)
    : m_dwVnum(vnum), m_dwVID(0), m_dwLastActionTime(0),
      m_dwSummonItemVID(0), m_dwSummonItemVnum(0), m_owner(owner)
{
}

CMountActor::~CMountActor()
{
    Unsummon();
}

bool CMountActor::IsSummoned() const
{
    return ecs::PlayerRuntime::IsValid(m_character);
}

void CMountActor::SetName()
{
    if (!ecs::PlayerRuntime::IsValid(m_owner))
        return;
    m_name = std::string(ecs::PlayerRuntime::GetName(m_owner)) + "'s Mount";
    if (IsSummoned())
        g_registry.emplace_or_replace<ecs::PlayerName>(m_character, m_name);
}

bool CMountActor::Mount(entt::entity item)
{
    int32_t duration;
    if (!MountDuration(m_owner, item, duration))
        return false;
#ifdef BLOCK_RIDING_INSIDE_WAR
    if (ecs::SocialSystem::GetWarMap(m_owner))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(m_owner, CHAT_TYPE_INFO, 852, "");
#endif
        Unmount();
        return false;
    }
#endif
    ClearHorse(m_owner);
    const uint32_t skin = MountSkin(m_owner);
    const uint32_t ridingVnum = skin ? skin : m_dwVnum;
    if (MountSystem::GetMountVnum(m_owner) != ridingVnum)
        Unmount();
    const auto* proto = ItemSystem::GetItemProto(item);
#ifdef ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93
    if (!AffectSystem::FindAffect(m_owner, AFFECT_MOUNT_BONUS))
#endif
    {
        for (const auto& apply : proto->aApplies)
            if (apply.bType != APPLY_NONE)
                AffectSystem::AddAffect(m_owner, AFFECT_MOUNT_BONUS,
                    aApplyInfo[apply.bType].bPointType, apply.lValue,
                    AFF_NONE, duration, 0, false);
        AffectSystem::AddAffect(m_owner, AFFECT_MOUNT_BONUS, POINT_MOV_SPEED, 50,
            AFF_NONE, duration, 0, false);
    }
    AffectSystem::AddAffect(m_owner, AFFECT_MOUNT, POINT_MOUNT, ridingVnum,
        AFF_NONE, duration, 0, true);
    // Report the new state, not the mount vnum captured before applying it.
    m_ridingVnum = MountSystem::GetMountVnum(m_owner) == ridingVnum ? ridingVnum : 0;
    return m_ridingVnum != 0;
}

void CMountActor::Unmount()
{
    m_ridingVnum = 0;
    if (!ecs::PlayerRuntime::IsValid(m_owner))
        return;
    AffectSystem::RemoveAffect(m_owner, AFFECT_MOUNT);
    AffectSystem::RemoveAffect(m_owner, AFFECT_MOUNT_BONUS);
    MountSystem::SetMountVnum(m_owner, 0);
    ClearHorse(m_owner);
    ecs::PointSystem::Change(m_owner, POINT_ST, 0);
    ecs::PointSystem::Change(m_owner, POINT_DX, 0);
    ecs::PointSystem::Change(m_owner, POINT_HT, 0);
    ecs::PointSystem::Change(m_owner, POINT_IQ, 0);
}

void CMountActor::Unsummon()
{
    const uint32_t ridingVnum = std::exchange(m_ridingVnum, 0u);
    if (ridingVnum && ecs::PlayerRuntime::IsValid(m_owner) && MountSystem::GetMountVnum(m_owner) == ridingVnum)
        Unmount();
    // Clear handles before destruction so callbacks/repeated teardown cannot
    // dereference an already freed character or adopt a recycled entity.
    const auto character = std::exchange(m_character, entt::null);
    const bool hadSummon = character != entt::null || m_summonItem != entt::null;
    m_dwVID = 0;
    SetSummonItem(entt::null);
    if (ecs::PlayerRuntime::IsValid(character))
        ecs::PlayerRuntime::DestroyCharacter(character);
    if (hadSummon && ecs::PlayerRuntime::IsValid(m_owner) && MountSystem::GetMountVnum(m_owner) == 0)
    {
        MountSystem::SetMountVnum(m_owner, 0);
        ClearHorse(m_owner);
    }
}

uint32_t CMountActor::Summon(entt::entity item, bool spawnFar)
{
    if (!IsOwnedSummonItem(m_owner, item))
        return 0;
    int32_t x = ecs::PlayerRuntime::GetX(m_owner);
    int32_t y = ecs::PlayerRuntime::GetY(m_owner);
    const auto z = ecs::PlayerRuntime::GetZ(m_owner);
    x += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);
    y += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);
    if (IsSummoned())
    {
        if (!SnapFollowerToOwner(m_character, m_owner, x, y, z))
            return 0;
        SetSummonItem(item);
        return m_dwVID;
    }
    Unsummon();
    const auto skin = MountSkin(m_owner);
    m_character = CHARACTER_MANAGER::instance().SpawnMobEntity(skin ? skin : m_dwVnum,
        ecs::PlayerRuntime::GetMapIndex(m_owner), x, y, z, false,
        static_cast<int>(ecs::PlayerRuntime::GetRotation(m_owner) + 180), false);
    if (!IsSummoned())
        return 0;
    g_registry.get_or_emplace<ecs::StatusFlags>(m_character).isMount = true;
    ecs::PlayerRuntime::SetEmpire(m_character, ecs::PlayerRuntime::GetEmpire(m_owner));
    m_dwVID = ecs::PlayerRuntime::GetPacketVID(m_character);
    SetName();
    SetSummonItem(item);
    if (!ecs::MovementSystem::Show(m_character, ecs::PlayerRuntime::GetMapIndex(m_owner), x, y, z))
    {
        Unsummon();
        return 0;
    }
    return m_dwVID;
}

bool CMountActor::UpdateFollowAI()
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
        if (SnapFollowerToOwner(m_character, m_owner, ownerX - approach * cos(rotation),
                ownerY - approach * sin(rotation), ecs::PlayerRuntime::GetZ(m_owner)))
            return true;
    }
    if (distance >= 300.f)
    {
        ecs::MovementSystem::SyncWalkingWrite(m_character, false);
        Follow(approach);
        m_dwLastActionTime = get_dword_time();
        CombatSystem::SetLastAttacked(m_character, m_dwLastActionTime);
    }
    else
        ecs::MovementSystem::SendMovePacket(m_character, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

bool CMountActor::Update(uint32_t)
{
    if (!ecs::PlayerRuntime::IsValid(m_owner) || !IsSummoned()
        || CombatSystem::IsDead(m_owner) || CombatSystem::IsDead(m_character)
        || !IsOwnedSummonItem(m_owner, m_summonItem))
    {
        Unsummon();
        return true;
    }
    return UpdateFollowAI();
}

bool CMountActor::Follow(float minDistance)
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

void CMountActor::SetSummonItem(entt::entity item)
{
    if (!IsOwnedSummonItem(m_owner, item))
    {
        if (ecs::PlayerRuntime::IsValid(m_owner))
            if (auto* state = g_registry.try_get<ecs::MountComponent>(m_owner);
                state && m_summonItem != entt::null && state->item == m_summonItem)
                *state = {};
        m_summonItem = entt::null;
        m_dwSummonItemVID = 0;
        m_dwSummonItemVnum = 0;
        return;
    }
    m_summonItem = item;
    m_dwSummonItemVID = ItemSystem::GetItemVID(item);
    m_dwSummonItemVnum = ItemSystem::GetItemVnum(item);
    auto& state = g_registry.get_or_emplace<ecs::MountComponent>(m_owner);
    state.item = item;
    state.owner = m_owner;
    state.itemID = ItemSystem::GetItemID(item);
    state.itemVID = m_dwSummonItemVID;
    state.itemVnum = m_dwSummonItemVnum;
    state.level = 0;
    state.state = IsSummoned() ? 1u : 0u;
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        state.sockets[i] = static_cast<int32_t>(ItemSystem::GetItemSocket(item, i));
}

CMountSystem::CMountSystem(entt::entity owner)
    : m_owner(owner), m_dwUpdatePeriod(400), m_dwLastUpdateTime(0)
{
    if (ecs::PlayerRuntime::IsValid(owner))
        g_registry.get_or_emplace<ecs::MountRuntimeRefs>(owner).mountSystem = this;
}

CMountSystem::~CMountSystem()
{
    Destroy();
}

void CMountSystem::Destroy()
{
    event_cancel(&m_pkMountSystemUpdateEvent);
    m_mountActorMap.clear();
    if (ecs::PlayerRuntime::IsValid(m_owner))
        if (auto* refs = g_registry.try_get<ecs::MountRuntimeRefs>(m_owner); refs && refs->mountSystem == this)
            refs->mountSystem = nullptr;
}

bool CMountSystem::Update(uint32_t deltaTime)
{
    const uint32_t now = get_dword_time();
    if (m_dwUpdatePeriod > now - m_dwLastUpdateTime)
        return true;
    bool result = true;
    for (auto& [vnum, actor] : m_mountActorMap)
        // A stale non-null handle needs cleanup even though IsSummoned is false.
        if (actor->GetCharacter() != entt::null || actor->GetSummonItem() != entt::null)
            result = actor->Update(deltaTime) && result;
    m_dwLastUpdateTime = now;
    return result;
}

void CMountSystem::DeleteMount(uint32_t vnum)
{
    m_mountActorMap.erase(vnum);
}

void CMountSystem::DeleteMount(CMountActor* actor)
{
    for (auto it = m_mountActorMap.begin(); it != m_mountActorMap.end(); ++it)
        if (it->second.get() == actor)
        {
            m_mountActorMap.erase(it);
            return;
        }
}

void CMountSystem::Unsummon(uint32_t vnum, bool deleteFromList)
{
    auto* actor = GetByVnum(vnum);
    if (!actor)
        return;
    actor->Unsummon();
    if (deleteFromList)
        DeleteMount(vnum);
    if (CountSummoned() == 0)
        event_cancel(&m_pkMountSystemUpdateEvent);
}

void CMountSystem::Unsummon(CMountActor* actor, bool deleteFromList)
{
    if (actor)
        Unsummon(actor->GetVnum(), deleteFromList);
}

void CMountSystem::Summon(uint32_t vnum, entt::entity item, bool spawnFar)
{
    if (!IsOwnedSummonItem(m_owner, item))
        return;
    g_registry.get_or_emplace<ecs::MountRuntimeRefs>(m_owner).mountSystem = this;
    auto* actor = GetByVnum(vnum);
    if (!actor)
    {
        auto fresh = std::make_unique<CMountActor>(m_owner, vnum);
        actor = fresh.get();
        m_mountActorMap.emplace(vnum, std::move(fresh));
    }
    if (!actor->Summon(item, spawnFar))
        return;
    if (!m_pkMountSystemUpdateEvent)
    {
        auto* info = AllocEventInfo<mountsystem_event_info>();
        info->owner = m_owner;
        m_pkMountSystemUpdateEvent = event_create(mountsystem_update_event, info, PASSES_PER_SEC(1) / 4);
    }
    if (ItemSystem::GetItemSocket(item, 2) == 1)
        Mount(vnum, item);
}

void CMountSystem::Mount(uint32_t vnum, entt::entity item)
{
    auto* actor = GetByVnum(vnum);
    if (!actor)
        return;
    int32_t duration;
    if (!MountDuration(m_owner, item, duration))
        return;
#ifdef BLOCK_RIDING_INSIDE_WAR
    if (ecs::SocialSystem::GetWarMap(m_owner))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(m_owner, CHAT_TYPE_INFO, 852, "");
#endif
        return;
    }
#endif
    Unsummon(vnum, false);
    if (actor->Mount(item))
        ItemSystem::SetItemSocket(item, 2, 1);
}

void CMountSystem::Unmount(uint32_t vnum)
{
    auto* actor = GetByVnum(vnum);
    if (!actor || !ecs::PlayerRuntime::IsValid(m_owner))
        return;
    actor->Unmount();
    const auto item = ItemSystem::GetWearItem(m_owner, WEAR_COSTUME_MOUNT);
    if (IsOwnedSummonItem(m_owner, item))
    {
        ItemSystem::SetItemSocket(item, 2, 0);
        Summon(vnum, item, false);
    }
}

CMountActor* CMountSystem::GetByVID(uint32_t vid) const
{
    if (vid != 0)
        for (const auto& [vnum, actor] : m_mountActorMap)
            if (actor->IsSummoned() && actor->GetVID() == vid)
                return actor.get();
    return nullptr;
}

CMountActor* CMountSystem::GetByVnum(uint32_t vnum) const
{
    const auto it = m_mountActorMap.find(vnum);
    return it != m_mountActorMap.end() ? it->second.get() : nullptr;
}

size_t CMountSystem::CountSummoned() const
{
    return std::count_if(m_mountActorMap.begin(), m_mountActorMap.end(),
        [](const auto& entry) { return entry.second->IsSummoned(); });
}

void CMountSystem::SetUpdatePeriod(uint32_t ms)
{
    m_dwUpdatePeriod = ms;
}

#ifdef ENABLE_COSTUME_MOUNT
void CMountActor::UpdateMountSkin()
{
    const auto item = m_summonItem;
    if (IsOwnedSummonItem(m_owner, item))
    {
        Unsummon();
        Summon(item, false);
    }
}

void CMountSystem::UpdateMountSkin()
{
    for (auto& [vnum, actor] : m_mountActorMap)
        if (actor->IsSummoned())
            actor->UpdateMountSkin();
}
#endif
