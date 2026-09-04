#pragma once

#include <map>
#include <unordered_set>

#include <cstdint>

#include "../../event.h"

class CPetSystem;
class CMountSystem;
class CNewPetSystem;

class CBuffOnAttributes;

class CMountInventory;

namespace ecs {

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

// The mount subsystem, so the skin and unsummon paths can be reached from an
// entity. Kept beside PetRuntimeRefs, which already does this for pets.
// The per-attribute buff pools a character carries, keyed by point type. The
// pointers are owned here: PlayerRuntime::BuffOnAttr_Destroy frees them when
// the character is torn down, the same job CHARACTER's destructor used to do.
struct BuffOnAttrs {
    std::map<uint8_t, CBuffOnAttributes*> pools;
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
    CMountInventory* inventory { nullptr };
};

struct MountRuntimeRefs {
    CMountSystem* mountSystem { nullptr };
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
