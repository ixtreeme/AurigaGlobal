#pragma once
#include <entt/entity/fwd.hpp>
#include <cstdint>

// Legacy item shop (ENABLE_ITEMSHOP) for a character on the native item system:
// one store commit takes the Sárkányérme from the account wallet and puts the
// native item into the character's inventory, or does neither. Returns false
// when the character is not on the native item system (legacy path applies).
bool BeginTypedItemShopBuy(entt::entity character, std::uint32_t shopId, int count, bool logOpen);
