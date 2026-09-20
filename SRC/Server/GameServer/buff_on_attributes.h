#pragma once
#include <cstdint>
#include <map>

namespace ecs {
struct AttributeBuffPool {
    uint8_t value { 0 };
    std::map<uint8_t, int> attributes;
};

// Value-owned state; no component references survive ApplyPoint callbacks.
struct BuffOnAttrs {
    std::map<uint8_t, AttributeBuffPool> pools;
    std::map<uint8_t, int> pending;
    uint64_t dispatchToken { 0 };
};
}
