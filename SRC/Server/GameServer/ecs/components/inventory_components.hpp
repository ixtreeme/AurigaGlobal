#pragma once
#include <array>
#include <cstdint>
#include <common/tables.h>

namespace ecs {
struct ItemLoadState { bool loaded {false}; };
struct GoldAmount { int64_t amount {0}; };
struct QuickSlots {
    std::array<TQuickslot, QUICKSLOT_MAX_NUM> slots {};
    uint64_t revision {0};
};
struct SafeboxRef {
    int safeboxLoadTime {0};
    bool isOpening {false};
};
}
