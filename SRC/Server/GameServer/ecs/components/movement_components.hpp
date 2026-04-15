#pragma once

#include <cstdint>
#include <winsock2.h>

namespace ecs {

struct MovementDestination {
    int32_t x, y;
};

struct MovementState {
    uint32_t moveStartTime;
    uint32_t moveDuration;
    uint32_t lastMoveTime;
    uint32_t lastAttackTime;
    uint32_t walkStartTime;
    uint32_t stopTime;
    bool isWalking;
    bool isNowWalking;
    bool staminaConsume;
};

struct MovementSpeed {
    int32_t walk;
    int32_t run;
};

struct SyncState {
    float lastSyncTime;
    int syncHackCount;
    timeval lastSyncTimeval;
};

} // namespace ecs
