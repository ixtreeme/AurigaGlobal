#pragma once
#include <cstdint>

namespace auriga::item
{
// Retained packet numbers for the offline migration period. Native owners must
// never execute these cell/vnum based operations. Pickup (15) is deliberately
// shared with typed ground loot; quickslots also carry skills and emotes.
constexpr bool IsLegacyItemMutation(std::uint8_t header)
{
    switch (header)
    {
    case 11: case 12: case 13: case 20: case 21: case 22: case 23:
    case 27: case 50: case 55: case 57: case 60:
    case 69: case 70: case 71: case 77: case 83: case 96:
    case 125: case 126: case 127: case 146: case 171: case 211: case 215:
        return true;
    default:
        return false;
    }
}
}
