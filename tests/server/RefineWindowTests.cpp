#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/packet.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/dirty_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/DragonSoulSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
namespace {
int checks = 0, packets = 0;
bool connected = true;
std::function<void()> onPacket;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected deck/item/live service"); }
// Opaque token consumed only by the Packet service double, never dereferenced.
LPDESC DescriptorToken() { static int token; return reinterpret_cast<LPDESC>(&token); }
void Reset() { g_registry.clear(); connected = true; packets = 0; onPacket = {}; }
void LifecycleChecks() {
    Reset();
    const auto owner = g_registry.create(), npc = g_registry.create(), other = g_registry.create();
    Check(!DragonSoulSystem::CanRefine(owner), "window initially open");
    Check(!g_registry.all_of<ecs::DragonSoulRuntimeStateComponent>(owner), "read created window state");
    Check(!DragonSoulSystem::OpenRefineWindow(entt::null, npc), "null owner");
    Check(!DragonSoulSystem::OpenRefineWindow(owner, entt::null), "null opener");
    connected = false;
    Check(!DragonSoulSystem::OpenRefineWindow(owner, npc), "disconnected open");
    Check(!DragonSoulSystem::CanRefine(owner) && packets == 0, "failed open granted refine permission");
    Check(!g_registry.all_of<ecs::DragonSoulRuntimeStateComponent>(owner), "failed open created state");
    connected = true;
    Check(DragonSoulSystem::OpenRefineWindow(owner, npc), "entity-only open requires CHARACTER");
    Check(DragonSoulSystem::CanRefine(owner) && DragonSoulSystem::GetRefineWindowOpener(owner) == npc,
        "native opener identity");
    Check(g_registry.all_of<ecs::DirtyTag>(owner), "open not dirty");
    Check(DragonSoulSystem::OpenRefineWindow(owner, other) &&
        DragonSoulSystem::GetRefineWindowOpener(owner) == npc, "active opener replaced");
    Check(DragonSoulSystem::CloseRefineWindow(owner) && !DragonSoulSystem::CanRefine(owner), "close");
    Check(DragonSoulSystem::CloseRefineWindow(owner), "repeated close");
    Check(DragonSoulSystem::OpenRefineWindow(owner, owner), "self-open alchemy command");
    Check(DragonSoulSystem::GetRefineWindowOpener(owner) == owner, "self opener");
    Check(DragonSoulSystem::CloseRefineWindow(owner), "self-close");
    Check(DragonSoulSystem::OpenRefineWindow(owner, npc), "reopen");
    g_registry.destroy(npc);
    const auto replacement = g_registry.create();
    Check(entt::to_entity(npc) == entt::to_entity(replacement) && npc != replacement, "fixture did not recycle index");
    Check(!DragonSoulSystem::CanRefine(owner) &&
        DragonSoulSystem::GetRefineWindowOpener(owner) == entt::null, "stale opener inherited replacement");
    const int sent = packets;
    Check(!DragonSoulSystem::OpenRefineWindow(owner, npc) && packets == sent, "stale open sent packet");
    Check(DragonSoulSystem::OpenRefineWindow(owner, replacement) &&
        DragonSoulSystem::GetRefineWindowOpener(owner) == replacement, "stale slot not replaced by live opener");
    g_registry.destroy(owner);
    const auto newOwner = g_registry.create();
    Check(owner != newOwner && !DragonSoulSystem::CanRefine(owner), "stale owner");
    Check(!DragonSoulSystem::CloseRefineWindow(owner), "closed stale owner");
    Check(!DragonSoulSystem::OpenRefineWindow(owner, replacement), "opened recycled owner");
    Check(!g_registry.all_of<ecs::DragonSoulRuntimeStateComponent>(newOwner), "replacement owner mutated");
}
void CallbackChecks() {
    for (int mode = 0; mode < 4; ++mode) {
        Reset();
        const auto owner = g_registry.create(), npc = g_registry.create();
        entt::entity replacement = entt::null;
        onPacket = [&] {
            Check(DragonSoulSystem::CanRefine(owner), "packet published before state");
            if (mode == 0) {
                g_registry.destroy(owner); replacement = g_registry.create();
            } else if (mode == 1) {
                g_registry.destroy(npc); replacement = g_registry.create();
            } else if (mode == 2) {
                DragonSoulSystem::CloseRefineWindow(owner);
            } else {
                // Force component pool growth while publishing the packet.
                for (int i = 0; i < 4096; ++i)
                    g_registry.emplace<ecs::DragonSoulRuntimeStateComponent>(g_registry.create());
            }
        };
        Check(DragonSoulSystem::OpenRefineWindow(owner, npc) == (mode == 3), "callback invalidation ignored");
        if (replacement != entt::null)
            Check(!g_registry.all_of<ecs::DragonSoulRuntimeStateComponent, ecs::DirtyTag>(replacement),
                "callback replacement inherited state");
        if (mode == 3) Check(DragonSoulSystem::GetRefineWindowOpener(owner) == npc, "pool growth lost opener");
    }
}
}
namespace ecs::PlayerRuntime {
LPDESC GetDesc(entt::entity e) { Check(g_registry.valid(e), "invalid descriptor lookup"); return connected ? DescriptorToken() : nullptr; }
}
void DESC::Packet(const void* data, int size) {
    Check(this == DescriptorToken() && data && size == sizeof(TPacketGCDragonSoulRefine), "unexpected packet");
    const auto& packet = *static_cast<const TPacketGCDragonSoulRefine*>(data);
    Check(packet.header == HEADER_GC_DRAGON_SOUL_REFINE && packet.bSubType == DS_SUB_HEADER_OPEN, "wrong open packet");
    ++packets;
    if (onPacket) onPacket();
}
namespace ItemSystem {
entt::entity GetInventoryItem(entt::entity, uint16_t) { Unexpected(); }
bool SetItemSocket(entt::entity, int, uint32_t, bool) { Unexpected(); }
uint32_t GetItemVnum(entt::entity) { Unexpected(); }
}
namespace AffectSystem {
CAffect* FindAffect(entt::entity, uint32_t, uint8_t) { Unexpected(); }
bool AddAffect(entt::entity, uint32_t, uint8_t, int32_t, uint32_t, int32_t, int32_t, bool, bool) { Unexpected(); }
bool RemoveAffect(entt::entity, uint32_t) { Unexpected(); }
}
bool DSManager::ActivateDragonSoul(entt::entity) { Unexpected(); }
bool DSManager::DeactivateDragonSoul(entt::entity, bool) { Unexpected(); }
bool DSManager::IsTimeLeftDragonSoul(entt::entity) const { Unexpected(); }
time_t get_global_time() { return 1000; }

int main() {
    try {
        LifecycleChecks(); CallbackChecks();
        std::cout << "Refine window checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
