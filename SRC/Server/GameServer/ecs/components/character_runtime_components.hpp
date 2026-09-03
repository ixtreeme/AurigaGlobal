#pragma once

#include <cstdint>

#include "../../event.h"

class CPetSystem;
class CMountSystem;
class CNewPetSystem;

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
