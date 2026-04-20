#pragma once

#include <cstdint>

#include <common/length.h>

namespace ecs {

struct CharacterStatsComponent {
    int64_t points[POINT_MAX_NUM] {};
};

} // namespace ecs
