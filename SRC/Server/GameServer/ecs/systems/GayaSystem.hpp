#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace GayaSystem {

void Load(entt::entity pc);
void OpenTypedMarket(entt::entity pc);
void BuyTypedMarketOffer(entt::entity pc,std::uint64_t session,std::uint32_t slot);
void CraftItems(entt::entity pc, int slot);

} // namespace GayaSystem
