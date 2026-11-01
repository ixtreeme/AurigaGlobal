#pragma once
// GC_TYPED_INVENTORY (225): the character's typed items to its own client
// (item system F4 D1, IC-034). Sent when the login load completes and again at
// enter-game, so it arrives whichever happens last; nothing is sent before
// PHASE_GAME or for characters without typed items.
#include <entt/entity/fwd.hpp>
#include <cstdint>

namespace auriga::item::persistence { struct LoadedItemOwner; }

void SendTypedInventory(entt::entity character);
// A ground item can be picked up while its former owner is on another core.
void NotifyRemoteTypedItemOwner(const auriga::item::persistence::LoadedItemOwner&);
void RefreshRemoteTypedItemOwner(std::uint32_t pid, std::uint64_t session, std::uint64_t revision);
bool PumpRemoteTypedItemRefresh(entt::entity character);
void TickTypedItemRefreshes();
