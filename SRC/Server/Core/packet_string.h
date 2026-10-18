#pragma once

#include <cstddef>
#include <cstring>

namespace packet_string {
template <std::size_t N>
bool IsTerminated(const char (&field)[N])
{
    return std::memchr(field, '\0', N) != nullptr;
}
}
