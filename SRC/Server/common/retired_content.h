#pragma once
#include <cstdint>
#include "item_length.h"

namespace auriga::retired_content
{
// These item/subtype codes belonged to the retired DragonSoul feature. Keep
// their numeric positions occupied, but never hydrate or create such content.
constexpr bool Item(std::uint8_t type, std::uint8_t subtype) noexcept
{
    return type == ITEM_RETIRED_29 || type == ITEM_RETIRED_30 || type == ITEM_RETIRED_31 ||
        (type == ITEM_MATERIAL && subtype >= MATERIAL_RETIRED_5 && subtype <= MATERIAL_RETIRED_7) ||
        (type == ITEM_USE && (subtype == USE_RETIRED_27 || subtype == USE_RETIRED_28 || subtype == USE_RETIRED_42));
}
constexpr bool Affect(std::uint32_t type) noexcept
{
    return (type >= 540 && type <= 542) || (type >= 563 && type <= 566);
}
} // namespace auriga::retired_content
