#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "../../packet.h"

namespace NetworkSyncSystem {

void UpdatePacket(entt::entity e);
void MainCharacterPacket(entt::entity e);
void PointsPacket(entt::entity e);
bool BuildCharAdditionalInfo(entt::registry& reg, entt::entity source, TPacketGCCharacterAdditionalInfo& packet);
void SendCharAdditionalInfo(entt::registry& reg, entt::entity source, entt::entity recipient);
void BroadcastCharAdditionalInfo(entt::registry& reg, entt::entity source);

} // namespace NetworkSyncSystem

void NetworkSyncSystem_Update(entt::registry& reg, uint32_t tick);
