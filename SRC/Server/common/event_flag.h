#pragma once

#include <charconv>
#include <cmath>
#include <cstring>
#include <string_view>
#include "tables.h"

namespace EventFlag {
inline bool ParseValue(std::string_view text, int64_t& value)
{
    if (text.empty()) return false;
    int64_t parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        return false;
    value = parsed;
    return true;
}

// The bundled Lua uses double numbers. Larger exact integers use decimal strings.
inline bool FromNumber(double number, int64_t& value)
{
    constexpr double maxExactInteger = 9007199254740991.0;
    if (!std::isfinite(number) || std::trunc(number) != number ||
        number < -maxExactInteger || number > maxExactInteger)
        return false;
    value = static_cast<int64_t>(number);
    return true;
}

inline bool BuildPacket(std::string_view name, int64_t value, TPacketSetEventFlag& packet)
{
    if (name.empty() || name.size() > EVENT_FLAG_NAME_MAX_LEN || name.find('\0') != std::string_view::npos)
        return false;
    packet = {};
    std::memcpy(packet.szFlagName, name.data(), name.size());
    packet.lValue = value;
    return true;
}
} // namespace EventFlag
