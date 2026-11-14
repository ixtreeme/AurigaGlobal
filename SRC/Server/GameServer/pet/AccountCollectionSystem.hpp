#pragma once
#include "CollectionBonusRules.hpp"
#include <entt/entt.hpp>
namespace AccountCollectionSystem {
CollectionBonuses::Totals Collect(entt::entity owner, uint8_t window);
void Compute(entt::entity owner, uint8_t window);
void Send(entt::entity owner, uint8_t window);
bool Persist(const char* sql, uint32_t account, uint8_t window);
void Invalidate(uint32_t account, uint8_t window);
}
