#pragma once

#include <cstdint>

#include "../../event.h"

namespace ecs {

struct FishingState {
    LPEVENT fishingNewEvent { nullptr };
    uint32_t fishVnum { 0 };
    uint32_t chance { 0 };
    uint32_t elapsedSeconds { 0 };
    uint8_t catchCount { 0 };
    uint32_t catchFailed { 0 };
    int32_t lastCatchTime { 0 };
};

struct FishingActiveTag {};

} // namespace ecs
