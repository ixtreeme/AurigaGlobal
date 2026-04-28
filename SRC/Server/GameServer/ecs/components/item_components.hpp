#pragma once

#include <array>
#include <cstdint>

#include <common/length.h>
#include <common/tables.h>

namespace ecs {

struct ItemIdentity {
    uint32_t id { 0 };
    uint32_t vnum { 0 };
    uint32_t vid { 0 };
    uint32_t maskVnum { 0 };
};

struct ItemLocation {
    uint8_t window { 0 };
    uint16_t cell { 0 };
};

struct ItemCount {
    int count { 0 };
};

struct ItemPrototypeMeta {
    uint8_t type { 0 };
    uint8_t subType { 0 };
};

struct ItemOwner {
    uint32_t ownerPID { 0 };
    uint32_t lastOwnerPID { 0 };
    uint32_t ownershipPID { 0 };
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

} // namespace ecs
