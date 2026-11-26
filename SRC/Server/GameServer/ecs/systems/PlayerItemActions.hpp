#pragma once
#include "../Registry.hpp"
#include "../components/inventory_components.hpp"
#include <span>

namespace PlayerItemActions {
bool CanHandleItems(entt::entity owner, bool skipRefine = false, bool skipObserver = false);
bool IsItemLoaded(entt::entity);
void SetItemLoaded(entt::entity);
ecs::QuickSlots MakeQuickSlots(std::span<const TQuickslot, QUICKSLOT_MAX_NUM> saved);
void SendQuickslots(entt::entity);
bool GetQuickslot(entt::entity, uint8_t, TQuickslot&);
bool SetQuickslot(entt::entity, uint8_t, const TQuickslot&);
bool DelQuickslot(entt::entity, uint8_t);
bool SwapQuickslot(entt::entity, uint8_t, uint8_t);
} // namespace PlayerItemActions
