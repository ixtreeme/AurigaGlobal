#pragma once

#include <array>
#include <cstdint>

#include <common/length.h>

#include <entt/entt.hpp>

namespace ecs {

// Creature-side state: independent of the regular pet's owner-side snapshot.
struct GrowthPetComponent {
    entt::entity owner { entt::null };
    entt::entity item { entt::null };
    uint32_t level { 1 };
};

struct NewPetSkillState {
    std::array<uint32_t, 4> cooldowns {};
    entt::entity immortalSource { entt::null };
};

struct PetComponent {
    entt::entity owner { entt::null };
    entt::entity item { entt::null };
    uint32_t itemID { 0 };
    uint32_t itemVID { 0 };
    uint32_t itemVnum { 0 };
    std::array<int32_t, ITEM_SOCKET_MAX_NUM> sockets {};
    uint32_t level { 0 };
    uint32_t state { 0 };
};

struct MountComponent {
    entt::entity owner { entt::null };
    entt::entity item { entt::null };
    uint32_t itemID { 0 };
    uint32_t itemVID { 0 };
    uint32_t itemVnum { 0 };
    std::array<int32_t, ITEM_SOCKET_MAX_NUM> sockets {};
    uint32_t level { 0 };
    uint32_t state { 0 };
};

// The pet enchant level, and when a seed or moon bottle was last used.
// Both were CHARACTER fields only the item code read.
struct PetEnchant {
    int value { 0 };
};

struct SeedBottleTime {
    int pulse { 0 };
};

} // namespace ecs
