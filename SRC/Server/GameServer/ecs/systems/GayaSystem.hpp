#pragma once

#include <cstdint>
#include <string>

#include <entt/entt.hpp>

namespace GayaSystem {

void Load(entt::entity pc);
bool CheckItemsFull(entt::entity pc);
void ClearMarket(entt::entity pc);
void InfoMarket(entt::entity pc);
bool CheckSlot(entt::entity pc, int slot);
void BuyItems(entt::entity pc, int slot);
void RefreshItemsMarket(entt::entity pc);
void UpdateSlot(entt::entity pc, int slot);
void UpdateItems0(entt::entity pc);
void UpdateItems(entt::entity pc);
void CraftItems(entt::entity pc, int slot);
void MarketItems(entt::entity pc, int slot);
void RefreshItems(entt::entity pc);
int GetState(entt::entity pc, const std::string& state);
void SetState(entt::entity pc, const std::string& state, int value);
void StartCheckTimeMarket(entt::entity pc);

} // namespace GayaSystem
