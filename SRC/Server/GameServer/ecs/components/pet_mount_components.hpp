#pragma once

#include <array>
#include <cstdint>

#include <common/length.h>
#include "../../event.h"

#include <entt/entt.hpp>

namespace ecs {

// The persistent horse data and owned timer handles of a rider. MountState
// remains the sole source of the riding flag; THorseInfo is a DB snapshot only.
struct HorseRuntime {
    uint8_t level { 0 };
    int16_t health { 0 };
    int16_t stamina { 0 };
    uint32_t healthDropTime { 0 };
    uint64_t timerRevision { 0 };
    LPEVENT regen;
    LPEVENT consume;
};

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

// Creature-side ownership for costume mounts. The actor map is keyed by
// vnum, but cleanup must also work when skins or transmutations change it.
struct MountOwner {
    entt::entity owner { entt::null };
};

// The rider on this horse. CHARACTER kept it as m_chRider, a pointer the
// horse held to its rider, and SetRider/GetRider were the whole interface.
struct HorseRider {
    entt::entity rider { entt::null };
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
