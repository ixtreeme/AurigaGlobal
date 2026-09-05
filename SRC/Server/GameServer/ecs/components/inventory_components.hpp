#pragma once

#include <array>
#include <cstdint>
#include <entt/entity/entity.hpp>

#include <common/tables.h>

#include "../../safebox.h"
#include "../../typedef.h"
#include "../../cuberenewal.h"
#include "../../attr_transfer.h"
#include "item_components.hpp"

namespace ecs {

struct MainInventoryRuntimeComponent {
    std::array<entt::entity, INVENTORY_AND_EQUIP_SLOT_MAX> items;
    std::array<uint16_t, INVENTORY_AND_EQUIP_SLOT_MAX> itemGrid {};

    MainInventoryRuntimeComponent() { items.fill(entt::null); }
};

#ifdef ENABLE_EXTRA_INVENTORY
struct ExtraInventoryRuntimeComponent {
    std::array<entt::entity, EXTRA_INVENTORY_MAX_NUM> items;
    std::array<uint16_t, EXTRA_INVENTORY_MAX_NUM> itemGrid {};

    ExtraInventoryRuntimeComponent() { items.fill(entt::null); }
};
#endif

struct CubeWindowComponent {
    std::array<entt::entity, CUBE_MAX_NUM> items;
    LPCHARACTER pNpc { nullptr };

    CubeWindowComponent() { items.fill(entt::null); }
};

struct DragonSoulInventoryComponent {
    std::array<entt::entity, DRAGON_SOUL_INVENTORY_MAX_NUM> items;
    std::array<uint16_t, DRAGON_SOUL_INVENTORY_MAX_NUM> itemGrid {};

    DragonSoulInventoryComponent() { items.fill(entt::null); }
};

struct DragonSoulRuntimeStateComponent {
    int32_t activeDeck { -1 };
    LPENTITY pRefineWindowOpener { nullptr };
};

#ifdef __ATTR_TRANSFER_SYSTEM__
struct AttrTransferWindowComponent {
    std::array<entt::entity, MAX_ATTR_TRANSFER_SLOT> items;
    std::array<int, MAX_ATTR_TRANSFER_SLOT> cells;
    entt::entity npc { entt::null };
    bool busy { false };

    AttrTransferWindowComponent() { items.fill(entt::null); cells.fill(-1); }
};
#endif

#ifdef ENABLE_ACCE_SYSTEM
struct AcceWindowComponent {
    std::array<entt::entity, ACCE_WINDOW_MAX_MATERIALS> materials;
    bool combinationOpen { false };
    bool absorptionOpen { false };

    AcceWindowComponent() { materials.fill(entt::null); }
};
#endif

#ifdef ENABLE_SWITCHBOT
struct SwitchbotRuntimeComponent {
    std::array<entt::entity, SWITCHBOT_SLOT_COUNT> items;

    SwitchbotRuntimeComponent() { items.fill(entt::null); }
};
#endif

struct GoldAmount { int64_t amount; };

#ifdef ENABLE_ATTR_COSTUMES
// Per-character dialog state. An invalid command clears the previous choice.
struct CostumeAttributeSelection {
    int rareSlot { 0 };
};
#endif

struct QuickSlots {
    std::array<TQuickslot, QUICKSLOT_MAX_NUM> slots;
};

struct SafeboxRef {
    CSafebox* safebox { nullptr };
    CSafebox* mall { nullptr };
    int safeboxSize { -1 };
    int safeboxLoadTime { 0 };
    int mallLoadTime { 0 };
    bool isOpening { false };
    int32_t openX { -1000 };
    int32_t openY { -1000 };
};

} // namespace ecs
