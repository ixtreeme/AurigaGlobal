#pragma once

#include <cstdint>

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

} // namespace ecs
