#pragma once
#include <entt/entity/fwd.hpp>
#include <cstdint>

// A single native commit grants the offer and records the account wallet mutation.
bool BeginTypedItemShopBuy(entt::entity character, std::uint32_t shopId, int count, std::uint64_t quote);
void SendTypedItemShopCatalog(entt::entity character);
void SendTypedItemShopHistory(entt::entity character, std::uint32_t offset);
