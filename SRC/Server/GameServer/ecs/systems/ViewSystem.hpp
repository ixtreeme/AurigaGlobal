#pragma once

#include <entt/entt.hpp>

namespace ecs::ViewSystem {

// Disconnect directed view edges and publish removal without resolving CEntity.
// Implementations live with the common visibility reconciliation engine.
void ViewCleanup(entt::entity self);
void ViewReencode(entt::entity self);

// Broadcast to current nearby viewers. Recipients and identity are snapshotted.
void PacketView(entt::entity self, const void* data, int bytes,
                entt::entity except = entt::null);

} // namespace ecs::ViewSystem
