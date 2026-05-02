#pragma once

#include <cstdint>

#include <common/item_length.h>
#include <common/tables.h>

namespace ecs {

// Snapshot of proto-derived item data. Populated at CreateItemEntity so
// read accessors do not need to resolve LPITEM just to inspect proto fields.
struct ItemProtoRef {
    uint32_t base_vnum { 0 };
    uint8_t type { 0 };
    uint8_t subtype { 0 };

    uint32_t weapon_min { 0 };
    uint32_t weapon_max { 0 };
    uint32_t defense { 0 };
    uint32_t magic_min { 0 };
    uint32_t magic_max { 0 };

    char name[ITEM_NAME_MAX_LEN + 1] {};
    uint8_t size { 0 };

    uint8_t level_limit { 0 };
    uint32_t wear_flags { 0 };
    uint32_t anti_flags { 0 };
    uint32_t immune_flags { 0 };

    uint32_t refined_vnum { 0 };
    uint8_t refine_level { 0 };

    int8_t limit_timer_wear_index { -1 };

    // Transitional compatibility pointer for generic proto reads.
    // Remove when all callers use explicit ECS proto fields.
    const TItemTable* proto { nullptr };
};

} // namespace ecs
