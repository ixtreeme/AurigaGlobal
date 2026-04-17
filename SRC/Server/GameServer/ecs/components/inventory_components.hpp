#pragma once

#include <array>
#include <cstdint>

#include <common/tables.h>

#include "../../safebox.h"
#include "../../typedef.h"
#include "item_components.hpp"

namespace ecs {

struct EquipmentSlots {
    std::array<LPITEM, WEAR_MAX_NUM> items {};
};

struct InventoryGrid {
    std::array<LPITEM, INVENTORY_MAX_NUM> items {};
};

struct GoldAmount { int64_t amount; };

struct QuickSlots {
    std::array<TQuickslot, QUICKSLOT_MAX_NUM> slots;
};

struct SafeboxRef {
    CSafebox* safebox { nullptr };
    CSafebox* mall { nullptr };
    int safeboxSize;
    int safeboxLoadTime;
    int mallLoadTime;
    bool isOpening;
};

} // namespace ecs
