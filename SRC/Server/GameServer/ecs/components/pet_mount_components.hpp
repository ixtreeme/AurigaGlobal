#pragma once

#include <array>
#include <cstdint>

#include <common/length.h>

#include <entt/entt.hpp>

namespace ecs {

struct PetComponent {
    entt::entity owner { entt::null };
    uint32_t itemID { 0 };
    uint32_t itemVID { 0 };
    uint32_t itemVnum { 0 };
    std::array<int32_t, ITEM_SOCKET_MAX_NUM> sockets {};
    uint32_t level { 0 };
    uint32_t state { 0 };
};

struct MountComponent {
    entt::entity owner { entt::null };
    uint32_t itemID { 0 };
    uint32_t itemVID { 0 };
    uint32_t itemVnum { 0 };
    std::array<int32_t, ITEM_SOCKET_MAX_NUM> sockets {};
    uint32_t level { 0 };
    uint32_t state { 0 };
};

} // namespace ecs
