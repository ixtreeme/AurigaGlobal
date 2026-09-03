#pragma once

#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

#include "../../char.h"

namespace ecs {

struct DungeonDamage {
    std::unordered_map<int, int> highestByRace;
};

struct AffectList {
    std::list<CAffect*> affects;
    std::vector<TAffectSkills> skillAffects;
    TAffectFlag flags;
    bool isLoaded;
};

struct StatusFlags {
    bool isGM : 1;
    bool isInvisible : 1;
    bool isStunned : 1;
    bool isPolymorph : 1;
    bool isDead : 1;
    bool isInvincible : 1;
    // isMountActive is about the RIDER: this player is currently riding.
    // isPet / isMount / isNewPet are about the CREATURE: this character IS
    // a pet or a mount. Opposite subjects - they were being conflated, see
    // CHARACTER::SetMount and pc_is_mount.
    bool isMountActive : 1;
    bool isPet : 1;
    bool isMount : 1;
    bool isNewPet : 1;
    bool isObserverMode : 1;
    bool hasPoisoned : 1;
    bool hasBled : 1;
    bool blockExp : 1;
    bool cannotDead : 1;
    bool detailLog : 1;
    bool monsterLog : 1;
    bool isKillerMode : 1;
    bool isSpawnState : 1;
    bool isPartyState : 1;
    bool isArenaObserver : 1;
};

struct PolymorphState {
    uint32_t raceVnum;
    bool maintainStat;
};

struct ImmunityFlags {
    uint32_t flags { 0 };
};

struct AffectEventState {
    LPEVENT poisonEvent;
#ifdef ENABLE_WOLFMAN_CHARACTER
    LPEVENT bleedingEvent;
#endif
    LPEVENT fireEvent;
};

struct DeadTag {};
struct StunTag {};
struct ObserverModeTag {};
struct SafeZoneTag {};
struct PoisonTag {};
struct BleedTag {};
struct FireTag {};

} // namespace ecs
