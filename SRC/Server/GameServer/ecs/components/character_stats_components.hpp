#pragma once

#include <cstdint>

#include <common/length.h>

namespace ecs {

struct CharacterStatsComponent {
    int64_t points[POINT_MAX_NUM] {};
    int32_t bonusFractionRaw[POINT_MAX_NUM] {};
    bool precisionWrite { false };
    int precisionEntryType { -1 };
    int precisionActiveType { -1 };
    double bonusPreviousValue { 0 };
    uint64_t bonusCatalogRevision { 0 };
    bool recomputing { false };
};

} // namespace ecs
