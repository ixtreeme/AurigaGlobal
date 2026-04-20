#pragma once

#include <cstdint>

#include <common/tables.h>

namespace ecs {

struct AppearancePartsComponent {
    uint16_t parts[PART_MAX_NUM] {};
    uint8_t basePart { 0 };
};

} // namespace ecs
