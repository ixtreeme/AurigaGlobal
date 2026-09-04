#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <common/length.h>
#include <common/tables.h>

#include "../../debug_allocator.h"
#include "../../event.h"

class CItem;

namespace ecs {

struct LegacyItemPtr {
    CItem* ptr { nullptr };
};

struct ItemIdentity {
    uint32_t id { 0 };
    uint32_t vnum { 0 };
    uint32_t originalVnum { 0 };
    uint32_t vid { 0 };
    uint32_t maskVnum { 0 };
    uint32_t sigVnum { 0 };
    int32_t specialGroup { 0 };
    uint32_t transmutationVnum { 0 };
};

struct ItemLocation {
    uint8_t window { 0 };
    uint16_t cell { 0 };
};

struct ItemGroundPosition {
    int32_t x { 0 };
    int32_t y { 0 };
    int32_t z { 0 };
};

struct ItemCount {
    int count { 0 };
};

struct ItemPrototypeMeta {
    uint8_t type { 0 };
    uint8_t subType { 0 };
};

// The timers an item carries. They were eight LPEVENT members on CItem, and
// every use is one of four shapes - a null test, an assignment, event_cancel
// on the address, or event_time - so they move together into one component
// handed out by reference.
// Optional per-item bonus table, set once from the extra-proto manager.
struct ItemExtraProtoRef {
    TItemExtraProto* proto { nullptr };
};

struct ItemEvents {
    LPEVENT destroy { nullptr };
    LPEVENT expire { nullptr };
    LPEVENT soulItem { nullptr };
    LPEVENT uniqueExpire { nullptr };
    LPEVENT timerBasedOnWearExpire { nullptr };
    LPEVENT realTimeExpire { nullptr };
    LPEVENT accessorySocketExpire { nullptr };
    LPEVENT ownership { nullptr };
};

struct ItemOwner {
    // The owner entity is the identity; ownerPID is the persisted form of it.
    // They are not interchangeable - a character carrying an item before its
    // PID is known, or one that has none at all, has a valid entity and a zero
    // PID - so the entity is stored rather than derived from the PID.
    entt::entity owner { entt::null };
    uint32_t ownerPID { 0 };
    uint32_t lastOwnerPID { 0 };
    uint32_t ownershipPID { 0 };
};

struct ItemOwnershipDisplay {
    std::string ownerName;
};

struct ItemEquipped {
    bool equipped { false };
    uint8_t slot { 0 };
};

struct ItemFlags {
    int32_t flags { 0 };
    bool exchanging { false };
    bool skipSave { false };
    bool isLocked { false };
};

struct ItemSockets {
    std::array<int32_t, ITEM_SOCKET_MAX_NUM> sockets {};
};

struct ItemAttributes {
    std::array<TPlayerItemAttribute, ITEM_ATTRIBUTE_MAX_NUM> attrs {};
};

struct ItemLockedAttribute {
    short index { -1 };
};

} // namespace ecs
