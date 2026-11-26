#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include <common/length.h>
#include "../../world/event.h"

#include <entt/entt.hpp>

namespace ecs {

// The persistent horse data and owned timer handles of a rider. MountState
// remains the sole source of the riding flag; THorseInfo is a DB snapshot only.
struct HorseRuntime {
    uint8_t level { 0 };
    int16_t health { 0 };
    int16_t stamina { 0 };
    uint64_t healthDropTime { 0 };
    uint64_t timerRevision { 0 };
    LPEVENT regen;
    LPEVENT consume;
};

// The visible level of a typed growth-pet follower, ready before its first insert.
struct TypedPetLevel {
    uint16_t value { 1 };
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

// World followers are projections of typed seals. Their lifetime, progression
// and bonuses live in the typed item state; only the creature is an entity.
struct PetActorState {
    uint32_t vnum { 0 };
    uint32_t options { 0 };
    entt::entity character { entt::null };
    uint64_t typedSeal { 0 };
};

struct PetRuntime {
    uint32_t updatePeriod { 400 };
    uint32_t lastUpdateTime { 0 };
    LPEVENT updateEvent;
    bool destroying { false };
    // Actor lookups remain stable when a different follower form is added.
    std::deque<PetActorState> actors;
};

// Authoritative runtime state for costume mounts. A record is keyed by the
// stable mount vnum; the spawned creature and summon item are entity handles.
struct CostumeMountActorState {
    uint32_t vnum { 0 };
    entt::entity character { entt::null };
    entt::entity summonItem { entt::null };
    uint32_t lastActionTime { 0 };
};

struct CostumeMountRuntime {
    uint32_t updatePeriod { 400 };
    uint32_t lastUpdateTime { 0 };
    LPEVENT updateEvent;
    bool destroying { false };
    std::vector<CostumeMountActorState> actors;
};

// The rider on this horse. CHARACTER kept it as m_chRider, a pointer the
// horse held to its rider, and SetRider/GetRider were the whole interface.
struct HorseRider {
    entt::entity rider { entt::null };
};

struct SeedBottleTime {
    int pulse { 0 };
};

} // namespace ecs
