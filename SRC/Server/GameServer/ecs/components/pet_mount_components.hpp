#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

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

// Authoritative runtime state for regular pets. One record per stable vnum;
// the spawned creature and the summon item are entity handles. The buff
// snapshot is kept so removal survives an item prototype change.
struct PetActorState {
    uint32_t vnum { 0 };
    uint32_t options { 0 };
    entt::entity character { entt::null };
    entt::entity summonItem { entt::null };
    uint32_t ridingVnum { 0 };
    std::array<TItemApply, ITEM_APPLY_MAX_NUM> buffApplies {};
};

struct PetRuntime {
    uint32_t updatePeriod { 400 };
    uint32_t lastUpdateTime { 0 };
    LPEVENT updateEvent;
    bool destroying { false };
    std::vector<PetActorState> actors;
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

// The pet enchant level, and when a seed or moon bottle was last used.
// Both were CHARACTER fields only the item code read.
struct PetEnchant {
    int value { 0 };
};

struct SeedBottleTime {
    int pulse { 0 };
};

// The VID of the pet egg a player most recently requested a name for. It was
// a field of the deleted new-pet system reference block.
struct NewPetEggVID {
    int vid { 0 };
};

// Authoritative runtime state for the growth ("new") pet. One record per
// stable vnum; the spawned creature and the summon seal are entity handles.
// The defaults mirror the deleted CNewPetActor constructor.
struct NewPetActorState {
    struct FeedSelection {
        entt::entity item { entt::null };
        int cell { -1 };
    };

    uint32_t vnum { 0 };
    uint32_t options { 0 };
    entt::entity character { entt::null };
    entt::entity summonItem { entt::null };
    uint32_t ridingVnum { 0 };
    uint32_t vid { 0 };
    uint32_t level { 1 };
    int levelStep { 0 };
    int expFromMob { 0 };
    int expFromItem { 0 };
    int expItem { 0 };
    int evolution { 0 };
    int timePet { 0 };
    int slotImm { 0 };
    uint32_t immTime { 0 };
    std::array<FeedSelection, 9> feedItems {};
    std::array<int, 4> skill {};
    std::array<int, 4> skillSlot { -1, -1, -1, -1 };
    std::array<std::array<int, 2>, 3> bonusPet {{{ 69, 0 }, { 63, 0 }, { 119, 0 }}};
    uint32_t exp { 0 };
    uint32_t lastActionTime { 0 };
    uint32_t summonItemVID { 0 };
    uint32_t summonItemID { 0 };
    uint32_t summonItemVnum { 0 };
    uint32_t duration { 0 };
    uint32_t totalDuration { 0 };
#ifdef ENABLE_NEW_PET_EDITS
    int32_t minAge { 0 };
    uint32_t minAgeStart { 0 };
    uint8_t ageTier { 0 };
#endif
    int16_t originalMoveSpeed { 0 };
    std::string name;
};

struct NewPetRuntime {
    uint32_t updatePeriod { 400 };
    uint32_t lastUpdateTime { 0 };
    LPEVENT updateEvent;
    LPEVENT expireEvent;
    bool destroying { false };
    std::vector<NewPetActorState> actors;
};

} // namespace ecs
