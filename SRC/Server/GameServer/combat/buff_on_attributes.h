#pragma once
#include <cstdint>
#include <map>

namespace ecs {
struct AttributeBuffPool {
    double value { 0 };
    std::map<uint8_t, double> attributes;
};

// Value-owned state; no component references survive ApplyPoint callbacks.
struct BuffOnAttrs {
    std::map<uint8_t, AttributeBuffPool> pools;
    std::map<uint8_t, double> pending;
    uint64_t dispatchToken { 0 };
};
}
