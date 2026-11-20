#pragma once

#include <cstdint>
#include <string_view>

#include <entt/entt.hpp>

namespace GayaSystem {

void Load(entt::entity pc);
void OpenTypedMarket(entt::entity pc);
void BuyTypedMarketOffer(entt::entity pc,std::uint64_t session,std::uint32_t slot);
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
int GetState(entt::entity pc, std::string_view state);
void SetState(entt::entity pc, std::string_view state, int value);
void StartCheckTimeMarket(entt::entity pc);

} // namespace GayaSystem
