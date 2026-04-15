#pragma once

#include <cstdint>

namespace ecs {

struct Position {
    int32_t x, y, z;
};

struct WarpPosition {
    int32_t x, y;
    int32_t mapIndex;
};

struct ExitPosition {
    int32_t x, y;
    int32_t mapIndex;
};

struct RegenPosition {
    int32_t x, y;
    float angle;
};

struct RotationComponent { float yaw; };
struct MapIndex { int32_t value; };

} // namespace ecs
