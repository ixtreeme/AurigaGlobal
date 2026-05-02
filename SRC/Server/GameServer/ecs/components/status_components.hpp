#pragma once

#include <cstdint>
#include <list>
#include <vector>

#include "../../char.h"

namespace ecs {

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
};

struct PolymorphState {
    uint32_t raceVnum;
    bool maintainStat;
};

struct ImmunityFlags {
    uint32_t flags { 0 };
};

struct DeadTag {};
struct StunTag {};
struct ObserverModeTag {};
struct SafeZoneTag {};
struct PoisonTag {};
struct BleedTag {};
struct FireTag {};

} // namespace ecs
