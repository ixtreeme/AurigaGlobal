#include "../../core/stdafx.h"
#include "PlayerItemActions.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "../components/dirty_components.hpp"
#include "../../network/desc.h"
#include "../../core/packet.h"
#include "../components/status_components.hpp"
#include "../components/social_components.hpp"
#include "../components/character_runtime_components.hpp"

namespace {
ecs::QuickSlots* GetQuickSlots(entt::entity e)
{
    return g_registry.valid(e) ? &g_registry.get_or_emplace<ecs::QuickSlots>(e) : nullptr;
}
}
namespace PlayerItemActions {
bool CanHandleItems(entt::entity owner, bool skipRefine, bool skipObserver)
{
    if (!g_registry.valid(owner))
        return false;
    const auto* status = g_registry.try_get<ecs::StatusFlags>(owner);
    if (!skipObserver && status && status->isObserverMode)
        return false;
    const auto* shop = g_registry.try_get<ecs::ShopState>(owner);
    if (shop && (shop->myShop != entt::null || (!skipRefine && shop->underRefine)))
        return false;

    const auto* events = g_registry.try_get<ecs::LegacyCharEvents>(owner);
    if (events && events->warp)
        return false;
    return true;
}

bool IsItemLoaded(entt::entity e)
{
    const auto* state = g_registry.valid(e) ? g_registry.try_get<ecs::ItemLoadState>(e) : nullptr;
    return state && state->loaded;
}

void SetItemLoaded(entt::entity e)
{
    if (g_registry.valid(e))
        g_registry.get_or_emplace<ecs::ItemLoadState>(e).loaded = true;
}

static bool IsQuickslotValueValid(const TQuickslot& slot)
{
    switch (slot.type)
    {
        case QUICKSLOT_TYPE_SKILL: return slot.pos < SKILL_MAX_NUM;
        case QUICKSLOT_TYPE_COMMAND: return true;
        default: return false;
    }
}

static bool HasQuickslotRevision(entt::entity e, uint64_t revision)
{
    if (!g_registry.valid(e))
        return false;
    const auto* slots = g_registry.try_get<ecs::QuickSlots>(e);
    return slots && slots->revision == revision;
}

ecs::QuickSlots MakeQuickSlots(std::span<const TQuickslot, QUICKSLOT_MAX_NUM> saved)
{
    ecs::QuickSlots result {};
    for (size_t pos = 0; pos < saved.size(); ++pos)
    {
        const auto slot = saved[pos];
        if (!IsQuickslotValueValid(slot))
            continue;
        // Preserve the last valid occurrence, as the old ordered load did.
        for (size_t previous = 0; previous < pos; ++previous)
            if (result.slots[previous].type == slot.type && result.slots[previous].pos == slot.pos)
                result.slots[previous] = {};
        result.slots[pos] = slot;
    }
    return result;
}

void SendQuickslots(entt::entity e)
{
    if (!g_registry.valid(e)) return;
    const auto* slots = g_registry.try_get<ecs::QuickSlots>(e);
    if (!slots) return;
    const auto snapshot = *slots;
    for (uint8_t pos = 0; pos < QUICKSLOT_MAX_NUM && HasQuickslotRevision(e, snapshot.revision); ++pos)
    {
        if (snapshot.slots[pos].type == QUICKSLOT_TYPE_NONE)
            NetworkSyncSystem::SendQuickslotDelete(e, pos);
        else
            NetworkSyncSystem::SendQuickslotAdd(e, pos, snapshot.slots[pos]);
    }
}



bool GetQuickslot(entt::entity e, uint8_t pos, TQuickslot& out)
{
    out = {};
    if (!g_registry.valid(e) || pos >= QUICKSLOT_MAX_NUM)
        return false;
    if (const auto* slots = g_registry.try_get<ecs::QuickSlots>(e))
        out = slots->slots[pos];
    return true;
}

bool SetQuickslot(entt::entity e, uint8_t pos, const TQuickslot& value)
{
    const TQuickslot slot = value;
    if (pos >= QUICKSLOT_MAX_NUM || !IsQuickslotValueValid(slot))
        return false;
    auto* slots = GetQuickSlots(e);
    if (!slots)
        return false;
    std::array<bool, QUICKSLOT_MAX_NUM> removed {};
    for (size_t i = 0; i < slots->slots.size(); ++i)
        if (i != pos && slots->slots[i].type == slot.type && slots->slots[i].pos == slot.pos)
        {
            removed[i] = true;
            slots->slots[i] = {};
        }
    slots->slots[pos] = slot;
    const auto revision = ++slots->revision;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM && HasQuickslotRevision(e, revision); ++i)
        if (removed[i])
            NetworkSyncSystem::SendQuickslotDelete(e, i);
    if (HasQuickslotRevision(e, revision))
        NetworkSyncSystem::SendQuickslotAdd(e, pos, slot);
    return true;
}

bool DelQuickslot(entt::entity e, uint8_t pos)
{
    if (pos >= QUICKSLOT_MAX_NUM)
        return false;
    auto* slots = GetQuickSlots(e);
    if (!slots)
        return false;
    slots->slots[pos] = {};
    ++slots->revision;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    NetworkSyncSystem::SendQuickslotDelete(e, pos);
    return true;
}

bool SwapQuickslot(entt::entity e, uint8_t a, uint8_t b)
{
    if (a >= QUICKSLOT_MAX_NUM || b >= QUICKSLOT_MAX_NUM)
        return false;
    auto* slots = GetQuickSlots(e);
    if (!slots)
        return false;
    std::swap(slots->slots[a], slots->slots[b]);
    ++slots->revision;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
    NetworkSyncSystem::SendQuickslotSwap(e, a, b);
    return true;
}

} // namespace PlayerItemActions
