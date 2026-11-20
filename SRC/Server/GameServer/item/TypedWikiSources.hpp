#pragma once

#include <cstddef>
#include <entt/entity/fwd.hpp>

inline constexpr std::size_t TypedWikiSourcesRequestSize = 12;
void HandleTypedWikiSources(entt::entity player, const char* data);
