#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerItemActions.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
namespace {
int checks = 0, packets = 0;
std::function<void(entt::entity)> onPacket;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
void Published(entt::entity e) { ++packets; if (onPacket) onPacket(e); }
}
namespace NetworkSyncSystem {
void SendQuickslotAdd(entt::entity e, uint8_t, TQuickslot) { Published(e); }
void SendQuickslotDelete(entt::entity e, uint8_t) { Published(e); }
void SendQuickslotSwap(entt::entity e, uint8_t, uint8_t) { Published(e); }
}
int main() {
    try {
        namespace A = PlayerItemActions;
        const auto e = g_registry.create();
        Check(A::CanHandleItems(e), "empty character gate");
        g_registry.emplace<ecs::StatusFlags>(e).isObserverMode = true;
        Check(!A::CanHandleItems(e) && A::CanHandleItems(e, false, true), "observer gate");
        g_registry.get<ecs::StatusFlags>(e).isObserverMode = false;
        g_registry.emplace<ecs::ShopState>(e).myShop = e;
        Check(!A::CanHandleItems(e), "market gate");
        g_registry.get<ecs::ShopState>(e).myShop = entt::null;
        Check(!A::IsItemLoaded(e), "login barrier opened early");
        A::SetItemLoaded(e);
        Check(A::IsItemLoaded(e), "login barrier not set");
        Check(!A::SetQuickslot(e, 0, {QUICKSLOT_TYPE_ITEM, 0}), "raw inventory quickslot accepted");
        Check(!A::SetQuickslot(e, 0, {QUICKSLOT_TYPE_SKILL, SKILL_MAX_NUM}), "out-of-range skill");
        Check(!A::SetQuickslot(e, QUICKSLOT_MAX_NUM, {QUICKSLOT_TYPE_SKILL, 1}), "invalid quickslot");
        Check(A::SetQuickslot(e, 0, {QUICKSLOT_TYPE_SKILL, 1}), "skill bind");
        Check(A::SetQuickslot(e, 1, {QUICKSLOT_TYPE_SKILL, 1}), "duplicate bind");
        TQuickslot slot{};
        A::GetQuickslot(e, 0, slot); Check(slot.type == QUICKSLOT_TYPE_NONE, "duplicate retained");
        A::GetQuickslot(e, 1, slot); Check(slot.type == QUICKSLOT_TYPE_SKILL && slot.pos == 1, "new bind lost");
        Check(A::SwapQuickslot(e, 1, 2), "swap failed");
        A::GetQuickslot(e, 2, slot); Check(slot.type == QUICKSLOT_TYPE_SKILL, "swap not committed");
        Check(A::DelQuickslot(e, 2), "delete failed");
        A::GetQuickslot(e, 2, slot); Check(slot.type == QUICKSLOT_TYPE_NONE, "delete not committed");
        std::array<TQuickslot, QUICKSLOT_MAX_NUM> saved{};
        saved[0] = {QUICKSLOT_TYPE_ITEM, 17};
        saved[1] = {QUICKSLOT_TYPE_SKILL, 1};
        saved[2] = saved[1];
        saved[3] = {QUICKSLOT_TYPE_COMMAND, 65535};
        const auto loaded = A::MakeQuickSlots(saved);
        Check(loaded.slots[0].type == QUICKSLOT_TYPE_NONE, "loaded old item binding");
        Check(loaded.slots[1].type == QUICKSLOT_TYPE_NONE && loaded.slots[2].pos == 1, "loaded duplicate not normalized");
        Check(loaded.slots[3].pos == 65535, "command value truncated");
        // A nested publication can retire the character. No later slot may publish.
        g_registry.emplace_or_replace<ecs::QuickSlots>(e, loaded);
        packets = 0;
        onPacket = [](auto actor) { g_registry.destroy(actor); };
        A::SendQuickslots(e);
        Check(packets == 1 && !g_registry.valid(e), "publication continued after retirement");
        const auto replacement = g_registry.create();
        Check(!A::SetQuickslot(e, 0, {QUICKSLOT_TYPE_SKILL, 1}) && !A::IsItemLoaded(replacement), "stale handle reached replacement");
        std::cout << checks << " native player-action/quickslot checks passed.\n";
        return 0;
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
