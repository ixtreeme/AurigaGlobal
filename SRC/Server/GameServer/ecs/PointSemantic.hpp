#pragma once

#include <cstdint>

#include "../char.h"

namespace ecs {

inline constexpr bool IsDedicatedAuthorityPoint(uint8_t type)
{
    switch (type) {
    case POINT_NONE:
    case POINT_LEVEL:
    case POINT_EXP:
    case POINT_NEXT_EXP:
    case POINT_HP:
    case POINT_MAX_HP:
    case POINT_SP:
    case POINT_MAX_SP:
    case POINT_STAMINA:
    case POINT_MAX_STAMINA:
    case POINT_GOLD:
    case POINT_INVEN:
    case POINT_GAYA:
        return true;
    default:
        return false;
    }
}

inline constexpr bool IsStatArrayPoint(uint8_t type)
{
    return type < POINT_MAX_NUM && !IsDedicatedAuthorityPoint(type);
}

} // namespace ecs
