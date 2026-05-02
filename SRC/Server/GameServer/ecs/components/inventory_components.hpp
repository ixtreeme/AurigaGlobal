#pragma once

#include <array>
#include <cstdint>

#include <common/tables.h>

#include "../../safebox.h"
#include "../../typedef.h"
#include "../../cuberenewal.h"
#include "../../attr_transfer.h"
#include "item_components.hpp"

namespace ecs {

struct EquipmentSlots {
    std::array<LPITEM, WEAR_MAX_NUM> items {};
};

struct InventoryGrid {
    std::array<LPITEM, INVENTORY_MAX_NUM> items {};
};

struct MainInventoryRuntimeComponent {
    LPITEM pItems[INVENTORY_AND_EQUIP_SLOT_MAX] {};
    uint16_t bItemGrid[INVENTORY_AND_EQUIP_SLOT_MAX] {};
};

#ifdef ENABLE_EXTRA_INVENTORY
struct ExtraInventoryRuntimeComponent {
    LPITEM pItems[EXTRA_INVENTORY_MAX_NUM] {};
    uint16_t wItemGrid[EXTRA_INVENTORY_MAX_NUM] {};
};
#endif

struct CubeWindowComponent {
    LPITEM pItems[CUBE_MAX_NUM] {};
    LPCHARACTER pNpc { nullptr };
};

struct DragonSoulInventoryComponent {
    LPITEM pItems[DRAGON_SOUL_INVENTORY_MAX_NUM] {};
    uint16_t wItemGrid[DRAGON_SOUL_INVENTORY_MAX_NUM] {};
};

struct DragonSoulRuntimeStateComponent {
    int32_t activeDeck { -1 };
    LPENTITY pRefineWindowOpener { nullptr };
};

#ifdef __ATTR_TRANSFER_SYSTEM__
struct AttrTransferWindowComponent {
    LPITEM pItems[MAX_ATTR_TRANSFER_SLOT] {};
    LPCHARACTER pNpc { nullptr };
};
#endif

#ifdef ENABLE_ACCE_SYSTEM
struct AcceWindowComponent {
    LPITEM pMaterials[ACCE_WINDOW_MAX_MATERIALS] {};
    bool combinationOpen { false };
    bool absorptionOpen { false };
};
#endif

#ifdef ENABLE_SWITCHBOT
struct SwitchbotRuntimeComponent {
    LPITEM pItems[SWITCHBOT_SLOT_COUNT] {};
};
#endif

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
