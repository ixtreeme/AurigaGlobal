#pragma once

#include <cstdint>
#include <string>

#include <entt/entt.hpp>

#include "../../packet.h"

namespace NetworkSyncSystem {

bool SetSyncOwner(entt::entity e, entt::entity owner, bool bRemoveFromList = true);
bool IsSyncOwner(entt::entity e, entt::entity candidate);
void ClearSync(entt::entity e);


void UpdatePacket(entt::entity e);
void MainCharacterPacket(entt::entity e);
void PointsPacket(entt::entity e);
void SendQuickslotAdd(entt::entity e, uint8_t pos, TQuickslot slot);
void SendQuickslotDelete(entt::entity e, uint8_t pos);
void SendQuickslotSwap(entt::entity e, uint8_t from, uint8_t to);
bool BuildCharAdditionalInfo(entt::registry& reg, entt::entity source, TPacketGCCharacterAdditionalInfo& packet);
void SendCharAdditionalInfo(entt::registry& reg, entt::entity source, entt::entity recipient);
void BroadcastCharAdditionalInfo(entt::registry& reg, entt::entity source);
bool BuildCharacterUpdatePacket(entt::registry& reg, entt::entity source, TPacketGCCharacterUpdate& packet);
bool BuildPointsPacket(entt::registry& reg, entt::entity source, TPacketGCPoints& packet);
std::string GetItemOnTitlePrefix(entt::registry& reg, entt::entity source);
std::string GetDisplayedNameWithItemOnTitle(entt::registry& reg, entt::entity source);
void SendItemOnTitleNameToDesc(entt::registry& reg, entt::entity source, entt::entity recipient);
void UpdateItemOnTitleName(entt::registry& reg, entt::entity source, bool force = false);
bool BuildViewEquipmentPacket(entt::registry& reg, entt::entity wearer, TPacketViewEquip& packet);
void SendEquipmentToViewer(entt::registry& reg, entt::entity wearer, entt::entity viewer);
void BroadcastEquipmentChange(entt::registry& reg, entt::entity wearer);
entt::entity FindCharacterInView(entt::registry& reg, entt::entity source, const char* name, bool findPCOnly);
bool BuildPartyUpdatePacket(entt::registry& reg, entt::entity member, TPacketGCPartyUpdate& packet);
void BroadcastSyncPacket(entt::registry& reg, entt::entity source);
void BroadcastEffect(entt::registry& reg, entt::entity source, uint8_t effectType);
void BroadcastSpecificEffect(entt::registry& reg, entt::entity source, const char* effectName);
void SendConfirmWithMsg(entt::registry& reg, entt::entity recipient, const char* message, int timeout, uint32_t requestPID);

} // namespace NetworkSyncSystem

namespace ecs::ItemNetworkSystem {

// Rebuilds and sends the owner-facing item packet. CItem::UpdatePacket is a
// one-line forward to this.
void SendItemUpdate(entt::registry& reg, entt::entity item);

} // namespace ecs::ItemNetworkSystem

void NetworkSyncSystem_Update(entt::registry& reg, uint32_t tick);
