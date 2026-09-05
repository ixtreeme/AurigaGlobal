#pragma once

#include <cstdint>
#include <list>
#include <utility>
#include <unordered_map>
#include <vector>

#include "../../char.h"

namespace ecs {

struct DungeonDamage {
    std::unordered_map<int, int> highestByRace;
};

struct AffectList {
    // The registry owns live affects. Short-lived leases protect a callback's
    // input when a nested operation removes an affect or destroys its owner.
    std::list<std::shared_ptr<CAffect>> affects;
    std::vector<TAffectSkills> skillAffects;
    TAffectFlag flags;
    bool isLoaded { false };
    uint64_t refreshToken { 0 };
    uint64_t expiryToken { 0 };
    uint64_t horseNameToken { 0 };
    // Reentrant operations of the same type invalidate an unfinished add.
    std::unordered_map<uint32_t, uint64_t> mutationTokens;
};

// Persistent deadline, independent of clearing or replacing the live affects.
struct BattlePassTiming {
    uint32_t deadline { 0 };
};

// Own only the affect/recovery scheduler here. Its callback still delegates the
// unmigrated recovery/expiry rules, but never owns a CHARACTER pointer.
struct AffectTickState {
    LPEVENT timer;
    uint64_t startingToken { 0 };
    AffectTickState() = default;
    AffectTickState(const AffectTickState&) = delete;
    AffectTickState& operator=(const AffectTickState&) = delete;
    AffectTickState(AffectTickState&& other) noexcept
        : timer(std::move(other.timer)), startingToken(std::exchange(other.startingToken, 0)) {}
    AffectTickState& operator=(AffectTickState&& other) noexcept {
        if (this != &other) {
            event_cancel(&timer);
            timer = std::move(other.timer);
            startingToken = std::exchange(other.startingToken, 0);
        }
        return *this;
    }
    ~AffectTickState() { event_cancel(&timer); }
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
