#pragma once

#include <cstdint>

class CPetSystem;
class CNewPetSystem;

namespace ecs {

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
