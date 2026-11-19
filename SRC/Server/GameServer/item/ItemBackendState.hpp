#pragma once
#include <cstdint>

namespace ecs
{
// A token fences late completions after logout/relogin. Native state is mandatory:
// a failed owner lookup closes the login instead of loading a second inventory.
struct TypedItemLogin
{
    std::uint64_t token = 0;
    enum class Mode { Pending, Native, Unavailable };
    Mode mode = Mode::Pending;
};
}
