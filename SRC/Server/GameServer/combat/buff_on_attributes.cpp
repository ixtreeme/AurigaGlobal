#include "stdafx.h"
#include "buff_on_attributes.h"
#include "char.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include <algorithm>
#include <vector>

namespace {
bool Live(entt::entity e) { return e != entt::null && g_registry.valid(e); }

std::vector<uint8_t> Slots(uint8_t type)
{
    switch (type) {
    case POINT_ENERGY:
        return { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST,
            WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_SHIELD };
    case POINT_COSTUME_ATTR_BONUS:
        return { WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR, WEAR_COSTUME_MOUNT,
#ifdef ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93
            WEAR_COSTUME_PET_SKIN, WEAR_COSTUME_EFFECT_BODY, WEAR_COSTUME_EFFECT_WEAPON,
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
            WEAR_COSTUME_WEAPON,
#endif
#ifdef ENABLE_STOLE_COSTUME
            WEAR_COSTUME_ACCE,
#endif
            WEAR_COSTUME_ACCE_SLOT };
    default: return {};
    }
}

// Nested point changes append to the queue. Reacquire after every callback:
// a replacement component or recycled owner must not inherit an older drain.
void Drain(entt::entity owner)
{
    auto* state = Live(owner) ? g_registry.try_get<ecs::BuffOnAttrs>(owner) : nullptr;
    if (!state || state->dispatchToken) return;
    static uint64_t nextToken = 0;
    const auto token = ++nextToken;
    state->dispatchToken = token;
    try {
        for (;;) {
            state = Live(owner) ? g_registry.try_get<ecs::BuffOnAttrs>(owner) : nullptr;
            if (!state || state->dispatchToken != token) return;
            if (state->pending.empty()) { state->dispatchToken = 0; return; }
            const auto [type, delta] = *state->pending.begin();
            state->pending.erase(state->pending.begin());
            if (delta) ecs::PointSystem::ApplyPoint(owner, type, delta);
        }
    } catch (...) {
        state = Live(owner) ? g_registry.try_get<ecs::BuffOnAttrs>(owner) : nullptr;
        if (state && state->dispatchToken == token) state->dispatchToken = 0;
        throw;
    }
}

void ChangeItem(entt::entity owner, entt::entity item, int sign)
{
    if (!Live(owner) || !Live(item)) return;
    const auto cell = ItemSystem::GetItemCell(item);
    if (cell < INVENTORY_MAX_NUM) return;
    std::vector<TPlayerItemAttribute> attributes;
    for (int j = 0, n = ItemSystem::GetItemAttributeCount(item); j < n; ++j)
        attributes.push_back(ItemSystem::GetItemAttribute(item, j));

    auto* state = g_registry.try_get<ecs::BuffOnAttrs>(owner);
    if (!state) return;
    for (auto& [type, pool] : state->pools) {
        if (!pool.value) continue;
        const auto slots = Slots(type);
        if (std::find(slots.begin(), slots.end(), cell - INVENTORY_MAX_NUM) == slots.end()) continue;
        for (const auto& attr : attributes) {
            auto it = pool.attributes.find(attr.bType);
            if (sign < 0 && it == pool.attributes.end()) break;
            const int oldSum = it == pool.attributes.end() ? 0 : it->second;
            const int newSum = oldSum + sign * attr.sValue;
            state->pending[attr.bType] += newSum * pool.value / 100 - oldSum * pool.value / 100;
            pool.attributes[attr.bType] = newSum;
        }
    }
    Drain(owner);
}
}

namespace ecs::PlayerRuntime {
void BuffOnAttr_AddBuffsFromItem(entt::entity owner, entt::entity item) { ChangeItem(owner, item, 1); }
void BuffOnAttr_RemoveBuffsFromItem(entt::entity owner, entt::entity item) { ChangeItem(owner, item, -1); }

void BuffOnAttr_ClearAll(entt::entity owner)
{
    // Point recomputation resets the underlying points itself: do not subtract.
    if (Live(owner)) (void)g_registry.remove<ecs::BuffOnAttrs>(owner);
}
void BuffOnAttr_Destroy(entt::entity owner) { BuffOnAttr_ClearAll(owner); }

void BuffOnAttr_ValueChange(entt::entity owner, uint8_t type, uint8_t oldValue, uint8_t newValue)
{
    const auto slots = Slots(type);
    if (!Live(owner) || slots.empty()) return;
    auto* state = g_registry.try_get<ecs::BuffOnAttrs>(owner);
    if (!newValue) {
        if (!state) return;
        const auto it = state->pools.find(type);
        if (it == state->pools.end()) return;
        for (const auto& [attr, sum] : it->second.attributes)
            state->pending[attr] -= sum * it->second.value / 100;
        it->second = {};
        Drain(owner);
        return;
    }

    auto* pool = state && state->pools.count(type) ? &state->pools.at(type) : nullptr;
    if (pool && pool->value) {
        if (!oldValue) return;
        // Preserve the existing subtract-only percentage-change path.
        for (const auto& [attr, sum] : pool->attributes)
            state->pending[attr] -= sum * pool->value / 100;
        pool->value = newValue;
        Drain(owner);
        return;
    }

    AttributeBuffPool fresh;
    fresh.value = newValue;
    struct LockedNotice { entt::entity item; entt::entity owner; int index; };
    std::vector<LockedNotice> notices;
    for (const auto slot : slots) {
        const auto item = ItemSystem::GetWearItem(owner, slot);
        if (!Live(item)) continue;
        for (int j = 0, n = ItemSystem::GetItemAttributeCount(item); j < n; ++j) {
#ifdef ATTR_LOCK
            if (ItemSystem::GetItemLockedAttributeIndex(item) == j) {
#ifdef TEXTS_IMPROVEMENT
                notices.push_back({item, ItemSystem::GetItemOwnerEntity(item), j});
#endif
                continue;
            }
#endif
            const auto attr = ItemSystem::GetItemAttribute(item, j);
            fresh.attributes[attr.bType] += attr.sValue;
        }
    }

    (void)g_registry.get_or_emplace<ecs::BuffOnAttrs>(owner);
    // Component construction listeners may remove the owner or the component.
    state = Live(owner) ? g_registry.try_get<ecs::BuffOnAttrs>(owner) : nullptr;
    if (!state) return;
    if (const auto it = state->pools.find(type); it != state->pools.end() && it->second.value) return;
    state->pools[type] = std::move(fresh);
    for (const auto& [attr, sum] : state->pools.at(type).attributes)
        state->pending[attr] += sum * newValue / 100;
    Drain(owner);
#ifdef TEXTS_IMPROVEMENT
    for (const auto& notice : notices) {
        if (!Live(owner)) return;
        if (Live(notice.owner) && Live(notice.item))
            ecs::ChatSystem::SendNew(notice.owner, CHAT_TYPE_INFO, 781,
                "%d#%s", notice.index, ItemSystem::GetItemName(notice.item));
    }
#endif
}
}



