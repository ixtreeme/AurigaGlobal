#pragma once
// GC_TYPED_INVENTORY (225): the character's typed items to its own client
// (item system F4 D1, IC-034). Sent when the login load completes and again at
// enter-game, so it arrives whichever happens last; nothing is sent before
// PHASE_GAME or for characters without typed items.
#include <entt/entity/fwd.hpp>

void SendTypedInventory(entt::entity character);
