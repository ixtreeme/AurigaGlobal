#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace NetworkSyncSystem {

void UpdatePacket(entt::entity e);
void MainCharacterPacket(entt::entity e);
void PointsPacket(entt::entity e);

} // namespace NetworkSyncSystem

void NetworkSyncSystem_Update(entt::registry& reg, uint32_t tick);
