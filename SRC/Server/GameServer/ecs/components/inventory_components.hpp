#pragma once

#include <array>
#include <cstdint>
#include <entt/entity/entity.hpp>

#include <common/tables.h>

#include "../../typedef.h"
#include "../../cuberenewal.h"
#include "../../attr_transfer.h"
#include "../../MountInventory.h"
#include "item_components.hpp"

namespace ecs {

struct MainInventoryRuntimeComponent {
    std::array<entt::entity, INVENTORY_AND_EQUIP_SLOT_MAX> items;
    std::array<uint16_t, INVENTORY_AND_EQUIP_SLOT_MAX> itemGrid {};

    MainInventoryRuntimeComponent() { items.fill(entt::null); }
};

// Set once the database item load for this player has been applied.
struct ItemLoadState {
    bool loaded { false };
};

// Drop rate limits: gold drops are g_GoldDropTimeLimitValue apart, and with
// ENABLE_ANTICHEAT more than four item drops within 25 pulses disconnect.
struct DropLimiter {
    uint32_t lastGoldDropTime { 0 };
    int32_t lastItemDropPulse { 0 };
    int32_t itemDropCount { 0 };
};

// The key expansion and the extra inventory unlock share a cooldown: the
// global time from which the next unlock is allowed.
struct InventoryUnlockCooldown {
    int32_t until { 0 };
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
    entt::entity npc { entt::null };

    CubeWindowComponent() { items.fill(entt::null); }
};

struct DragonSoulInventoryComponent {
    std::array<entt::entity, DRAGON_SOUL_INVENTORY_MAX_NUM> items;
    std::array<uint16_t, DRAGON_SOUL_INVENTORY_MAX_NUM> itemGrid {};

    DragonSoulInventoryComponent() { items.fill(entt::null); }
};

struct DragonSoulRuntimeStateComponent {
    int32_t activeDeck { -1 };
    entt::entity refineWindowOpener { entt::null };
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
    std::array<TQuickslot, QUICKSLOT_MAX_NUM> slots {};
    uint64_t revision { 0 };
};

// Account-scoped mount inventory state.  The character stores only the
// registry handle (MountInventoryRef); all mutable inventory data lives on
// this registry-owned entity so no heap inventory or raw owner pointer is
// required.  The wire format and legacy grid are 12 columns by 16 rows.
struct MountInventoryComponent {
    static constexpr uint8_t Width = MOUNT_INVENTORY_WIDTH;
    static constexpr uint8_t MaxHeight = MOUNT_INVENTORY_MAX_HEIGHT;
    static constexpr uint16_t SlotCount = Width * MaxHeight;

    entt::entity owner { entt::null };
    uint32_t accountId { 0 };
    uint8_t height { MaxHeight };
    bool loaded { false };
    bool destroying { false };
    std::array<entt::entity, SlotCount> items;
    std::array<uint8_t, SlotCount> occupied {};

    MountInventoryComponent() { items.fill(entt::null); }
};

// Authoritative state for one open safebox or mall window. The storage is a
// registry-owned entity published by SafeboxRef on its owner. Items are
// entities, the occupancy array replaces the legacy CGrid, and destroying is
// the reentrancy guard for teardown. Operations revalidate the entity after
// callbacks instead of keeping a shared_ptr alive; the operation count defers
// owner-driven retirement until an in-flight operation has finished.
struct SafeboxStorageComponent {
    static constexpr int Width = 16;
    static constexpr int MaxHeight = SAFEBOX_MAX_NUM / Width;

    entt::entity owner { entt::null };
    uint8_t windowMode { SAFEBOX };
    int size { 0 };
    int32_t gold { 0 };
    bool destroying { false };
    bool retirePending { false };
    int operations { 0 };
    std::array<entt::entity, SAFEBOX_MAX_NUM> items;
    std::array<uint8_t, SAFEBOX_MAX_NUM> occupied {};

    SafeboxStorageComponent() { items.fill(entt::null); }
};

struct SafeboxRef {
    entt::entity safebox { entt::null };
    entt::entity mall { entt::null };
    int safeboxSize { -1 };
    int safeboxLoadTime { 0 };
    int mallLoadTime { 0 };
    bool isOpening { false };
    int32_t openX { -1000 };
    int32_t openY { -1000 };
    bool closingSafebox { false };
    bool closingMall { false };
};

} // namespace ecs
