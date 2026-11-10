#pragma once

#include <entt/entt.hpp>
#include <string>
#include <string_view>

namespace nexoria::benchmark::equipment {
// For logged-in development characters only. All temporary items remain
// non-persistent; the original worn items and character points are snapshotted.
bool Apply(entt::entity character, int tier, std::string_view weaponDefense,
           bool defender, std::string& message);
void Restore(entt::entity character, std::string& message);
bool IsActive(entt::entity character);
} // namespace nexoria::benchmark::equipment
