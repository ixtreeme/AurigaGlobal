#pragma once

#ifdef ENABLE_SORT_INVEN

#include <entt/entt.hpp>
#include <cstdint>

// Server-authoritative sort for the typed backpack and the selected extra category.
namespace InventorySortSystem {

void SortInventory(entt::entity character);
void SortExtraInventory(entt::entity character, std::uint8_t category);

} // namespace InventorySortSystem

#endif // ENABLE_SORT_INVEN
