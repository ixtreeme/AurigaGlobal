#pragma once

#include <map>
#include <string>
#include <unordered_set>

#include <cstdint>

#include "../../event.h"

class CPetSystem;
class CNewPetSystem;

#include "../../buff_on_attributes.h"

namespace ecs {

struct InteractionCounters {
    uint8_t chat { 0 };
    uint8_t mount { 0 };
};

struct ProtectionTimes {
    std::map<std::string, int, std::less<>> values;
};

// The dead, stun and recovery events CHARACTER used to hold as members.
// They stay legacy LPEVENTs - this is where they live, not what they are -
// so that the functions that cancel and recreate them can take an entity.
// The code carried "deferred until ECS component covers this" notes for
// exactly this move.
struct LegacyCharEvents {
    LPEVENT dead { nullptr };
    LPEVENT stun { nullptr };
    LPEVENT recovery { nullptr };
    LPEVENT fishing { nullptr };
    LPEVENT timed { nullptr };
    LPEVENT warp { nullptr };
    LPEVENT warpNPC { nullptr };
    LPEVENT battlePassStayOnline { nullptr };
    LPEVENT drop { nullptr };
    LPEVENT mining { nullptr };
    LPEVENT destroyWhenIdle { nullptr };
    LPEVENT save { nullptr };
};

// The pulse of the last /gotoxy and the last savepoint write; each command
// waits ten seconds after its own.
struct CommandCooldowns {
    int goToXYPulse { 0 };
    int savePointPulse { 0 };
};

struct CharacterRuntimeFlagsComponent {
    uint32_t aiFlag = 0;
    int32_t instantFlag = 0;
    int32_t position = 0;
    uint32_t immuneFlag = 0;
    uint32_t lastShoutPulse = 0;
    uint8_t gmLevel = 0;
    uint8_t blockMode = 0;
    float rotation = 0.0f;
};

// Who currently owns this character's movement sync, and the reverse edge.
// Legacy kept these as m_pkChrSyncOwner plus a CHARACTER_LIST of the characters
// this one owns; the list was only ever iterated, appended to and removed from
// by identity, so a set says the same thing and makes the removal O(1).
struct LastSyncTime {
    timeval tv { 0, 0 };
};

struct DungeonTicketExtraMetin {
    bool value { false };
};

struct SyncOwner {
    entt::entity owner { entt::null };
    float syncTime { 0.0f };
};

struct SyncOwned {
    std::unordered_set<entt::entity> owned;
};

struct MountInventoryRef {
    entt::entity inventory { entt::null };
};

// Prevent duplicate account-inventory queries while the asynchronous DB
// response is in flight.  The account id is captured so a stale response
// cannot populate a character that has since changed accounts.
struct MountInventoryLoadState {
    uint32_t accountId { 0 };
    uint64_t requestId { 0 };
};

struct PetRuntimeRefs {
#ifdef __PET_SYSTEM__
    CPetSystem* petSystem { nullptr };
#endif
#ifdef __NEWPET_SYSTEM__
    CNewPetSystem* newPetSystem { nullptr };
    int eggVID { 0 };
#endif
};

} // namespace ecs
